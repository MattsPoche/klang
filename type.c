#include "common.h"

/* --- TYPE-CHECKER --- */

struct type_var_bindings {
	uint32_t len, cap;
	struct type_pair {
		KCType *var, *type;
	} *elems;
};

KC_PUBLIC KCType AST_TYPE_BOOL   = {.tag = ast_type_bool};
KC_PUBLIC KCType AST_TYPE_VOID   = {.tag = ast_type_void};
KC_PUBLIC KCType AST_TYPE_U8	 = {.tag = ast_type_u8};
KC_PUBLIC KCType AST_TYPE_U16	 = {.tag = ast_type_u16};
KC_PUBLIC KCType AST_TYPE_U32	 = {.tag = ast_type_u32};
KC_PUBLIC KCType AST_TYPE_U64    = {.tag = ast_type_u64};
KC_PUBLIC KCType AST_TYPE_I8     = {.tag = ast_type_i8};
KC_PUBLIC KCType AST_TYPE_I16    = {.tag = ast_type_i16};
KC_PUBLIC KCType AST_TYPE_I32    = {.tag = ast_type_i32};
KC_PUBLIC KCType AST_TYPE_I64    = {.tag = ast_type_i64};
KC_PUBLIC KCType AST_TYPE_F32    = {.tag = ast_type_f32};
KC_PUBLIC KCType AST_TYPE_F64    = {.tag = ast_type_f64};
KC_PUBLIC KCType AST_TYPE_STRING = {.tag = ast_type_slice, .as.slice = &AST_TYPE_I8};

KC_PRIVATE KCType *infer_type(Parser *p, struct typing_context ctx, struct expression *exp);
KC_PRIVATE bool unify(Parser *p, struct typing_context ctx, KCType *t, KCType *u);
KC_PRIVATE KCType *type_var_subst(KCType *type, struct type_var_bindings *bindings);
KC_PRIVATE KCType *resolve_alias(Parser *p, KCType *t, struct scope *scope);
KC_PRIVATE KCType *fresh_type_var(enum type_class c);
KC_PRIVATE void add_type_var_binding(struct type_var_bindings *bindings, KCType *var, KCType *type);
KC_PRIVATE KCType *find_type_var_binding(struct type_var_bindings *bindings, KCType *var);
KC_PRIVATE KCType *instantiate_type_scheme(struct type_scheme *scm);
KC_PRIVATE void type_env_add(struct typing_context *ctx, struct token *name, struct type_scheme scm);
KC_PRIVATE struct type_env *lookup_type_env(struct typing_context *ctx, struct token *var);
KC_PRIVATE struct type_scheme generalize_type(struct typing_context *ctx, KCType *type);
KC_PRIVATE void specialize_generic_procedure(Parser *p, struct typing_context ctx, struct definition *def,
										 struct type_spec *spec_def);
KC_PRIVATE struct definition specialize_definition(Parser *p, struct typing_context ctx, struct definition *def,
											   struct type_var_bindings *bindings);
KC_PRIVATE struct expression *specialize_expression(Parser *p, struct typing_context ctx,
												struct expression *exp, struct type_var_bindings *bindings);

KC_PRIVATE bool
exp_is_integer_literal(struct expression *exp)
{
	return exp->tag == ast_exp_literal && exp->as.lit.tag == LITERAL_INT;
}

KC_PRIVATE KCType *
type_application(Parser *p, struct type_app *app, struct scope *scope)
{
	struct type_definition *def = lookup_type(scope, token_to_strview(app->cons));
	if (def == NULL) ERROR_UNDEFINED_IDENT(app->cons);
	struct type_var_bindings bindings = {0};
	KCType *cons = def->type;
	struct type_ptrs *formals = &def->args;
	struct type_ptrs *actuals = &app->args;
	assert(formals->len == actuals->len);
	if (actuals->len == 0) return copy_type(cons);
	for (size_t i = 0; i < formals->len; ++i) {
		assert(formals->elems[i]->tag == ast_type_var);
		add_type_var_binding(&bindings, formals->elems[i], actuals->elems[i]);
	}
	KCType *newtype = type_var_subst(cons, &bindings);
	da_free(&bindings);
	return newtype;
}

KC_PRIVATE KCType *
type_get_underlying(struct scope *scope, KCType *type, KCType **out_type)
{
	type = type_find(type);
	if (type->tag == ast_type_app) {
		struct type_definition *def = lookup_type(scope, token_to_strview(type->as.app.cons));
		if (def == NULL) {
			if (out_type) *out_type = type;
			return NULL;
		}
		return type_get_underlying(scope, def->type, out_type);
	}
	if (out_type) *out_type = type;
	return type;
}

KC_PRIVATE void
fprint_full_type_name(Parser *p, struct typing_context ctx, KCType *t, FILE *f)
{
	KCType *u = resolve_alias(p, t, ctx.scope);
	fputs("`", f);
	ast_type_fprint(t, f);
	if (t != u) {
		fputs("` aka. `", f);
		ast_type_fprint(u, f);
	}
	fputs("`", f);
}

KC_PRIVATE void
type_mismatch_error(Parser *p, struct typing_context ctx, KCType *t, KCType *u,
					struct token *tok, char *debug_file, int debug_line)
{
	fflush(stdout);
	{ /* check for undefined identifiers in signatures */
		KCType *chk = NULL;
		if (!type_get_underlying(ctx.scope, t, &chk)) ERROR_UNDEFINED_IDENT(chk->as.app.cons);
		if (!type_get_underlying(ctx.scope, u, &chk)) ERROR_UNDEFINED_IDENT(chk->as.app.cons);
	}
	struct strview msg = {0};
	FILE *stream = open_memstream(&msg.ptr, &msg.len);
	fputs("KCType error. ", stream);
	fprint_full_type_name(p, ctx, t, stream);
	fputs(" is incompatible with ", stream);
	fprint_full_type_name(p, ctx, u, stream);
	fputs(".", stream);
	fclose(stream);
	log_error_impl(p->lexer.filename, tok, debug_file, debug_line, "%s", msg.ptr);
	EXIT(1);
}

KC_PUBLIC KCType *
resolve_type(Parser *p, KCType *type, struct scope *scope)
{
	type = type_find(type);
	switch (type->tag) {
	case ast_type_void:
	case ast_type_noreturn:
	case ast_type_bool:
	case ast_type_i8:
	case ast_type_i16:
	case ast_type_i32:
	case ast_type_i64:
	case ast_type_u8:
	case ast_type_u16:
	case ast_type_u32:
	case ast_type_u64:
	case ast_type_f32:
	case ast_type_f64:
		return type;
	case ast_type_ptr:
	case ast_type_mut_ptr:
		type->as.ptr = resolve_type(p, type->as.ptr, scope);
		type->tag = ast_type_ptr;
		return type;
	case ast_type_slice:
	case ast_type_mut_slice:
		type->as.slice = resolve_type(p, type->as.slice, scope);
		type->tag = ast_type_slice;
		return type;
	case ast_type_array:
		type->as.array.base = resolve_type(p, type->as.array.base, scope);
		return type;
	case ast_type_proc:
		for (size_t i = 0; i < type->as.proc.args.len; ++i) {
			type->as.proc.args.elems[i] = resolve_type(p, type->as.proc.args.elems[i], scope);
		}
		type->as.proc.ret = resolve_type(p, type->as.proc.ret, scope);
		return type;
	case ast_type_istruct:
	case ast_type_struct:
		for (size_t i = 0; i < type->as.struct_t.len; ++i) {
			type->as.struct_t.elems[i].type = resolve_type(p, type->as.struct_t.elems[i].type, scope);
		}
		return type;
	case ast_type_union:
		for (size_t i = 0; i < type->as.union_t.len; ++i) {
			type->as.union_t.elems[i].type = resolve_type(p, type->as.union_t.elems[i].type, scope);
		}
		return type;
	case ast_type_vector: FAILWITH("TODO: ast_type_vector"); break;
	case ast_type_app: return resolve_type(p, type_application(p, &type->as.app, scope), scope);
	case ast_type_var:
		switch (type->as.var.class) {
		case type_class_integer:
		case type_class_signed_integer:
			type->tag = ast_type_i64;
			return type;
		case type_class_unsigned_integer:
			type->tag = ast_type_u64;
			return type;
		case type_class_float:
			type->tag = ast_type_f64;
			return type;
		case type_class_any:
		case type_class_scalar:
		case type_class_numeric:
		case type_class_length:
		case type_class_indexable:
		case type_class_struct:
		case type_class_union:
		case type_class_procedure:
			break;
		default: FAILWITH("Unreachable"); break;
		}
		break;
	default: FAILWITH("Unreachable"); break;
	}
	return NULL;
}

KC_PUBLIC bool
type_is_var(KCType *t)
{
	t = type_find(t);
	return t->tag == ast_type_var;
}

KC_PUBLIC bool
type_is_integer(KCType *t)
{
	t = type_find(t);
	switch (t->tag) {
	case ast_type_i8:
	case ast_type_i16:
	case ast_type_i32:
	case ast_type_i64:
	case ast_type_u8:
	case ast_type_u16:
	case ast_type_u32:
	case ast_type_u64:
		return true;
	case ast_type_void:
	case ast_type_bool:
	case ast_type_f32:
	case ast_type_f64:
	case ast_type_app:
	case ast_type_ptr:
	case ast_type_mut_ptr:
	case ast_type_slice:
	case ast_type_mut_slice:
	case ast_type_array:
	case ast_type_istruct:
	case ast_type_struct:
	case ast_type_union:
	case ast_type_vector:
	case ast_type_proc:
	case ast_type_noreturn:
		return false;
	case ast_type_var:
		return false;
	default:
		FAILWITH("Unreachable condition");
		return false;
	}
}

KC_PUBLIC bool
type_is_signed_integer(KCType *t)
{
	t = type_find(t);
	switch (t->tag) {
	case ast_type_i8:
	case ast_type_i16:
	case ast_type_i32:
	case ast_type_i64:
		return true;
	case ast_type_u8:
	case ast_type_u16:
	case ast_type_u32:
	case ast_type_u64:
	case ast_type_void:
	case ast_type_bool:
	case ast_type_f32:
	case ast_type_f64:
	case ast_type_app:
	case ast_type_ptr:
	case ast_type_mut_ptr:
	case ast_type_slice:
	case ast_type_mut_slice:
	case ast_type_array:
	case ast_type_istruct:
	case ast_type_struct:
	case ast_type_union:
	case ast_type_vector:
	case ast_type_proc:
	case ast_type_noreturn:
		return false;
	case ast_type_var:
		FAILWITH("Unreachable ast_type_var");
		return false;
	default:
		FAILWITH("Unreachable condition");
		return false;
	}
}

KC_PUBLIC bool
type_is_unsigned_integer(KCType *t)
{
	t = type_find(t);
	switch (t->tag) {
	case ast_type_u8:
	case ast_type_u16:
	case ast_type_u32:
	case ast_type_u64:
		return true;
	case ast_type_i8:
	case ast_type_i16:
	case ast_type_i32:
	case ast_type_i64:
	case ast_type_void:
	case ast_type_bool:
	case ast_type_f32:
	case ast_type_f64:
	case ast_type_app:
	case ast_type_ptr:
	case ast_type_mut_ptr:
	case ast_type_slice:
	case ast_type_mut_slice:
	case ast_type_array:
	case ast_type_istruct:
	case ast_type_struct:
	case ast_type_union:
	case ast_type_vector:
	case ast_type_proc:
	case ast_type_noreturn:
		return false;
	case ast_type_var:
		FAILWITH("Unreachable ast_type_var");
		return false;
	default:
		FAILWITH("Unreachable condition");
		return false;
	}
}

