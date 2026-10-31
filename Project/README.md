# LINUX-SYSTEM-BACKUP-USING-SYSTEM-CALLS

Linux File Backup and Synchronization System Using System Calls

OSSP (25CS2104E) project for Team 24.

## Features
- Recursive directory backup
- Incremental backup using file size and modification time
- Synchronization
- Optional mirror mode to remove files that no longer exist in the source
- File restoration
- Backup status reporting
- Timestamped operation logging
- POSIX/Linux APIs and system calls: `open()`, `read()`, `write()`, `close()`, `stat()`, `lstat()`, `mkdir()`, `opendir()`, `readdir()`, `unlink()`, `rmdir()`

## Requirements
Ubuntu Linux, WSL Ubuntu, or another POSIX-like Linux environment with GCC and Make.

## Build
```bash
make
```

## Run
```bash
./backup --help
./backup backup ./test_data/source ./test_data/backup
./backup status ./test_data/source ./test_data/backup
./backup sync ./test_data/source ./test_data/backup --mirror
./backup restore ./test_data/backup ./test_data/restored
./backup log
```

## Demo
1. Put files in `test_data/source`.
2. Run the backup command.
3. Modify one file and add another.
4. Run backup again. Unchanged files are skipped.
5. Use `status` to show new/modified/unchanged files.
6. Use `restore` to recover files.
7. Use `sync --mirror` only when you want the destination to mirror the source, including deletion of extra destination files.

## Project mapping to OSSP abstract
- Backup module: recursive file copy using Linux file APIs.
- Change detection: `stat()` metadata (size and modification time).
- Incremental backup: only changed/new files are copied.
- Synchronization: source and destination are brought into consistency.
- Restoration: backup contents can be copied to a destination.
- Logging: timestamped operations are written to `logs/backup.log`.

## Team responsibilities
- Pavan: system design, synchronization module, documentation, testing, logging.
- Shiva Prasad: backup module, Linux system-call integration, GitHub repository management.
