#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>

static char log_path[4096] = "backup.log";

void logger_init(const char *path) {
    if (path && *path) {
        snprintf(log_path, sizeof(log_path), "%s", path);
    }
}

void log_message(const char *level, const char *fmt, ...) {
    FILE *fp = fopen(log_path, "a");
    if (!fp) return;
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_now);
    fprintf(fp, "[%s] [%s] ", ts, level ? level : "INFO");
    va_list ap;
    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);
    fputc('\n', fp);
    fclose(fp);
}