KC_PUBLIC bool
type_is_floating_point(KCType *t)
{
	t = type_find(t);
	switch (t->tag) {
	case ast_type_i8:
	case ast_type_i16:
	case ast_type_i32:
	case ast_type_i64:
	case ast_type_u8:
	case ast_type_u16:
	case ast_type_u32:
	case ast_type_u64:
	case ast_type_void:
	case ast_type_bool:
	case ast_type_app:
	case ast_type_ptr:
	case ast_type_mut_ptr:
	case ast_type_slice:
	case ast_type_mut_slice:
	case ast_type_array:
	case ast_type_istruct:
	case ast_type_struct:
	case ast_type_union:
	case ast_type_vector:
	case ast_type_proc:
	case ast_type_noreturn:
		return false;
	case ast_type_f32:
	case ast_type_f64:
		return true;
	case ast_type_var:
		FAILWITH("Unreachable ast_type_var");
		return false;
	default:
		FAILWITH("Unreachable condition");
		return false;
	}
}

KC_PUBLIC bool
type_is_numeric_scalar(KCType *t)
{
	t = type_find(t);
	switch (t->tag) {
	case ast_type_i8:
	case ast_type_i16:
	case ast_type_i32:
	case ast_type_i64:
	case ast_type_u8:
	case ast_type_u16:
	case ast_type_u32:
	case ast_type_u64:
	case ast_type_f32:
	case ast_type_f64:
		return true;
	case ast_type_void:
	case ast_type_bool:
	case ast_type_app:
	case ast_type_ptr:
	case ast_type_mut_ptr:
	case ast_type_slice:
	case ast_type_mut_slice:
	case ast_type_array:
	case ast_type_istruct:
	case ast_type_struct:
	case ast_type_union:
	case ast_type_vector:
	case ast_type_proc:
	case ast_type_noreturn:
		return false;
	case ast_type_var:
		FAILWITH("Unreachable ast_type_var");
		return false;
	default:
		FAILWITH("Unreachable condition");
		return false;
	}
}

KC_PUBLIC bool
type_is_signed_scalar(KCType *t)
{
	t = type_find(t);
	return type_is_signed_integer(t) || type_is_floating_point(t);
}

KC_PUBLIC bool
type_is_bool(KCType *t)
{
	t = type_find(t);
	return t->tag == ast_type_bool;
}

KC_PUBLIC bool
type_is_void(KCType *t)
{
	t = type_find(t);
	return t->tag == ast_type_void;
}

KC_PUBLIC bool
type_is_procedure(KCType *t)
{
	t = type_find(t);
	return t->tag == ast_type_proc;
}

KC_PUBLIC bool
type_is_pointer(KCType *t)
{
	t = type_find(t);
	return t->tag == ast_type_ptr
		|| t->tag == ast_type_mut_ptr;
}

KC_PUBLIC bool
type_is_struct(KCType *t, struct scope *scope)
{
	t = type_find(t);
	if (t->tag == ast_type_app)
		return type_is_struct(type_get_underlying(scope, t, NULL), scope);
	return t->tag == ast_type_struct;
}

KC_PUBLIC bool
type_is_struct_ptr(KCType *t, struct scope *scope)
{
	t = type_find(t);
	if (t->tag == ast_type_app)
		return type_is_struct_ptr(type_get_underlying(scope, t, NULL), scope);
	return type_is_pointer(t) && type_is_struct(t->as.ptr, scope);
}

KC_PUBLIC bool
type_is_union(KCType *t, struct scope *scope)
{
	t = type_find(t);
	if (t->tag == ast_type_app)
		return type_is_union(type_get_underlying(scope, t, NULL), scope);
	return t->tag == ast_type_union;
}

KC_PUBLIC bool
type_is_union_ptr(KCType *t, struct scope *scope)
{
	t = type_find(t);
	if (t->tag == ast_type_app)
		return type_is_union_ptr(type_get_underlying(scope, t, NULL), scope);
	return type_is_pointer(t) && type_is_union(t->as.ptr, scope);
}

KC_PUBLIC bool
type_is_array(KCType *t)
{
	t = type_find(t);
	return t->tag == ast_type_array;
}

KC_PUBLIC bool
type_is_array_ptr(KCType *t)
{
	t = type_find(t);
	return type_is_pointer(t)
		&& type_is_array(t->as.ptr);
}

KC_PUBLIC bool
type_is_slice(KCType *t)
{
	t = type_find(t);
	return t->tag == ast_type_slice
		|| t->tag == ast_type_mut_slice;
}

UNUSED KC_PUBLIC bool
type_is_slice_ptr(KCType *t)
{
	t = type_find(t);
	return type_is_pointer(t)
		&& type_is_slice(t->as.ptr);
}

KC_PUBLIC bool
type_is_indexable(KCType *t)
{
	t = type_find(t);
	return type_is_array(t) || type_is_array_ptr(t) || type_is_slice(t);
}

KC_PRIVATE KCType *
get_slice_pointer(KCType *t)
{
	KCType *arr = MEM_ALLOC(KCType);
	arr->tag = ast_type_array;
	KCType *ptr = MEM_ALLOC(KCType);
	switch ((int)t->tag) {
	case ast_type_slice:
		ptr->tag = ast_type_ptr;
		arr->as.array.base = t->as.slice;
		break;
	case ast_type_mut_slice:
		ptr->tag = ast_type_mut_ptr;
		arr->as.array.base = t->as.mut_slice;
		break;
	default:
		FAILWITH("KCType is not a slice");
		break;
	}
	ptr->as.ptr = arr;
	return ptr;
}

KC_PRIVATE KCType *
get_indexable_base_type(KCType *t)
{
	t = type_find(t);
	assert(type_is_indexable(t));
	if (type_is_array(t))     return t->as.array.base;
	if (type_is_array_ptr(t)) return t->as.ptr->as.array.base;
	if (type_is_slice(t))     return t->as.slice;
	return NULL;
}

KC_PRIVATE bool
type_has_length(KCType *t, struct scope *scope)
{
	t = type_find(t);
	if (t->tag == ast_type_app)
		return type_has_length(type_get_underlying(scope, t, NULL), scope);
	if (type_is_array_ptr(t)) return type_has_length(t->as.ptr, scope);
	if (type_is_array(t))     return t->as.array.is_sized;
	return type_is_slice(t);
}

KC_PUBLIC bool
type_is_scalar(KCType *t)
{
	t = type_find(t);
	return type_is_numeric_scalar(t) || type_is_pointer(t) || type_is_bool(t);
}

KC_PRIVATE bool
type_contains_var(KCType *t)
{
	t = type_find(t);
	switch (t->tag) {
	case ast_type_void:
	case ast_type_noreturn:
	case ast_type_bool:
	case ast_type_i8:
	case ast_type_i16:
	case ast_type_i32:
	case ast_type_i64:
	case ast_type_u8:
	case ast_type_u16:
	case ast_type_u32:
	case ast_type_u64:
	case ast_type_f32:
	case ast_type_f64:
		return false;
	case ast_type_app:
		for (size_t i = 0; i < t->as.app.args.len; ++i) {
			if (type_contains_var(t->as.app.args.elems[i])) return true;
		}
		return false;
	case ast_type_ptr:
	case ast_type_mut_ptr:
		return type_contains_var(t->as.ptr);
	case ast_type_slice:
	case ast_type_mut_slice:
		return type_contains_var(t->as.slice);
	case ast_type_array:
		return type_contains_var(t->as.array.base);
	case ast_type_istruct:
	case ast_type_struct:
		for (size_t i = 0; i < t->as.struct_t.len; ++i) {
			if (type_contains_var(t->as.struct_t.elems[i].type)) return true;
		}
		return false;
	case ast_type_union:
		for (size_t i = 0; i < t->as.union_t.len; ++i) {
			if (type_contains_var(t->as.union_t.elems[i].type)) return true;
		}
		return false;
	case ast_type_proc:
		for (size_t i = 0; i < t->as.proc.args.len; ++i) {
			if (type_contains_var(t->as.proc.args.elems[i])) return true;
		}
		return type_contains_var(t->as.proc.ret);
	case ast_type_var:
		return true;
	case ast_type_vector: FAILWITH("TODO: ast_type_vector"); break;
	default: FAILWITH("Unreachable"); break;
	}
	return false;
}

KC_PUBLIC bool
type_is_polymorphic(KCType *t)
{
	t = type_find(t);
	return type_is_procedure(t) && type_contains_var(t);
}

KC_PUBLIC struct struct_type *
struct_type_members(Parser *p, KCType *type, struct scope *scope)
{
	if (type->tag == ast_type_app)
		return struct_type_members(p, type_application(p, &type->as.app, scope), scope);
	if (type->tag == ast_type_struct)
		return &type->as.struct_t;
	if (type->tag == ast_type_ptr && type->as.ptr->tag == ast_type_struct)
		return &type->as.ptr->as.struct_t;
	if (type->tag == ast_type_mut_ptr && type->as.ptr->tag == ast_type_struct)
		return &type->as.mut_ptr->as.struct_t;
	FAILWITH("KCType has no struct members");
	return NULL;
}

KC_PUBLIC KCType *
type_slice_to_array_ptr(KCType *t)
{
	KCType *ptr = MEM_ALLOC(KCType);
	KCType *array = MEM_ALLOC(KCType);
	array->tag = ast_type_array;
	array->as.array.is_sized = false;
	array->as.array.base = t->as.slice;
	if (t->tag == ast_type_mut_slice) {
		ptr->tag = ast_type_mut_ptr;
	} else {
		assert(t->tag == ast_type_slice);
		ptr->tag = ast_type_ptr;
	}
	ptr->as.ptr = array;
	return ptr;
}

UNUSED KC_PRIVATE KCType *
type_array_to_slice(KCType *t, bool mutable_p)
{
	KCType *slice = MEM_ALLOC(KCType);
	slice->tag = mutable_p ? ast_type_mut_slice : ast_type_slice;
	slice->as.slice = t->as.array.base;
	return slice;
}

KC_PUBLIC KCType *
copy_type(KCType *type)
{
	KCType *newtype = NULL;
	switch (type->tag) {
	case ast_type_noreturn: FAILWITH("TODO: ast_type_noreturn"); break;
	case ast_type_void: return &AST_TYPE_VOID;
	case ast_type_bool: return &AST_TYPE_BOOL;
	case ast_type_i8:   return &AST_TYPE_I8;
	case ast_type_i16:  return &AST_TYPE_I16;
	case ast_type_i32:  return &AST_TYPE_I32;
	case ast_type_i64:  return &AST_TYPE_I64;
	case ast_type_u8:   return &AST_TYPE_U8;
	case ast_type_u16:  return &AST_TYPE_U64;
	case ast_type_u32:  return &AST_TYPE_U64;
	case ast_type_u64:  return &AST_TYPE_U64;
	case ast_type_f32:  return &AST_TYPE_F32;
	case ast_type_f64:  return &AST_TYPE_F64;
	case ast_type_app:
		newtype = MEM_ALLOC(KCType);
		newtype->tag = type->tag;
		newtype->as.app.cons = type->as.app.cons;
		for (size_t i = 0; i < type->as.app.args.len; ++i) {
			da_append(&newtype->as.app.args, copy_type(type->as.app.args.elems[i]));
		}
		return newtype;
	case ast_type_var:
		newtype = MEM_ALLOC(KCType);
		*newtype = *type;
		return newtype;
	case ast_type_ptr:
	case ast_type_mut_ptr:
		newtype = MEM_ALLOC(KCType);
		newtype->tag = type->tag;
		newtype->as.ptr = copy_type(type->as.ptr);
		return newtype;
	case ast_type_slice:
	case ast_type_mut_slice:
		newtype = MEM_ALLOC(KCType);
		newtype->tag = type->tag;
		newtype->as.slice = copy_type(type->as.slice);
		return newtype;
	case ast_type_array:
		newtype = MEM_ALLOC(KCType);
		*newtype = *type;
		newtype->as.array.base = copy_type(type->as.array.base);
		return newtype;
	case ast_type_istruct:
	case ast_type_struct:
		newtype = MEM_ALLOC(KCType);
		newtype->tag = type->tag;
		for (size_t i = 0; i < type->as.struct_t.len; ++i) {
			da_append(&newtype->as.struct_t, (struct struct_member) {
					.name = type->as.struct_t.elems[i].name,
					.type = copy_type(type->as.struct_t.elems[i].type),
				});
		}
		return newtype;
	case ast_type_union:
		newtype = MEM_ALLOC(KCType);
		newtype->tag = type->tag;
		for (size_t i = 0; i < type->as.struct_t.len; ++i) {
			da_append(&newtype->as.union_t, (struct union_member) {
					.name      = type->as.union_t.elems[i].name,
					.tag_value = type->as.union_t.elems[i].tag_value,
					.type      = copy_type(type->as.union_t.elems[i].type),
				});
		}
		return newtype;
	case ast_type_proc:
		newtype = MEM_ALLOC(KCType);
		newtype->tag = type->tag;
		for (size_t i = 0; i < type->as.proc.args.len; ++i) {
			da_append(&newtype->as.proc.args, copy_type(type->as.proc.args.elems[i]));
		}
		newtype->as.proc.ret = copy_type(type->as.proc.ret);
		return newtype;
	case ast_type_vector: FAILWITH("TODO: ast_type_vector"); break;
	default: FAILWITH("TODO: Unreachable"); break;
	}
	return NULL;
}

