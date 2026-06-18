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

typedef struct bump_buffer {
	struct bump_buffer *next;
	struct bump_buffer *prev;
	ubyte *bump;
	ubyte *end;
	ubyte data[];
} Bump_buffer;

typedef struct {
	Bump_buffer *first;
	Bump_buffer *last;
} Mem_pool;

#define MEM_POOL_PTR_SZ(ptr) (((size_t *)(void *)(ptr))[-2])

Mem_pool mem_pool_create(void);
void mem_pool_destroy(Mem_pool *pool);
void *mem_pool_alloc(Mem_pool *pool, size_t size);
void mem_pool_total_allocated(Mem_pool *pool, size_t *total_out, size_t *bufc_out);

#endif /* MEMPOOL_H_ */

#ifdef MEMPOOL_IMPLEMENTATION

#define PAGE_SIZE 4096
#define BUF_MAX   (PAGE_SIZE - sizeof(Bump_buffer))

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

static inline Bump_buffer *
bb_create(void)
{
	Bump_buffer *buf = malloc(PAGE_SIZE);
	assert(buf != NULL);
	buf->prev = NULL;
	buf->next = NULL;
	buf->end = (ubyte *)buf + PAGE_SIZE;
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

static inline uintptr_t
align_adjust(uintptr_t x)
{
	uintptr_t mask = 0xf - 1;
	return x & mask ? (x & ~mask) + 0x10 : x;
}

static inline void *
mem_pool_alloc_large(Mem_pool *pool, size_t size)
{
	Bump_buffer *buf = bb_create_with_size(size + 0x10);
	pool->first->prev = buf;
	buf->next = pool->first;
	pool->first = buf;
	void *ptr = buf->bump;
	buf->bump += size + 0x10;
	*(size_t *)ptr = size;
	return ptr + 0x10;
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
	Bump_buffer *buf = pool->first;
	Bump_buffer *next;
	while (buf) {
		next = buf->next;
		free(buf);
		buf = next;
	}
	pool->first = NULL;
	pool->last  = NULL;
}

void *
mem_pool_alloc(Mem_pool *pool, size_t size)
{
	if (size + 0x10 > BUF_MAX) return mem_pool_alloc_large(pool, size);
	Bump_buffer *buf = pool->last;
	void *ptr = NULL;
	if (buf->bump >= buf->end || size + 0x10 >= bb_usable_size(buf)) {
		buf = bb_create();
		pool->last->next = buf;
		buf->prev = pool->last;
		pool->last = buf;
	}
	ptr = buf->bump;
	buf->bump = (void *)align_adjust((uintptr_t)buf->bump + size + 0x10);
	*(size_t *)ptr = size;
	return ptr + 0x10;
}

void
mem_pool_total_allocated(Mem_pool *pool, size_t *total_out, size_t *bufc_out)
{
	Bump_buffer *buf = pool->first;
	size_t total = 0;
	size_t bufc = 0;
	for (; buf; buf = buf->next) {
		total += bb_total_allocated(buf);
		bufc  += 1;
	}
	if (total_out) *total_out = total;
	if (bufc_out)  *bufc_out  = bufc;
}

#endif /* MEMPOOL_IMPLEMENTATION */
