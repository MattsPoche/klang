#pragma once

static void type_check(Parser *p, struct typing_context *ctx, struct expression_stack *exps);
static bool type_equiv(Parser *p, KCType *t, KCType *u, struct scope *scope);
static uintptr_t align_adjust(uintptr_t x, uintptr_t alignment);
static KCType *copy_type(KCType *type);
static KCType *type_find(KCType *type);
static size_t type_size(KCType *t);
static size_t type_alignment(KCType *t);
static size_t struct_member_offset(struct struct_type *st, size_t index);
static size_t get_struct_member_idx(KCType *t, struct token *mem);