KC_PUBLIC struct type_spec *
lookup_poly_proc_spec(Parser *p, struct definition *def, KCType *t, struct scope *scope)
{
	assert(def->type->tag == ast_type_proc);
	for (size_t i = 0; i < def->specs.len; ++i) {
		if (type_equiv(p, def->specs.elems[i].type, t, scope))
			return &def->specs.elems[i];
	}
	return NULL;
}

KC_PRIVATE KCType *
resolve_alias(Parser *p, KCType *t, struct scope *scope)
{
	if (t->tag == ast_type_app) {
		struct type_definition *def = lookup_type(scope, token_to_strview(t->as.app.cons));
		if (def == NULL) ERROR_UNDEFINED_IDENT(t->as.app.cons);
		if (def->is_alias) return type_application(p, &t->as.app, scope);
	}
	return t;
}

KC_PUBLIC bool
type_equiv(Parser *p, KCType *t, KCType *u, struct scope *scope)
{
	if (t == u) return true;
	t = resolve_alias(p, type_find(t), scope);
	u = resolve_alias(p, type_find(u), scope);
	switch (t->tag) {
	case ast_type_noreturn: FAILWITH("TODO: ast_type_noreturn"); break;
	case ast_type_var: return t == u;
	case ast_type_app:
		if (t->tag != u->tag)
			return false;
		if (t->as.app.args.len != u->as.app.args.len)
			return false;
		if (!sv_is_equal(token_to_strview(t->as.app.cons), token_to_strview(u->as.app.cons)))
			return false;
		for (size_t i = 0; i < t->as.app.args.len; ++i) {
			if (!type_equiv(p, t->as.app.args.elems[i], u->as.app.args.elems[i], scope)) return false;
		}
		return true;
	case ast_type_void:
	case ast_type_bool:
	case ast_type_i8:
	case ast_type_i16:
	case ast_type_i32:
	case ast_type_i64:
	case ast_type_u8:
	case ast_type_u16:
	case ast_type_u32:
	case ast_type_u64:
	case ast_type_f32:
	case ast_type_f64:
		return t->tag == u->tag;
	case ast_type_ptr:
	case ast_type_mut_ptr:
	case ast_type_slice:
	case ast_type_mut_slice:
		return t->tag == u->tag && type_equiv(p, t->as.ptr, u->as.ptr, scope);
	case ast_type_array:
		return t->tag == u->tag
			&& t->as.array.is_sized == u->as.array.is_sized
			&& t->as.array.size == u->as.array.size
			&& type_equiv(p, t->as.array.base, t->as.array.base, scope);
	case ast_type_istruct:
	case ast_type_struct:
		if (t->tag != u->tag) return false;
		if (t->as.struct_t.len != u->as.struct_t.len) return false;
		for (size_t i = 0; i < t->as.struct_t.len; ++i) {
			if (!sv_is_equal(token_to_strview(t->as.struct_t.elems[i].name),
							 token_to_strview(u->as.struct_t.elems[i].name)))
				return false;
			if (!type_equiv(p, t->as.struct_t.elems[i].type, u->as.struct_t.elems[i].type, scope))
				return false;
		}
		return true;
	case ast_type_union: FAILWITH("TODO: ast_type_union"); break;
	case ast_type_vector: FAILWITH("TODO: ast_type_vector"); break;
	case ast_type_proc:
		if (t->tag != u->tag) return false;
		if (!type_equiv(p, t->as.proc.ret, t->as.proc.ret, scope)) return false;
		if (t->as.proc.args.len != u->as.proc.args.len) return false;
		for (size_t i = 0; i < t->as.proc.args.len; ++i) {
			if (!type_equiv(p, t->as.proc.args.elems[i], u->as.proc.args.elems[i], scope))
				return false;
		}
		return true;
	default: FAILWITH("Unreachable"); break;
	}
	return false;
}

KC_PRIVATE void
add_type_var_binding(struct type_var_bindings *bindings, KCType *var, KCType *type)
{
	da_append(bindings, (struct type_pair){var, type});
}

KC_PRIVATE KCType *
find_type_var_binding(struct type_var_bindings *bindings, KCType *var)
{
	var = type_find(var);
	for (size_t i = 0; i < bindings->len; ++i) {
		KCType *key = type_find(bindings->elems[i].var);
		if (key == var) {
			return bindings->elems[i].type;
		}
	}
	return NULL;
}

KC_PRIVATE void
bind_polymorphic_type_vars(Parser *p, struct scope *scope, struct type_var_bindings *bindings,
						   KCType *poly, KCType *mono)
{
	switch (poly->tag) {
	case ast_type_void:
	case ast_type_noreturn:
	case ast_type_bool:
	case ast_type_i8:
	case ast_type_i16:
	case ast_type_i32:
	case ast_type_i64:
	case ast_type_u8:
	case ast_type_u16:
	case ast_type_u32:
	case ast_type_u64:
	case ast_type_f32:
	case ast_type_f64:
		if (poly->tag != mono->tag)
			FAILWITH("%s != %s\n", ast_type_to_str(poly), ast_type_to_str(mono));
		return;
	case ast_type_app:
		assert(mono->tag == ast_type_app);
		assert(poly->as.app.args.len == mono->as.app.args.len);
		for (size_t i = 0; i < poly->as.app.args.len; ++i) {
			bind_polymorphic_type_vars(p, scope, bindings,
									   poly->as.app.args.elems[i],
									   mono->as.app.args.elems[i]);
		}
		break;
	case ast_type_ptr:
	case ast_type_mut_ptr:
		assert(type_is_pointer(mono));
		bind_polymorphic_type_vars(p, scope, bindings, poly->as.ptr, mono->as.ptr);
		return;
	case ast_type_slice:
	case ast_type_mut_slice:
		assert(type_is_slice(mono));
		bind_polymorphic_type_vars(p, scope, bindings, poly->as.slice, mono->as.slice);
		return;
	case ast_type_array:
		assert(type_is_array(mono));
		bind_polymorphic_type_vars(p, scope, bindings, poly->as.array.base, mono->as.array.base);
		return;
	case ast_type_istruct:
	case ast_type_struct:
		assert(type_is_struct(mono, scope));
		assert(poly->as.struct_t.len == mono->as.struct_t.len);
		for (size_t i = 0; i < poly->as.struct_t.len; ++i) {
			bind_polymorphic_type_vars(p, scope, bindings,
									   poly->as.struct_t.elems[i].type,
									   mono->as.struct_t.elems[i].type);
		}
		return;
	case ast_type_union: FAILWITH("TODO: ast_type_union"); break;
	case ast_type_vector: FAILWITH("TODO: ast_type_vector"); break;
	case ast_type_proc:
		assert(type_is_procedure(mono));
		assert(poly->as.proc.args.len == mono->as.proc.args.len);
		for (size_t i = 0; i < poly->as.proc.args.len; ++i) {
			bind_polymorphic_type_vars(p, scope, bindings, poly->as.proc.args.elems[i], mono->as.proc.args.elems[i]);
		}
		bind_polymorphic_type_vars(p, scope, bindings, poly->as.proc.ret, mono->as.proc.ret);
		return;
	case ast_type_var: {
		KCType *b = find_type_var_binding(bindings, poly);
		if (b == NULL) {
			add_type_var_binding(bindings, poly, mono);
		} else {
			assert(type_equiv(p, b, mono, scope));
		}
		return;
	} break;
	default: FAILWITH("Unreachable"); break;
	}
	FAILWITH("TODO");
}

KC_PRIVATE KCType *
type_var_subst(KCType *type, struct type_var_bindings *bindings)
{
	if (type == NULL) return NULL;
	KCType *newtype = NULL;
	switch (type->tag) {
	case ast_type_noreturn: FAILWITH("TODO: ast_type_noreturn"); break;
	case ast_type_void: return &AST_TYPE_VOID;
	case ast_type_bool: return &AST_TYPE_BOOL;
	case ast_type_i8:   return &AST_TYPE_I8;
	case ast_type_i16:  return &AST_TYPE_I16;
	case ast_type_i32:  return &AST_TYPE_I32;
	case ast_type_i64:  return &AST_TYPE_I64;
	case ast_type_u8:   return &AST_TYPE_U8;
	case ast_type_u16:  return &AST_TYPE_U64;
	case ast_type_u32:  return &AST_TYPE_U64;
	case ast_type_u64:  return &AST_TYPE_U64;
	case ast_type_f32:  return &AST_TYPE_F32;
	case ast_type_f64:  return &AST_TYPE_F64;
	case ast_type_app:
		newtype = MEM_ALLOC(KCType);
		newtype->tag = ast_type_app;
		newtype->as.app.cons = type->as.app.cons;
		for (size_t i = 0; i < type->as.app.args.len; ++i) {
			da_append(&newtype->as.app.args,
					  type_var_subst(type->as.app.args.elems[i], bindings));
		}
		return newtype;
	case ast_type_var: {
		newtype = find_type_var_binding(bindings, type);
		if (newtype) return newtype;
		return type;
	} break;
	case ast_type_ptr:
	case ast_type_mut_ptr:
		newtype = MEM_ALLOC(KCType);
		newtype->tag = type->tag;
		newtype->as.ptr = type_var_subst(type->as.ptr, bindings);
		return newtype;
	case ast_type_slice:
	case ast_type_mut_slice:
		newtype = MEM_ALLOC(KCType);
		newtype->tag = type->tag;
		newtype->as.slice = type_var_subst(type->as.slice, bindings);
		return newtype;
	case ast_type_array:
		newtype = MEM_ALLOC(KCType);
		newtype->tag = type->tag;
		newtype->as.array.base = type_var_subst(type->as.array.base, bindings);
		return newtype;
	case ast_type_istruct:
	case ast_type_struct:
		newtype = MEM_ALLOC(KCType);
		newtype->tag = type->tag;
		for (size_t i = 0; i < type->as.struct_t.len; ++i) {
			da_append(&newtype->as.struct_t, (struct struct_member) {
					.name = type->as.struct_t.elems[i].name,
					.type = type_var_subst(type->as.struct_t.elems[i].type, bindings),
				});
		}
		return newtype;
	case ast_type_union:
		newtype = MEM_ALLOC(KCType);
		newtype->tag = type->tag;
		for (size_t i = 0; i < type->as.union_t.len; ++i) {
			da_append(&newtype->as.union_t, (struct union_member) {
					.name      = type->as.union_t.elems[i].name,
					.tag_value = type->as.union_t.elems[i].tag_value,
					.type      = type_var_subst(type->as.union_t.elems[i].type, bindings),
				});
		}
		return newtype;
	case ast_type_proc:
		newtype = MEM_ALLOC(KCType);
		newtype->tag = type->tag;
		for (size_t i = 0; i < type->as.proc.args.len; ++i) {
			da_append(&newtype->as.proc.args, type_var_subst(type->as.proc.args.elems[i], bindings));
		}
		newtype->as.proc.ret = type_var_subst(type->as.proc.ret, bindings);
		return newtype;
	case ast_type_vector: FAILWITH("TODO: ast_type_vector"); break;
	default: FAILWITH("TODO: Unreachable"); break;
	}
	return NULL;
}

