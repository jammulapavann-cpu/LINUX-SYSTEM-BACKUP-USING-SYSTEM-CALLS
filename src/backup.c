#define _XOPEN_SOURCE 700
#include "backup.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <utime.h>
#include <limits.h>

static int ensure_dir(const char *path, mode_t mode) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; return -1; }
        return 0;
    }
    if (errno != ENOENT) return -1;
    char tmp[PATH_SIZE];
    if (snprintf(tmp, sizeof(tmp), "%s", path) >= (int)sizeof(tmp)) { errno = ENAMETOOLONG; return -1; }
    size_t len = strlen(tmp);
    while (len > 1 && tmp[len-1] == '/') tmp[--len] = '\0';
    for (char *p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) < 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) < 0 && errno != EEXIST) return -1;
    return 0;
}

static int join_path(char *out, size_t n, const char *a, const char *b) {
    if (!a || !b) return -1;
    int r = snprintf(out, n, "%s/%s", a, b);
    return (r < 0 || (size_t)r >= n) ? -1 : 0;
}

static int copy_file(const char *src, const char *dst, const struct stat *src_st) {
    int in = open(src, O_RDONLY);
    if (in < 0) return -1;
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, src_st->st_mode & 0777);
    if (out < 0) { close(in); return -1; }
    char buf[65536];
    ssize_t n;
    int rc = 0;
    while ((n = read(in, buf, sizeof(buf))) > 0) {
        ssize_t off = 0;
        while (off < n) {
            ssize_t w = write(out, buf + off, (size_t)(n - off));
            if (w < 0) { rc = -1; break; }
            off += w;
        }
        if (rc < 0) break;
    }
    if (n < 0) rc = -1;
    if (close(in) < 0) rc = -1;
    if (close(out) < 0) rc = -1;
    if (rc == 0) {
        struct timespec times[2];
        times[0] = src_st->st_atim;
        times[1] = src_st->st_mtim;
        utimensat(AT_FDCWD, dst, times, 0);
    }
    return rc;
}

static int needs_copy(const char *dst, const struct stat *src_st) {
    struct stat dst_st;
    if (stat(dst, &dst_st) < 0) return 1;
    if (!S_ISREG(dst_st.st_mode)) return 1;
    return dst_st.st_size != src_st->st_size || dst_st.st_mtime != src_st->st_mtime || ((dst_st.st_mode & 0777) != (src_st->st_mode & 0777));
}

static int process_tree(const char *src, const char *dst, BackupStats *stats, int restore_mode) {
    DIR *dir = opendir(src);
    if (!dir) return -1;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
        char s[PATH_SIZE], d[PATH_SIZE];
        if (join_path(s, sizeof(s), src, ent->d_name) || join_path(d, sizeof(d), dst, ent->d_name)) { stats->errors++; continue; }
        struct stat st;
        if (lstat(s, &st) < 0) { stats->errors++; log_message("ERROR", "stat failed: %s (%s)", s, strerror(errno)); continue; }
        if (S_ISDIR(st.st_mode)) {
            if (ensure_dir(d, st.st_mode & 0777) < 0) { stats->errors++; log_message("ERROR", "mkdir failed: %s (%s)", d, strerror(errno)); continue; }
            if (process_tree(s, d, stats, restore_mode) < 0) stats->errors++;
        } else if (S_ISREG(st.st_mode)) {
            stats->scanned++;
            if (needs_copy(d, &st)) {
                if (copy_file(s, d, &st) == 0) {
                    stats->copied++;
                    log_message("INFO", "%s: %s", restore_mode ? "RESTORED" : "COPIED", s);
                } else {
                    stats->errors++;
                    log_message("ERROR", "copy failed: %s -> %s (%s)", s, d, strerror(errno));
                }
            } else {
                stats->skipped++;
                log_message("INFO", "SKIPPED unchanged: %s", s);
            }
        } else {
            log_message("WARN", "Unsupported file type skipped: %s", s);
        }
    }
    closedir(dir);
    return 0;
}

