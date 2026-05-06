#pragma once

static struct symtbl_entry *
symtbl_find(struct symtbl *symtbl, struct strview name)
{
	for (size_t i = 0; i < symtbl->len; ++i) {
		if (sv_is_equal(name, token_to_strview(symtbl->elems[i].name))) {
			return &symtbl->elems[i];
		}
	}
	return NULL;
}

static struct symtbl_entry *
symtbl_add(struct symtbl *symtbl, struct definition *def, struct expression *tl_exp)
{
	struct symtbl_entry *entry = NULL;
	if (symtbl_find(symtbl, token_to_strview(def->id)) == NULL) {
		entry = da_allot(symtbl);
		entry->name = def->id;
		entry->tag  = SYMTBL_VARIABL;
		entry->as.variable.def = def;
		entry->as.variable.tl_exp = tl_exp;
	} else {
		FAILWITH("TODO: Symbol already defined.");
	}
	return entry;
}

static void
symtbl_add_valcons(struct symtbl *symtbl, struct token *name,
				   int64_t tag_val, KCType *type, struct type_definition *def)
{
	struct symtbl_entry *e = symtbl_find(symtbl, token_to_strview(name));
	if (e == NULL) {
		e = da_allot(symtbl);
		e->name = name;
		e->tag  = SYMTBL_VALCONS;
	}
	da_append(&e->as.valcons, (struct valcons_entry){
			.tag_val = tag_val,
			.td      = def,
			.type    = type,
		});
}

static struct type_definition *
typetbl_find(struct typetbl *typetbl, struct strview name)
{
	for (size_t i = 0; i < typetbl->len; ++i) {
		if (sv_is_equal(name, token_to_strview(typetbl->elems[i].name))) {
			return &typetbl->elems[i];
		}
	}
	return NULL;
}

static void
typetbl_add_impl(struct typetbl *typetbl, struct type_definition def)
{
	assert(def.name != NULL);
	if (typetbl_find(typetbl, token_to_strview(def.name)) != NULL)
		FAILWITH("TODO: Type is already defined.");
	da_append(typetbl, def);
}

static struct symtbl_entry *
lookup_entry(struct scope *scope, struct strview name)
{
	while (scope) {
		struct symtbl_entry *entry = symtbl_find(&scope->symtbl, name);
		if (entry) return entry;
		scope = scope->parent;
	}
	return NULL;
}

static struct type_definition *
lookup_type(struct scope *scope, struct strview name)
{
	while (scope) {
		struct type_definition *def = typetbl_find(&scope->typetbl, name);
		if (def) return def;
		scope = scope->parent;
	}
	return NULL;
}
