#include "hashing.h"

size_t string_hash(const void* p) {
  const char* c = (const char*) p;
  size_t hash   = 0;

  for (; *c; c++) {
    hash += *c;
    hash += (hash << 10);
    hash ^= (hash >> 6);
  }
  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);
  return hash;
}
