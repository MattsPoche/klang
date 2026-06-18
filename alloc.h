#pragma once

#include "stdlib.h"

void *kc_malloc(size_t size);
void kc_free(void *ptr);
void *kc_calloc(size_t n, size_t size);
void *kc_realloc(void *ptr, size_t size);

#define MEM_ALLOC_ARRAY(type, n) kc_calloc(n, sizeof(type))
#define MEM_ALLOC(type) MEM_ALLOC_ARRAY(type, 1)
#define MALLOC  kc_malloc
#define CALLOC  kc_calloc
#define REALLOC kc_realloc
#define FREE    kc_free