static int delete_extras_recursive(const char *src, const char *dst, BackupStats *stats) {
    DIR *dir = opendir(dst);
    if (!dir) return -1;
    struct dirent *ent;
    while ((ent = readdir(dir))) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
        char s[PATH_SIZE], d[PATH_SIZE];
        if (join_path(s, sizeof(s), src, ent->d_name) || join_path(d, sizeof(d), dst, ent->d_name)) continue;
        struct stat dstst;
        if (lstat(d, &dstst) < 0) continue;
        struct stat srcst;
        if (lstat(s, &srcst) < 0 && errno == ENOENT) {
            if (S_ISDIR(dstst.st_mode)) {
                delete_extras_recursive("", d, stats);
                rmdir(d);
            } else if (S_ISREG(dstst.st_mode)) unlink(d);
            stats->deleted++;
            log_message("INFO", "DELETED extra: %s", d);
        } else if (S_ISDIR(dstst.st_mode) && S_ISDIR(srcst.st_mode)) {
            delete_extras_recursive(s, d, stats);
        }
    }
    closedir(dir);
    return 0;
}

int run_backup(const char *source, const char *destination, BackupStats *stats) {
    memset(stats, 0, sizeof(*stats));
    struct stat st;
    if (stat(source, &st) < 0 || !S_ISDIR(st.st_mode)) return -1;
    if (ensure_dir(destination, 0755) < 0) return -1;
    log_message("INFO", "BACKUP STARTED: %s -> %s", source, destination);
    int rc = process_tree(source, destination, stats, 0);
    log_message("INFO", "BACKUP COMPLETED: copied=%zu skipped=%zu errors=%zu", stats->copied, stats->skipped, stats->errors);
    return rc;
}

int run_sync(const char *source, const char *destination, int mirror, BackupStats *stats) {
    int rc = run_backup(source, destination, stats);
    if (rc < 0) return rc;
    if (mirror) delete_extras_recursive(source, destination, stats);
    log_message("INFO", "SYNC COMPLETED: deleted=%zu", stats->deleted);
    return 0;
}

int run_restore(const char *backup, const char *destination, BackupStats *stats) {
    memset(stats, 0, sizeof(*stats));
    struct stat st;
    if (stat(backup, &st) < 0 || !S_ISDIR(st.st_mode)) return -1;
    if (ensure_dir(destination, 0755) < 0) return -1;
    log_message("INFO", "RESTORE STARTED: %s -> %s", backup, destination);
    int rc = process_tree(backup, destination, stats, 1);
    log_message("INFO", "RESTORE COMPLETED: copied=%zu skipped=%zu errors=%zu", stats->copied, stats->skipped, stats->errors);
    return rc;
}

static void status_tree(const char *src, const char *dst, size_t *newc, size_t *changed, size_t *same) {
    DIR *dir = opendir(src);
    if (!dir) return;
    struct dirent *ent;
    while ((ent = readdir(dir))) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
        char s[PATH_SIZE], d[PATH_SIZE];
        if (join_path(s, sizeof(s), src, ent->d_name) || join_path(d, sizeof(d), dst, ent->d_name)) continue;
        struct stat st;
        if (lstat(s, &st) < 0) continue;
        if (S_ISDIR(st.st_mode)) status_tree(s, d, newc, changed, same);
        else if (S_ISREG(st.st_mode)) {
            struct stat dt;
            if (stat(d, &dt) < 0) (*newc)++;
            else if (dt.st_size != st.st_size || dt.st_mtime != st.st_mtime) (*changed)++;
            else (*same)++;
        }
    }
    closedir(dir);
}

int run_status(const char *source, const char *destination) {
    size_t n=0,c=0,s=0;
    status_tree(source, destination, &n, &c, &s);
    printf("\nBackup Status\n------------\nNew files      : %zu\nModified files : %zu\nUnchanged files: %zu\n\n", n,c,s);
    return 0;
}