#define UNIFY(ctx, t, u, exp)											\
	do {																\
		KCType * _T = (t);											\
		KCType * _U = (u);											\
		if (!unify(p, ctx, _T, _U))										\
			type_mismatch_error(p, ctx, _T, _U, (exp)->tok, __FILE__, __LINE__); \
	} while (0)

#define UNIFY_EXP(ctx, exp, u)  \
	do {											\
		struct expression *_E = (exp);				\
		UNIFY(ctx, infer_type(p, ctx, _E), u, _E);	\
	} while (0)

KC_PRIVATE struct definition
specialize_definition(Parser *p,
					  struct typing_context ctx,
					  struct definition *def,
					  struct type_var_bindings *bindings)
{
	return (struct definition) {
		.id			= def->id,
		.type		= type_var_subst(def->type, bindings),
		.exp		= def->exp ? specialize_expression(p, ctx, def->exp, bindings) : NULL,
		.is_mut		= def->is_mut,
		.is_global	= def->is_global,
	};
}

KC_PRIVATE struct expression *
specialize_expression(Parser *p,
					  struct typing_context ctx,
					  struct expression *exp,
					  struct type_var_bindings *bindings)
{
	struct expression *newexp = MEM_ALLOC(struct expression);
	newexp->tag = exp->tag;
	newexp->tok = exp->tok;
	newexp->is_lvalue = exp->is_lvalue;
	newexp->is_mutable = exp->is_mutable;
	if (exp->tag != ast_exp_ident)
		newexp->type = type_var_subst(exp->type, bindings);
	switch (exp->tag) {
	case ast_exp_definition: FAILWITH("TODO: ast_exp_definition"); break;
	case ast_exp_let:
		newexp->as.let.def = specialize_definition(p, ctx, &exp->as.let.def, bindings);
		newexp->as.let.scope.parent = ctx.scope;
		symtbl_add(&newexp->as.let.scope.symtbl, &newexp->as.let.def, NULL);
		ctx.scope = &newexp->as.let.scope;
		type_env_add(&ctx, newexp->as.let.def.id,
					 generalize_type(&ctx, type_recursive_find(newexp->as.let.def.type)));
		newexp->as.let.body = specialize_expression(p, ctx, exp->as.let.body, bindings);
		break;
	case ast_exp_literal: newexp->as.lit = exp->as.lit; break;
	case ast_exp_array_initializer: FAILWITH("TODO: ast_exp_array_initializer"); break;
	case ast_exp_struct_initializer: FAILWITH("TODO: ast_exp_struct_initializer"); break;
	case ast_exp_named_struct_initializer: FAILWITH("TODO: ast_exp_named_struct_initializer"); break;
	case ast_exp_zero_struct_initializer: FAILWITH("TODO: ast_exp_zero_struct_initializer"); break;
	case ast_exp_procedure_literal: break;
	case ast_exp_undefined: FAILWITH("TODO: ast_exp_undefined"); break;
	case ast_exp_ident: {
		struct type_env *env = lookup_type_env(&ctx, newexp->tok);
		if (env == NULL)
			FAILWITH("Undefined variable: `"SV_FMT"`.", SV_ARGS(token_to_strview(newexp->tok)));
		newexp->type = instantiate_type_scheme(&env->scheme);
	} break;
	case ast_exp_binary:
		newexp->as.bin.op = exp->as.bin.op;
		newexp->as.bin.left = specialize_expression(p, ctx, exp->as.bin.left, bindings);
		newexp->as.bin.right = specialize_expression(p, ctx, exp->as.bin.right, bindings);
		break;
	case ast_exp_value_cons: FAILWITH("TODO: ast_exp_value_cons"); break;
	case ast_exp_unary:
		newexp->as.una.op = exp->as.una.op;
		switch ((enum unaop)exp->as.una.op) {
		case unaop_lnot:
		case unaop_not:
		case unaop_neg:
		case unaop_pos:
		case unaop_address_of:
		case unaop_dereference:
			newexp->as.una.exp = specialize_expression(p, ctx, exp->as.una.exp, bindings);
			break;
		case unaop_index:
			newexp->as.idx.exp = specialize_expression(p, ctx, exp->as.idx.exp, bindings);
			newexp->as.idx.idx = specialize_expression(p, ctx, exp->as.idx.idx, bindings);
			break;
		case unaop_call: {
			newexp->as.call.proc = specialize_expression(p, ctx, exp->as.call.proc, bindings);
			if (type_is_polymorphic(newexp->as.call.proc->type)) {
				struct call *call = &newexp->as.call;
				if (call->proc->tag != ast_exp_ident) {
					FAILWITH("TODO: polymorphic procedure must be let-bound.");
				}
				struct definition *generic = NULL;
				struct symtbl_entry *entry = lookup_entry(ctx.scope, token_to_strview(call->proc->tok));
				if (entry == NULL) ERROR_UNDEFINED_IDENT(call->proc->tok);
				if (entry->tag == SYMTBL_VARIABL) {
					generic = entry->as.variable.def;
				} else {
					FAILWITH("TODO: expected procedure.");
				}
				assert(generic->type->tag == ast_type_proc);
				KCType *inf_type = MEM_ALLOC(KCType);
				inf_type->tag = ast_type_proc;
				KCType *ret_type = newexp->type;
				inf_type->as.proc.ret = ret_type;
				for (size_t i = 0; i < exp->as.call.args.len; ++i) {
					struct expression *arg_exp = exp->as.call.args.elems[i];
					arg_exp = specialize_expression(p, ctx, arg_exp, bindings);
					assert(arg_exp->type);
					da_append(&newexp->as.call.args, arg_exp);
					da_append(&inf_type->as.proc.args, arg_exp->type);
				}
				UNIFY(ctx, call->proc->type, inf_type, newexp);
				if (lookup_poly_proc_spec(p, generic, inf_type, ctx.scope) == NULL) {
					da_append(&generic->specs, (struct type_spec) {
							.type = inf_type,
						});
				}
			} else {
				for (size_t i = 0; i < exp->as.call.args.len; ++i) {
					struct expression *arg_exp = exp->as.call.args.elems[i];
					da_append(&newexp->as.call.args, specialize_expression(p, ctx, arg_exp, bindings));
				}
			}
		} break;
		case unaop_cast:
			newexp->as.cast.type = type_var_subst(exp->as.cast.type, bindings);
			newexp->as.cast.exp = specialize_expression(p, ctx, exp->as.cast.exp, bindings);
			break;
		case unaop_slice:
			newexp->as.slice.exp = specialize_expression(p, ctx, exp->as.slice.exp, bindings);
			newexp->as.slice.idx = specialize_expression(p, ctx, exp->as.slice.idx, bindings);
			newexp->as.slice.len = specialize_expression(p, ctx, exp->as.slice.len, bindings);
			break;
		default: FAILWITH("Unreachable"); break;
		}
		break;
	case ast_exp_while:
		newexp->as.wloop.cond = specialize_expression(p, ctx, exp->as.wloop.cond, bindings);
		newexp->as.wloop.body = specialize_expression(p, ctx, exp->as.wloop.body, bindings);
		break;
	case ast_exp_if:
		newexp->as.iff.cond = specialize_expression(p, ctx, exp->as.iff.cond, bindings);
		newexp->as.iff.tb   = specialize_expression(p, ctx, exp->as.iff.tb, bindings);
		if (exp->as.iff.fb)
			newexp->as.iff.fb = specialize_expression(p, ctx, exp->as.iff.fb, bindings);
		break;
	case ast_exp_case: FAILWITH("TODO: ast_exp_case"); break;
	case ast_exp_return: FAILWITH("TODO: ast_exp_return"); break;
	case ast_exp_break: FAILWITH("TODO: ast_exp_break"); break;
	case ast_exp_continue: FAILWITH("TODO: ast_exp_continue"); break;
	case ast_exp_extern_symbol: FAILWITH("TODO: ast_exp_extern_symbol"); break;
	case ast_exp_get_ptr:
		newexp->as.get_ptr = specialize_expression(p, ctx, exp->as.get_ptr, bindings);
		break;
	case ast_exp_get_len:
		newexp->as.get_len = specialize_expression(p, ctx, exp->as.get_len, bindings);
		break;
	case ast_exp_size_of: FAILWITH("TODO: ast_exp_size_of"); break;
	default: FAILWITH("Unreachable"); break;
	}
	return newexp;
}

void specialize_generic_procedure(Parser *p,
								  struct typing_context ctx,
								  struct definition *def,
								  struct type_spec *spec_def)
{
	printf("[Debug] Instantiating procedure `"SV_FMT"` for type: `", SV_ARGS(token_to_strview(def->id)));
	ast_type_fprint(spec_def->type, stdout);
	printf("`\n");
	assert(def->exp->tag == ast_exp_procedure_literal);
	assert(def->type->tag == ast_type_proc);
	KCType *generic_type = type_recursive_find(def->type);
	KCType *spec_type = type_recursive_find(spec_def->type);
	spec_def->type = spec_type;
	spec_def->exp = MEM_ALLOC(struct expression);
	spec_def->exp->tag = ast_exp_procedure_literal;
	spec_def->exp->type = spec_type;
	spec_def->exp->is_lvalue = def->exp->is_lvalue;
	spec_def->exp->is_mutable = def->exp->is_mutable;
	spec_def->exp->tok = def->exp->tok;
	struct type_var_bindings bindings = {0};
	struct procedure *newproc = &spec_def->exp->as.proc;
	struct procedure *proc = &def->exp->as.proc;
	{
		bind_polymorphic_type_vars(p, ctx.scope, &bindings, generic_type, spec_type);
		type_env_add(&ctx, def->id, generalize_type(&ctx, spec_type));
		for (size_t i = 0; i < proc->formals.len; ++i) {
			struct definition *arg_def = da_allot(&newproc->formals);
			*arg_def = proc->formals.elems[i];
			arg_def->type = type_var_subst(proc->formals.elems[i].type, &bindings);
			symtbl_add(&newproc->scope.symtbl, arg_def, NULL);
			type_env_add(&ctx, proc->formals.elems[i].id, (Forall){.type=arg_def->type});
		}
	}
	newproc->ret = type_var_subst(proc->ret, &bindings);
	newproc->scope.parent = ctx.scope;
	ctx.scope = &newproc->scope;
	newproc->body = specialize_expression(p, ctx, proc->body, &bindings);
	da_free(&bindings);
}

UNUSED KC_PRIVATE KCType *
find_union_member_type(struct union_type *ut, struct token *cons_name)
{
	struct strview name = token_to_strview(cons_name);
	for (size_t i = 0; i < ut->len; ++i) {
		if (sv_is_equal(name, token_to_strview(ut->elems[i].name)))
			return ut->elems[i].type;
	}
	return NULL;
}

KC_PRIVATE struct type_env *
lookup_type_env(struct typing_context *ctx, struct token *var)
{
	struct strview var_name = token_to_strview(var);
	struct type_env *env;
	for (env = ctx->env; env != NULL; env = env->next) {
		if (sv_is_equal(var_name, env->name)) break;
	}
	return env;
}

