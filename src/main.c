#include "backup.h"
#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static void usage(const char *p) {
    printf("Linux File Backup & Synchronization System\n\n");
    printf("Usage:\n");
    printf("  %s backup  <source> <destination>\n", p);
    printf("  %s sync    <source> <destination> [--mirror]\n", p);
    printf("  %s restore <backup> <destination>\n", p);
    printf("  %s status  <source> <destination>\n", p);
    printf("  %s log\n", p);
    printf("  %s --help\n", p);
    printf("\nOptions:\n  --mirror  Make destination mirror source by removing extra files.\n");
}

static void print_stats(const BackupStats *s) {
    printf("\n----------------------------------------\n");
    printf("Files scanned : %zu\n", s->scanned);
    printf("Files copied  : %zu\n", s->copied);
    printf("Files skipped : %zu\n", s->skipped);
    printf("Files deleted : %zu\n", s->deleted);
    printf("Errors        : %zu\n", s->errors);
    printf("----------------------------------------\n");
}

int main(int argc, char **argv) {
    logger_init("logs/backup.log");
    if (argc < 2 || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
        usage(argv[0]);
        return argc < 2 ? 1 : 0;
    }
    if (!strcmp(argv[1], "log")) {
        FILE *fp = fopen("logs/backup.log", "r");
        if (!fp) { perror("logs/backup.log"); return 1; }
        char line[4096]; while (fgets(line, sizeof(line), fp)) fputs(line, stdout);
        fclose(fp); return 0;
    }
    BackupStats stats;
    int rc = -1;
    if (!strcmp(argv[1], "backup") && argc == 4) rc = run_backup(argv[2], argv[3], &stats);
    else if (!strcmp(argv[1], "sync") && (argc == 4 || (argc == 5 && !strcmp(argv[4], "--mirror")))) rc = run_sync(argv[2], argv[3], argc == 5, &stats);
    else if (!strcmp(argv[1], "restore") && argc == 4) rc = run_restore(argv[2], argv[3], &stats);
    else if (!strcmp(argv[1], "status") && argc == 4) return run_status(argv[2], argv[3]);
    else { usage(argv[0]); return 1; }
    if (rc < 0) { fprintf(stderr, "Operation failed: %s\n", strerror(errno)); return 1; }
    print_stats(&stats);
    return stats.errors ? 2 : 0;
}
