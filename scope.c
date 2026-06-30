#include "common.h"

KC_PUBLIC struct symtbl_entry *
symtbl_find(struct symtbl_entry *symtbl, struct strview name)
{
    for (; symtbl; symtbl = symtbl->next) {
        if (sv_is_equal(name, token_to_strview(symtbl->name))) {
            return symtbl;
        }
    }
    return NULL;
}

KC_PUBLIC struct symtbl_entry *
symtbl_add(struct symtbl_entry **symtbl, struct definition *def, struct expression *tl_exp)
{
    struct symtbl_entry *entry = NULL;
    if (symtbl_find(*symtbl, token_to_strview(def->id)) == NULL) {
        entry = MEM_ALLOC(struct symtbl_entry);
        entry->name = def->id;
        entry->tag  = SYMTBL_VARIABL;
        entry->variable.def = def;
        entry->variable.tl_exp = tl_exp;
        entry->next = *symtbl;
        *symtbl = entry;
    } else {
        FAILWITH("TODO: Symbol already defined.");
    }
    return entry;
}

KC_PUBLIC struct symtbl_entry *
symtbl_add_valcons(struct symtbl_entry **symtbl, struct token *name,
                   int64_t tag_val, KCType *type, struct type_definition *def)
{
    struct symtbl_entry *entry = NULL;
    if (symtbl_find(*symtbl, token_to_strview(name)) == NULL) {
        entry = MEM_ALLOC(struct symtbl_entry);
        entry->name = name;
        entry->tag  = SYMTBL_VALCONS;
        entry->valcons.tag_val = tag_val;
        entry->valcons.td = def;
        entry->valcons.type = type;
        entry->next = *symtbl;
        *symtbl = entry;
    } else {
        FAILWITH("TODO: Symbol already defined.");
    }
    return entry;
}

KC_PUBLIC struct symtbl_entry *
symtbl_add_type(struct symtbl_entry **symtbl, struct token *name, struct type_definition def)
{
    struct symtbl_entry *entry = NULL;
    if (symtbl_find(*symtbl, token_to_strview(name)) == NULL) {
        entry = MEM_ALLOC(struct symtbl_entry);
        entry->name = name;
        entry->tag  = SYMTBL_TYPE;
        entry->type = def;
        entry->next = *symtbl;
        *symtbl = entry;
    } else {
        FAILWITH("TODO: Symbol already defined.");
    }
    return entry;
}

KC_PUBLIC struct symtbl_entry *
lookup_entry(struct scope *scope, struct strview name)
{
    while (scope) {
        struct symtbl_entry *entry = symtbl_find(scope->symtbl, name);
        if (entry) return entry;
        scope = scope->parent;
    }
    return NULL;
}

KC_PUBLIC struct scope *
scope_join(struct scope *sc1, struct scope *sc2)
{
    assert(sc1->parent == NULL);
    assert(sc2->parent == NULL);
    struct symtbl_entry *entry = sc1->symtbl;
    for (; entry->next; entry = entry->next);
    entry->next = sc2->symtbl;
    return sc1;
}