UNUSED KC_PRIVATE bool
type_occurs_in(KCType *var, KCType *type)
{
	var = type_find(var);
	type = type_find(type);
	switch (type->tag) {
	case ast_type_void:
	case ast_type_noreturn:
	case ast_type_bool:
	case ast_type_i8:
	case ast_type_i16:
	case ast_type_i32:
	case ast_type_i64:
	case ast_type_u8:
	case ast_type_u16:
	case ast_type_u32:
	case ast_type_u64:
	case ast_type_f32:
	case ast_type_f64: return false;
	case ast_type_app:
		for (size_t i = 0; i < type->as.app.args.len; ++i) {
			if (type_occurs_in(var, type->as.app.args.elems[i]))
				return true;
		}
		return false;
	case ast_type_ptr:
	case ast_type_mut_ptr:
		return type_occurs_in(var, type->as.ptr);
	case ast_type_slice:
	case ast_type_mut_slice:
		return type_occurs_in(var, type->as.slice);
	case ast_type_array:
		return type_occurs_in(var, type->as.array.base);
	case ast_type_istruct:
	case ast_type_struct:
		for (size_t i = 0; i < type->as.struct_t.len; ++i) {
			if (type_occurs_in(var, type->as.struct_t.elems[i].type))
				return true;
		}
		return false;
	case ast_type_union:
		for (size_t i = 0; i < type->as.union_t.len; ++i) {
			if (type_occurs_in(var, type->as.union_t.elems[i].type))
				return true;
		}
		return false;
	case ast_type_vector: FAILWITH("TODO: ast_type_vector"); break;
	case ast_type_proc:
		for (size_t i = 0; i < type->as.proc.args.len; ++i) {
			if (type_occurs_in(var, type->as.proc.args.elems[i]))
				return true;
		}
		return type_occurs_in(var, type->as.proc.ret);
	case ast_type_var:
		return type == var;
	default: FAILWITH("Unreachable");
	}
}

KC_PRIVATE bool
type_var_is_free(struct typing_context *ctx, KCType *var)
{
	assert(var->tag == ast_type_var);
	var = type_find(var);
	struct type_env *env;
	for (env = ctx->env; env != NULL; env = env->next) {
		for (size_t i = 0; i < env->scheme.args.len; ++i) {
			if (var == type_find(env->scheme.args.elems[i])) return false;
		}
	}
	return true;
}

KC_PRIVATE void
build_type_scheme(struct typing_context *ctx, KCType *type, struct type_scheme *scm)
{
	switch (type->tag) {
	case ast_type_void:
	case ast_type_noreturn:
	case ast_type_bool:
	case ast_type_i8:
	case ast_type_i16:
	case ast_type_i32:
	case ast_type_i64:
	case ast_type_u8:
	case ast_type_u16:
	case ast_type_u32:
	case ast_type_u64:
	case ast_type_f32:
	case ast_type_f64: return;
	case ast_type_app:
		for (size_t i = 0; i < type->as.app.args.len; ++i) {
			build_type_scheme(ctx, type->as.app.args.elems[i], scm);
		}
		return;
	case ast_type_ptr:
	case ast_type_mut_ptr:
		return build_type_scheme(ctx, type->as.ptr, scm);
	case ast_type_slice:
	case ast_type_mut_slice:
		return build_type_scheme(ctx, type->as.slice, scm);
	case ast_type_array:
		return build_type_scheme(ctx, type->as.array.base, scm);
	case ast_type_istruct:
	case ast_type_struct:
		for (size_t i = 0; i < type->as.struct_t.len; ++i) {
			build_type_scheme(ctx, type->as.struct_t.elems[i].type, scm);
		}
		return;
	case ast_type_union:
		for (size_t i = 0; i < type->as.union_t.len; ++i) {
			build_type_scheme(ctx, type->as.union_t.elems[i].type, scm);
		}
		return;
	case ast_type_vector:	 FAILWITH("TODO: ast_type_vector");	   break;
	case ast_type_proc:
		for (size_t i = 0; i < type->as.proc.args.len; ++i) {
			build_type_scheme(ctx, type->as.proc.args.elems[i], scm);
		}
		return build_type_scheme(ctx, type->as.proc.ret, scm);
	case ast_type_var:
		if (type_var_is_free(ctx, type))
			da_append(&scm->args, type);
		return;
	default: FAILWITH("Unreachable");
	}
}

KC_PRIVATE struct type_scheme
generalize_type(struct typing_context *ctx, KCType *type)
{
	struct type_scheme scm = {.type = type};
	build_type_scheme(ctx, type, &scm);
	return scm;
}

KC_PRIVATE KCType *
fresh_type_var(enum type_class c)
{
	KCType *t = MEM_ALLOC(KCType);
	t->tag = ast_type_var;
	t->as.var.class = c;
	return t;
}

KC_PRIVATE KCType *
fresh_type_var_from(KCType *type)
{
	assert(type->tag == ast_type_var);
	KCType *t = fresh_type_var(type->as.var.class);
	return t;
}


KC_PRIVATE KCType *
instantiate_type_scheme(struct type_scheme *scm)
{
	if (scm->args.len == 0) return scm->type;
	struct type_var_bindings bindings = {0};
	for (size_t i = 0; i < scm->args.len; ++i) {
		da_append(&bindings, (struct type_pair){
				.var  = scm->args.elems[i],
				.type = fresh_type_var_from(scm->args.elems[i]),
			});
	}
	KCType *type = type_var_subst(scm->type, &bindings);
	da_free(&bindings);
	return type;
}

KC_PUBLIC KCType *
type_find(KCType *type)
{
	for (; type->tag == ast_type_var; type = type->as.var.forward) {
		if (type->as.var.forward == NULL)
			return type;
	}
	return type;
}

KC_PUBLIC KCType *
type_recursive_find(KCType *type)
{
	KCType *newtype = NULL;
	type = type_find(type);
	switch (type->tag) {
	case ast_type_void:
	case ast_type_noreturn:
	case ast_type_bool:
	case ast_type_i8:
	case ast_type_i16:
	case ast_type_i32:
	case ast_type_i64:
	case ast_type_u8:
	case ast_type_u16:
	case ast_type_u32:
	case ast_type_u64:
	case ast_type_f32:
	case ast_type_f64:
	case ast_type_var:
		return type;
	case ast_type_ptr:
	case ast_type_mut_ptr:
		newtype = MEM_ALLOC(KCType);
		newtype->tag = type->tag;
		newtype->as.ptr = type_recursive_find(type->as.ptr);
		return newtype;
	case ast_type_slice:
	case ast_type_mut_slice:
		newtype = MEM_ALLOC(KCType);
		newtype->tag = type->tag;
		newtype->as.slice = type_recursive_find(type->as.slice);
		return newtype;
	case ast_type_array:
		newtype = MEM_ALLOC(KCType);
		newtype->tag = type->tag;
		newtype->as.array.is_sized = type->as.array.is_sized;
		newtype->as.array.size = type->as.array.size;
		newtype->as.array.base = type_recursive_find(type->as.array.base);
		return newtype;
	case ast_type_app:
		newtype = MEM_ALLOC(KCType);
		newtype->tag = type->tag;
		newtype->as.app.cons = type->as.app.cons;
		for (size_t i = 0; i < type->as.app.args.len; ++i) {
			da_append(&newtype->as.app.args, type_recursive_find(type->as.app.args.elems[i]));
		}
		return newtype;
	case ast_type_struct:
	case ast_type_istruct:
		newtype = MEM_ALLOC(KCType);
		newtype->tag = type->tag;
		for (size_t i = 0; i < type->as.struct_t.len; ++i) {
			da_append(&newtype->as.struct_t, (struct struct_member) {
					.name = type->as.struct_t.elems[i].name,
					.type = type_recursive_find(type->as.struct_t.elems[i].type),
				});
		}
		return newtype;
	case ast_type_union: FAILWITH("TODO"); break;
	case ast_type_vector: FAILWITH("TODO"); break;
	case ast_type_proc:
		newtype = MEM_ALLOC(KCType);
		newtype->tag = type->tag;
		for (size_t i = 0; i < type->as.proc.args.len; ++i) {
			da_append(&newtype->as.proc.args, type_recursive_find(type->as.proc.args.elems[i]));
		}
		newtype->as.proc.ret = type_recursive_find(type->as.proc.ret);
		return newtype;
	default: FAILWITH("Unreachable");
	}
}

KC_PRIVATE void
type_var_set_equal_to(KCType *t, KCType *u)
{
	t = type_find(t);
	assert(t->tag == ast_type_var);
	t->as.var.forward = u;
}

