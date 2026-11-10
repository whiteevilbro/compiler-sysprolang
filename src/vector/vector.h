#ifndef VECTOR_H
#define VECTOR_H

#include <stddef.h>

struct Vec {
  size_t len;
  size_t cap;
  void* data;
};

#define VecDef(type)                                                               \
  _Pragma("clang diagnostic push")                                                 \
      _Pragma("clang diagnostic ignored \"-Wzero-length-array\"") typedef struct { \
    struct Vec inner; /*type magic*/                                               \
    type* phantom[0];                                                              \
    _Pragma("clang diagnostic pop")                                                \
  }

#define vecPush(vec, data) _Generic((data), typeof(**((vec)->phantom)): vec_push(&(vec)->inner, sizeof(**((vec)->phantom)), &(data)))
#define vecPop(vec) ((typeof(**((vec)->phantom)) vec_pop(&(vec)->inner)))

#define vecGetPtr(vec, idx) ((typeof(*(vec)->phantom)) vec_get_ptr(&(vec)->inner, sizeof(**((vec)->phantom)), idx))
#define vecTop(vec) (vecGetPtr((vec), vecSize((vec))))

#define vecFree(vec) vec_free(&(vec)->inner)

#define vecSize(vec) (((vec)->inner).len)

void vec_push(struct Vec* ptr, const size_t sizeof_data, const void* data);
void vec_pop(struct Vec* ptr);
void* vec_get_ptr(const struct Vec* ptr, const size_t sizeof_data, const size_t idx);
void vec_free(struct Vec* ptr);

#endif
