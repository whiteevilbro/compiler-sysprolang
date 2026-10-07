#include "spl_runtime.h"

#include <stdio.h>

void print_int(int64_t val) {
  printf("%lld", (long long) val);
}

void println_int(int64_t val) {
  printf("%lld\n", (long long) val);
}

void println(void) {
  printf("\n");
}

void print_string(const char* val) {
  if (val != NULL) {
    printf("%s", val);
  }
}

void println_string(const char* val) {
  if (val != NULL) {
    printf("%s\n", val);
  } else {
    printf("\n");
  }
}