KC_PRIVATE bool
unify(Parser *p, struct typing_context ctx, KCType *t, KCType *u)
{
	t = resolve_alias(p, type_find(t), ctx.scope);
	u = resolve_alias(p, type_find(u), ctx.scope);
	if (!type_is_var(t) && type_is_var(u))
		return unify(p, ctx, u, t);
	if (type_is_void(u)) {
		if (type_is_var(t))
			type_var_set_equal_to(t, u);
		return true;
	}
	switch (t->tag) {
	case ast_type_void:
		return true;
	case ast_type_noreturn: FAILWITH("TODO: ast_type_noreturn"); break;
	case ast_type_bool:
	case ast_type_i8:
	case ast_type_i16:
	case ast_type_i32:
	case ast_type_i64:
	case ast_type_u8:
	case ast_type_u16:
	case ast_type_u32:
	case ast_type_u64:
	case ast_type_f32:
	case ast_type_f64:
		if (t->tag == u->tag) return true;
		return false;
	case ast_type_app: {
		if ((u->tag == ast_type_union
			 || u->tag == ast_type_istruct
			 || u->tag == ast_type_array)
			&& unify(p, ctx, type_application(p, &t->as.app, ctx.scope), u)) {
			return true;
		}
		if (u->tag != ast_type_app) return false;
		if (!sv_is_equal(token_to_strview(t->as.app.cons), token_to_strview(u->as.app.cons)))
			return false;
		assert(t->as.app.args.len == u->as.app.args.len);
		for (size_t i = 0; i < t->as.app.args.len; ++i) {
			KCType *tm = t->as.app.args.elems[i];
			KCType *um = u->as.app.args.elems[i];
			if (!unify(p, ctx, tm, um)) {
				return false;
			}
		}
		return true;
	} break;
	case ast_type_ptr: {
		if (type_is_pointer(u)) return unify(p, ctx, t->as.ptr, u->as.ptr);
		return false;
	} break;
	case ast_type_mut_ptr: FAILWITH("TODO: ast_type_mut_ptr"); break;
	case ast_type_slice: {
		if (type_is_slice(u))
			return unify(p, ctx, t->as.slice, u->as.slice);
		return false;
	} break;
	case ast_type_mut_slice: FAILWITH("TODO: ast_type_mut_slice"); break;
	case ast_type_array: {
		if (type_is_array(u)) {
			if (!unify(p, ctx, t->as.array.base, u->as.array.base)) return false;
			if (t->as.array.is_sized && u->as.array.is_sized)
				return t->as.array.size == u->as.array.size;
			if (t->as.array.is_sized) {
				u->as.array.is_sized = true;
				u->as.array.size = t->as.array.size;
				return true;
			}
			t->as.array.is_sized = true;
			t->as.array.size = u->as.array.size;
			return true;
		}
		return false;
	} break;
	case ast_type_union: {
		if (u->tag == ast_type_union) {
			if (t->as.union_t.len != u->as.union_t.len) return false;
			for (size_t i = 0; i < t->as.union_t.len; ++i) {
				if (!sv_is_equal(token_to_strview(t->as.union_t.elems[i].name),
								 token_to_strview(u->as.union_t.elems[i].name)))
					return false;
				if (t->as.union_t.elems[i].tag_value != u->as.union_t.elems[i].tag_value)
					return false;
				if (!unify(p, ctx, t->as.union_t.elems[i].type, u->as.union_t.elems[i].type)) {
					KCType *tm = t->as.union_t.elems[i].type;
					KCType *um = u->as.union_t.elems[i].type;
					printf("tm = ");
					ast_type_fprint(tm, stdout);
					printf("\n");
					printf("um = ");
					ast_type_fprint(um, stdout);
					printf("\n");
					return false;
				}
			}
			return true;
		} else {
			FAILWITH("TODO");
		}
	} break;
	case ast_type_istruct: {
		if (u->tag == ast_type_app)
			u = type_application(p, &u->as.app, ctx.scope);
		if (u->tag == ast_type_struct) {
			if (t->as.struct_t.len != u->as.struct_t.len) return false;
			for (size_t i = 0; i < t->as.struct_t.len; ++i) {
				if (t->as.struct_t.elems[i].name) {
					if (u->as.struct_t.elems[i].name
						&& !sv_is_equal(token_to_strview(t->as.struct_t.elems[i].name),
										token_to_strview(u->as.struct_t.elems[i].name)))
						return false;
				}
				if (!unify(p, ctx, t->as.struct_t.elems[i].type, u->as.struct_t.elems[i].type))
					return false;
			}
			t->tag = ast_type_struct;
			return true;
		}
		return false;
	} break;
	case ast_type_struct: {
		if (u->tag == ast_type_struct) {
			if (t->as.struct_t.len != u->as.struct_t.len) return false;
			for (size_t i = 0; i < t->as.struct_t.len; ++i) {
				if (t->as.struct_t.elems[i].name && u->as.struct_t.elems[i].name
					&& !sv_is_equal(token_to_strview(t->as.struct_t.elems[i].name),
									token_to_strview(u->as.struct_t.elems[i].name)))
					return false;
				if (!unify(p, ctx, t->as.struct_t.elems[i].type, u->as.struct_t.elems[i].type))
					return false;
			}
			return true;
		}
		return false;
	} break;
	case ast_type_vector: FAILWITH("TODO: ast_type_vector"); break;
	case ast_type_proc: {
		if (!type_is_procedure(u)) return false;
		if (t->as.proc.args.len != u->as.proc.args.len) return false;
		for (size_t i = 0; i < t->as.proc.args.len; ++i) {
			if (!unify(p, ctx, t->as.proc.args.elems[i], u->as.proc.args.elems[i]))
				return false;
		}
		return unify(p, ctx, t->as.proc.ret, u->as.proc.ret);
	} break;
	case ast_type_var: {
		if (t == u) return true;
		switch (t->as.var.class) {
		case type_class_any:
			type_var_set_equal_to(t, u);
			return true;
		case type_class_unsigned_integer:
			if (type_is_var(u)) FAILWITH("TODO");
			if (type_is_unsigned_integer(u)) {
				type_var_set_equal_to(t, u);
				return true;
			}
			return false;
		case type_class_signed_integer:
			if (type_is_var(u)) FAILWITH("TODO");
			if (type_is_signed_integer(u)) {
				type_var_set_equal_to(t, u);
				return true;
			}
			return false;
		case type_class_integer:
//			printf("t = %s\n", ast_type_to_str(t));
//			printf("u = %s\n", ast_type_to_str(u));
			if (type_is_integer(u)) {
				type_var_set_equal_to(t, u);
				return true;
			}
			if (type_is_var(u)) {
				switch (u->as.var.class) {
				case type_class_any:
				case type_class_scalar:
				case type_class_numeric:
					u->as.var.class = t->as.var.class;
					type_var_set_equal_to(t, u);
					return true;
				case type_class_signed_integer:
				case type_class_unsigned_integer:
					type_var_set_equal_to(t, u);
					return true;
				case type_class_integer:
					type_var_set_equal_to(t, u);
					return true;
				case type_class_float:
				case type_class_length:
				case type_class_indexable:
				case type_class_struct:
				case type_class_union:
				case type_class_procedure:
					return false;
				default: FAILWITH("Unreachable");
				}
			}
			return false;
		case type_class_float:
			if (type_is_var(u)) FAILWITH("TODO");
			if (type_is_floating_point(u)) {
				type_var_set_equal_to(t, u);
				return true;
			}
			return false;
		case type_class_numeric:
			if (type_is_var(u)) {
				switch (u->as.var.class) {
				case type_class_any:
					u->as.var.class = t->as.var.class;
					type_var_set_equal_to(t, u);
					return true;
				case type_class_numeric:
				case type_class_scalar:
				case type_class_signed_integer:
				case type_class_unsigned_integer:
				case type_class_integer:
				case type_class_float:
					type_var_set_equal_to(t, u);
					return true;
				case type_class_length:
				case type_class_indexable:
				case type_class_struct:
				case type_class_union:
				case type_class_procedure:
					return false;
				default: FAILWITH("Unreachable");
				}
			} else if (type_is_numeric_scalar(u)) {
				type_var_set_equal_to(t, u);
				return true;
			}
			return false;
		case type_class_scalar: {
			KCType *v = NULL;
			if (type_is_var(u)) {
				switch (u->as.var.class) {
				case type_class_any:
					u->as.var.class = t->as.var.class;
					type_var_set_equal_to(t, u);
					return true;
				case type_class_numeric:
				case type_class_signed_integer:
				case type_class_unsigned_integer:
				case type_class_integer:
				case type_class_float:
					type_var_set_equal_to(t, u);
					return true;
				case type_class_scalar:
					return true;
				case type_class_length:
				case type_class_indexable:
				case type_class_struct:
				case type_class_union:
				case type_class_procedure:
					return false;
				default: FAILWITH("Unreachable");
				}
			} else if (type_is_scalar(u)) {
				type_var_set_equal_to(t, u);
				return true;
			} else if (u->tag == ast_type_app
					   && type_get_underlying(ctx.scope, u, &v)
					   && type_is_scalar(v)) {
				type_var_set_equal_to(t, u);
				return true;
			}
			return false;
		} break;
		case type_class_length:
			if (type_is_var(u)) FAILWITH("TODO");
			if (type_has_length(u, ctx.scope)) {
				type_var_set_equal_to(t, u);
				return true;
			}
			return false;
		case type_class_indexable: {
			if (type_is_var(u)) {
				switch (u->as.var.class) {
				case type_class_any:
				case type_class_indexable:
					u->as.var.class = t->as.var.class;
					type_var_set_equal_to(t, u);
					return true;
				case type_class_length:
					return true;
				case type_class_numeric:
				case type_class_signed_integer:
				case type_class_unsigned_integer:
				case type_class_integer:
				case type_class_float:
				case type_class_scalar:
				case type_class_struct:
				case type_class_union:
				case type_class_procedure:
					return false;
				default: FAILWITH("Unreachable");
				}
			}
			if (type_is_indexable(u)) {
				type_var_set_equal_to(t, u);
				return unify(p, ctx, t->as.var.contains, get_indexable_base_type(u));
			} else {
				return false;
			}
		} break;
		case type_class_struct: {
			if (type_is_var(u)) FAILWITH("TODO");
			if (type_is_struct(u, ctx.scope) || type_is_struct_ptr(u, ctx.scope)) {
				type_var_set_equal_to(t, u);
				return true;
			} else {
				FAILWITH("TODO");
			}
		} break;
		case type_class_union: {
			if (type_is_var(u)) FAILWITH("TODO");
			if (type_is_union(u, ctx.scope) || type_is_union_ptr(u, ctx.scope)) {
				type_var_set_equal_to(t, u);
				return true;
			} else {
				FAILWITH("TODO");
			}
		} break;
		case type_class_procedure: {
			if (type_is_var(u)) FAILWITH("TODO");
			if (type_is_procedure(u)) {
				type_var_set_equal_to(t, u);
				return true;
			} else {
				FAILWITH("TODO");
			}
		} break;
		default: FAILWITH("Unreachable"); break;
		}
	} break;
	default: FAILWITH("Unreachable"); break;
	}
	return false;
}

KC_PRIVATE bool
unify_cast(Parser *p, struct typing_context ctx, KCType *t, KCType *u)
{
	t = resolve_alias(p, type_find(t), ctx.scope);
	u = resolve_alias(p, type_find(u), ctx.scope);
	if (type_is_void(u)) {
		return true;
	} else if (type_is_var(t) || type_is_var(u)) {
		return true;
	} else if (type_is_scalar(t) && type_is_scalar(u)) {
		return true;
	} else {
		return false;
	}
}

KC_PRIVATE void
type_env_add(struct typing_context *ctx, struct token *name, struct type_scheme scm)
{
	struct type_env *env = MEM_ALLOC(struct type_env);
	env->name   = token_to_strview(name);
	env->scheme = scm;
	env->next   = ctx->env;
	ctx->env    = env;
}

KC_PRIVATE KCType *
get_valcons_entry_type(struct valcons_entry *e)
{
	KCType *u = MEM_ALLOC(KCType);
	u->tag = ast_type_app;
	u->as.app.cons = e->td->name;
	u->as.app.args = e->td->args;
	return u;
}

KC_PRIVATE KCType *
get_fresh_valcons_entry_type(struct typing_context *ctx, struct valcons_entry *e)
{
	KCType *t = get_valcons_entry_type(e);
	Forall s = generalize_type(ctx, t);
	KCType *u = instantiate_type_scheme(&s);
	free(t);
	return u;
}

