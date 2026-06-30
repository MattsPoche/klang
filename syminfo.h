#pragma once

/* opaque handle to hashmap */
typedef uintptr_t Syminfo;

typedef struct symbol_info {
    struct symbol_info     *next;
    struct symtbl_entry    *info;
} *Syminfo_list;

typedef struct {
    uint32_t count, index;
    Syminfo si;
} Syminfo_iter;

typedef struct {
    struct strview name;
    Syminfo_list info_list;
} Syminfo_pair;

#define syminfo_iter(S) ((Syminfo_iter){.si=(S)})

KC_PUBLIC Syminfo syminfo_create(void);
KC_PUBLIC void syminfo_add(Syminfo *si, struct strview key, struct symtbl_entry *val);
KC_PUBLIC Syminfo_list syminfo_lookup(Syminfo si, struct strview key);
KC_PUBLIC bool syminfo_iter_next(Syminfo_iter *iter, Syminfo_pair *pair);
