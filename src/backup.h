#ifndef BACKUP_H
#define BACKUP_H

#include <stddef.h>

#define PATH_SIZE 4096

typedef struct {
    size_t scanned;
    size_t copied;
    size_t skipped;
    size_t deleted;
    size_t errors;
} BackupStats;

int run_backup(const char *source, const char *destination, BackupStats *stats);
int run_sync(const char *source, const char *destination, int mirror, BackupStats *stats);
int run_restore(const char *backup, const char *destination, BackupStats *stats);
int run_status(const char *source, const char *destination);

#endif