KC_PRIVATE KCType *
infer_type(Parser *p, struct typing_context ctx, struct expression *exp)
{
	switch (exp->tag) {
	case ast_exp_definition: {
		assert(exp->as.def.type != NULL);
		if (exp->as.def.exp->tag == ast_exp_extern_symbol) {
			exp->as.def.exp->type = exp->as.def.type;
		} else {
			UNIFY_EXP(ctx, exp->as.def.exp, exp->as.def.type);
		}
		exp->as.def.is_typechecked = true;
		return exp->type = &AST_TYPE_VOID;
	} break;
	case ast_exp_let: {
		struct definition *def = &exp->as.let.def;
		UNIFY_EXP(ctx, def->exp, exp->as.def.type);
		ctx.scope = &exp->as.let.scope;
		KCType *type = fresh_type_var(type_class_any);
		type_env_add(&ctx, exp->as.let.def.id, generalize_type(&ctx, type_recursive_find(exp->as.let.def.type)));
		UNIFY_EXP(ctx, exp->as.let.body, type);
		exp->as.let.def.is_typechecked = true;
		return exp->type = type;
	} break;
	case ast_exp_literal: {
		struct literal *lit = &exp->as.lit;
		KCType *type;
		switch (lit->tag) {
		case LITERAL_BOOL:   type = &AST_TYPE_BOOL;                     break;
		case LITERAL_INT:    type = fresh_type_var(type_class_integer); break;
		case LITERAL_FLOAT:  type = fresh_type_var(type_class_float);   break;
		case LITERAL_STRING: type = &AST_TYPE_STRING;                   break;
		case LITERAL_CHAR:   type = &AST_TYPE_CHAR;                     break;
		}
		return exp->type = type;
	} break;
	case ast_exp_array_initializer: {
		KCType *base = fresh_type_var(type_class_any);
		KCType *type = MEM_ALLOC(KCType);
		type->tag = ast_type_array;
		for (size_t i = 0; i < exp->as.init.len; ++i) {
			UNIFY_EXP(ctx, exp->as.init.elems[i], base);
		}
		type->as.array.base = base;
		type->as.array.is_sized = true;
		type->as.array.size = exp->as.init.len;
		return exp->type = type;
	} break;
	case ast_exp_struct_initializer: {
		KCType *type = MEM_ALLOC(KCType);
		type->tag = ast_type_istruct;
		for (size_t i = 0; i < exp->as.init.len; ++i) {
			KCType *tv = fresh_type_var(type_class_any);
			UNIFY_EXP(ctx, exp->as.init.elems[i], tv);
			da_append(&type->as.struct_t, (struct struct_member) {
					.type = tv,
				});
		}
		return exp->type = type;
	} break;
	case ast_exp_named_struct_initializer: {
		KCType *type = MEM_ALLOC(KCType);
		type->tag = ast_type_istruct;
		for (size_t i = 0; i < exp->as.named_init.ids.len; ++i) {
			KCType *tv = fresh_type_var(type_class_any);
			UNIFY_EXP(ctx, exp->as.named_init.exps.elems[i], tv);
			da_append(&type->as.struct_t, (struct struct_member) {
					.name = exp->as.named_init.ids.elems[i],
					.type = tv,
				});
		}
		return exp->type = type;
	} break;
	case ast_exp_zero_struct_initializer: FAILWITH("TODO: infer_type (ast_exp_zero_struct_initializer)"); break;
	case ast_exp_procedure_literal: {
		struct procedure *proc = &exp->as.proc;
		KCType *type = MEM_ALLOC(KCType);
		type->tag = ast_type_proc;
		type->as.proc = procedure_type(proc);
		ctx.scope = &proc->scope;
		for (size_t i = 0; i < proc->formals.len; ++i) {
			type_env_add(&ctx, proc->formals.elems[i].id, generalize_type(&ctx, proc->formals.elems[i].type));
		}
		UNIFY_EXP(ctx, proc->body, type->as.proc.ret);
		return exp->type = type;
	} break;
	case ast_exp_undefined: FAILWITH("TODO: infer_type (ast_exp_undefined)"); break;
	case ast_exp_ident: {
		struct symtbl_entry *entry = lookup_entry(ctx.scope, token_to_strview(exp->tok));
		if (entry == NULL) ERROR_UNDEFINED_IDENT(exp->tok);
		switch (entry->tag) {
		case SYMTBL_VARIABL: {
			struct definition *def = entry->as.variable.def;
			struct type_env *env = lookup_type_env(&ctx, exp->tok);
			assert(env != NULL);
			exp->is_mutable = def->is_mut;
			exp->is_lvalue = true;
			return exp->type = instantiate_type_scheme(&env->scheme);
		} break;
		case SYMTBL_VALCONS: {
			FAILWITH("Unreachable");
		} break;
		default: FAILWITH("Unreachable");
		}
	} break;
	case ast_exp_binary: {
		switch ((enum binop)exp->as.bin.op) {
		case binop_sequence: {
			UNIFY_EXP(ctx, exp->as.bin.left, &AST_TYPE_VOID);
			KCType *type = fresh_type_var(type_class_any);
			UNIFY_EXP(ctx, exp->as.bin.right, type);
			return exp->type = type;
		} break;
		case binop_add:
		case binop_sub:
		case binop_mul:
		case binop_div: {
			KCType *type = fresh_type_var(type_class_numeric);
			UNIFY_EXP(ctx, exp->as.bin.left, type);
			UNIFY_EXP(ctx, exp->as.bin.right, type);
			return exp->type = type;
		} break;
		case binop_mod:
		case binop_xor:
		case binop_land:
		case binop_lor:
		case binop_shift_left:
		case binop_shift_right: {
			KCType *type = fresh_type_var(type_class_integer);
			UNIFY_EXP(ctx, exp->as.bin.left, type);
			UNIFY_EXP(ctx, exp->as.bin.right, type);
			return exp->type = type;
		} break;
		case binop_equal:
		case binop_less_than:
		case binop_more_than:
		case binop_not_equal:
		case binop_less_equal:
		case binop_more_equal: {
			KCType *type = fresh_type_var(type_class_scalar);
			UNIFY_EXP(ctx, exp->as.bin.left, type);
			UNIFY_EXP(ctx, exp->as.bin.right, type);
			return exp->type = &AST_TYPE_BOOL;
		} break;
		case binop_or:
		case binop_and: {
			UNIFY_EXP(ctx, exp->as.bin.left, &AST_TYPE_BOOL);
			UNIFY_EXP(ctx, exp->as.bin.right, &AST_TYPE_BOOL);
			return exp->type = &AST_TYPE_BOOL;
		} break;
		case binop_assign: {
			KCType *type = fresh_type_var(type_class_any);
			UNIFY_EXP(ctx, exp->as.bin.left, type);
			UNIFY_EXP(ctx, exp->as.bin.right, type);
			if (!exp->as.bin.left->is_lvalue)
				log_error_and_die(p->lexer.filename, exp->as.bin.left->tok,
								  "Memory address is unbound in left hand side of assignment (not an lvalue).");
			if (!exp->as.bin.left->is_mutable)
				log_error_and_die(p->lexer.filename, exp->as.bin.left->tok,
								  "Left hand side of assignment is immutable.");
			return exp->type = type;
		} break;
		case binop_add_assign:
		case binop_sub_assign:
		case binop_mul_assign:
		case binop_div_assign: {
			KCType *type = fresh_type_var(type_class_numeric);
			UNIFY_EXP(ctx, exp->as.bin.left, type);
			UNIFY_EXP(ctx, exp->as.bin.right, type);
			if (!exp->as.bin.left->is_lvalue)
				log_error_and_die(p->lexer.filename, exp->as.bin.left->tok,
								  "Memory address is unbound in left hand side of assignment (not an lvalue).");
			if (!exp->as.bin.left->is_mutable)
				log_error_and_die(p->lexer.filename, exp->as.bin.left->tok,
								  "Left hand side of assignment is immutable.");
			return exp->type = type;
		} break;
		case binop_member: {
			KCType *type, *infered = infer_type(p, ctx, exp->as.bin.left);
			if (infered->tag == ast_type_app) {
				type = type_application(p, &infered->as.app, ctx.scope);
			} else {
				type = infered;
			}
			if (type_is_struct_ptr(type, ctx.scope))
				type = type->as.ptr;
			if (!type_is_struct(type, ctx.scope)) {
				log_error_and_die(p->lexer.filename, exp->tok,
								  "KCType error. `%s` is not a structure.",
								  ast_type_to_str(infered));
			}
			if (exp->as.bin.right->tag == ast_exp_ident) {
				struct strview sv_mem = token_to_strview(exp->as.bin.right->tok);
				for (size_t i = 0; i < type->as.struct_t.len; ++i) {
					if (type->as.struct_t.elems[i].name != NULL
						&& sv_is_equal(token_to_strview(type->as.struct_t.elems[i].name), sv_mem)) {
						return exp->type = type->as.struct_t.elems[i].type;
					}
				}
				log_error_and_die(p->lexer.filename, exp->tok,
								  "KCType error. `%s` has no member `"SV_FMT"`.",
								  ast_type_to_str(infered),
								  SV_ARGS(sv_mem));
			}
			if (exp_is_integer_literal(exp->as.bin.right)) {
				int64_t i = exp->as.bin.right->as.lit.as.i;
				if (i < 0 || i >= type->as.struct_t.len) {
					FAILWITH("TODO: Invalid struct member accessor `%ld` for type %s.",
							 i, ast_type_to_str(infered));
				}
				return exp->type = type->as.struct_t.elems[i].type;
			}
			FAILWITH("Unreachable");
		} break;
		case binop_and_assign: FAILWITH("TODO: binop_and_assign"); break;
		case binop_lor_assign: FAILWITH("TODO: binop_lor_assign"); break;
		case binop_xor_assign: FAILWITH("TODO: binop_xor_assign"); break;
		case binop_mod_assign: FAILWITH("TODO: binop_mod_assign"); break;
		case binop_shift_left_assign: FAILWITH("TODO: binop_shift_left_assign"); break;
		case binop_shift_right_assign: FAILWITH("TODO: binop_shift_right_assign"); break;
		default: FAILWITH("Unreachable"); break;
		}
		FAILWITH("TODO: infer_type (ast_exp_binary)");
	} break;
	case ast_exp_value_cons: {
		struct symtbl_entry *entry = lookup_entry(ctx.scope, token_to_strview(exp->as.valcons.cons));
		if (entry == NULL) ERROR_UNDEFINED_IDENT(exp->as.valcons.cons);
		assert(entry->tag == SYMTBL_VALCONS);
		int64_t tag_val = entry->as.valcons.elems[0].tag_val;
		exp->as.valcons.tag_val = tag_val;
		KCType *U = get_fresh_valcons_entry_type(&ctx, &entry->as.valcons.elems[0]);
		KCType *I = type_application(p, &U->as.app, ctx.scope);
		KCType *T = I->as.union_t.elems[tag_val].type;
		if (exp->as.valcons.exp == NULL) {
			if (!type_is_void(T))
				FAILWITH("TODO: union cons type mismatch.");
		} else {
			UNIFY_EXP(ctx, exp->as.valcons.exp, T);
		}
		free(I);
		return exp->type = U;
	} break;
	case ast_exp_unary: {
		switch ((enum unaop)exp->as.una.op) {
		case unaop_not: {
			UNIFY_EXP(ctx, exp->as.una.exp, &AST_TYPE_BOOL);
			return exp->type = &AST_TYPE_BOOL;
		} break;
		case unaop_lnot: FAILWITH("TODO: ast_exp_unary"); break;
		case unaop_neg:
		case unaop_pos: {
			KCType *type = fresh_type_var(type_class_numeric);
			UNIFY_EXP(ctx, exp->as.una.exp, type);
			return exp->type = type;
		} break;
		case unaop_address_of: {
			KCType *type = fresh_type_var(type_class_any);
			UNIFY_EXP(ctx, exp->as.una.exp, type);
			if (!exp->as.una.exp->is_lvalue)
				log_error_and_die(p->lexer.filename, exp->tok, "Error. Expression is not an lvalue.");
			KCType *ptr = MEM_ALLOC(KCType);
			ptr->tag = exp->as.una.exp->is_mutable ? ast_type_mut_ptr : ast_type_ptr;
			ptr->as.ptr = type;
			return exp->type = ptr;
		} break;
		case unaop_dereference: {
			KCType *type = MEM_ALLOC(KCType);
			type->tag = ast_type_ptr;
			type->as.ptr = fresh_type_var(type_class_any);
			UNIFY_EXP(ctx, exp->as.una.exp, type);
			exp->is_lvalue = true;
			return exp->type = type->as.ptr;
		} break;
		case unaop_index: {
			KCType *arr_type = fresh_type_var(type_class_indexable);
			KCType *idx_type = fresh_type_var(type_class_integer);
			KCType *type = fresh_type_var(type_class_any);
			arr_type->as.var.contains = type;
			UNIFY_EXP(ctx, exp->as.idx.exp, arr_type);
			UNIFY_EXP(ctx, exp->as.idx.idx, idx_type);
			exp->is_lvalue = exp->as.idx.exp->is_lvalue;
			return exp->type = type;
		} break;
		case unaop_slice: {
			KCType *arr_type = fresh_type_var(type_class_indexable);
			KCType *idx_type = fresh_type_var(type_class_integer);
			KCType *len_type = fresh_type_var(type_class_integer);
			KCType *base_type = fresh_type_var(type_class_any);
			arr_type->as.var.contains = base_type;
			UNIFY_EXP(ctx, exp->as.slice.exp, arr_type);
			UNIFY_EXP(ctx, exp->as.slice.idx, idx_type);
			UNIFY_EXP(ctx, exp->as.slice.len, len_type);
			if (!exp->as.slice.exp->is_lvalue) {
				log_error_and_die(p->lexer.filename, exp->as.slice.exp->tok,
								  "Error. Expression is not an lvalue.");
			}
			exp->is_lvalue = exp->as.slice.exp->is_lvalue;
			assert(exp->is_lvalue);
			KCType *res_type = MEM_ALLOC(KCType);
			res_type->tag = ast_type_slice;
			res_type->as.slice = base_type;
			return exp->type = res_type;
		} break;
		case unaop_call: {
			struct call *call = &exp->as.call;
			KCType *proc_type = infer_type(p, ctx, call->proc);
			KCType *ret_type = fresh_type_var(type_class_any);
			KCType *infered = MEM_ALLOC(KCType);
			infered->tag = ast_type_proc;
			for (size_t i = 0; i < call->args.len; ++i) {
				KCType *arg = fresh_type_var(type_class_any);
				UNIFY_EXP(ctx, call->args.elems[i], arg);
				da_append(&infered->as.proc.args, arg);
			}
			infered->as.proc.ret = ret_type;
			UNIFY(ctx, proc_type, infered, exp);
			if (call->proc->tag == ast_exp_ident) {
				struct type_env *env = lookup_type_env(&ctx, call->proc->tok);
				assert(env != NULL);
				if (env->scheme.args.len > 0) {
					struct symtbl_entry *entry = lookup_entry(ctx.scope, token_to_strview(call->proc->tok));
					assert(entry != NULL);
					assert(entry->tag == SYMTBL_VARIABL);
					struct definition *def = entry->as.variable.def;
					if (lookup_poly_proc_spec(p, def, proc_type, ctx.scope) == NULL) {
						da_append(&def->specs, (struct type_spec) {
								.type = proc_type,
							});
					}
				}
			}
			return exp->type = ret_type;
		} break;
		case unaop_cast: {
			if (exp_is_integer_literal(exp->as.cast.exp)
				&& type_is_integer(exp->as.cast.type)) {
				KCType *type = exp->as.cast.type;
				*exp = *exp->as.cast.exp;
				return exp->type = type;
			}
			KCType *type = fresh_type_var(type_class_any);
			UNIFY_EXP(ctx, exp->as.cast.exp, type);
			if (!unify_cast(p, ctx, type, exp->as.cast.type))
				type_mismatch_error(p, ctx, type, exp->as.cast.type, exp->tok, __FILE__, __LINE__);
			return exp->type = exp->as.cast.type;
		} break;
		}
		FAILWITH("TODO: ast_exp_unary");
	} break;
	case ast_exp_while: {
		UNIFY_EXP(ctx, exp->as.wloop.cond, &AST_TYPE_BOOL);
		UNIFY_EXP(ctx, exp->as.wloop.body, &AST_TYPE_VOID);
		return exp->type = &AST_TYPE_VOID;
	} break;
	case ast_exp_if: {
		UNIFY_EXP(ctx, exp->as.iff.cond, &AST_TYPE_BOOL);
		KCType *type = fresh_type_var(type_class_any);
		UNIFY_EXP(ctx, exp->as.iff.tb, type);
		if (exp->as.iff.fb != NULL)
			UNIFY_EXP(ctx, exp->as.iff.fb, type);
		return exp->type = type;
	} break;
	case ast_exp_case: {
		KCType *type = fresh_type_var(type_class_any);
		struct case_branches *branches = &exp->as.ccase.branches;
		assert(branches->len > 0);
		struct symtbl_entry *entry = lookup_entry(ctx.scope, token_to_strview(branches->elems[0].cons));
		if (entry == NULL) ERROR_UNDEFINED_IDENT(branches->elems[0].cons);
		assert(entry->tag == SYMTBL_VALCONS);
		struct type_definition *td = entry->as.valcons.elems[0].td;
		KCType *U = get_fresh_valcons_entry_type(&ctx, &entry->as.valcons.elems[0]);
		KCType *I = type_application(p, &U->as.app, ctx.scope);
		UNIFY_EXP(ctx, exp->as.ccase.cexp, U);
		for (size_t i = 0; i < branches->len; ++i) {
			struct case_branch *br = &branches->elems[i];
			struct symtbl_entry *entry = lookup_entry(ctx.scope, token_to_strview(br->cons));
			if (entry == NULL) ERROR_UNDEFINED_IDENT(br->cons);
			assert(entry->tag == SYMTBL_VALCONS);
			if (entry->as.valcons.elems[0].td != td)
				FAILWITH("TODO: KCType error");
			br->tag_val = entry->as.valcons.elems[0].tag_val;
			struct typing_context br_ctx = ctx;
			br_ctx.scope = &br->scope;
			if (br->binds_value) {
				KCType *t = I->as.union_t.elems[br->tag_val].type;
				if (br->binding_is_ref) {
					KCType *tmp = MEM_ALLOC(KCType);
					tmp->tag = ast_type_ptr;
					tmp->as.ptr = t;
					t = tmp;
				}
				br->binding.type = t;
				type_env_add(&br_ctx, br->binding.id, generalize_type(&br_ctx, type_recursive_find(t)));
			}
			if (br->guard)
				FAILWITH("TODO: case guard");
			UNIFY_EXP(br_ctx, br->body, type);
		}
		return exp->type = type;
	} break;
	case ast_exp_return: FAILWITH("TODO: infer_type (ast_exp_return)"); break;
	case ast_exp_break: FAILWITH("TODO: infer_type (ast_exp_break)"); break;
	case ast_exp_continue: FAILWITH("TODO: infer_type (ast_exp_continue)"); break;
	case ast_exp_extern_symbol: FAILWITH("TODO: infer_type (ast_exp_extern_symbol)"); break;
	case ast_exp_get_ptr: {
		KCType *type = MEM_ALLOC(KCType);
		type->tag = ast_type_slice;
		type->as.slice = fresh_type_var(type_class_any);
		UNIFY_EXP(ctx, exp->as.get_ptr, type);
		return exp->type = get_slice_pointer(type);
	} break;
	case ast_exp_get_len: {
		KCType *type = fresh_type_var(type_class_length);
		UNIFY_EXP(ctx, exp->as.get_len, type);
		return exp->type = &AST_TYPE_U64;
	} break;
	case ast_exp_size_of: {
		if (exp->as.size_of.exp) {
			exp->as.size_of.type = infer_type(p, ctx, exp->as.size_of.exp);
		}
		assert(exp->as.size_of.type);
		return exp->type = &AST_TYPE_U64;
	} break;
	default: FAILWITH("Unreachable"); break;
	}
	return exp->type = NULL;
}

