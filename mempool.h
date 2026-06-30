//#define MEMPOOL_IMPLEMENTATION
#ifndef MEMPOOL_H_
#define MEMPOOL_H_

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef unsigned char ubyte;

struct mem_pool_bump_buffer;
struct mem_pool_resizable_buffer;

typedef struct {
    struct mem_pool_bump_buffer *first;
    struct mem_pool_bump_buffer *last;
    struct mem_pool_resizable_buffer *arrays;
} Mem_pool;

#define MEM_POOL_PTR_SZ(ptr) (((size_t *)(void *)(ptr))[-2])

Mem_pool mem_pool_create(void);
void mem_pool_destroy(Mem_pool *pool);
void *mem_pool_alloc(Mem_pool *pool, size_t size);
void *mem_pool_alloc_resizable(Mem_pool *pool, size_t size);
void *mem_pool_realloc_resizable(Mem_pool *pool, void *resizable_ptr, size_t size);
void mem_pool_free_resizable(Mem_pool *pool, void *resizable_ptr);

#endif /* MEMPOOL_H_ */

#ifdef MEMPOOL_IMPLEMENTATION

typedef struct mem_pool_bump_buffer {
    struct mem_pool_bump_buffer *next;
    struct mem_pool_bump_buffer *prev;
    ubyte *bump;
    ubyte *end;
    ubyte data[];
} Bump_buffer;

typedef struct mem_pool_resizable_buffer {
    struct mem_pool_resizable_buffer *next;
    struct mem_pool_resizable_buffer *prev;
    ubyte data[];
} Resizable_buffer;

#define PAGE_SIZE 4096
#define BUF_MAX   (PAGE_SIZE - sizeof(Bump_buffer))
#define ALIGN(ptr)                                                      \
    ((typeof(ptr))((uintptr_t)(ptr) & 0x0f ? ((uintptr_t)(ptr) & ~0x0f) + 0x10 : (uintptr_t)(ptr)))

static inline Bump_buffer *
bb_create_with_size(size_t size)
{
    Bump_buffer *buf = malloc(sizeof(*buf) + size);
    assert(buf != NULL);
    buf->prev = NULL;
    buf->next = NULL;
    buf->end = buf->data + size;
    buf->bump = buf->data;
    return buf;
}

static inline size_t
bb_usable_size(Bump_buffer *buf)
{
    return buf->end - buf->bump;
}

static inline size_t
bb_total_allocated(Bump_buffer *buf)
{
    return buf->bump - (ubyte *)buf;
}

static inline Bump_buffer *
bb_create(void)
{
    return bb_create_with_size(PAGE_SIZE);
}

static inline void *
mem_pool_alloc_large(Mem_pool *pool, size_t size)
{
    Bump_buffer *buf = bb_create_with_size(size);
    pool->first->prev = buf;
    buf->next = pool->first;
    pool->first = buf;
    void *ptr = buf->bump;
    buf->bump += size;
    *(size_t *)ptr = size;
    return ptr;
}

Mem_pool
mem_pool_create(void)
{
    Mem_pool pool = {0};
    pool.first = pool.last = bb_create();
    return pool;
}

void
mem_pool_destroy(Mem_pool *pool)
{
    {
        Bump_buffer *buf = pool->first;
        Bump_buffer *next;
        while (buf) {
            next = buf->next;
            free(buf);
            buf = next;
        }
    }
    {
        Resizable_buffer *buf = pool->arrays;
        Resizable_buffer *next;
        while (buf) {
            next = buf->next;
            free(buf);
            buf = next;
        }
    }
    pool->first  = NULL;
    pool->last   = NULL;
    pool->arrays = NULL;
}

void *
mem_pool_alloc(Mem_pool *pool, size_t size)
{
    if (size > BUF_MAX) return mem_pool_alloc_large(pool, size);
    Bump_buffer *buf = pool->last;
    void *ptr = NULL;
    if (buf->bump >= buf->end || size >= bb_usable_size(buf)) {
        buf = bb_create();
        pool->last->next = buf;
        buf->prev = pool->last;
        pool->last = buf;
    }
    ptr = buf->bump;
    buf->bump = ALIGN(ptr + size);
    *(size_t *)ptr = size;
    return ptr;
}

void *
mem_pool_alloc_resizable(Mem_pool *pool, size_t size)
{
    Resizable_buffer *buf = malloc(sizeof(*buf) + size);
    assert(buf != NULL);
    if (pool->arrays) {
        pool->arrays->prev = buf;
        buf->next = pool->arrays;
        buf->prev = NULL;
        pool->arrays = buf;
    } else {
        buf->next = NULL;
        buf->prev = NULL;
        pool->arrays = buf;
    }
    return buf->data;
}

void *
mem_pool_realloc_resizable(Mem_pool *pool, void *resizable_ptr, size_t size)
{
    Resizable_buffer *buf     = (Resizable_buffer *)resizable_ptr - 1;
    Resizable_buffer *prev    = buf->prev;
    Resizable_buffer *next    = buf->next;
    Resizable_buffer *new_buf = realloc(buf, sizeof(*buf) + size);
    assert(new_buf != NULL);
    if (new_buf == buf) return new_buf->data;
    if (prev) prev->next   = new_buf;
    else      pool->arrays = new_buf;
    if (next) next->prev   = new_buf;
    return new_buf->data;
}

void
mem_pool_free_resizable(Mem_pool *pool, void *resizable_ptr)
{
    Resizable_buffer *buf     = (Resizable_buffer *)resizable_ptr - 1;
    Resizable_buffer *prev    = buf->prev;
    Resizable_buffer *next    = buf->next;
    if (prev) prev->next   = next;
    else      pool->arrays = next;
    if (next) next->prev   = prev;
    free(buf);
}

#endif /* MEMPOOL_IMPLEMENTATION */
