#ifndef HASHMAP_BASE_H
#define HASHMAP_BASE_H

#include <stddef.h>
struct hashmap_entry;

struct hashmap_base {
  size_t table_size;
  size_t size;
  struct hashmap_entry* table;
  size_t (*hash)(const void*);
  int (*compare)(const void*, const void*);
  const void* (*key_dup)(const void*);
  void (*key_free)(void*);
};

void hashmap_base_init(struct hashmap_base*, size_t (*)(const void*), int (*)(const void*, const void*));
int hashmap_base_insert(struct hashmap_base*, const void*, void*);
void* hashmap_base_get(struct hashmap_base*, const void*);
void hashmap_base_cleanup(struct hashmap_base*);
void hashmap_base_set_key_alloc_funcs(struct hashmap_base*, const void* (*) (const void*), void (*)(void*));

#endif