KC_PRIVATE bool
specialize(Parser *p, struct typing_context ctx, struct definition *def)
{
	bool succ = false;
	if (def->specs.len == 0) return succ;
	for (size_t i = 0; i < def->specs.len; ++i) {
		struct type_spec *spec = &def->specs.elems[i];
		if (spec->exp == NULL) {
			succ = true;
			specialize_generic_procedure(p, ctx, def, spec);
		}
	}
	return succ;
}

KC_PUBLIC void
type_check(Parser *p, struct typing_context *ctx, struct expression_stack *exps)
{
	for (size_t i = 0; i < exps->len; ++i) {
		if (exps->elems[i]->tag == ast_exp_definition) {
			struct definition *def = &exps->elems[i]->as.def;
			Forall spec = generalize_type(ctx, type_recursive_find(def->type));
			type_env_add(ctx, def->id, spec);
		}
	}
	for (size_t i = 0; i < exps->len; ++i) {
		infer_type(p, *ctx, exps->elems[i]);
	}
	bool loop;
	do {
		loop = false;
		for (size_t i = 0; i < exps->len; ++i) {
			if (exps->elems[i]->tag == ast_exp_definition) {
				loop |= specialize(p, *ctx, &exps->elems[i]->as.def);
			}
		}
	} while (loop);
}

KC_PUBLIC uintptr_t
align_adjust(uintptr_t x, uintptr_t alignment)
{ /* alignment should be a non-zero power of two */
	if (alignment == 1) return x;
	uintptr_t mask = alignment - 1;
	return x & mask ? (x & ~mask) + alignment : x;
}

KC_PUBLIC size_t
type_size(KCType *t)
{
	switch (t->tag) {
	case ast_type_void:
	case ast_type_noreturn:
		return 0;
	case ast_type_bool:
	case ast_type_i8:
	case ast_type_u8:
		return 1;
	case ast_type_i16:
	case ast_type_u16:
		return 2;
	case ast_type_i32:
	case ast_type_u32:
	case ast_type_f32:
		return 4;
	case ast_type_i64:
	case ast_type_u64:
	case ast_type_f64:
	case ast_type_ptr:
	case ast_type_mut_ptr:
	case ast_type_proc:
		return 8;
	case ast_type_slice:
	case ast_type_mut_slice:
		return 16;
	case ast_type_array:
		if (t->as.array.is_sized) {
			return type_size(t->as.array.base) * t->as.array.size;
		} else {
			FAILWITH("TODO: Error: Cannot determine size of type unsized array.");
		}
		break;
	case ast_type_istruct: FAILWITH("TODO: ast_type_istruct"); break;
	case ast_type_struct: {
		struct struct_type *st = &t->as.struct_t;
		size_t size = 0;
		size_t alignment = 1;
		for (size_t i = 0; i < st->len; ++i) {
			size_t ts = type_size(st->elems[i].type);
			size_t a = type_alignment(st->elems[i].type);
			alignment = MAX(alignment, a);
			size = align_adjust(size, a);
			size += ts;
		}
		return align_adjust(size, alignment);
	} break;
	case ast_type_union: {
		struct union_type *ut = &t->as.union_t;
		size_t size = 8;
		size_t alignment = 8;
		for (size_t i = 0; i < ut->len; ++i) {
			size_t ts = type_size(ut->elems[i].type);
			size_t a = type_alignment(ut->elems[i].type);
			ts = align_adjust(8, a) + ts;
			size = MAX(size, ts);
		}
		return align_adjust(size, alignment);
	} break;
	case ast_type_app:
		ast_type_fprint(t, stdout);
		fputc('\n', stdout);
		FAILWITH("TODO: ast_type_app");
		break;
	case ast_type_vector: FAILWITH("TODO: ast_type_vector"); break;
	case ast_type_var: FAILWITH("Unreachable: ast_type_var"); break;
	default: FAILWITH("Unreachable"); break;
	}
}

KC_PUBLIC size_t
type_alignment(KCType *t)
{
	switch (t->tag) {
	case ast_type_void:
	case ast_type_noreturn:
		return 0;
	case ast_type_bool:
	case ast_type_i8:
	case ast_type_u8:
		return 1;
	case ast_type_i16:
	case ast_type_u16:
		return 2;
	case ast_type_i32:
	case ast_type_u32:
	case ast_type_f32:
		return 4;
	case ast_type_i64:
	case ast_type_u64:
	case ast_type_f64:
	case ast_type_ptr:
	case ast_type_mut_ptr:
	case ast_type_proc:
	case ast_type_slice:
	case ast_type_mut_slice:
		return 8;
	case ast_type_array:
		return type_alignment(t->as.array.base);
	case ast_type_istruct: FAILWITH("TODO: ast_type_istruct"); break;
	case ast_type_struct: {
		struct struct_type *st = &t->as.struct_t;
		size_t alignment = 1;
		for (size_t i = 0; i < st->len; ++i) {
			size_t a = type_alignment(st->elems[i].type);
			alignment = MAX(alignment, a);
		}
		return alignment;
	} break;
	case ast_type_union: return 8;
	case ast_type_app: FAILWITH("TODO: ast_type_app"); break;
	case ast_type_vector: FAILWITH("TODO: ast_type_vector"); break;
	case ast_type_var: FAILWITH("Unreachable: ast_type_var"); break;
	}
	FAILWITH("Unreachable");
	return 0;
}

KC_PUBLIC size_t
struct_member_offset(struct struct_type *st, size_t index)
{
	assert(index < st->len);
	size_t offset = 0;
	for (size_t i = 0; i < index; ++i) {
		offset = align_adjust(offset, type_alignment(st->elems[i].type));
		offset += type_size(st->elems[i].type);
	}
	return align_adjust(offset, type_alignment(st->elems[index].type));
}

KC_PUBLIC size_t
get_struct_member_idx(KCType *t, struct token *mem)
{
	assert(t->tag == ast_type_struct);
	struct strview sv_mem = token_to_strview(mem);
	for (size_t i = 0; i < t->as.struct_t.len; ++i) {
		assert(t->as.struct_t.elems[i].name != NULL);
		if (sv_is_equal(sv_mem, token_to_strview(t->as.struct_t.elems[i].name)))
			return i;
	}
	FAILWITH("Unreachable struct type `%s` has no member `"SV_FMT"`",
			 ast_type_to_str(t), SV_ARGS(sv_mem));
	return 0;
}
