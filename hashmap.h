/*  Generic Hashmap */

#ifdef HASHMAP_IMPLEMENTATION
#undef HASHMAP_H_
#endif

#ifndef HASHMAP_H_
#define HASHMAP_H_

#ifndef NO_STD_HEADERS
#include <assert.h>
#include <stdint.h>
#include <string.h>
#endif

#ifndef HASHMAP_MALLOC
#define HASHMAP_MALLOC malloc
#endif

#ifndef HASHMAP_FREE
#define HASHMAP_FREE free
#endif

#ifndef HASHMAP_ASSERT
#define HASHMAP_ASSERT assert
#endif

/* Key type */
#ifndef HASHMAP_K
#define HASHMAP_K uint64_t
#endif

/* Value type */
#ifndef HASHMAP_V
#define HASHMAP_V uint64_t
#endif

/* Entry type name */
#ifndef HASHMAP_E
#define HASHMAP_E Hashmap_entry
#endif

/* Hashmap type name */
#ifndef HASHMAP_S
#define HASHMAP_S Hashmap
#endif

/* Hashmap prefix */
#ifndef HASHMAP_P
#define HASHMAP_P hashmap
#endif

/* Hash function
 * uint64_t hash(HASHMAP_K)
 */
#ifndef HASHMAP_HASH
#define HASHMAP_HASH(x) x
#endif

/* Key comparison function
 * bool equals(HASHMAP_K, HASHMAP_K)
 */
#ifndef HASHMAP_EQUALS
#define HASHMAP_EQUALS(x, y) (x == y)
#endif

/* Check if key is a NIL value.
 * If key is NIL, then the entry is empty.
 * bool isnil(HASHMAP_K)
 */

#ifndef HASHMAP_NIL_KEY
#define HASHMAP_NIL_KEY -1LU
#endif

#ifndef HASHMAP_KEY_FINALIZE
#define HASHMAP_KEY_FINALIZE(key) ((void)(key))
#endif


#ifndef HASHMAP_VALUE_FINALIZE
#define HASHMAP_VALUE_FINALIZE(val) ((void)(val))
#endif

#ifndef HASHMAP_KEY_IS_NIL
#define HASHMAP_KEY_IS_NIL(x) HASHMAP_EQUALS(x, HASHMAP_NIL_KEY)
#endif

#define PASTE2(p, n, d) p##d##n
#define CONCAT(p, n, ...) PASTE2(p, n, _##__VA_ARGS__)
#define HASHMAP_FN(name) CONCAT(HASHMAP_P, name)

typedef struct {
    HASHMAP_K key;
    HASHMAP_V value;
} HASHMAP_E;

typedef struct {
    uint32_t len, cap;
    HASHMAP_E entries[];
} *HASHMAP_S;

HASHMAP_S HASHMAP_FN(create)(uint32_t cap);
HASHMAP_S HASHMAP_FN(copy)(HASHMAP_S src);
void HASHMAP_FN(grow)(HASHMAP_S *hs);
void HASHMAP_FN(destroy)(HASHMAP_S hs);
HASHMAP_E *HASHMAP_FN(lookup_with_hash)(HASHMAP_S hs, HASHMAP_K key, uint64_t hash);
HASHMAP_E *HASHMAP_FN(lookup)(HASHMAP_S hs, HASHMAP_K key);
void HASHMAP_FN(remove)(HASHMAP_S *hs, HASHMAP_K key);
void HASHMAP_FN(insert)(HASHMAP_S *hs, HASHMAP_K key, HASHMAP_V value);

#ifndef HASHMAP_IMPLEMENTATION
#undef HASHMAP_MALLOC
#undef HASHMAP_FREE
#undef HASHMAP_ASSERT
#undef HASHMAP_K
#undef HASHMAP_V
#undef HASHMAP_E
#undef HASHMAP_S
#undef PASTE2
#undef CONCAT
#undef HASHMAP_FN
#undef HASHMAP_HASH
#undef HASHMAP_EQUALS
#undef HASHMAP_KEY_IS_NIL
#undef HASHMAP_KEY_FINALIZE
#undef HASHMAP_VALUE_FINALIZE
#endif

#endif /* HASHMAP_H_ */

#ifdef HASHMAP_IMPLEMENTATION
#undef HASHMAP_IMPLEMENTATION

#define _HASHMAP_RESIZE_THREASHOLD 0.7

