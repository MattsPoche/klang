#ifndef MEM_H_
#define MEM_H_

struct mem_region {
	struct mem_region *link;
	uint8_t *head;
	uint8_t *end;
	uint8_t data[];
};

struct mem_arena {
	struct mem_region *begin;
	struct mem_region *end;
};

#define MEM_REGION_SIZE 0x1000
#define MEM_REGION_USABLE (MEM_REGION_SIZE - sizeof(struct mem_region))

struct mem_region *mem_region_create(size_t sz);
struct mem_region *mem_region_create_default(void);
size_t mem_region_allocated(struct mem_region *r);
size_t mem_region_remaining(struct mem_region *r);
void mem_region_align_head(struct mem_region *r);
void *mem_arena_alloc_small(struct mem_arena *arena, size_t sz);
void *mem_arena_alloc_large(struct mem_arena *arena, size_t sz);
void *mem_arena_alloc(struct mem_arena *arena, size_t sz);
void *mem_arena_zero_alloc(struct mem_arena *arena, size_t sz);
void mem_arena_free(struct mem_arena *arena);

#endif /* MEM_H_ */

#ifdef MEM_H_IMPLEMENTATION

static uintptr_t word_align(uintptr_t x);

/* --- MEM ARENA --- */
struct mem_region *mem_region_create(size_t sz)
{
	struct mem_region *r = calloc(sz, 1);
	assert(r != NULL);
	r->link = NULL;
	r->head = r->data;
	r->end = (uint8_t *)r + sz;
	return r;
}

struct mem_region *mem_region_create_default(void)
{
	return mem_region_create(MEM_REGION_SIZE);
}

size_t mem_region_allocated(struct mem_region *r)
{
	return r->head - r->data;
}

size_t mem_region_remaining(struct mem_region *r)
{
	return r->end - r->head;
}

uintptr_t word_align(uintptr_t x)
{
	static const size_t alignment = 16;
	static const size_t mask = alignment - 1;
	return x & mask ? (x & ~mask) + alignment : x;
}

/* Memory is allocated on 16 byte boundries, so we always keep the
 * region head aligned.
 */
void mem_region_align_head(struct mem_region *r)
{
	r->head = (uint8_t *)word_align((uintptr_t)r->head);
}

void *mem_arena_alloc_small(struct mem_arena *arena, size_t sz)
{
	if (arena->end == NULL) {
		arena->begin = mem_region_create_default();
		arena->end = arena->begin;
	} else if (sz >= mem_region_remaining(arena->end)) {
		arena->end->link = mem_region_create_default();
		arena->end = arena->end->link;
	}
	/* TODO: This looks like a bug. The offset needed to align head
	 * is not included in sz.
	 */
	mem_region_align_head(arena->end);
	struct mem_region *r = arena->end;
	void *ptr = r->head;
	r->head += sz;
	return ptr;
}

void *mem_arena_alloc_large(struct mem_arena *arena, size_t sz)
{
	struct mem_region *r = NULL;
	if (arena->end == NULL) {
		arena->begin = mem_region_create(sizeof(struct mem_region) + sz);
		arena->end = arena->begin;
		r = arena->begin;
	} else {
		r = mem_region_create(sizeof(struct mem_region) + sz);
		r->link = arena->begin;
		arena->begin = r;
	}
	r->head = r->end;
	return r->data;
}

void *mem_arena_alloc(struct mem_arena *arena, size_t sz)
{
	if (sz < MEM_REGION_USABLE) {
		return mem_arena_alloc_small(arena, sz);
	} else {
		return mem_arena_alloc_large(arena, sz);
	}
}

void *mem_arena_zero_alloc(struct mem_arena *arena, size_t sz)
{
	void *ptr = mem_arena_alloc(arena, sz);
	memset(ptr, 0, sz);
	return ptr;
}

void mem_arena_free(struct mem_arena *arena)
{
	struct mem_region *r = arena->begin;
	while (r != NULL) {
		struct mem_region *tmp = r;
		r = r->link;
		free(tmp);
	}
	arena->begin = NULL;
	arena->end = NULL;
}

#undef MEM_H_IMPLEMENTATION
#endif /* MEM_H_IMPLEMENTATION */
