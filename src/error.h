#ifndef ERROR_H
#define ERROR_H

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#define NO_LOCATION ((size_t) ~0)
#define LOG_BUFFER_SIZE 1024

typedef enum {
  DEBUG   = 0,
  INFO    = 1,
  WARNING = 2,
  ERROR   = 3,
} LogLevel;

typedef struct Logger {
  LogLevel level;
  FILE* stream;
  char* buffer;
} Logger;

[[gnu::format(printf, 4, 5)]]
void log_write(Logger*, LogLevel, size_t, const char*, ...);

void vlog_write(Logger*, LogLevel, size_t, const char*, va_list);

#endif
