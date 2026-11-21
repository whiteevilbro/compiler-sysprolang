#include "error.h"

#include <stdarg.h>
#include <stdio.h>

void log_write(Logger* logger, LogLevel level, size_t location, const char* format, ...) {
  va_list args;
  va_start(args, format); // second argument might be ommited since C23, but ill keep it
  vlog_write(logger, level, location, format, args);
  va_end(args);
}

void vlog_write(Logger* logger, LogLevel level, [[maybe_unused]] size_t location, const char* format, va_list args) {
  if (level < logger->level)
    return;
  if (logger->stream) {
    vfprintf(logger->stream, format, args);
  }
}