HASHMAP_S HASHMAP_FN(create)(uint32_t cap)
{
    HASHMAP_S hs = HASHMAP_MALLOC(sizeof(*hs) + (sizeof(hs->entries[0]) * cap));
    hs->len = 0;
    hs->cap = cap;
    for (size_t i = 0; i < cap; ++i) {
        hs->entries[i].key = HASHMAP_NIL_KEY;
    }
    return hs;
}

void HASHMAP_FN(destroy)(HASHMAP_S hs)
{
    for (size_t i = 0; i < hs->cap; ++i) {
        if (!HASHMAP_KEY_IS_NIL(hs->entries[i].key)) {
            HASHMAP_KEY_FINALIZE(hs->entries[i].key);
            HASHMAP_VALUE_FINALIZE(hs->entries[i].value);
        }
    }
    hs->len = 0;
    hs->cap = 0;
    HASHMAP_FREE(hs);
}

HASHMAP_S HASHMAP_FN(copy)(HASHMAP_S src)
{
    size_t sz = sizeof(*src) + (sizeof(src->entries[0]) * src->cap);
    HASHMAP_S new = HASHMAP_MALLOC(sz);
    return memcpy(new, src, sz);
}

void HASHMAP_FN(grow)(HASHMAP_S *hs)
{
    HASHMAP_S old_hs = *hs;
    HASHMAP_S new_hs = HASHMAP_FN(create)(old_hs->cap * 2);
    for (size_t i = 0; i < old_hs->cap; ++i) {
        if (!HASHMAP_KEY_IS_NIL(old_hs->entries[i].key)) {
            HASHMAP_FN(insert)(&new_hs, old_hs->entries[i].key, old_hs->entries[i].value);
        }
    }
    HASHMAP_FREE(old_hs);
    *hs = new_hs;
}

HASHMAP_E *HASHMAP_FN(lookup_with_hash)(HASHMAP_S hs, HASHMAP_K key, uint64_t hash)
{
    /* compute hash */
    size_t i, idx = hash % hs->cap;
    /* search for entry */
    for (i = idx; i < hs->cap; ++i) {
        if (HASHMAP_KEY_IS_NIL(hs->entries[i].key)
            || HASHMAP_EQUALS(hs->entries[i].key, key)) {
            return &hs->entries[i];
        }
    }
    /* wrap back to top and continue search */
    for (i = 0; i < idx; ++i) {
        if (HASHMAP_KEY_IS_NIL(hs->entries[i].key)
            || HASHMAP_EQUALS(hs->entries[i].key, key)) {
            return &hs->entries[i];
        }
    }
    HASHMAP_ASSERT(!"unreachable");
    return NULL;
}

HASHMAP_E *HASHMAP_FN(lookup)(HASHMAP_S hs, HASHMAP_K key)
{
    return HASHMAP_FN(lookup_with_hash)(hs, key, HASHMAP_HASH(key));
}

void HASHMAP_FN(insert)(HASHMAP_S *hs_ptr, HASHMAP_K key, HASHMAP_V value)
{
    HASHMAP_S hs = *hs_ptr;
    HASHMAP_E *slot = HASHMAP_FN(lookup)(hs, key);
    if (HASHMAP_KEY_IS_NIL(slot->key)) {
        slot->key = key;
        slot->value = value;
        hs->len++;
    } else {
        HASHMAP_VALUE_FINALIZE(slot->value);
        slot->value = value;
    }
    if (((double)hs->len / (double)hs->cap) > _HASHMAP_RESIZE_THREASHOLD) {
        HASHMAP_FN(grow)(hs_ptr);
    }
}

void HASHMAP_FN(remove)(HASHMAP_S *hs_ptr, HASHMAP_K key)
{
    HASHMAP_S hs = *hs_ptr;
    HASHMAP_E *slot = HASHMAP_FN(lookup)(hs, key);
    if (!HASHMAP_KEY_IS_NIL(slot->key)) {
        HASHMAP_KEY_FINALIZE(slot->key);
        HASHMAP_VALUE_FINALIZE(slot->value);
        slot->key = HASHMAP_NIL_KEY;
        hs->len--;
    }
}

#undef HASHMAP_MALLOC
#undef HASHMAP_FREE
#undef HASHMAP_ASSERT
#undef HASHMAP_K
#undef HASHMAP_V
#undef HASHMAP_E
#undef HASHMAP_S
#undef PASTE2
#undef CONCAT
#undef HASHMAP_FN
#undef HASHMAP_HASH
#undef HASHMAP_EQUALS
#undef HASHMAP_KEY_IS_NIL
#undef HASHMAP_KEY_FINALIZE
#undef HASHMAP_VALUE_FINALIZE

#endif /* HASHMAP_IMPLEMENTATION */
