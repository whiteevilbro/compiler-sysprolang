#include "./vector.h"

#include "../memory/managment.h"

#include <stdlib.h>
#include <string.h>

void vec_push(struct Vec* ptr, const size_t sizeof_data, const void* data) {
  if (ptr->len >= ptr->cap) {
    ptr->cap++;
    ptr->cap *= 2;
    ptr->data = srealloc(ptr->data, ptr->cap * sizeof_data);
  }
  memcpy((char*) ptr->data + ptr->len * sizeof_data, data, sizeof_data);
  ptr->len++;
}

void vec_pop(struct Vec* ptr) {
  ptr->len--;
}

void* vec_get_ptr(const struct Vec* ptr, const size_t sizeof_data, const size_t idx) {
  return (char*) ptr->data + idx * sizeof_data;
}

void vec_free(struct Vec* ptr) {
  free(ptr->data);
}
