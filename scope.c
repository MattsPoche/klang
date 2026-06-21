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

KC_PUBLIC struct type_definition *
typetbl_find(struct typetbl_entry *typetbl, struct strview name)
{
	for (; typetbl; typetbl = typetbl->next) {
		if (sv_is_equal(name, token_to_strview(typetbl->def.name))) {
			return &typetbl->def;
		}
	}
	return NULL;
}

KC_PUBLIC struct type_definition *
typetbl_add_impl(struct typetbl_entry **typetbl, struct type_definition def)
{
	assert(def.name != NULL);
	struct typetbl_entry *entry = NULL;
	if (typetbl_find(*typetbl, token_to_strview(def.name)) == NULL) {
		entry = MEM_ALLOC(struct typetbl_entry);
		entry->def = def;
		entry->next = *typetbl;
		*typetbl = entry;
	} else {
		FAILWITH("TODO: Type is already defined.");
	}
	return &entry->def;
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

KC_PUBLIC struct type_definition *
lookup_type(struct scope *scope, struct strview name)
{
	while (scope) {
		struct type_definition *def = typetbl_find(scope->typetbl, name);
		if (def) return def;
		scope = scope->parent;
	}
	return NULL;
}
