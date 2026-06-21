#pragma once

#include "common.h"

KC_PUBLIC_DATA KCType AST_TYPE_BOOL;
KC_PUBLIC_DATA KCType AST_TYPE_VOID;
KC_PUBLIC_DATA KCType AST_TYPE_U8;
KC_PUBLIC_DATA KCType AST_TYPE_U16;
KC_PUBLIC_DATA KCType AST_TYPE_U32;
KC_PUBLIC_DATA KCType AST_TYPE_U64;
KC_PUBLIC_DATA KCType AST_TYPE_I8;
KC_PUBLIC_DATA KCType AST_TYPE_I16;
KC_PUBLIC_DATA KCType AST_TYPE_I32;
KC_PUBLIC_DATA KCType AST_TYPE_I64;
KC_PUBLIC_DATA KCType AST_TYPE_F32;
KC_PUBLIC_DATA KCType AST_TYPE_F64;
KC_PUBLIC_DATA KCType AST_TYPE_STRING;

#define AST_TYPE_UNION_TAG AST_TYPE_I64
#define AST_TYPE_CHAR      AST_TYPE_I8

KC_PUBLIC void type_check(KC_session *session, struct typing_context *ctx, struct expression_stack *exps);
KC_PUBLIC bool type_equiv(KCType *t, KCType *u, struct scope *scope);
KC_PUBLIC uintptr_t align_adjust(uintptr_t x, uintptr_t alignment);
KC_PUBLIC KCType *copy_type(KCType *type);
KC_PUBLIC KCType *type_find(KCType *type);
KC_PUBLIC KCType *type_recursive_find(KCType *type);
KC_PUBLIC KCType *resolve_type(KCType *type, struct scope *scope);
KC_PUBLIC size_t type_size(KCType *t);
KC_PUBLIC size_t type_alignment(KCType *t);
KC_PUBLIC size_t struct_member_offset(struct struct_type *st, size_t index);
KC_PUBLIC size_t get_struct_member_idx(KCType *t, struct token *mem);
KC_PUBLIC struct type_spec *lookup_poly_proc_spec(struct definition *def, KCType *t, struct scope *scope);
KC_PUBLIC KCType *type_slice_to_array_ptr(KCType *t);
KC_PUBLIC struct struct_type * struct_type_members(KCType *type, struct scope *scope);
KC_PUBLIC KCType get_temp_app_type_from_definition(struct type_definition *td);
KC_PUBLIC bool type_is_var(KCType *t);
KC_PUBLIC bool type_is_integer(KCType *t);
KC_PUBLIC bool type_is_signed_integer(KCType *t);
KC_PUBLIC bool type_is_unsigned_integer(KCType *t);
KC_PUBLIC bool type_is_floating_point(KCType *t);
KC_PUBLIC bool type_is_numeric_scalar(KCType *t);
KC_PUBLIC bool type_is_signed_scalar(KCType *t);
KC_PUBLIC bool type_is_bool(KCType *t);
KC_PUBLIC bool type_is_void(KCType *t);
KC_PUBLIC bool type_is_procedure(KCType *t);
KC_PUBLIC bool type_is_pointer(KCType *t);
KC_PUBLIC bool type_is_struct(KCType *t, struct scope *scope);
KC_PUBLIC bool type_is_struct_ptr(KCType *t, struct scope *scope);
KC_PUBLIC bool type_is_union(KCType *t, struct scope *scope);
KC_PUBLIC bool type_is_union_ptr(KCType *t, struct scope *scope);
KC_PUBLIC bool type_is_array(KCType *t);
KC_PUBLIC bool type_is_array_ptr(KCType *t);
KC_PUBLIC bool type_is_slice(KCType *t);
KC_PUBLIC bool type_is_slice_ptr(KCType *t);
KC_PUBLIC bool type_is_indexable(KCType *t);
KC_PUBLIC bool type_is_scalar(KCType *t);
KC_PUBLIC bool type_is_polymorphic(KCType *t);
KC_PUBLIC uintptr_t align_adjust(uintptr_t x, uintptr_t alignment);
KC_PUBLIC size_t type_size(KCType *t);
KC_PUBLIC bool type_is_floating_point(KCType *t);
