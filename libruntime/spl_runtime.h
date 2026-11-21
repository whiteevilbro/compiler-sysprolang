#ifndef SPL_RUNTIME_H
#define SPL_RUNTIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

  /* Print an integer to stdout (no newline) */
  void print_int(int64_t val);

  /* Print an integer followed by newline */
  void println_int(int64_t val);

  /* Print a newline only */
  void println(void);

  /* Print a string to stdout */
  void print_string(const char* val);

  /* Print a string followed by newline */
  void println_string(const char* val);

#ifdef __cplusplus
}
#endif

#endif /* SPL_RUNTIME_H */
