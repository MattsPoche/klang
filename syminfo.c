#include "common.h"

KC_INLINE uint64_t
djb_hash(const char *str, size_t len)
{
	uint64_t hash = 5381;
	while (len--) hash = ((hash << 5) + hash) + *str++;
	return hash;
}

typedef struct symbol_info *Syminfo_list;

#define HASHMAP_IMPLEMENTATION
#define HASHMAP_K             struct strview
#define HASHMAP_V             Syminfo_list
#define HASHMAP_E             Syminfo_e
#define HASHMAP_S             Syminfo_t
#define HASHMAP_P             symhash
#define HASHMAP_HASH(k)       djb_hash((k).ptr, (k).len)
#define HASHMAP_EQUALS(x,y)   sv_is_equal(x, y)
#define HASHMAP_NIL_KEY       ((struct strview){0})
#define HASHMAP_KEY_IS_NIL(k) ((k).len == 0)
#define HASHMAP_MALLOC(sz)    mem_pool_alloc_resizable(KC_HEAP, sz)
#define HASHMAP_FREE(ptr)     mem_pool_free_resizable(KC_HEAP, ptr)

#include "hashmap.h"

KC_PUBLIC Syminfo
syminfo_create(void)
{
	return (Syminfo)symhash_create(32);
}

KC_PUBLIC void
syminfo_add(Syminfo *si, struct strview key, struct symtbl_entry *val)
{
	Syminfo_t *info = (Syminfo_t *)si;
	Syminfo_list list = MEM_ALLOC(struct symbol_info);
	list->info = val;
	Syminfo_e *e = symhash_lookup(*info, key);
	if (e->key.len) {
		list->next = e->value;
		e->value = list;
	} else {
		symhash_insert(info, key, list);
	}
}

KC_PUBLIC Syminfo_list
syminfo_lookup(Syminfo si, struct strview key)
{
	Syminfo_t info = (Syminfo_t)si;
	Syminfo_e *e = symhash_lookup(info, key);
	if (e->key.len) {
		return e->value;
	} else {
		return NULL;
	}
}

KC_PUBLIC bool
syminfo_iter_next(Syminfo_iter *iter, Syminfo_pair *pair)
{
	Syminfo_t ht = (Syminfo_t)iter->si;
	Syminfo_e *e = NULL;
	for (; iter->count < ht->len && iter->index < ht->cap; ++iter->index) {
		e = &ht->entries[iter->index];
		if (e->key.len) {
			pair->name = e->key;
			pair->info_list = e->value;
			iter->index++;
			iter->count++;
			return true;
		}
	}
	return false;
}
