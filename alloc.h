#pragma once

#include "mempool.h"

extern Mem_pool *KC_HEAP;

#define PAGE_SIZE 4096LU

#define MALLOC(sz) mem_pool_alloc(KC_HEAP, sz)
#define CALLOC(n, sz) memset(mem_pool_alloc(KC_HEAP, (n) * (sz)), 0, (n) * (sz))
#define REALLOC assert(false && "REALLOC UNIMPLEMENTED");
#define FREE(sz)
#define MEM_ALLOC_ARRAY(type, n) CALLOC(n, sizeof(type))
#define MEM_ALLOC(type) MEM_ALLOC_ARRAY(type, 1)

#define DA_MALLOC(sz)       mem_pool_alloc_resizable(KC_HEAP, sz)
#define DA_REALLOC(ptr, sz) mem_pool_realloc_resizable(KC_HEAP, ptr, sz)
#define DA_FREE(ptr)        mem_pool_free_resizable(KC_HEAP, ptr)

#define BB_MALLOC(sz)       mem_pool_alloc_resizable(KC_HEAP, sz)
#define BB_REALLOC(ptr, sz) mem_pool_realloc_resizable(KC_HEAP, ptr, sz)
#define BB_FREE(ptr)        mem_pool_free_resizable(KC_HEAP, ptr)
