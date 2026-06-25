#pragma once

KC_PUBLIC struct symtbl_entry *symtbl_find(
	struct symtbl_entry *symtbl,
	struct strview name);

KC_PUBLIC struct symtbl_entry *symtbl_add(
	struct symtbl_entry **symtbl,
	struct definition *def,
	struct expression *tl_exp);

KC_PUBLIC struct symtbl_entry *symtbl_add_valcons(
	struct symtbl_entry **symtbl,
	struct token *name,
	int64_t tag_val,
	KCType *type,
	struct type_definition *def);

KC_PUBLIC struct symtbl_entry *symtbl_add_type(
	struct symtbl_entry **symtbl,
	struct token *name,
	struct type_definition def);


KC_PUBLIC struct symtbl_entry *lookup_entry(
	struct scope *scope,
	struct strview name);

KC_PUBLIC struct scope *scope_join(struct scope *sc1, struct scope *sc2);
