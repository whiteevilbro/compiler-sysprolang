#ifndef HASHMAP_H
#define HASHMAP_H

#include "hashmap_base.h" // IWYU pragma: keep

#define HashmapDef(key_type, data_type)                                            \
  _Pragma("clang diagnostic push")                                                 \
      _Pragma("clang diagnostic ignored \"-Wzero-length-array\"") typedef struct { \
    struct hashmap_base map_base;                                                  \
                                                                                   \
    struct {                                                                       \
      const key_type* t_key;                                                       \
      data_type* t_data;                                                           \
    } map_types[0];                                                                \
                                                                                   \
    _Pragma("clang diagnostic pop")                                                \
  }

// sadly, calling a function from wrong-typed pointer is an UB, so no type-checking here
#define hashmap_init(map, hash_f, cmp_f) \
  hashmap_base_init(&((map)->map_base), (size_t (*)(const void*))(hash_f), (int (*)(const void*, const void*))(cmp_f))
// _Generic((hash_f), typeof((map)->map_types[0].t_hash_f):
// _Generic((cmp_f), typeof((map)->map_types[0].t_cmp_f):

// clang-format off
#define hashmap_insert(map, key, data)               \
  _Generic((key), typeof(((map)->map_types)->t_key):   \
  _Generic((data), typeof(((map)->map_types)->t_data): \
  hashmap_base_insert(&((map)->map_base), (const void*)(key), (void*)(data) )))

#define hashmap_get(map, key)                      \
  _Generic((key), typeof(((map)->map_types)->t_key): \
  hashmap_base_get(&((map)->map_base), (const void*)(key) ))
// clang-format on

#define hashmap_cleanup(map) hashmap_base_cleanup(&(map)->map_base)

#define hashmap_set_key_alloc_functions(map, key_dup_f, key_free_f) hashmap_base_set_key_alloc_funcs(&(map)->map_base, (key_dup_f), (key_free_f))

#endif
