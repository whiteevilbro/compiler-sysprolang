#include "./managment.h"

#include <stddef.h>
#include <stdlib.h>

void* smalloc(const size_t size) {
  void* ptr = malloc(size);
  if (ptr)
    return ptr;
  exit(-2);
  // return ptr ? ptr : (exit(-1), NULL);
}

void* srealloc(void* ptr, const size_t new_size) {
  ptr = realloc(ptr, new_size);
  if (ptr)
    return ptr;
  exit(-2);
  // return ptr ? ptr : (exit(-1), NULL);
}
