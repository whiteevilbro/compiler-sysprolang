#include "../memory/managment.h"
#include "hashmap_base.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#define HASHMAP_SIZE_DEFAULT 32

#define POWERSOFTWO
#ifdef POWERSOFTWO
  #define HASHMAP_MOD_SIZE(map, val) ((val) & ((map)->table_size - 1))
#else
  #define HASHMAP_MOD_SIZE(map, val) ((val) % ((map)->table_size));
#endif
#define HASHMAP_PROBE_NEXT(map, index) HASHMAP_MOD_SIZE(map, (index) + 1)

typedef size_t (*hash_f)(const void*);
typedef int (*cmp_f)(const void*, const void*);

struct hashmap_entry {
  const void* key;
  void* data;
};

static inline size_t hashmap_calc_size(const struct hashmap_base* map, size_t size) {
  if (size * 4 < map->table_size * 3)
    return map->table_size;

  // not the best solution, but it'll do
  return map->table_size * 2;
}

static inline size_t hashmap_calc_index(const struct hashmap_base* map, const void* key) {
  size_t index = map->hash(key);

  return HASHMAP_MOD_SIZE(map, index);
}

void hashmap_base_init(struct hashmap_base* map, hash_f hash, cmp_f cmp) {
  assert(hash != NULL);
  assert(cmp != NULL);

  *map = (struct hashmap_base) {};

  map->table_size = HASHMAP_SIZE_DEFAULT;
  map->table      = (struct hashmap_entry*) scalloc(map->table_size, sizeof(struct hashmap_entry));
  map->hash       = hash;
  map->compare    = cmp;
}

static struct hashmap_entry* hashmap_entry_find(const struct hashmap_base* map, const void* key, bool find_empty) {
  struct hashmap_entry* entry;

  size_t index = hashmap_calc_index(map, key);

  for (size_t i = 0; i < map->table_size; i++) {
    entry = &map->table[index];
    if (!entry->key) {
      if (find_empty)
        return entry;
      return NULL;
    }
    if (!map->compare(key, entry->key))
      return entry;
    index = HASHMAP_PROBE_NEXT(map, index);
  }
  return NULL;
}

static void hashmap_rehash(struct hashmap_base* map, size_t table_size) {
  size_t old_table_size           = map->table_size;
  struct hashmap_entry* old_table = map->table;

  struct hashmap_entry* new_table;
  struct hashmap_entry* entry;
  struct hashmap_entry* new_entry;

  assert(table_size >= map->table_size);

  new_table = (struct hashmap_entry*) scalloc(table_size, sizeof(struct hashmap_entry));

  map->table_size = table_size;
  map->table      = new_table;

  if (old_table) {
    for (entry = old_table; entry < old_table + old_table_size; entry++) {
      if (!entry->key)
        continue;
      new_entry = hashmap_entry_find(map, entry->key, true);

      assert(new_entry != NULL);

      *new_entry = *entry;
    }
    free(old_table);
  }
  return;
}

int hashmap_base_insert(struct hashmap_base* map, const void* key, void* data) {
  if (!key || !data) {
    return -1;
  }

  size_t table_size = table_size = hashmap_calc_size(map, map->size);
  if (table_size > map->table_size) {
    hashmap_rehash(map, table_size);
  }

  struct hashmap_entry* entry = hashmap_entry_find(map, key, true);

  assert(entry != NULL);

  if (!entry->key) {
    if (map->key_dup) {
      entry->key = map->key_dup(key);
    } else {
      entry->key = key;
    }
    map->size++;
  }

  entry->data = data;
  return 0;
}

void* hashmap_base_get(struct hashmap_base* map, const void* key) {
  if (!key)
    return NULL;

  struct hashmap_entry* entry = hashmap_entry_find(map, key, true);
  if (!entry)
    return NULL;
  return entry->data;
}

static void hashmap_free_keys(struct hashmap_base* map) {
  if (!map || !map->key_dup)
    return;

  for (struct hashmap_entry* entry = map->table; entry < map->table + map->table_size; entry++) {
    if (entry->key)
      map->key_free((void*) entry->key);
  }
}

void hashmap_base_cleanup(struct hashmap_base* map) {
  if (!map) {
    return;
  }
  hashmap_free_keys(map);
  free(map->table);
  *map = (struct hashmap_base) {};
}

void hashmap_base_set_key_alloc_funcs(struct hashmap_base* map, const void* (*key_dup_f)(const void*), void (*key_free_f)(void*)) {
  assert(map->size == 0);

  map->key_dup  = key_dup_f;
  map->key_free = key_free_f;
}
