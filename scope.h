#pragma once

#define typetbl_add(tbl, ...) typetbl_add_impl(tbl, (struct type_definition){__VA_ARGS__})

static struct symtbl_entry *symtbl_find(struct symtbl *symtbl, struct strview name);
static struct symtbl_entry *symtbl_add(struct symtbl *symtbl, struct definition *def, struct expression *tl_exp);
static void symtbl_add_valcons(struct symtbl *symtbl, struct token *name,
							   int64_t tag_val, KCType *type, struct type_definition *def);
static struct type_definition *typetbl_find(struct typetbl *typetbl, struct strview name);
static void typetbl_add_impl(struct typetbl *typetbl, struct type_definition def);
static struct symtbl_entry *lookup_entry(struct scope *scope, struct strview name);
static struct type_definition *lookup_type(struct scope *scope, struct strview name);
