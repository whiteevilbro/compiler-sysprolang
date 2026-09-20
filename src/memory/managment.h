#ifndef MEMMGNT_H
#define MEMMGNT_H

#include <stddef.h>

void* smalloc(size_t size);
void* srealloc(void* ptr, size_t new_size);

#endif
