#ifndef LOGGER_H
#define LOGGER_H

void logger_init(const char *path);
void log_message(const char *level, const char *fmt, ...);

#endif
