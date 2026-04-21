#include <dlfcn.h>
#include <unistd.h>
#include "common.h"
#include "da.h"
#define MEM_H_IMPLEMENTATION
#include "mem.h"
#define STRVIEW_IMPLEMENTATION
#include "strview.h"
#include "ast.h"
#include "log.h"
#include "abi.h"
#include "lex.h"
#include "parse.h"

#include "log.c"
#include "lex.c"
#include "parse.c"

struct da_pointers {
	uint32_t len, cap;
	void **elems;
};

struct type_var_bindings {
	uint32_t len, cap;
	struct type_pair {
		struct type *var, *type;
	} *elems;
};

static uintptr_t align_adjust(uintptr_t x, uintptr_t alignment);

/* --- TYPE-CHECKER --- */

struct type *infer_type(Parser *p, struct typing_context ctx, struct expression *exp);
struct type *check_type(Parser *p, struct typing_context ctx, struct expression *exp, struct type *type);
static bool type_equiv(Parser *p, struct type *t, struct type *u, struct scope *scope);
static struct type *type_var_subst(Parser *p, struct type *type, struct type_var_bindings *bindings);
static struct type *copy_type(Parser *p, struct type *type);
struct type *resolve_alias(Parser *p, struct type *t, struct scope *scope);

static struct type *
type_application(Parser *p, struct type_app *app, struct scope *scope)
{
	struct type_definition *def = lookup_type(scope, token_to_strview(app->cons));
	if (def == NULL) ERROR_UNDEFINED_IDENT(app->cons);
	struct type_var_bindings bindings = {0};
	struct type *cons = def->type;
	struct type_ptrs *formals = &def->args;
	struct type_ptrs *actuals = &app->args;
	assert(formals->len == actuals->len);
	if (actuals->len == 0) return copy_type(p, cons);
	for (size_t i = 0; i < formals->len; ++i) {
		assert(formals->elems[i]->tag == ast_type_var);
		da_append(&bindings, (struct type_pair) {
				.var  = formals->elems[i],
				.type = actuals->elems[i],
			});
	}
	struct type *newtype = type_var_subst(p, cons, &bindings);
	da_free(&bindings);
	return newtype;
}

static struct type *
type_get_underlying(struct scope *scope, struct type *type, struct type **out_type)
{
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

static void
fprint_full_type_name(Parser *p, struct typing_context ctx, struct type *t, FILE *f)
{
	struct type *u = resolve_alias(p, t, ctx.scope);
	fputs("`", f);
	ast_type_fprint(t, f);
	if (t != u) {
		fputs("` aka. `", f);
		ast_type_fprint(u, f);
	}
	fputs("`", f);
}

void type_mismatch_error(Parser *p, struct typing_context ctx, struct type *t, struct type *u,
						 struct token *tok, char *debug_file, int debug_line)
{
	fflush(stdout);
	{ /* check for undefined identifiers in signatures */
		struct type *chk = NULL;
		if (!type_get_underlying(ctx.scope, t, &chk)) ERROR_UNDEFINED_IDENT(chk->as.app.cons);
		if (!type_get_underlying(ctx.scope, u, &chk)) ERROR_UNDEFINED_IDENT(chk->as.app.cons);
	}
	struct strview msg = {0};
	FILE *stream = open_memstream(&msg.ptr, &msg.len);
	fputs("Type error. ", stream);
	fprint_full_type_name(p, ctx, t, stream);
	fputs(" is incompatible with ", stream);
	fprint_full_type_name(p, ctx, u, stream);
	fputs(".", stream);
	fclose(stream);
	log_error_impl(p->lexer.filename, tok, debug_file, debug_line, "%s", msg.ptr);
	EXIT(1);
}

struct type *resolve_type(Parser *p, struct type *type, struct scope *scope)
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
		case type_class_any:	   FAILWITH("Unreachable type_class_any");       break;
		case type_class_scalar:	   FAILWITH("Unreachable type_class_scalar");    break;
		case type_class_numeric:   FAILWITH("Unreachable type_class_numeric");   break;
		case type_class_length:	   FAILWITH("Unreachable type_class_length");    break;
		case type_class_indexable: FAILWITH("Unreachable type_class_indexable"); break;
		case type_class_struct:	   FAILWITH("Unreachable type_class_struct");    break;
		case type_class_union:	   FAILWITH("Unreachable type_class_union");     break;
		case type_class_procedure: FAILWITH("Unreachable type_class_procedure"); break;
		case type_class_pointer:   FAILWITH("Unreachable type_class_pointer");   break;
		default: FAILWITH("Unreachable"); break;
		}
		break;
	default: FAILWITH("Unreachable"); break;
	}
	return NULL;
}

bool type_is_var(struct type *t)
{
	return t->tag == ast_type_var;
}

bool type_is_integer(struct type *t)
{
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

bool type_is_signed_integer(struct type *t)
{
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

bool type_is_unsigned_integer(struct type *t)
{
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

bool type_is_floating_point(struct type *t)
{
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

bool type_is_numeric_scalar(struct type *t)
{
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

bool type_is_signed_scalar(struct type *t)
{
	return type_is_signed_integer(t) || type_is_floating_point(t);
}

bool type_is_bool(struct type *t)
{
	return t->tag == ast_type_bool;
}

bool type_is_void(struct type *t)
{
	return t->tag == ast_type_void;
}

bool type_is_procedure(struct type *t)
{
	return t->tag == ast_type_proc;
}

bool type_is_pointer(struct type *t)
{
	return t->tag == ast_type_ptr
		|| t->tag == ast_type_mut_ptr
		|| t->tag == ast_type_proc;
}

bool type_is_struct(struct type *t, struct scope *scope)
{
	if (t->tag == ast_type_app)
		return type_is_struct(type_get_underlying(scope, t, NULL), scope);
	return t->tag == ast_type_struct;
}

bool type_is_struct_ptr(struct type *t, struct scope *scope)
{
	if (t->tag == ast_type_app)
		return type_is_struct_ptr(type_get_underlying(scope, t, NULL), scope);
	return type_is_pointer(t) && type_is_struct(t->as.ptr, scope);
}

bool type_is_union(struct type *t, struct scope *scope)
{
	if (t->tag == ast_type_app)
		return type_is_union(type_get_underlying(scope, t, NULL), scope);
	return t->tag == ast_type_union;
}

bool type_is_union_ptr(struct type *t, struct scope *scope)
{
	if (t->tag == ast_type_app)
		return type_is_union_ptr(type_get_underlying(scope, t, NULL), scope);
	return type_is_pointer(t) && type_is_union(t->as.ptr, scope);
}

bool type_is_array(struct type *t)
{
	return t->tag == ast_type_array;
}

bool type_is_array_ptr(struct type *t)
{
	return type_is_pointer(t)
		&& type_is_array(t->as.ptr);
}

bool type_is_slice(struct type *t)
{
	return t->tag == ast_type_slice
		|| t->tag == ast_type_mut_slice;
}

bool type_is_slice_ptr(struct type *t)
{
	return type_is_pointer(t)
		&& type_is_slice(t->as.ptr);
}

bool type_is_indexable(struct type *t)
{
	return type_is_array(t) || type_is_array_ptr(t) || type_is_slice(t);
}

static struct type *
get_slice_pointer(Parser *p, struct type *t)
{
	struct type *arr = POOL_ALLOC(&p->data, struct type);
	arr->tag = ast_type_array;
	struct type *ptr = POOL_ALLOC(&p->data, struct type);
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
		ast_type_fprint(t, stdout);
		printf("\n");
		FAILWITH("Type is not a slice");
		break;
	}
	ptr->as.ptr = arr;
	return ptr;
}

static struct type *
get_indexable_base_type(struct type *t)
{
	assert(type_is_indexable(t));
	if (type_is_array(t))     return t->as.array.base;
	if (type_is_array_ptr(t)) return t->as.ptr->as.array.base;
	if (type_is_slice(t))     return t->as.slice;
	return NULL;
}

bool type_has_length(struct type *t, struct scope *scope)
{
	if (t->tag == ast_type_app)
		return type_has_length(type_get_underlying(scope, t, NULL), scope);
	if (type_is_array_ptr(t)) return type_has_length(t->as.ptr, scope);
	if (type_is_array(t))     return t->as.array.is_sized;
	return type_is_slice(t);
}

bool type_is_scalar(struct type *t)
{
	return type_is_numeric_scalar(t) || type_is_pointer(t) || type_is_bool(t);
}

static bool type_contains_var(struct type *t)
{
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
	case ast_type_struct:
		for (size_t i = 0; i < t->as.struct_t.len; ++i) {
			if (type_contains_var(t->as.struct_t.elems[i].type)) return true;
		}
		return false;
	case ast_type_union: FAILWITH("TODO: ast_type_union"); break;
	case ast_type_proc:
		for (size_t i = 0; i < t->as.proc.args.len; ++i) {
			if (type_contains_var(t->as.proc.args.elems[i])) return true;
		}
		if (type_contains_var(t->as.proc.ret)) return true;
		return false;
	case ast_type_var:
		return true;
	case ast_type_vector: FAILWITH("TODO: ast_type_vector"); break;
	default: FAILWITH("Unreachable"); break;
	}
	return false;
}

bool type_is_polymorphic(struct type *t)
{
	return type_is_procedure(t) && type_contains_var(t);
}

struct struct_type *
struct_type_members(Parser *p, struct type *type, struct scope *scope)
{
	if (type->tag == ast_type_app)
		return struct_type_members(p, type_application(p, &type->as.app, scope), scope);
	if (type->tag == ast_type_struct)
		return &type->as.struct_t;
	if (type->tag == ast_type_ptr && type->as.ptr->tag == ast_type_struct)
		return &type->as.ptr->as.struct_t;
	if (type->tag == ast_type_mut_ptr && type->as.ptr->tag == ast_type_struct)
		return &type->as.mut_ptr->as.struct_t;
	FAILWITH("Type has no struct members");
	return NULL;
}

struct type *type_slice_to_array_ptr(struct mem_arena *pool, struct type *t)
{
	struct type *ptr = POOL_ALLOC(pool, struct type);
	struct type *array = POOL_ALLOC(pool, struct type);
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

struct type *type_array_to_slice(struct mem_arena *pool, struct type *t, bool mutable_p)
{
	struct type *slice = POOL_ALLOC(pool, struct type);
	slice->tag = mutable_p ? ast_type_mut_slice : ast_type_slice;
	slice->as.slice = t->as.array.base;
	return slice;
}

static struct type *
copy_type(Parser *p, struct type *type)
{
	struct type *newtype = NULL;
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
		newtype = POOL_ALLOC(&p->data, struct type);
		newtype->tag = type->tag;
		newtype->as.app.cons = type->as.app.cons;
		for (size_t i = 0; i < type->as.app.args.len; ++i) {
			da_append(&newtype->as.app.args, copy_type(p, type->as.app.args.elems[i]));
		}
		return newtype;
	case ast_type_var:
		newtype = POOL_ALLOC(&p->data, struct type);
		*newtype = *type;
		return newtype;
	case ast_type_ptr:
	case ast_type_mut_ptr:
		newtype = POOL_ALLOC(&p->data, struct type);
		newtype->tag = type->tag;
		newtype->as.ptr = copy_type(p, type->as.ptr);
		return newtype;
	case ast_type_slice:
	case ast_type_mut_slice:
		newtype = POOL_ALLOC(&p->data, struct type);
		newtype->tag = type->tag;
		newtype->as.slice = copy_type(p, type->as.slice);
		return newtype;
	case ast_type_array:
		newtype = POOL_ALLOC(&p->data, struct type);
		*newtype = *type;
		newtype->as.array.base = copy_type(p, type->as.array.base);
		return newtype;
	case ast_type_struct:
		newtype = POOL_ALLOC(&p->data, struct type);
		newtype->tag = type->tag;
		for (size_t i = 0; i < type->as.struct_t.len; ++i) {
			da_append(&newtype->as.struct_t, (struct struct_member) {
					.name = type->as.struct_t.elems[i].name,
					.type = copy_type(p, type->as.struct_t.elems[i].type),
				});
		}
		return newtype;
	case ast_type_union:
		newtype = POOL_ALLOC(&p->data, struct type);
		newtype->tag = type->tag;
		for (size_t i = 0; i < type->as.struct_t.len; ++i) {
			da_append(&newtype->as.union_t, (struct union_member) {
					.name      = type->as.union_t.elems[i].name,
					.tag_value = type->as.union_t.elems[i].tag_value,
					.type      = copy_type(p, type->as.union_t.elems[i].type),
				});
		}
		return newtype;
	case ast_type_proc:
		newtype = POOL_ALLOC(&p->data, struct type);
		newtype->tag = type->tag;
		for (size_t i = 0; i < type->as.proc.args.len; ++i) {
			da_append(&newtype->as.proc.args, copy_type(p, type->as.proc.args.elems[i]));
		}
		newtype->as.proc.ret = copy_type(p, type->as.proc.ret);
		return newtype;
	case ast_type_vector: FAILWITH("TODO: ast_type_vector"); break;
	default: FAILWITH("TODO: Unreachable"); break;
	}
	return NULL;
}

struct definition *
lookup_poly_proc_instance(Parser *p, struct definition *def, struct type *t, struct scope *scope)
{
	assert(def->is_polymorphic);
	for (size_t i = 0; i < def->specs.len; ++i) {
		if (type_equiv(p, def->specs.elems[i].type, t, scope)) return &def->specs.elems[i];
	}
	return NULL;
}

struct type *
resolve_alias(Parser *p, struct type *t, struct scope *scope)
{
	if (t->tag == ast_type_app) {
		struct type_definition *def = lookup_type(scope, token_to_strview(t->as.app.cons));
		if (def == NULL) ERROR_UNDEFINED_IDENT(t->as.app.cons);
		if (def->is_alias) return type_application(p, &t->as.app, scope);
	}
	return t;
}

static bool type_equiv(Parser *p, struct type *t, struct type *u, struct scope *scope)
{
	if (t == u) return true;
	t = resolve_alias(p, t, scope);
	u = resolve_alias(p, u, scope);
	switch (t->tag) {
	case ast_type_noreturn: FAILWITH("TODO: ast_type_noreturn"); break;
	case ast_type_var: return t->tag == u->tag;
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

#define TYPE_MISMATCH(t, u) type_mismatch_error(p, ctx, t, u, exp->tok, __FILE__, __LINE__)
#define UNIFY_TYPES(t, u)												\
	do {																\
		struct type *_T = (t);											\
		struct type *_U = (u);											\
		if (!unify(p, ctx, _T, _U))										\
			TYPE_MISMATCH(_T, _U);										\
	} while (0)

static void
add_type_var_binding(struct type_var_bindings *bindings, struct type *var, struct type *type)
{
	da_append(bindings, (struct type_pair){var, type});
}

static struct type *
find_type_var_binding(struct type_var_bindings *bindings, struct type *var)
{
	for (size_t i = 0; i < bindings->len; ++i) {
		if (bindings->elems[i].var == var) return bindings->elems[i].type;
	}
	return NULL;
}

static void
bind_polymorphic_type_vars(Parser *p, struct scope *scope, struct type_var_bindings *bindings,
						   struct type *poly, struct type *mono)
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
		assert(poly->tag == mono->tag);
		return;
	case ast_type_app:
		assert(mono->tag == ast_type_app);
		assert(poly->as.app.args.len == mono->as.app.args.len);
		//assert(type_equiv(poly->as.app.cons, mono->as.app.cons));
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
		struct type *b = find_type_var_binding(bindings, poly);
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

static struct type *
type_var_subst(Parser *p, struct type *type, struct type_var_bindings *bindings)
{
	if (type == NULL) return NULL;
	struct type *newtype = NULL;
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
		newtype = POOL_ALLOC(&p->data, struct type);
		newtype->tag = ast_type_app;
		newtype->as.app.cons = type->as.app.cons;
		for (size_t i = 0; i < type->as.app.args.len; ++i) {
			da_append(&newtype->as.app.args,
					  type_var_subst(p, type->as.app.args.elems[i], bindings));
		}
		return newtype;
	case ast_type_var:
		if ((newtype = find_type_var_binding(bindings, type)) == NULL) {
			newtype = POOL_ALLOC(&p->data, struct type);
			*newtype = *type;
		}
		return newtype;
	case ast_type_ptr:
	case ast_type_mut_ptr:
		newtype = POOL_ALLOC(&p->data, struct type);
		newtype->tag = type->tag;
		newtype->as.ptr = type_var_subst(p, type->as.ptr, bindings);
		return newtype;
	case ast_type_slice:
	case ast_type_mut_slice:
		newtype = POOL_ALLOC(&p->data, struct type);
		newtype->tag = type->tag;
		newtype->as.slice = type_var_subst(p, type->as.slice, bindings);
		return newtype;
	case ast_type_array:
		newtype = POOL_ALLOC(&p->data, struct type);
		newtype->tag = type->tag;
		newtype->as.array.base = type_var_subst(p, type->as.array.base, bindings);
		return newtype;
	case ast_type_struct:
		newtype = POOL_ALLOC(&p->data, struct type);
		newtype->tag = type->tag;
		for (size_t i = 0; i < type->as.struct_t.len; ++i) {
			da_append(&newtype->as.struct_t, (struct struct_member) {
					.name = type->as.struct_t.elems[i].name,
					.type = type_var_subst(p, type->as.struct_t.elems[i].type, bindings),
				});
		}
		return newtype;
	case ast_type_union:
		newtype = POOL_ALLOC(&p->data, struct type);
		newtype->tag = type->tag;
		for (size_t i = 0; i < type->as.union_t.len; ++i) {
			da_append(&newtype->as.union_t, (struct union_member) {
					.name      = type->as.union_t.elems[i].name,
					.tag_value = type->as.union_t.elems[i].tag_value,
					.type      = type_var_subst(p, type->as.union_t.elems[i].type, bindings),
				});
		}
		return newtype;
	case ast_type_proc:
		newtype = POOL_ALLOC(&p->data, struct type);
		newtype->tag = type->tag;
		for (size_t i = 0; i < type->as.proc.args.len; ++i) {
			da_append(&newtype->as.proc.args, type_var_subst(p, type->as.proc.args.elems[i], bindings));
		}
		newtype->as.proc.ret = type_var_subst(p, type->as.proc.ret, bindings);
		return newtype;
	case ast_type_vector: FAILWITH("TODO: ast_type_vector"); break;
	default: FAILWITH("TODO: Unreachable"); break;
	}
	return NULL;
}

void instantiate_generic_procedure(Parser *p, struct scope *scope, struct definition *def, struct type *mono);
static struct definition
instantiate_definition(Parser *p, struct definition *def, struct scope *scope, struct type_var_bindings *bindings);
static struct expression *
instantiate_expression(Parser *p, struct expression *exp, struct scope *scope, struct type_var_bindings *bindings);

static struct definition
instantiate_definition(Parser *p, struct definition *def, struct scope *scope, struct type_var_bindings *bindings)
{
	return (struct definition) {
		.id			= def->id,
		.type		= type_var_subst(p, def->type, bindings),
		.exp		= def->exp ? instantiate_expression(p, def->exp, scope, bindings) : NULL,
		.is_mut		= def->is_mut,
		.is_global	= def->is_global,
	};
}

static struct expression *
instantiate_expression(Parser *p, struct expression *exp, struct scope *scope, struct type_var_bindings *bindings)
{
	struct expression *newexp = POOL_ALLOC(&p->data, struct expression);
	newexp->tag = exp->tag;
	newexp->tok = exp->tok;
	newexp->is_lvalue = exp->is_lvalue;
	newexp->is_mutable = exp->is_mutable;
	newexp->type = type_var_subst(p, exp->type, bindings);
	switch (exp->tag) {
	case ast_exp_definition: FAILWITH("TODO: ast_exp_definition"); break;
	case ast_exp_let:
		newexp->as.let.def = instantiate_definition(p, &exp->as.let.def, scope, bindings);
		newexp->as.let.scope.parent = scope;
		symtbl_add(&newexp->as.let.scope.symtbl, &newexp->as.let.def, NULL);
		newexp->as.let.body = instantiate_expression(p, exp->as.let.body, &newexp->as.let.scope, bindings);
		break;
	case ast_exp_literal: newexp->as.lit = exp->as.lit; break;
	case ast_exp_string: /* do nothing */ break;
	case ast_exp_array_initializer: FAILWITH("TODO: ast_exp_array_initializer"); break;
	case ast_exp_struct_initializer: FAILWITH("TODO: ast_exp_struct_initializer"); break;
	case ast_exp_named_struct_initializer: FAILWITH("TODO: ast_exp_named_struct_initializer"); break;
	case ast_exp_zero_struct_initializer: FAILWITH("TODO: ast_exp_zero_struct_initializer"); break;
	case ast_exp_procedure_literal: FAILWITH("TODO: ast_exp_procedure_literal"); break;
	case ast_exp_undefined: FAILWITH("TODO: ast_exp_undefined"); break;
	case ast_exp_ident: break;
	case ast_exp_binary:
		newexp->as.bin.op = exp->as.bin.op;
		newexp->as.bin.left = instantiate_expression(p, exp->as.bin.left, scope, bindings);
		newexp->as.bin.right = instantiate_expression(p, exp->as.bin.right, scope, bindings);
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
			newexp->as.una.exp = instantiate_expression(p, exp->as.una.exp, scope, bindings);
			break;
		case unaop_index:
			newexp->as.idx.exp = instantiate_expression(p, exp->as.idx.exp, scope, bindings);
			newexp->as.idx.idx = instantiate_expression(p, exp->as.idx.idx, scope, bindings);
			break;
		case unaop_call: {
			newexp->as.call.proc = instantiate_expression(p, exp->as.call.proc, scope, bindings);
			if (type_is_polymorphic(exp->as.call.proc->type)) {
				newexp->as.call.proc = instantiate_expression(p, exp->as.call.proc, scope, bindings);
				struct call *call = &newexp->as.call;
				if (call->proc->tag != ast_exp_ident) {
					FAILWITH("TODO: polymorphic procedure must be let-bound.");
				}
				struct definition *generic = NULL;
				struct symtbl_entry *entry = lookup_entry(scope, token_to_strview(call->proc->tok));
				assert(entry != NULL);
				if (entry == NULL) ERROR_UNDEFINED_IDENT(call->proc->tok);
				if (entry->tag == SYMTBL_VAR) {
					generic = entry->as.variable.def;
				} else {
					FAILWITH("TODO: expected procedure.");
				}
				assert(generic->is_polymorphic);
				struct type *inf_type = POOL_ALLOC(&p->data, struct type);
				inf_type->tag = ast_type_proc;
				struct type *ret_type = newexp->type;
				inf_type->as.proc.ret = ret_type;
				for (size_t i = 0; i < exp->as.call.args.len; ++i) {
					struct expression *arg_exp = exp->as.call.args.elems[i];
					arg_exp = instantiate_expression(p, arg_exp, scope, bindings);
					assert(arg_exp->type);
					da_append(&newexp->as.call.args, arg_exp);
					da_append(&inf_type->as.proc.args, arg_exp->type);
				}
				if (!type_is_polymorphic(inf_type)) {
					instantiate_generic_procedure(p, scope, generic, inf_type);
				}
				call->proc->type = inf_type;
			} else {
				for (size_t i = 0; i < exp->as.call.args.len; ++i) {
					struct expression *arg_exp = exp->as.call.args.elems[i];
					da_append(&newexp->as.call.args, instantiate_expression(p, arg_exp, scope, bindings));
				}
			}
		} break;
		case unaop_cast:
			newexp->as.cast.type = type_var_subst(p, exp->as.cast.type, bindings);
			newexp->as.cast.exp = instantiate_expression(p, exp->as.cast.exp, scope, bindings);
			break;
		case unaop_slice:
			newexp->as.slice.exp = instantiate_expression(p, exp->as.slice.exp, scope, bindings);
			newexp->as.slice.idx = instantiate_expression(p, exp->as.slice.idx, scope, bindings);
			newexp->as.slice.len = instantiate_expression(p, exp->as.slice.len, scope, bindings);
			break;
		default: FAILWITH("Unreachable"); break;
		}
		break;
	case ast_exp_while:
		newexp->as.wloop.cond = instantiate_expression(p, exp->as.wloop.cond, scope, bindings);
		newexp->as.wloop.body = instantiate_expression(p, exp->as.wloop.body, scope, bindings);
		break;
	case ast_exp_if:
		newexp->as.iff.cond = instantiate_expression(p, exp->as.iff.cond, scope, bindings);
		newexp->as.iff.tb   = instantiate_expression(p, exp->as.iff.tb, scope, bindings);
		if (exp->as.iff.fb)
			newexp->as.iff.fb = instantiate_expression(p, exp->as.iff.fb, scope, bindings);
		break;
	case ast_exp_case: FAILWITH("TODO: ast_exp_case"); break;
	case ast_exp_return: FAILWITH("TODO: ast_exp_return"); break;
	case ast_exp_break: FAILWITH("TODO: ast_exp_break"); break;
	case ast_exp_continue: FAILWITH("TODO: ast_exp_continue"); break;
	case ast_exp_extern_symbol: FAILWITH("TODO: ast_exp_extern_symbol"); break;
	case ast_exp_get_ptr:
		newexp->as.get_ptr = instantiate_expression(p, exp->as.get_ptr, scope, bindings);
		break;
	case ast_exp_get_len:
		newexp->as.get_len = instantiate_expression(p, exp->as.get_len, scope, bindings);
		break;
	default: FAILWITH("Unreachable"); break;
	}
	return newexp;
}

void instantiate_generic_procedure(Parser *p, struct scope *scope, struct definition *def, struct type *mono)
{
	struct expression *proc = def->exp;
	assert(proc->tag == ast_exp_procedure_literal);
	assert(def->is_polymorphic);
	assert(!type_is_polymorphic(mono));
	struct definition *spec_def = lookup_poly_proc_instance(p, def, mono, scope);
	if (spec_def != NULL) return;
	struct expression *newproc = POOL_ALLOC(&p->data, struct expression);
	newproc->tag = proc->tag;
	newproc->tok = proc->tok;
	newproc->is_lvalue = proc->is_lvalue;
	newproc->is_mutable = proc->is_mutable;
	newproc->type = mono;
	struct type_var_bindings bindings = {0};
	bind_polymorphic_type_vars(p, scope, &bindings, def->type, mono);
	struct def_array formals = {0};
	newproc->as.proc.scope.parent = proc->as.proc.scope.parent;
	for (size_t i = 0; i < proc->as.proc.formals.len; ++i) {
		struct definition *def = da_allot(&formals);
		*def = proc->as.proc.formals.elems[i];
		def->type = mono->as.proc.args.elems[i];
		symtbl_add(&newproc->as.proc.scope.symtbl, def, NULL);
	}
	/* Order here is important. The new definition must be added before calling
	 * `instantiate_expression`. Otherwise infinite recursion may occur.
	 */
	da_append(&def->specs, (struct definition) {
			.exp			= newproc,
			.id				= def->id,
			.is_global		= def->is_global,
			.is_mut			= def->is_mut,
			.type			= newproc->type,
			.is_polymorphic = false,
		});
	newproc->as.proc.formals = formals;
	newproc->as.proc.ret     = type_var_subst(p, proc->as.proc.ret, &bindings);
	newproc->as.proc.body    = instantiate_expression(p, proc->as.proc.body, &newproc->as.proc.scope, &bindings);
	da_free(&bindings);
}

static bool unify(Parser *p, struct typing_context ctx, struct type *t, struct type *u)
{
	t = resolve_alias(p, t, ctx.scope);
	u = resolve_alias(p, u, ctx.scope);
	switch (t->tag) {
	case ast_type_void: {
		if (type_is_var(u)) *u = *t;
		return true;
	} break;
	case ast_type_noreturn: FAILWITH("TODO: ast_type_noreturn"); break;
	case ast_type_bool: {
		if (t->tag == u->tag) return true;
		if (type_is_var(u) && u->as.var.class == type_class_any) {
			*u = *t;
			return true;
		}
		return false;
	} break;
	case ast_type_i8:
	case ast_type_i16:
	case ast_type_i32:
	case ast_type_i64: {
		if (t->tag == u->tag) return true;
		if (type_is_var(u)) {
			switch (u->as.var.class) {
			case type_class_any:
			case type_class_scalar:
			case type_class_numeric:
			case type_class_signed_integer:
			case type_class_integer:
				*u = *t;
				return true;
			case type_class_unsigned_integer:
			case type_class_float:
			case type_class_length:
			case type_class_indexable:
			case type_class_struct:
			case type_class_union:
			case type_class_procedure:
			case type_class_pointer:
				return false;
			default: FAILWITH("Unreachable");
			}
		}
		return false;
	} break;
	case ast_type_u8:
	case ast_type_u16:
	case ast_type_u32:
	case ast_type_u64: {
		if (t->tag == u->tag) return true;
		if (type_is_var(u)) {
			switch (u->as.var.class) {
			case type_class_any:
			case type_class_scalar:
			case type_class_numeric:
			case type_class_unsigned_integer:
			case type_class_integer:
				*u = *t;
				return true;
			case type_class_signed_integer:
			case type_class_float:
			case type_class_length:
			case type_class_indexable:
			case type_class_struct:
			case type_class_union:
			case type_class_procedure:
			case type_class_pointer:
				return false;
			default: FAILWITH("Unreachable");
			}
		}
		return false;
	} break;
	case ast_type_f32: FAILWITH("TODO: ast_type_f32"); break;
	case ast_type_f64: FAILWITH("TODO: ast_type_f64"); break;
	case ast_type_app: {
		if ((u->tag == ast_type_var
			 || u->tag == ast_type_union
			 || u->tag == ast_type_struct
			 || u->tag == ast_type_array)
			&& unify(p, ctx, type_application(p, &t->as.app, ctx.scope), u)) {
			return true;
		}
		if (u->tag != ast_type_app) return false;
		if (!sv_is_equal(token_to_strview(t->as.app.cons), token_to_strview(u->as.app.cons)))
			return false;
		assert(t->as.app.args.len == u->as.app.args.len);
		for (size_t i = 0; i < t->as.app.args.len; ++i) {
			struct type *tm = t->as.app.args.elems[i];
			struct type *um = u->as.app.args.elems[i];
			if (!unify(p, ctx, tm, um)) {
				return false;
			}
		}
		return true;
	} break;
	case ast_type_ptr: {
		if (type_is_var(u)) {
			switch (u->as.var.class) {
			case type_class_any:
			case type_class_scalar:
			case type_class_pointer:
				*u = *t;
				return true;
			case type_class_numeric:
			case type_class_unsigned_integer:
			case type_class_integer:
			case type_class_signed_integer:
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
		if (type_is_pointer(u)) return unify(p, ctx, t->as.ptr, u->as.ptr);
		return false;
	} break;
	case ast_type_mut_ptr: FAILWITH("TODO: ast_type_mut_ptr"); break;
	case ast_type_slice: {
		if (type_is_slice(u))
			return unify(p, ctx, t->as.slice, u->as.slice);
		if (type_is_array(u) && u->as.array.is_sized)
			return unify(p, ctx, t->as.slice, u->as.array.base);
		if (type_is_var(u)) {
			switch (u->as.var.class) {
			case type_class_any:
			case type_class_indexable:
			case type_class_length:
				*u = *t;
				return true;
			case type_class_scalar:
			case type_class_numeric:
			case type_class_unsigned_integer:
			case type_class_integer:
			case type_class_signed_integer:
			case type_class_float:
			case type_class_struct:
			case type_class_union:
			case type_class_procedure:
			case type_class_pointer:
				return false;
			default: FAILWITH("Unreachable");
			}
		}
		return false;
	} break;
	case ast_type_mut_slice: FAILWITH("TODO: ast_type_mut_slice"); break;
	case ast_type_array: {
		if (type_is_array(u))
			return unify(p, ctx, t->as.array.base, u->as.array.base);
		if (type_is_var(u)) {
			switch (u->as.var.class) {
			case type_class_any:
			case type_class_indexable:
				*u = *t;
				return true;
			case type_class_length:
				if (t->as.array.is_sized) {
					*u = *t;
					return true;
				}
				return false;
			case type_class_scalar:
			case type_class_numeric:
			case type_class_unsigned_integer:
			case type_class_integer:
			case type_class_signed_integer:
			case type_class_float:
			case type_class_struct:
			case type_class_union:
			case type_class_procedure:
			case type_class_pointer:
				return false;
			default: FAILWITH("Unreachable");
			}
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
					struct type *tm = t->as.union_t.elems[i].type;
					struct type *um = u->as.union_t.elems[i].type;
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
		if (u->tag == ast_type_var) {
			switch (u->as.var.class) {
			case type_class_any:
			case type_class_struct:
				*u = *t;
				return true;
			case type_class_scalar:
			case type_class_pointer:
			case type_class_numeric:
			case type_class_integer:
			case type_class_signed_integer:
			case type_class_unsigned_integer:
			case type_class_float:
			case type_class_union:
			case type_class_procedure:
			case type_class_length:
			case type_class_indexable:
				return false;
			default: FAILWITH("Unreachable");
			}
		}
		FAILWITH("TODO: ast_type_struct");
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
			*t = *u;
			return true;
		case type_class_unsigned_integer:
			if (type_is_var(u)) FAILWITH("TODO");
			if (type_is_unsigned_integer(u)) {
				*t = *u;
				return true;
			}
			return false;
		case type_class_signed_integer:
			if (type_is_var(u)) FAILWITH("TODO");
			if (type_is_signed_integer(u)) {
				*t = *u;
				return true;
			}
			return false;
		case type_class_integer:
			if (type_is_var(u)) {
				switch (u->as.var.class) {
				case type_class_any:
				case type_class_scalar:
				case type_class_numeric:
					*u = *t;
					return true;
				case type_class_signed_integer:
				case type_class_unsigned_integer:
					*t = *u;
					return true;
				case type_class_integer:
					return true;
				case type_class_float:
				case type_class_length:
				case type_class_indexable:
				case type_class_struct:
				case type_class_union:
				case type_class_procedure:
				case type_class_pointer:
					return false;
				default: FAILWITH("Unreachable");
				}
			}
			if (type_is_integer(u)) {
				*t = *u;
				return true;
			}
			return false;
		case type_class_float:
			if (type_is_var(u)) FAILWITH("TODO");
			if (type_is_floating_point(u)) {
				*t = *u;
				return true;
			}
			return false;
		case type_class_numeric:
			if (type_is_var(u)) {
				switch (u->as.var.class) {
				case type_class_any:
					*u = *t;
					return true;
				case type_class_scalar:
				case type_class_signed_integer:
				case type_class_unsigned_integer:
				case type_class_integer:
				case type_class_float:
					*t = *u;
					return true;
				case type_class_numeric:
					return true;
				case type_class_length:
				case type_class_indexable:
				case type_class_struct:
				case type_class_union:
				case type_class_procedure:
				case type_class_pointer:
					return false;
				default: FAILWITH("Unreachable");
				}
			} else if (type_is_numeric_scalar(u)) {
				*t = *u;
				return true;
			}
			return false;
		case type_class_scalar: {
			struct type *v = NULL;
			if (type_is_var(u)) {
				switch (u->as.var.class) {
				case type_class_any:
					*u = *t;
					return true;
				case type_class_numeric:
				case type_class_signed_integer:
				case type_class_unsigned_integer:
				case type_class_integer:
				case type_class_float:
				case type_class_pointer:
					*t = *u;
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
				*t = *u;
				return true;
			} else if (u->tag == ast_type_app
					   && type_get_underlying(ctx.scope, u, &v)
					   && type_is_scalar(v)) {
				*t = *u;
				return true;
			}
			return false;
		} break;
		case type_class_length:
			if (type_is_var(u)) FAILWITH("TODO");
			if (type_has_length(u, ctx.scope)) {
				*t = *u;
				return true;
			}
			return false;
		case type_class_indexable: {
			if (type_is_var(u)) FAILWITH("TODO");
			if (type_is_indexable(u)) {
				*t = *u;
				return true;
			} else {
				FAILWITH("TODO");
			}
		} break;
		case type_class_struct: {
			if (type_is_var(u)) FAILWITH("TODO");
			if (type_is_struct(u, ctx.scope) || type_is_struct_ptr(u, ctx.scope)) {
				*t = *u;
				return true;
			} else {
				FAILWITH("TODO");
			}
		} break;
		case type_class_union: {
			if (type_is_var(u)) FAILWITH("TODO");
			if (type_is_union(u, ctx.scope) || type_is_union_ptr(u, ctx.scope)) {
				*t = *u;
				return true;
			} else {
				FAILWITH("TODO");
			}
		} break;
		case type_class_procedure: {
			if (type_is_var(u)) FAILWITH("TODO");
			if (type_is_procedure(u)) {
				*t = *u;
				return true;
			} else {
				FAILWITH("TODO");
			}
		} break;
		case type_class_pointer: {
			if (type_is_var(u)) {
				switch (u->as.var.class) {
				case type_class_any:
				case type_class_scalar:
					*u = *t;
					return true;
				case type_class_pointer:
					return true;
				case type_class_numeric:
				case type_class_integer:
				case type_class_signed_integer:
				case type_class_unsigned_integer:
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
			if (type_is_pointer(u)) {
				*t = *u;
				return true;
			}
			ast_type_fprint(u, stdout);
			printf("\n");
			FAILWITH("Unreachable");
		} break;
		default: FAILWITH("Unreachable"); break;
		}
	} break;
	default: FAILWITH("Unreachable"); break;
	}
	return false;
}

static struct type *
find_union_member_type(struct union_type *ut, struct token *cons_name)
{
	struct strview name = token_to_strview(cons_name);
	for (size_t i = 0; i < ut->len; ++i) {
		if (sv_is_equal(name, token_to_strview(ut->elems[i].name)))
			return ut->elems[i].type;
	}
	return NULL;
}

struct type *
infer_type(Parser *p, struct typing_context ctx, struct expression *exp)
{
	switch (exp->tag) {
	case ast_exp_definition: {
		assert(exp->as.def.type != NULL);
		exp->type = &AST_TYPE_VOID;
		exp->as.def.is_polymorphic = type_is_polymorphic(exp->as.def.type);
		struct type *type = exp->as.def.exp->tag == ast_exp_extern_symbol
			? exp->as.def.exp->type = exp->as.def.type
			: check_type(p, ctx, exp->as.def.exp, exp->as.def.type);
		exp->as.def.is_typechecked = true;
		return type;
	} break;
	case ast_exp_let: {
		struct definition *def = &exp->as.let.def;
		check_type(p, ctx, def->exp, def->type);
		def->exp->type = def->type;
		ctx.scope = &exp->as.let.scope;
		struct type *type = POOL_ALLOC(&p->data, struct type);
		type->tag = ast_type_var;
		type->as.var.class = type_class_any;
		type = check_type(p, ctx, exp->as.let.body, type);
		exp->as.let.def.is_typechecked = true;
		return type;
	} break;
	case ast_exp_literal: {
		struct literal *lit = &exp->as.lit;
		struct type *type = NULL;
		if (lit->token->tt == tt_intlit || lit->token->tt == tt_hexlit) {
			type = POOL_ALLOC(&p->data, struct type);
			type->tag = ast_type_var;
			type->as.var.class = type_class_integer;
		} else if (lit->token->tt == tt_floatlit) {
			type = POOL_ALLOC(&p->data, struct type);
			type->tag = ast_type_var;
			type->as.var.class = type_class_float;
		} else if (lit->token->tt == tt_true || lit->token->tt == tt_false) {
			type = &AST_TYPE_BOOL;
		} else {
			FAILWITH("TODO: ast_exp_literal");
		}
		return type;
	} break;
	case ast_exp_string: {
		return &AST_TYPE_STRING;
	} break;
	case ast_exp_array_initializer: {
		struct type *base = POOL_ALLOC(&p->data, struct type);
		base->tag = ast_type_var;
		base->as.var.class = type_class_any;
		struct type *type = POOL_ALLOC(&p->data, struct type);
		type->tag = ast_type_array;
		for (size_t i = 0; i < exp->as.init.len; ++i) {
			base = check_type(p, ctx, exp->as.init.elems[i], base);
		}
		type->as.array.base = base;
		type->as.array.is_sized = true;
		type->as.array.size = exp->as.init.len;
		exp->type = type;
		return type;
	} break;
	case ast_exp_struct_initializer: {
		struct type *type = POOL_ALLOC(&p->data, struct type);
		type->tag = ast_type_struct;
		for (size_t i = 0; i < exp->as.init.len; ++i) {
			struct type *tv = POOL_ALLOC(&p->data, struct type);
			tv->tag = ast_type_var;
			tv->as.var.class = type_class_any;
			da_append(&type->as.struct_t, (struct struct_member) {
					.type = check_type(p, ctx, exp->as.init.elems[i], tv),
				});
		}
		exp->type = type;
		return type;
	} break;
	case ast_exp_named_struct_initializer: {
		struct type *type = POOL_ALLOC(&p->data, struct type);
		type->tag = ast_type_struct;
		for (size_t i = 0; i < exp->as.named_init.ids.len; ++i) {
			struct type *tv = POOL_ALLOC(&p->data, struct type);
			tv->tag = ast_type_var;
			tv->as.var.class = type_class_any;
			da_append(&type->as.struct_t, (struct struct_member) {
					.name = exp->as.named_init.ids.elems[i],
					.type = check_type(p, ctx, exp->as.named_init.exps.elems[i], tv),
				});
		}
		exp->type = type;
		return type;
	} break;
	case ast_exp_zero_struct_initializer: FAILWITH("TODO: infer_type (ast_exp_zero_struct_initializer)"); break;
	case ast_exp_procedure_literal: {
		struct procedure *proc = &exp->as.proc;
		struct type *type = POOL_ALLOC(&p->data, struct type);
		type->tag = ast_type_proc;
		type->as.proc = procedure_type(proc);
		check_type(p, (struct typing_context){.scope = &proc->scope, .ret = type->as.proc.ret},
				   proc->body, type->as.proc.ret);
		return type;
	} break;
	case ast_exp_undefined: FAILWITH("TODO: infer_type (ast_exp_undefined)"); break;
	case ast_exp_ident: {
		struct symtbl_entry *entry = lookup_entry(ctx.scope, token_to_strview(exp->tok));
		if (entry == NULL) ERROR_UNDEFINED_IDENT(exp->tok);
		switch (entry->tag) {
		case SYMTBL_VAR: {
			struct definition *def = entry->as.variable.def;
			exp->is_mutable = def->is_mut;
			exp->is_lvalue = true;
			return exp->type = def->type;
		} break;
		case SYMTBL_VALCONS: {
			/* A valcons expression with no argumnet list */
			struct token *cons = exp->tok;
			exp->tag = ast_exp_value_cons;
			exp->as.valcons.cons = cons;
			exp->as.valcons.exp = NULL;
			exp->as.valcons.tag_val = entry->as.valcons.tag_val;
			return infer_type(p, ctx, exp);
		} break;
		default: FAILWITH("Unreachable");
		}
	} break;
	case ast_exp_binary: {
		switch ((enum binop)exp->as.bin.op) {
		case binop_sequence: {
			struct type *left = POOL_ALLOC(&p->data, struct type);
			left->tag = ast_type_var;
			left->as.var.class = type_class_any;
			check_type(p, ctx, exp->as.bin.left, left);
			struct type *type = POOL_ALLOC(&p->data, struct type);
			type->tag = ast_type_var;
			type->as.var.class = type_class_any;
			return check_type(p, ctx, exp->as.bin.right, type);
		} break;
		case binop_add:
		case binop_sub:
		case binop_mul:
		case binop_div: {
			struct type *con = POOL_ALLOC(&p->data, struct type);
			con->tag = ast_type_var;
			con->as.var.class = type_class_numeric;
			struct type *lhs = check_type(p, ctx, exp->as.bin.left, con);
			struct type *rhs = check_type(p, ctx, exp->as.bin.right, con);
			UNIFY_TYPES(lhs, rhs);
			exp->as.bin.left->type = lhs;
			exp->as.bin.right->type = lhs;
			return lhs;
		} break;
		case binop_mod:
		case binop_xor:
		case binop_land:
		case binop_lor:
		case binop_shift_left:
		case binop_shift_right: {
			struct type *con = POOL_ALLOC(&p->data, struct type);
			con->tag = ast_type_var;
			con->as.var.class = type_class_integer;
			struct type *lhs = check_type(p, ctx, exp->as.bin.left, con);
			struct type *rhs = check_type(p, ctx, exp->as.bin.right, con);
			UNIFY_TYPES(lhs, rhs);
			exp->as.bin.left->type = lhs;
			exp->as.bin.right->type = lhs;
			return lhs;
		} break;
		case binop_equal:
		case binop_less_than:
		case binop_more_than:
		case binop_not_equal:
		case binop_less_equal:
		case binop_more_equal: {
			struct type *con = POOL_ALLOC(&p->data, struct type);
			con->tag = ast_type_var;
			con->as.var.class = type_class_scalar;
			struct type *lhs = check_type(p, ctx, exp->as.bin.left, con);
			struct type *rhs = check_type(p, ctx, exp->as.bin.right, con);
			UNIFY_TYPES(lhs, rhs);
			exp->as.bin.left->type = lhs;
			exp->as.bin.right->type = lhs;
			return &AST_TYPE_BOOL;
		} break;
		case binop_or:
		case binop_and: {
			check_type(p, ctx, exp->as.bin.left, &AST_TYPE_BOOL);
			check_type(p, ctx, exp->as.bin.right, &AST_TYPE_BOOL);
			return &AST_TYPE_BOOL;
		} break;
		case binop_assign: {
			struct type *con = POOL_ALLOC(&p->data, struct type);
			con->tag = ast_type_var;
			con->as.var.class = type_class_any;
			struct type *lhs = check_type(p, ctx, exp->as.bin.left, con);
			struct type *rhs = check_type(p, ctx, exp->as.bin.right, con);
			UNIFY_TYPES(lhs, rhs);
			assert(exp->as.bin.left->is_lvalue);
			assert(exp->as.bin.left->is_mutable);
			exp->as.bin.left->type = lhs;
			exp->as.bin.right->type = lhs;
			return lhs;
		} break;
		case binop_add_assign:
		case binop_sub_assign:
		case binop_mul_assign:
		case binop_div_assign: {
			struct type *con = POOL_ALLOC(&p->data, struct type);
			con->tag = ast_type_var;
			con->as.var.class = type_class_numeric;
			struct type *lhs = check_type(p, ctx, exp->as.bin.left, con);
			struct type *rhs = check_type(p, ctx, exp->as.bin.right, con);
			UNIFY_TYPES(lhs, rhs);
			if (!exp->as.bin.left->is_lvalue)
				log_error_and_die(p->lexer.filename, exp->as.bin.left->tok,
								  "Memory address is unbound in left hand side of assignment (not an lvalue).");
			if (!exp->as.bin.left->is_mutable)
				log_error_and_die(p->lexer.filename, exp->as.bin.left->tok,
								  "Left hand side of assignment is immutable.");
			exp->as.bin.left->type = lhs;
			exp->as.bin.right->type = lhs;
			return lhs;
		} break;
		case binop_member: {
			struct type *con = POOL_ALLOC(&p->data, struct type);
			con->tag = ast_type_var;
			con->as.var.class = type_class_struct;
			struct type *lhs = check_type(p, ctx, exp->as.bin.left, con);
			exp->is_lvalue = exp->as.bin.left->is_lvalue;
			exp->is_mutable = exp->as.bin.left->is_mutable;
			if (type_is_struct_ptr(lhs, ctx.scope)) lhs = lhs->as.ptr;
			if (type_is_struct(lhs, ctx.scope)) {
				struct struct_type *mems = struct_type_members(p, lhs, ctx.scope);
				if (exp->as.bin.right->tag == ast_exp_ident) {
					struct strview name = token_to_strview(exp->as.bin.right->tok);
					for (size_t i = 0; i < mems->len; ++i) {
						if (sv_is_equal(token_to_strview(mems->elems[i].name), name)) {
							exp->as.bin.right->type = mems->elems[i].type;
							return mems->elems[i].type;
						}
					}
					log_error_and_die(p->lexer.filename, exp->as.bin.right->tok,
									  "Struct type `%s` has no member named `"SV_FMT"`",
									  ast_type_to_str(lhs),
									  SV_ARGS(name));
				} else if (exp->as.bin.right->tag == ast_exp_literal
						   && exp->as.bin.right->as.lit.token->tt == tt_intlit) {
					exp->as.bin.right->type = &AST_TYPE_I64;
					int64_t i = exp->as.bin.right->as.lit.as.i;
					if (i < 0 || i >= mems->len) {
						FAILWITH("TODO: `%ld` is not a member of struct type.", i);
					}
					return mems->elems[i].type;
				} else {
					FAILWITH("Unreachable");
				}
			} else {
				FAILWITH("TODO: unable to infer type of expression.");
			}
			FAILWITH("TODO: binop_member");
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
		struct type *type = NULL;
		struct symtbl_entry *entry = lookup_entry(ctx.scope, token_to_strview(exp->as.valcons.cons));
		if (entry == NULL) ERROR_UNDEFINED_IDENT(exp->as.valcons.cons);
		assert(entry->tag == SYMTBL_VALCONS);
		exp->as.valcons.tag_val = entry->as.valcons.tag_val;
		if (exp->as.valcons.exp == NULL) {
			assert(type_is_void(entry->as.valcons.type));
			type = copy_type(p, entry->as.valcons.td->type);
		} else {
			struct type_var_bindings bindings = {0};
			struct type_ptrs actuals = {0};
			struct type_ptrs *formals = &entry->as.valcons.td->args;
			for (size_t i = 0; i < formals->len; ++i) {
				assert(formals->elems[i]->tag == ast_type_var);
				struct type *tv = POOL_ALLOC(&p->data, struct type);
				tv->tag = ast_type_var;
				tv->as.var.class = type_class_any;
				da_append(&actuals, tv);
				da_append(&bindings, (struct type_pair) {
						.var  = formals->elems[i],
						.type = actuals.elems[i],
					});
			}
			type = type_var_subst(p, entry->as.valcons.td->type, &bindings);
			check_type(p, ctx, exp->as.valcons.exp, type_var_subst(p, entry->as.valcons.type, &bindings));
			da_free(&bindings);
			da_free(&actuals);
		}
		return type;
	} break;
	case ast_exp_unary: {
		switch ((enum unaop)exp->as.una.op) {
		case unaop_not: {
			return check_type(p, ctx, exp->as.una.exp, &AST_TYPE_BOOL);
		} break;
		case unaop_lnot: FAILWITH("TODO: ast_exp_unary"); break;
		case unaop_neg:
		case unaop_pos: {
			struct type *type = POOL_ALLOC(&p->data, struct type);
			type->tag = ast_type_var;
			type->as.var.class = type_class_numeric;
			return check_type(p, ctx, exp->as.una.exp, type);
		} break;
		case unaop_address_of: {
			struct type *type = POOL_ALLOC(&p->data, struct type);
			type->tag = ast_type_var;
			type->as.var.class = type_class_any;
			check_type(p, ctx, exp->as.una.exp, type);
			struct type *ptr = POOL_ALLOC(&p->data, struct type);
			assert(exp->as.una.exp->is_lvalue);
			ptr->tag = exp->as.una.exp->is_mutable ? ast_type_mut_ptr : ast_type_ptr;
			ptr->as.ptr = type;
			return ptr;
		} break;
		case unaop_dereference: {
			struct type *ptr_type = POOL_ALLOC(&p->data, struct type);
			ptr_type->tag = ast_type_var;
			ptr_type->as.var.class = type_class_pointer;
			ptr_type = check_type(p, ctx, exp->as.una.exp, ptr_type);
			exp->is_lvalue = true;
			if (ptr_type->tag == ast_type_ptr) {
				exp->is_mutable = false;
				return ptr_type->as.ptr;
			}
			if (ptr_type->tag == ast_type_mut_ptr) {
				exp->is_mutable = true;
				return ptr_type->as.mut_ptr;
			}
			FAILWITH("TODO: type error?");
		} break;
		case unaop_index: {
			struct type *arr_type = POOL_ALLOC(&p->data, struct type);
			arr_type->tag = ast_type_var;
			arr_type->as.var.class = type_class_indexable;
			struct type *idx_type = POOL_ALLOC(&p->data, struct type);
			idx_type->tag = ast_type_var;
			idx_type->as.var.class = type_class_integer;
			check_type(p, ctx, exp->as.idx.exp, arr_type);
			check_type(p, ctx, exp->as.idx.idx, idx_type);
			exp->is_lvalue = exp->as.idx.exp->is_lvalue;
			if (exp->as.idx.exp->is_mutable
				|| exp->as.idx.exp->type->tag == ast_type_mut_ptr
				|| exp->as.idx.exp->type->tag == ast_type_mut_slice) {
				exp->is_mutable = true;
			}
			return get_indexable_base_type(arr_type);
		} break;
		case unaop_slice: {
			struct type *arr_type = POOL_ALLOC(&p->data, struct type);
			arr_type->tag = ast_type_var;
			arr_type->as.var.class = type_class_indexable;
			struct type *idx_type = POOL_ALLOC(&p->data, struct type);
			idx_type->tag = ast_type_var;
			idx_type->as.var.class = type_class_integer;
			struct type *len_type = POOL_ALLOC(&p->data, struct type);
			len_type->tag = ast_type_var;
			len_type->as.var.class = type_class_integer;
			check_type(p, ctx, exp->as.slice.exp, arr_type);
			check_type(p, ctx, exp->as.slice.idx, idx_type);
			check_type(p, ctx, exp->as.slice.len, len_type);
			exp->is_lvalue = exp->as.slice.exp->is_lvalue;
			assert(exp->is_lvalue);
			struct type *res_type = POOL_ALLOC(&p->data, struct type);
			if (exp->as.slice.exp->is_mutable
				|| exp->as.slice.exp->type->tag == ast_type_mut_ptr
				|| exp->as.slice.exp->type->tag == ast_type_mut_slice) {
				exp->is_mutable = true;
				res_type->tag = ast_type_mut_slice;
			} else {
				res_type->tag = ast_type_slice;
			}
			res_type->as.slice = get_indexable_base_type(arr_type);
			return res_type;
		} break;
		case unaop_call: {
			struct call *call = &exp->as.call;
			struct type *proc_type = POOL_ALLOC(&p->data, struct type);
			proc_type->tag = ast_type_var;
			proc_type->as.var.class = type_class_procedure;
			/* A value constructor expression may have been parsed as a function call */
			if (call->proc->tag == ast_exp_ident) {
				struct symtbl_entry *entry = lookup_entry(ctx.scope, token_to_strview(call->proc->tok));
				if (entry == NULL) ERROR_UNDEFINED_IDENT(call->proc->tok);
				switch (entry->tag) {
				case SYMTBL_VAR: /* continue as procedure call */ break;
				case SYMTBL_VALCONS: {
					/* call was actually a value constructor */
					switch (call->args.len) {
					case 0: {
						struct token *cons = call->proc->tok;
						exp->tag = ast_exp_value_cons;
						exp->as.valcons.cons = cons;
						exp->as.valcons.exp = NULL;
						exp->as.valcons.tag_val = entry->as.valcons.tag_val;
						return infer_type(p, ctx, exp);
					} break;
					case 1: {
						struct token *cons = call->proc->tok;
						struct expression *arg = call->args.elems[0];
						da_free(&call->args);
						exp->tag = ast_exp_value_cons;
						exp->as.valcons.cons = cons;
						exp->as.valcons.exp = arg;
						exp->as.valcons.tag_val = entry->as.valcons.tag_val;
						return infer_type(p, ctx, exp);
					} break;
					default:
						FAILWITH("TODO: Invalid number of arguments provided to constructor");
						break;
					}
				} break;
				default: FAILWITH("Unreachable");
				}
			}
			check_type(p, ctx, call->proc, proc_type);
			assert(type_is_procedure(proc_type));
			assert(call->args.len == proc_type->as.proc.args.len);
			if (type_is_polymorphic(proc_type)) {
				if (call->proc->tag != ast_exp_ident) {
					FAILWITH("TODO: polymorphic procedure must be let-bound.");
				}
				struct symtbl_entry *entry = lookup_entry(ctx.scope, token_to_strview(call->proc->tok));
				if (entry == NULL) ERROR_UNDEFINED_IDENT(call->proc->tok);
				if (entry->tag != SYMTBL_VAR) FAILWITH("TODO: unexpected cons");
				struct definition *generic = entry->as.variable.def;
				if (!generic->is_polymorphic) {
					if (generic->is_typechecked)
						FAILWITH("TODO: Procedure `"SV_FMT"` is not polymorphic.",
								 SV_ARGS(token_to_strview(call->proc->tok)));
					if (entry->as.variable.tl_exp == NULL)
						FAILWITH("TODO: Procedure `"SV_FMT"` is not polymorphic.",
								 SV_ARGS(token_to_strview(call->proc->tok)));
					infer_type(p, ctx, entry->as.variable.tl_exp);
					if (!generic->is_polymorphic)
						FAILWITH("TODO: Type error.");
				}
				assert(generic->is_polymorphic);
				struct type *inf_type = POOL_ALLOC(&p->data, struct type);
				inf_type->tag = ast_type_proc;
				struct type *ret_type = POOL_ALLOC(&p->data, struct type);
				ret_type->tag = ast_type_var;
				ret_type->as.var.class = type_class_any;
				inf_type->as.proc.ret = ret_type;
				for (size_t i = 0; i < call->args.len; ++i) {
					da_append(&inf_type->as.proc.args,
							  check_type(p, ctx, call->args.elems[i],
										 copy_type(p, proc_type->as.proc.args.elems[i])));
				}
				UNIFY_TYPES(copy_type(p, proc_type->as.proc.ret), ret_type);
				if (!type_is_polymorphic(inf_type)) {
					instantiate_generic_procedure(p, ctx.scope, generic, inf_type);
				}
				call->proc->type = inf_type;
				return ret_type;
			} else {
				for (size_t i = 0; i < call->args.len; ++i) {
					check_type(p, ctx, call->args.elems[i], proc_type->as.proc.args.elems[i]);
				}
				return proc_type->as.proc.ret;
			}
		} break;
		case unaop_cast: {
			struct type *type = POOL_ALLOC(&p->data, struct type);
			type->tag = ast_type_var;
			type->as.var.class = type_class_any;
			check_type(p, ctx, exp->as.cast.exp, type);
			struct type *v = NULL;
			assert(type_is_var(type) || type_is_scalar(type));
			if (!(type_is_var(type) || type_is_scalar(type)))
				TYPE_MISMATCH(exp->as.cast.type, type);
			if (!(type_is_void(exp->as.cast.type)
				  || type_is_scalar(exp->as.cast.type)
				  || (exp->as.cast.type->tag == ast_type_app
					  && type_get_underlying(ctx.scope, exp->as.cast.type, &v)
					  && type_is_scalar(v))))
				TYPE_MISMATCH(exp->as.cast.type, type);
			return exp->as.cast.type;
		} break;
		}
		FAILWITH("TODO: ast_exp_unary");
	} break;
	case ast_exp_while: {
		check_type(p, ctx, exp->as.wloop.cond, &AST_TYPE_BOOL);
		return check_type(p, ctx, exp->as.wloop.body, &AST_TYPE_VOID);
	} break;
	case ast_exp_if: {
		check_type(p, ctx, exp->as.iff.cond, &AST_TYPE_BOOL);
		struct type *type = POOL_ALLOC(&p->data, struct type);
		type->tag = ast_type_var;
		type->as.var.class = type_class_any;
		check_type(p, ctx, exp->as.iff.tb, type);
		if (exp->as.iff.fb != NULL) check_type(p, ctx, exp->as.iff.fb, exp->as.iff.tb->type);
		return type;
	} break;
	case ast_exp_case: {
		struct type *type = POOL_ALLOC(&p->data, struct type);
		type->tag = ast_type_var;
		type->as.var.class = type_class_union;
		check_type(p, ctx, exp->as.ccase.cexp, type);
		struct type *union_type = type;
		if (type->tag == ast_type_app) union_type = type_application(p, &type->as.app, ctx.scope);
		assert(union_type->tag == ast_type_union);
		struct case_branches *branches = &exp->as.ccase.branches;
		struct type *res_type = POOL_ALLOC(&p->data, struct type);
		res_type->tag = ast_type_var;
		res_type->as.var.class = type_class_any;
		for (size_t i = 0; i < branches->len; ++i) {
			struct case_branch *branch = &branches->elems[i];
			struct typing_context branch_ctx = {
				.ret = ctx.ret,
				.scope = &branch->scope,
			};
			struct symtbl_entry *entry = lookup_entry(ctx.scope, token_to_strview(branch->cons));
			if (entry == NULL) ERROR_UNDEFINED_IDENT(exp->as.valcons.cons);
			assert(entry->tag == SYMTBL_VALCONS);
			branch->tag_val = entry->as.valcons.tag_val;
			if (branch->binds_value) {
				if (type_is_void(entry->as.valcons.type))
					FAILWITH("TODO: Type error.");
				/* NOTE: It seems ok if `bt` is NULL. Just results in a type error.  */
				struct type *bt = find_union_member_type(&union_type->as.union_t, branch->cons);
				if (bt == NULL) {
					log_error_and_die(p->lexer.filename, branch->cons,
									  "Type error. `"SV_FMT"` is not a member of type %s",
									  SV_ARGS(token_to_strview(branch->cons)),
									  ast_type_to_str(union_type));
				}
				if (branch->binding_is_ref) {
					struct type *tmp = POOL_ALLOC(&p->data, struct type);
					if (exp->as.ccase.cexp->is_mutable) {
						tmp->tag = ast_type_mut_ptr;
					} else {
						tmp->tag = ast_type_ptr;
					}
					tmp->as.ptr = bt;
					branch->binding.type = tmp;
				} else {
					branch->binding.type = bt;
				}
			}
			UNIFY_TYPES(union_type, copy_type(p, entry->as.valcons.td->type));
			if (branch->guard != NULL)
				check_type(p, branch_ctx, branch->guard, &AST_TYPE_BOOL);
			check_type(p, branch_ctx, branch->body, res_type);
		}
		if (exp->as.ccase.else_exp != NULL)
			check_type(p, ctx, exp->as.ccase.else_exp, res_type);
		return res_type;
	} break;
	case ast_exp_return: FAILWITH("TODO: infer_type (ast_exp_return)"); break;
	case ast_exp_break: FAILWITH("TODO: infer_type (ast_exp_break)"); break;
	case ast_exp_continue: FAILWITH("TODO: infer_type (ast_exp_continue)"); break;
	case ast_exp_extern_symbol: FAILWITH("TODO: infer_type (ast_exp_extern_symbol)"); break;
	case ast_exp_get_ptr: {
		struct type *type = POOL_ALLOC(&p->data, struct type);
		type->tag = ast_type_var;
		type->as.var.class = type_class_any;
		check_type(p, ctx, exp->as.get_ptr, type);
		return get_slice_pointer(p, type_get_underlying(ctx.scope, exp->as.get_ptr->type, NULL));
	} break;
	case ast_exp_get_len: {
		struct type *type = POOL_ALLOC(&p->data, struct type);
		type->tag = ast_type_var;
		type->as.var.class = type_class_length;
		exp->as.get_len->type = check_type(p, ctx, exp->as.get_len, type);
		return &AST_TYPE_U64;
	} break;
	default: FAILWITH("Unreachable"); break;
	}
	return NULL;
}

struct type *
check_type(Parser *p, struct typing_context ctx, struct expression *exp, struct type *type)
{
	UNIFY_TYPES(type, exp->type = infer_type(p, ctx, exp));
	return exp->type;
}

struct expression *
ast_desugar(Parser *p, struct expression *exp, struct scope *scope)
{
	if (exp->type == NULL) {
		log_error_and_die(p->lexer.filename, exp->tok, "Expression has no type.");
	}
	{
		struct type *t = resolve_type(p, exp->type, scope);
		if (t == NULL)
			FAILWITH("Failed to resolve type: %s\n", ast_type_to_str(exp->type));
		exp->type = t;
	}
	switch (exp->tag) {
	case ast_exp_definition: {
		struct definition *def = &exp->as.def;
		if (def->is_polymorphic) {
			for (size_t i = 0; i < def->specs.len; ++i) {
				struct definition *spec_def = &def->specs.elems[i];
				spec_def->type = resolve_type(p, spec_def->type, scope);
				spec_def->exp = ast_desugar(p, spec_def->exp, scope);
			}
		} else {
			def->type = resolve_type(p, def->type, scope);
			def->exp = ast_desugar(p, def->exp, scope);
		}
		return exp;
	} break;
	case ast_exp_let:
		exp->as.let.def.exp = ast_desugar(p, exp->as.let.def.exp, scope);
		exp->as.let.def.type = resolve_type(p, exp->as.let.def.type, scope);
		exp->as.let.body = ast_desugar(p, exp->as.let.body, &exp->as.let.scope);
		return exp;
	case ast_exp_while:
		exp->as.wloop.cond = ast_desugar(p, exp->as.wloop.cond, scope);
		exp->as.wloop.body = ast_desugar(p, exp->as.wloop.body, scope);
		return exp;
	case ast_exp_string: return exp;
	case ast_exp_array_initializer: [[fallthrough]];
	case ast_exp_struct_initializer: {
		struct expression_stack *exps = &exp->as.init;
		for (size_t i = 0; i < exps->len; ++i) {
			exps->elems[i] = ast_desugar(p, exps->elems[i], scope);
		}
		return exp;
	} break;
	case ast_exp_named_struct_initializer: {
		struct expression_stack *exps = &exp->as.named_init.exps;
		for (size_t i = 0; i < exps->len; ++i) {
			exps->elems[i] = ast_desugar(p, exps->elems[i], scope);
		}
		return exp;
	} break;
	case ast_exp_zero_struct_initializer:	FAILWITH("TODO: ast_exp_zero_struct_initializer"); break;
	case ast_exp_procedure_literal:
		for (size_t i = 0; i < exp->as.proc.formals.len; ++i) {
			exp->as.proc.formals.elems[i].type =
				resolve_type(p, exp->as.proc.formals.elems[i].type, scope);
		}
		exp->as.proc.ret = resolve_type(p, exp->as.proc.ret, scope);
		exp->as.proc.body = ast_desugar(p, exp->as.proc.body, &exp->as.proc.scope);
		return exp;
	case ast_exp_undefined:  FAILWITH("TODO: ast_exp_undefined"); break;
	case ast_exp_value_cons:
		if (exp->as.valcons.exp)
			exp->as.valcons.exp = ast_desugar(p, exp->as.valcons.exp, scope);
		return exp;
	case ast_exp_unary:
		if ((enum unaop)exp->as.una.op == unaop_call) {
			exp->as.call.proc = ast_desugar(p, exp->as.call.proc, scope);
			for (size_t i = 0; i < exp->as.call.args.len; ++i) {
				exp->as.call.args.elems[i] = ast_desugar(p, exp->as.call.args.elems[i], scope);
			}
		} else if ((enum unaop)exp->as.una.op == unaop_index) {
			struct index *idx = &exp->as.idx;
			idx->exp = ast_desugar(p, idx->exp, scope);
			idx->idx = ast_desugar(p, idx->idx, scope);
		} else if ((enum unaop)exp->as.una.op == unaop_slice) {
			struct slice *slice = &exp->as.slice;
			slice->exp = ast_desugar(p, slice->exp, scope);
			slice->idx = ast_desugar(p, slice->idx, scope);
			slice->len = ast_desugar(p, slice->len, scope);
		} else if (((enum unaop)exp->as.una.op == unaop_cast)) {
			struct cast *cast = &exp->as.cast;
			cast->type = resolve_type(p, cast->type, scope);
			cast->exp = ast_desugar(p, cast->exp, scope);
		} else {
			exp->as.una.exp = ast_desugar(p, exp->as.una.exp, scope);
		}
		return exp;
	case ast_exp_if:
		exp->as.iff.cond = ast_desugar(p, exp->as.iff.cond, scope);
		exp->as.iff.tb = ast_desugar(p, exp->as.iff.tb, scope);
		if (exp->as.iff.fb != NULL)
			exp->as.iff.fb = ast_desugar(p, exp->as.iff.fb, scope);
		return exp;
	case ast_exp_case: {
		exp->as.ccase.cexp = ast_desugar(p, exp->as.ccase.cexp, scope);
		struct case_branches *branches = &exp->as.ccase.branches;
		for (size_t i = 0; i < branches->len; ++i) {
			struct case_branch *branch = &branches->elems[i];
			struct scope *sc = &branch->scope;
			if (branch->binds_value)
				branch->binding.type = resolve_type(p, branch->binding.type, scope);
			if (branch->guard != NULL)
				branch->guard = ast_desugar(p, branch->guard, sc);
			branch->body = ast_desugar(p, branch->body, sc);
		}
		if (exp->as.ccase.else_exp != NULL)
			ast_desugar(p, exp->as.ccase.else_exp, scope);
		return exp;
	} break;
	case ast_exp_return:			FAILWITH("TODO: ast_exp_return"); break;
	case ast_exp_break:				FAILWITH("TODO: ast_exp_break"); break;
	case ast_exp_continue:			FAILWITH("TODO: ast_exp_continue"); break;
	case ast_exp_literal:
	case ast_exp_ident:
	case ast_exp_extern_symbol: return exp;
	case ast_exp_get_ptr:
		exp->as.get_ptr = ast_desugar(p, exp->as.get_ptr, scope);
		return exp;
	case ast_exp_get_len:
		exp->as.get_len = ast_desugar(p, exp->as.get_len, scope);
		return exp;
	case ast_exp_binary: {
		struct expression *left = ast_desugar(p, exp->as.bin.left, scope);
		struct expression *right = ast_desugar(p, exp->as.bin.right, scope);
		switch ((int)exp->as.bin.op) {
		case binop_and: {
			struct expression *iff = POOL_ALLOC(&p->data, struct expression);
			struct expression *fb  = POOL_ALLOC(&p->data, struct expression);
			iff->tag = ast_exp_if;
			iff->tok = exp->tok;
			iff->type = exp->type;
			iff->as.iff.cond = left;
			iff->as.iff.tb = right;
			iff->as.iff.fb = fb;
			fb->tag = ast_exp_literal;
			fb->type = &AST_TYPE_BOOL;
			fb->tok = exp->tok;
			fb->as.lit.as.i = 0;
			return iff;
		} break;
		case binop_or: {
			FAILWITH("TODO: binop_or");
		} break;
		case binop_member: {
			if (right->tag == ast_exp_literal) {
				exp->as.bin.left = left;
				exp->as.bin.right = right;
				return exp;
			} else {
				assert(right->tag == ast_exp_ident);
				struct struct_type *mems = struct_type_members(p, left->type, scope);
				for (size_t i = 0; i < mems->len; ++i) {
					if (sv_is_equal(token_to_strview(mems->elems[i].name),
									token_to_strview(right->tok))) {
						exp->as.bin.right = POOL_ALLOC(&p->data, struct expression);
						*exp->as.bin.right = *right;
						exp->as.bin.right->tag = ast_exp_literal;
						exp->as.bin.right->as.lit.as.i = i;
						return exp;
					}
				}
				FAILWITH("Unreachable");
			}
		} break;
		case binop_add_assign: exp->as.bin.op = op_add; goto desugar_assignment;
		case binop_and_assign: exp->as.bin.op = op_and; goto desugar_assignment;
		case binop_lor_assign: exp->as.bin.op = op_lor; goto desugar_assignment;
		case binop_xor_assign: exp->as.bin.op = op_xor; goto desugar_assignment;
		case binop_sub_assign: exp->as.bin.op = op_sub; goto desugar_assignment;
		case binop_mul_assign: exp->as.bin.op = op_mul; goto desugar_assignment;
		case binop_div_assign: exp->as.bin.op = op_div; goto desugar_assignment;
		case binop_mod_assign: exp->as.bin.op = op_mod; goto desugar_assignment;
		case binop_shift_left_assign:  exp->as.bin.op = op_shift_left;  goto desugar_assignment;
		case binop_shift_right_assign: exp->as.bin.op = op_shift_right; goto desugar_assignment;
		desugar_assignment:
		{
			struct expression *assign = POOL_ALLOC(&p->data, struct expression);
			*assign = *exp;
			assign->as.bin.op = op_assign;
			assign->as.bin.left = exp->as.bin.left;
			assign->as.bin.right = exp;
			return assign;
		} break;
		default:
			exp->as.bin.left = left;
			exp->as.bin.right = right;
			return exp;
		}
	} break;
	default: FAILWITH("Unreachable");
	}
	return NULL;
}

enum ir_opcode {
	ir_op_nop,
	ir_op_undefined,
	ir_op_mov,
	ir_op_add,
	ir_op_sub,
	ir_op_mul,
	ir_op_div,
	ir_op_mod,
	ir_op_lnot,
	ir_op_land,
	ir_op_lor,
	ir_op_xor,
	ir_op_shl,
	ir_op_shr,
	ir_op_neg,
	ir_op_cmpe,
	ir_op_cmpne,
	ir_op_cmpl,
	ir_op_cmpg,
	ir_op_cmple,
	ir_op_cmpge,
	ir_op_cast,
	ir_op_retval,
	ir_op_conslice,
	ir_op_alloca,
	ir_op_load,
	ir_op_loadglobl,
	ir_op_loadconst,
	ir_op_loadimm,
	ir_op_store,
	ir_op_copy,
	ir_op_getelemptr,
	ir_op_memzero,
	ir_op_pushfunarg,
	ir_op_call,
	IR_OPCODE_COUNT,
};

enum ir_opterm {
	ir_op_ret,
	ir_op_goto,
	ir_op_if,
	ir_op_tailcall,
};

typedef struct ir_ins {
	enum ir_opcode op : 16;
	uint16_t dst;
	union {
		uint32_t u32;
		int32_t  i32;
		uint16_t rx[2];
	} arg;
	struct type *type;
} IR_Ins;

struct ir_code {
	uint32_t len, cap;
	IR_Ins *elems;
};

struct ir_args {
	uint32_t len, cap;
	uint16_t *elems;
};

struct ir_blk_terminal {
	enum ir_opterm op;
	struct ir_args args;
	struct ir_blk *b0;
	struct ir_blk *b1;
	struct ir_blk *j0;
};

struct ir_blk {
	struct ir_args args;
	struct ir_code code;
	struct ir_blk_terminal term;
};

enum ir_obj_tag {
	IRO_PROC,
	IRO_DATA,
	IRO_EXTERN_DATA,
	IRO_EXTERN_PROC,
};

#define IR_OBJ_HDDR_MEMBERS						\
	enum ir_obj_tag tag;						\
	char *link;									\
	struct ir_proc *init_proc;					\
	bool is_static

struct ir_obj_hddr {
	IR_OBJ_HDDR_MEMBERS;
};

struct ir_proc {
	IR_OBJ_HDDR_MEMBERS;
	struct definition *def;
	struct procedure *node;
	struct ir_blk *entry;
	struct type *type;
	uint16_t regc;
	uint16_t argc;
	uint16_t retc;
};

struct ir_extern {
	IR_OBJ_HDDR_MEMBERS;
};

struct ir_data {
	IR_OBJ_HDDR_MEMBERS;
	size_t size;
	void *dat;
	struct type *type;
};

union ir_object {
	struct ir_obj_hddr hddr;
	enum ir_obj_tag tag;
	struct ir_proc proc;
	struct ir_extern ext;
	struct ir_data data;
};

struct ir_objects {
	uint32_t len, cap;
	union ir_object *elems;
};

struct ir_toplevel {
	uint32_t len, cap;
	union ir_object *elems;
	bool is_dll;
	struct mem_arena data;
};

size_t type_size(struct type *t);
size_t type_alignment(struct type *t);

size_t type_size(struct type *t)
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

size_t type_alignment(struct type *t)
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

size_t
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

#if 0 /* TODO: Unused */
enum sysv_x64_class {
	ABI_NO_CLASS,
	ABI_INTEGER,
	ABI_SSE,
	ABI_SSEUP,
	ABI_X87,
	ABI_X87UP,
	ABI_COMPLEX_X87,
	ABI_MEMORY,
};
#endif

struct ast_comp_dest {
	enum {
		DST_NONE,
		DST_VAL,
		DST_REF,
		DST_CPY,
		DST_RET,
	} tag;
	uint16_t reg;
};

#define DEST_VAL(_reg) ((struct ast_comp_dest){.tag = DST_VAL, .reg = (_reg)})
#define DEST_REF(_reg) ((struct ast_comp_dest){.tag = DST_REF, .reg = (_reg)})
#define DEST_CPY(_reg) ((struct ast_comp_dest){.tag = DST_CPY, .reg = (_reg)})
#define DEST_RET(_reg) ((struct ast_comp_dest){.tag = DST_RET, .reg = (_reg)})
#define DEST_NONE(...) ((struct ast_comp_dest){.tag = DST_NONE})

struct ir_toplevel ast_compile(struct scope *scope);
struct ir_blk *ast_compile_expression(struct expression *exp, struct ast_comp_dest dst, size_t proc_id,
									  struct ir_blk *blk, struct scope *scope, struct ir_toplevel *tl);
void ast_compile_procedure(size_t proc_id, struct ir_toplevel *tl);
union ir_object *get_toplevel_obj(struct ir_toplevel *tl, size_t id);
struct ir_proc *get_toplevel_proc(struct ir_toplevel *tl, size_t id);

union ir_object *get_toplevel_obj(struct ir_toplevel *tl, size_t id)
{
	assert(id < tl->len);
	return &tl->elems[id];
}

struct ir_proc *get_toplevel_proc(struct ir_toplevel *tl, size_t id)
{
	union ir_object *obj = get_toplevel_obj(tl, id);
	assert(obj->tag == IRO_PROC);
	return &obj->proc;
}

int ir_proc_new_reg(struct ir_toplevel *tl, size_t proc_id)
{
	struct ir_proc *proc = get_toplevel_proc(tl, proc_id);
	assert(proc->regc < UINT16_MAX);
	return proc->regc++;
}

void ast_compile_procedure(size_t proc_id, struct ir_toplevel *tl)
{
	struct ir_proc *proc = get_toplevel_proc(tl, proc_id);
	if (proc->node->ret->tag == ast_type_void) {
		proc->retc = 0;
	}
	struct def_array *formals = &proc->node->formals;
	struct scope *scope = &proc->node->scope;
	struct ir_blk *entry = POOL_ALLOC(&tl->data, struct ir_blk);
	proc->entry = entry;
	for (size_t argn = 0; argn < formals->len; ++argn) {
		struct definition *formal = &formals->elems[argn];
		assert(formal->type != NULL);
		int reg = ir_proc_new_reg(tl, proc_id);
		int sym = ir_proc_new_reg(tl, proc_id);
		da_append(&entry->args, reg);
		formal->ir_symbol = sym;
		/* push registers to stack */
		da_append(&entry->code, (IR_Ins){
				.op   = ir_op_alloca,
				.type = formal->type,
				.dst  = sym,
				.arg.i32 = 1,
			});
		da_append(&entry->code, (IR_Ins){
				.op   = ir_op_store,
				.type = formal->type,
				.dst  = sym,
				.arg.rx[0] = reg,
			});
	}
	proc->argc = entry->args.len;
	int reg = ir_proc_new_reg(tl, proc_id);
	struct ir_blk *res = ast_compile_expression(proc->node->body, DEST_RET(reg), proc_id, entry, scope, tl);
	da_append(&res->term.args, reg);
	res->term.op = ir_op_ret;
}

static struct ir_blk *
dst_cpy_initializer(struct expression *exp, struct ast_comp_dest dst, size_t proc_id,
					struct ir_blk *blk, struct scope *scope, struct ir_toplevel *tl)
{
	assert(dst.tag == DST_CPY);
	assert(exp->tag == ast_exp_array_initializer || exp->tag == ast_exp_struct_initializer);
	if (exp->type->tag == ast_type_array) {
		struct type *base_type = POOL_ALLOC(&tl->data, struct type);
		base_type->tag = ast_type_ptr;
		base_type->as.ptr = exp->type;
		for (size_t i = 0; i < exp->as.init.len; ++i) {
			int ptr = ir_proc_new_reg(tl, proc_id);
			int idx = ir_proc_new_reg(tl, proc_id);
			da_append(&blk->code, (IR_Ins){
					.op   = ir_op_loadimm,
					.type = &AST_TYPE_U64,
					.dst  = idx,
					.arg.u32 = i,
				});
			da_append(&blk->code, (IR_Ins){
					.op   = ir_op_getelemptr,
					.type = base_type,
					.dst  = ptr,
					.arg.rx[0] = dst.reg,
					.arg.rx[1] = idx,
				});
			blk = ast_compile_expression(exp->as.init.elems[i], DEST_CPY(ptr), proc_id, blk, scope, tl);
		}
		return blk;
	}
	if (exp->type->tag == ast_type_slice) {
		struct type *array_type = POOL_ALLOC(&tl->data, struct type);
		array_type->tag = ast_type_array;
		array_type->as.array.base = exp->type->as.slice;
		array_type->as.array.is_sized = true;
		array_type->as.array.size = exp->as.init.len;
		struct type *base_type = POOL_ALLOC(&tl->data, struct type);
		base_type->tag = ast_type_ptr;
		base_type->as.ptr = array_type;
		/* allocate space for array */
		int array_reg = ir_proc_new_reg(tl, proc_id);
		da_append(&blk->code, (IR_Ins){
				.op   = ir_op_alloca,
				.type = array_type,
				.dst  = array_reg,
				.arg.i32 = 1,
			});
		for (size_t i = 0; i < exp->as.init.len; ++i) {
			int ptr = ir_proc_new_reg(tl, proc_id);
			int idx = ir_proc_new_reg(tl, proc_id);
			da_append(&blk->code, (IR_Ins){
					.op   = ir_op_loadimm,
					.type = &AST_TYPE_U64,
					.dst  = idx,
					.arg.u32 = i,
				});
			da_append(&blk->code, (IR_Ins){
					.op   = ir_op_getelemptr,
					.type = base_type,
					.dst  = ptr,
					.arg.rx[0] = array_reg,
					.arg.rx[1] = idx,
				});
			blk = ast_compile_expression(exp->as.init.elems[i], DEST_CPY(ptr), proc_id, blk, scope, tl);
		}
		/* build slice */
		int slice_reg = ir_proc_new_reg(tl, proc_id);
		int len_reg   = ir_proc_new_reg(tl, proc_id);
		/* slice length */
		da_append(&blk->code, (IR_Ins){
				.op      = ir_op_loadimm,
				.type    = &AST_TYPE_U64,
				.dst     = len_reg,
				.arg.u32 = exp->as.init.len,
			});
		/* construct slice value */
		da_append(&blk->code, (IR_Ins){
				.op        = ir_op_conslice,
				.type      = exp->type,
				.dst       = slice_reg,
				.arg.rx[0] = array_reg,
				.arg.rx[1] = len_reg,
			});
		/* store value to dst */
		da_append(&blk->code, (IR_Ins){
				.op        = ir_op_store,
				.type      = exp->type,
				.dst       = dst.reg,
				.arg.rx[0] = slice_reg,
			});
		return blk;
	}
	if (exp->type->tag == ast_type_struct) {
		struct struct_type *st = &exp->type->as.struct_t;
		struct type *base_type = POOL_ALLOC(&tl->data, struct type);
		base_type->tag = ast_type_ptr;
		base_type->as.ptr = exp->type;
		for (size_t i = 0; i < st->len; ++i) {
			int ptr = ir_proc_new_reg(tl, proc_id);
			int idx = ir_proc_new_reg(tl, proc_id);
			da_append(&blk->code, (IR_Ins){
					.op   = ir_op_loadimm,
					.type = &AST_TYPE_U64,
					.dst  = idx,
					.arg.u32 = i,
				});
			da_append(&blk->code, (IR_Ins){
					.op   = ir_op_getelemptr,
					.type = base_type,
					.dst  = ptr,
					.arg.rx[0] = dst.reg,
					.arg.rx[1] = idx,
				});
			blk = ast_compile_expression(exp->as.init.elems[i], DEST_CPY(ptr), proc_id, blk, scope, tl);
		}
		return blk;
	}
	FAILWITH("TODO: ast_exp_struct_initializer");
	return NULL;
}

static uint32_t
get_struct_member_idx(struct type *t, struct token *mem)
{
	assert(t->tag == ast_type_struct);
	struct strview sv_mem = token_to_strview(mem);
	for (uint32_t i = 0; i < t->as.struct_t.len; ++i) {
		assert(t->as.struct_t.elems[i].name != NULL);
		if (sv_is_equal(sv_mem, token_to_strview(t->as.struct_t.elems[i].name)))
			return i;
	}
	FAILWITH("Unreachable struct type `%s` has no member `"SV_FMT"`",
			 ast_type_to_str(t), SV_ARGS(sv_mem));
	return 0;
}

static struct ir_blk *
dst_cpy_named_initializer(struct expression *exp, struct ast_comp_dest dst, size_t proc_id,
						  struct ir_blk *blk, struct scope *scope, struct ir_toplevel *tl)
{
	assert(dst.tag == DST_CPY);
	assert(exp->tag == ast_exp_named_struct_initializer);
	struct type *base_type = POOL_ALLOC(&tl->data, struct type);
	base_type->tag = ast_type_ptr;
	base_type->as.ptr = exp->type;
	for (size_t i = 0; i < exp->as.named_init.ids.len; ++i) {
		struct token *name = exp->as.named_init.ids.elems[i];
		struct expression *exp_init = exp->as.named_init.exps.elems[i];
		uint32_t idx = get_struct_member_idx(exp->type, name);
		int ptr_reg = ir_proc_new_reg(tl, proc_id);
		int idx_reg = ir_proc_new_reg(tl, proc_id);
		da_append(&blk->code, (IR_Ins){
				.op        = ir_op_loadimm,
				.type      = &AST_TYPE_U64,
				.dst       = idx_reg,
				.arg.u32   = idx,
			});
		da_append(&blk->code, (IR_Ins){
				.op        = ir_op_getelemptr,
				.type      = base_type,
				.dst       = ptr_reg,
				.arg.rx[0] = dst.reg,
				.arg.rx[1] = idx_reg,
			});
		blk = ast_compile_expression(exp_init, DEST_CPY(ptr_reg), proc_id, blk, scope, tl);
	}
	return blk;
}

static struct ir_blk *
dst_cpy_valcons(struct expression *exp, struct ast_comp_dest dst, size_t proc_id,
				struct ir_blk *blk, struct scope *scope, struct ir_toplevel *tl)
{
	assert(dst.tag == DST_CPY);
	assert(exp->tag == ast_exp_value_cons);
	int64_t tag_val = exp->as.valcons.tag_val;
	assert(tag_val <= INT32_MAX);
	assert(tag_val >= INT32_MIN);
	int tag = ir_proc_new_reg(tl, proc_id);
	int tmp = ir_proc_new_reg(tl, proc_id);
	int data_offset = ir_proc_new_reg(tl, proc_id);
	/* set tag field */
	da_append(&blk->code, (IR_Ins){
			.op   = ir_op_loadimm,
			.type = &AST_TYPE_UNION_TAG,
			.dst  = tag,
			.arg.i32 = tag_val,
		});
	da_append(&blk->code, (IR_Ins){
			.op   = ir_op_store,
			.type = &AST_TYPE_UNION_TAG,
			.dst  = dst.reg,
			.arg.i32 = tag,
		});
	if (exp->as.valcons.exp) {
		/* offset to data field */
		da_append(&blk->code, (IR_Ins){
				.op   = ir_op_loadimm,
				.type = &AST_TYPE_U64,
				.dst  = tmp,
				.arg.i32 = 1,
			});
		da_append(&blk->code, (IR_Ins){
				.op   = ir_op_getelemptr,
				.type = exp->type,
				.dst  = data_offset,
				.arg.rx[0] = dst.reg,
				.arg.rx[1] = tmp,
			});
		/* set data field */
		blk = ast_compile_expression(exp->as.valcons.exp, DEST_CPY(data_offset), proc_id, blk, scope, tl);
	}
	return blk;
}

struct ir_blk *
ast_compile_expression(struct expression *exp, struct ast_comp_dest dst, size_t proc_id,
					   struct ir_blk *blk, struct scope *scope, struct ir_toplevel *tl)
{
	switch (exp->tag) {
	case ast_exp_let: {
		struct let *let = &exp->as.let;
		struct definition *def = &let->def;
		int var = ir_proc_new_reg(tl, proc_id);
		def->ir_symbol = var;
		da_append(&blk->code, (IR_Ins){
				.op   = ir_op_alloca,
				.type = def->type,
				.dst  = var,
				.arg.i32 = 1,
			});
		struct ir_blk *res = ast_compile_expression(def->exp, DEST_CPY(var), proc_id, blk, scope, tl);
		return ast_compile_expression(let->body, dst, proc_id, res, &let->scope, tl);
	} break;
	case ast_exp_literal: {
		struct literal *lit = &exp->as.lit;
		if (type_is_integer(exp->type)) {
			assert(lit->as.i <= INT32_MAX && lit->as.i >= INT32_MIN);
			switch (dst.tag) {
			case DST_RET:
			case DST_VAL: {
				da_append(&blk->code, (IR_Ins){
						.op = ir_op_loadimm,
						.type = exp->type,
						.dst = dst.reg,
						.arg.i32 = lit->as.i,
					});
			} break;
			case DST_CPY: {
				int tmp = ir_proc_new_reg(tl, proc_id);
				da_append(&blk->code, (IR_Ins){
						.op = ir_op_loadimm,
						.type = exp->type,
						.dst = tmp,
						.arg.i32 = lit->as.i,
					});
				da_append(&blk->code, (IR_Ins){
						.op = ir_op_store,
						.type = exp->type,
						.dst = dst.reg,
						.arg.rx[0] = tmp,
					});
			} break;
			case DST_REF:  FAILWITH("TODO: ast_exp_literal"); break;
			case DST_NONE: FAILWITH("TODO: ast_exp_literal"); break;
			default:
				FAILWITH("TODO: ast_exp_literal");
				break;
			}
		} else if (type_is_bool(exp->type)) {
			assert(lit->as.i == 1 || lit->as.i == 0);
			switch (dst.tag) {
			case DST_VAL: {
				da_append(&blk->code, (IR_Ins){
						.op = ir_op_loadimm,
						.type = exp->type,
						.dst = dst.reg,
						.arg.i32 = lit->as.i,
					});
			} break;
			case DST_CPY:  FAILWITH("TODO: ast_exp_literal"); break;
			case DST_REF:  FAILWITH("TODO: ast_exp_literal"); break;
			case DST_NONE: FAILWITH("TODO: ast_exp_literal"); break;
			case DST_RET: FAILWITH("TODO: ast_exp_literal"); break;
			default:
				FAILWITH("TODO: ast_exp_literal");
				break;
			}
		} else {
			ast_type_fprint(exp->type, stdout);
			FAILWITH("TODO: ast_exp_literal (%s)", ast_type_to_str(exp->type));
		}
		return blk;
	} break;
	case ast_exp_zero_struct_initializer: {
		assert(dst.tag == DST_CPY);
		da_append(&blk->code, (IR_Ins){
				.op = ir_op_memzero,
				.type = exp->type,
				.dst = dst.reg,
			});
		return blk;
	} break;
	case ast_exp_value_cons: {
		switch (dst.tag) {
		case DST_RET: {
			if (type_size(exp->type) > 16) {
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_retval,
						.type = exp->type,
						.dst  = dst.reg,
					});
				return dst_cpy_valcons(exp, DEST_CPY(dst.reg), proc_id, blk, scope, tl);
			}
		} [[fallthrough]];
		case DST_VAL: {
			int tmp = ir_proc_new_reg(tl, proc_id);
			da_append(&blk->code, (IR_Ins){
					.op   = ir_op_alloca,
					.type = exp->type,
					.dst  = tmp,
					.arg.i32 = 1,
				});
			blk = dst_cpy_valcons(exp, DEST_CPY(tmp), proc_id, blk, scope, tl);
			da_append(&blk->code, (IR_Ins){
					.op        = ir_op_load,
					.type      = exp->type,
					.dst       = dst.reg,
					.arg.rx[0] = tmp,
				});
			return blk;
		} break;
		case DST_CPY: return dst_cpy_valcons(exp, dst, proc_id, blk, scope, tl);
		case DST_REF:  FAILWITH("TODO: ast_exp_value_cons"); break;
		case DST_NONE: FAILWITH("TODO: ast_exp_value_cons"); break;
		default: FAILWITH("Unreachable"); break;
		}
	} break;
	case ast_exp_array_initializer: [[fallthrough]];
	case ast_exp_struct_initializer: {
		switch (dst.tag) {
		case DST_RET: {
			if (type_size(exp->type) > 16) {
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_retval,
						.type = exp->type,
						.dst  = dst.reg,
					});
				return dst_cpy_initializer(exp, DEST_CPY(dst.reg), proc_id, blk, scope, tl);
			}
		} [[fallthrough]];
		case DST_VAL: {
			int tmp = ir_proc_new_reg(tl, proc_id);
			da_append(&blk->code, (IR_Ins){
					.op      = ir_op_alloca,
					.type    = exp->type,
					.dst     = tmp,
					.arg.i32 = 1,
				});
			blk = dst_cpy_initializer(exp, DEST_CPY(tmp), proc_id, blk, scope, tl);
			da_append(&blk->code, (IR_Ins){
					.op        = ir_op_load,
					.type      = exp->type,
					.dst       = dst.reg,
					.arg.rx[0] = tmp,
				});
			return blk;
		} break;
		case DST_CPY: return dst_cpy_initializer(exp, dst, proc_id, blk, scope, tl);
		case DST_REF: FAILWITH("TODO: DST_REF"); break;
		case DST_NONE: FAILWITH("TODO: DST_NONE"); break;
		default: FAILWITH("Unreachable"); break;
		}
	} break;
	case ast_exp_named_struct_initializer: {
		switch (dst.tag) {
		case DST_RET: {
			if (type_size(exp->type) > 16) {
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_retval,
						.type = exp->type,
						.dst  = dst.reg,
					});
				return dst_cpy_named_initializer(exp, DEST_CPY(dst.reg), proc_id, blk, scope, tl);
			}
		} [[fallthrough]];
		case DST_VAL: {
			int tmp = ir_proc_new_reg(tl, proc_id);
			da_append(&blk->code, (IR_Ins){
					.op      = ir_op_alloca,
					.type    = exp->type,
					.dst     = tmp,
					.arg.i32 = 1,
				});
			blk = dst_cpy_named_initializer(exp, DEST_CPY(tmp), proc_id, blk, scope, tl);
			da_append(&blk->code, (IR_Ins){
					.op        = ir_op_load,
					.type      = exp->type,
					.dst       = dst.reg,
					.arg.rx[0] = tmp,
				});
			return blk;
		} break;
		case DST_CPY: return dst_cpy_named_initializer(exp, dst, proc_id, blk, scope, tl);
		case DST_REF: FAILWITH("TODO: DST_REF"); break;
		case DST_NONE: FAILWITH("TODO: DST_NONE"); break;
		default: FAILWITH("Unreachable"); break;
		}
	} break;
	case ast_exp_procedure_literal: FAILWITH("TODO: ast_exp_procedure_literal"); break;
	case ast_exp_undefined: {
		switch (dst.tag) {
		case DST_VAL: {
			da_append(&blk->code, (IR_Ins){
					.op   = ir_op_undefined,
					.type = exp->type,
					.dst  = dst.reg,
				});
		} break;
		case DST_CPY:  FAILWITH("TODO: ast_exp_undefined"); break;
		case DST_REF:  FAILWITH("TODO: ast_exp_undefined"); break;
		case DST_NONE: FAILWITH("TODO: ast_exp_undefined"); break;
		case DST_RET: FAILWITH("TODO: ast_exp_literal"); break;
		default:       FAILWITH("Unreachable"); break;
		}
		return blk; /* do nothing */
	} break;
	case ast_exp_string: {
		char *str = NULL;
		size_t length = sv_unescape_string(token_to_strview(exp->tok), &str);
		size_t id = tl->len;
		union ir_object p = {
			.data.is_static = true,
			.data.tag  = IRO_DATA,
			.data.size = length,
			.data.dat  = str,
			.data.link = fmt_str(".LSTR%zu", id),
		};
		da_append(tl, p);
		switch (dst.tag) {
		case DST_VAL: {
			da_append(&blk->code, (IR_Ins){
					.op   = ir_op_loadglobl,
					.type = exp->type,
					.dst  = dst.reg,
					.arg.u32 = id,
				});
		} break;
		case DST_CPY: {
			int tmp = ir_proc_new_reg(tl, proc_id);
			da_append(&blk->code, (IR_Ins){
					.op   = ir_op_loadglobl,
					.type = exp->type,
					.dst  = tmp,
					.arg.u32 = id,
				});
			da_append(&blk->code, (IR_Ins){
					.op   = ir_op_store,
					.type = exp->type,
					.dst  = dst.reg,
					.arg.rx[0] = tmp,
				});
		} break;
		case DST_REF: {
			int tmp = ir_proc_new_reg(tl, proc_id);
			da_append(&blk->code, (IR_Ins){
					.op   = ir_op_alloca,
					.type = exp->type,
					.dst  = dst.reg,
					.arg.i32 = 1,
				});
			da_append(&blk->code, (IR_Ins){
					.op   = ir_op_loadglobl,
					.type = exp->type,
					.dst  = tmp,
					.arg.u32 = id,
				});
			da_append(&blk->code, (IR_Ins){
					.op   = ir_op_store,
					.type = exp->type,
					.dst  = dst.reg,
					.arg.rx[0] = tmp,
				});
		} break;
		case DST_RET: FAILWITH("TODO: ast_exp_literal"); break;
		case DST_NONE: FAILWITH("TODO: ast_exp_string"); break;
		default:      FAILWITH("Unreachable"); break;
		}
		return blk;
	} break;
	case ast_exp_extern_symbol: FAILWITH("TODO: ast_exp_extern_symbol"); break;
	case ast_exp_ident: {
		struct symtbl_entry *entry = lookup_entry(scope, token_to_strview(exp->tok));
		assert(entry->tag == SYMTBL_VAR);
		struct definition *def = entry->as.variable.def;
		assert(def != NULL);
		size_t ir_symbol;
		if (def->is_polymorphic) {
			struct definition *instance = lookup_poly_proc_instance(NULL, def, exp->type, scope);
			assert(instance != NULL);
			ir_symbol = instance->ir_symbol;
		} else {
			ir_symbol = def->ir_symbol;
		}
		switch (dst.tag) {
		case DST_RET:
		case DST_VAL: {
			if (def->is_global) {
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_loadglobl,
						.type = exp->type,
						.dst  = dst.reg,
						.arg.u32 = ir_symbol,
					});
			} else {
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_load,
						.type = exp->type,
						.dst  = dst.reg,
						.arg.rx[0] = ir_symbol,
					});
			}
		} break;
		case DST_CPY: {
			int tmp = ir_proc_new_reg(tl, proc_id);
			if (def->is_global) {
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_loadglobl,
						.type = exp->type,
						.dst  = tmp,
						.arg.u32 = ir_symbol,
					});
			} else {
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_load,
						.type = exp->type,
						.dst  = tmp,
						.arg.rx[0] = ir_symbol,
					});
			}
			da_append(&blk->code, (IR_Ins){
					.op   = ir_op_store,
					.type = exp->type,
					.dst  = dst.reg,
					.arg.rx[0] = tmp,
				});
		} break;
		case DST_REF: {
			if (def->is_global) {
				FAILWITH("TODO: ast_exp_ident");
			} else {
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_mov,
						.dst  = dst.reg,
						.arg.rx[0] = ir_symbol,
					});
			}
		} break;
		case DST_NONE: FAILWITH("TODO: ast_exp_ident"); break;
		default:
			FAILWITH("TODO: ast_exp_ident");
			break;
		}
		return blk;
	} break;
	case ast_exp_binary: {
		struct binary *bin = &exp->as.bin;
		enum ir_opcode op;
		switch ((enum binop)bin->op) {
		case binop_sequence: {
			struct ir_blk *left =
				ast_compile_expression(bin->left,
									   DEST_VAL(ir_proc_new_reg(tl, proc_id)),
									   proc_id, blk, scope, tl);
			return ast_compile_expression(bin->right, dst, proc_id, left, scope, tl);
		} break;
		case binop_add:			op = ir_op_add;  goto LBL_binop;
		case binop_sub:			op = ir_op_sub;  goto LBL_binop;
		case binop_mul:			op = ir_op_mul;  goto LBL_binop;
		case binop_div:			op = ir_op_div;  goto LBL_binop;
		case binop_mod:			op = ir_op_mod;  goto LBL_binop;
		case binop_xor:			op = ir_op_xor;  goto LBL_binop;
		case binop_land:		op = ir_op_land; goto LBL_binop;
		case binop_lor:			op = ir_op_lor;  goto LBL_binop;
		case binop_shift_left:	op = ir_op_shl;  goto LBL_binop;
		case binop_shift_right:	op = ir_op_shr;  goto LBL_binop;
		LBL_binop:
		{
			int left_reg = ir_proc_new_reg(tl, proc_id);
			int right_reg = ir_proc_new_reg(tl, proc_id);
			struct ir_blk *left  = ast_compile_expression(bin->left, DEST_VAL(left_reg), proc_id, blk, scope, tl);
			struct ir_blk *right = ast_compile_expression(bin->right, DEST_VAL(right_reg), proc_id, left, scope, tl);
			switch (dst.tag) {
			case DST_RET:
			case DST_VAL: {
				da_append(&right->code, (IR_Ins){
						.op   = op,
						.type = exp->type,
						.dst  = dst.reg,
						.arg.rx[0] = left_reg,
						.arg.rx[1] = right_reg,
					});
			} break;
			case DST_CPY: {
				int tmp = ir_proc_new_reg(tl, proc_id);
				da_append(&right->code, (IR_Ins){
						.op   = op,
						.type = exp->type,
						.dst  = tmp,
						.arg.rx[0] = left_reg,
						.arg.rx[1] = right_reg,
					});
				da_append(&right->code, (IR_Ins){
						.op   = ir_op_store,
						.type = exp->type,
						.dst  = dst.reg,
						.arg.rx[0] = tmp,
					});
			} break;
			case DST_REF: FAILWITH("TODO: DST_REF"); break;
			case DST_NONE: FAILWITH("TODO: DST_NONE"); break;
			default: FAILWITH("Unreachable"); break;
			}
			return right;
		} break;
		case binop_equal:      op = ir_op_cmpe;  goto LBL_cmp;
		case binop_not_equal:  op = ir_op_cmpne; goto LBL_cmp;
		case binop_less_than:  op = ir_op_cmpl;  goto LBL_cmp;
		case binop_more_than:  op = ir_op_cmpg;  goto LBL_cmp;
		case binop_less_equal: op = ir_op_cmple; goto LBL_cmp;
		case binop_more_equal: op = ir_op_cmpge; goto LBL_cmp;
		{
		LBL_cmp:
			int left_reg = ir_proc_new_reg(tl, proc_id);
			int right_reg = ir_proc_new_reg(tl, proc_id);
			struct ir_blk *left  = ast_compile_expression(bin->left, DEST_VAL(left_reg), proc_id, blk, scope, tl);
			struct ir_blk *right = ast_compile_expression(bin->right, DEST_VAL(right_reg), proc_id, left, scope, tl);
			switch (dst.tag) {
			case DST_VAL: {
				da_append(&right->code, (IR_Ins){
						.op   = op,
						.type = bin->left->type,
						.dst  = dst.reg,
						.arg.rx[0] = left_reg,
						.arg.rx[1] = right_reg,
					});
			} break;
			case DST_CPY: {
				int tmp = ir_proc_new_reg(tl, proc_id);
				da_append(&right->code, (IR_Ins){
						.op   = op,
						.type = bin->left->type,
						.dst  = tmp,
						.arg.rx[0] = left_reg,
						.arg.rx[1] = right_reg,
					});
				da_append(&blk->code, (IR_Ins){
						.op = ir_op_store,
						.type = exp->type,
						.dst = dst.reg,
						.arg.rx[0] = tmp,
					});
			} break;
			case DST_REF: FAILWITH("TODO: DST_REF"); break;
			case DST_NONE: FAILWITH("TODO: DST_NONE"); break;
			case DST_RET: FAILWITH("TODO: ast_exp_literal"); break;
			default: FAILWITH("Unreachable"); break;
			}
			return right;
		}
		case binop_member: {
			struct expression *base = bin->left;
			struct expression *idx = bin->right;
			switch (dst.tag) {
			case DST_RET:
			case DST_VAL: {
				int base_reg = ir_proc_new_reg(tl, proc_id);
				int index_reg = ir_proc_new_reg(tl, proc_id);
				struct type *base_type = base->type;
				if (type_is_struct_ptr(base->type, scope)) {
					blk = ast_compile_expression(base, DEST_VAL(base_reg), proc_id, blk, scope, tl);
				} else {
					assert(base->type->tag == ast_type_struct);
					blk = ast_compile_expression(base, DEST_REF(base_reg), proc_id, blk, scope, tl);
					base_type = POOL_ALLOC(&tl->data, struct type);
					base_type->tag = ast_type_ptr;
					base_type->as.ptr = base->type;
				}
				if (idx->tag == ast_exp_literal) {
					size_t index = idx->as.lit.as.i;
					int tmp = ir_proc_new_reg(tl, proc_id);
					da_append(&blk->code, (IR_Ins){
							.op   = ir_op_loadimm,
							.type = idx->type,
							.dst  = index_reg,
							.arg.u32 = index,
						});
					da_append(&blk->code, (IR_Ins){
							.op   = ir_op_getelemptr,
							.type = base_type,
							.dst  = tmp,
							.arg.rx[0] = base_reg,
							.arg.rx[1] = index_reg,
						});
					da_append(&blk->code, (IR_Ins){
							.op   = ir_op_load,
							.type = exp->type,
							.dst  = dst.reg,
							.arg.rx[0] = tmp,
						});
				} else {
					FAILWITH("TODO: binop_member");
				}
				return blk;
			} break;
			case DST_NONE: FAILWITH("TODO: DST_NONE"); break;
			case DST_REF: FAILWITH("Unreachable"); break;
			case DST_CPY: FAILWITH("Unreachable"); break;
			default: FAILWITH("Unreachable"); break;
			}
		} break;
		case binop_assign: {
			int right_reg = dst.tag == DST_VAL ? dst.reg : ir_proc_new_reg(tl, proc_id);
			struct ir_blk *right = ast_compile_expression(bin->right, DEST_VAL(right_reg),
														  proc_id, blk, scope, tl);
			switch ((int)bin->left->tag) {
			case ast_exp_ident: {
				struct symtbl_entry *entry = lookup_entry(scope, token_to_strview(bin->left->tok));
				assert(entry != NULL);
				assert(entry->tag == SYMTBL_VAR);
				struct definition *def = entry->as.variable.def;
				assert(type_is_numeric_scalar(def->type)
					   || type_is_bool(def->type)
					   || type_is_pointer(def->type));
				if (def->is_global) {
					FAILWITH("TODO: store_global");
				} else {
					da_append(&right->code, (IR_Ins){
							.op   = ir_op_store,
							.type = def->type,
							.dst  = def->ir_symbol,
							.arg.rx[0] = right_reg,
						});
				}
			} break;
			case ast_exp_unary: {
				switch ((int)bin->left->as.una.op) {
				case op_dereference: {
					int left_reg = ir_proc_new_reg(tl, proc_id);
					if (bin->left->as.una.exp->tag == ast_exp_ident) {
						right = ast_compile_expression(bin->left->as.una.exp, DEST_VAL(left_reg),
													   proc_id, right, scope, tl);
					} else {
						right = ast_compile_expression(bin->left->as.una.exp, DEST_REF(left_reg),
													   proc_id, right, scope, tl);
					}
					da_append(&right->code, (IR_Ins){
							.op   = ir_op_store,
							.type = exp->type,
							.dst  = left_reg,
							.arg.rx[0] = right_reg,
						});
				} break;
				default: FAILWITH("TODO: binop_assign"); break;
				}
			} break;
			default: FAILWITH("TODO: binop_assign"); break;
			}
			assert(dst.tag == DST_VAL || dst.tag == DST_NONE);
			return right;
		} break;
		/* NOTE: These operators should be desugared */
		case binop_add_assign: FAILWITH("TODO: binop_add_assign"); break;
		case binop_and_assign: FAILWITH("TODO: binop_and_assign"); break;
		case binop_lor_assign: FAILWITH("TODO: binop_lor_assign"); break;
		case binop_xor_assign: FAILWITH("TODO: binop_xor_assign"); break;
		case binop_sub_assign: FAILWITH("TODO: binop_sub_assign"); break;
		case binop_mul_assign: FAILWITH("TODO: binop_mul_assign"); break;
		case binop_div_assign: FAILWITH("TODO: binop_div_assign"); break;
		case binop_mod_assign: FAILWITH("TODO: binop_mod_assign"); break;
		case binop_shift_left_assign: FAILWITH("TODO: binop_shift_left_assign"); break;
		case binop_shift_right_assign: FAILWITH("TODO: binop_shift_right_assign"); break;
		case binop_or:  FAILWITH("TODO: binop_or"); break;
		case binop_and: FAILWITH("TODO: binop_and"); break;
		}
		FAILWITH("TODO: ast_exp_binary");
	} break;
	case ast_exp_get_len: {
		struct expression *target = exp->as.get_len;
		if (type_is_slice(target->type)) {
			switch (dst.tag) {
			case DST_VAL: {
				int target_reg = ir_proc_new_reg(tl, proc_id);
				int idx = ir_proc_new_reg(tl, proc_id);
				int tmp = ir_proc_new_reg(tl, proc_id);
				blk = ast_compile_expression(target, DEST_REF(target_reg), proc_id, blk, scope, tl);
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_loadimm,
						.type = &AST_TYPE_U64,
						.dst  = idx,
						.arg.u32 = 1,
					});
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_getelemptr,
						.type = target->type,
						.dst  = tmp,
						.arg.rx[0] = target_reg,
						.arg.rx[1] = idx,
					});
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_load,
						.type = exp->type,
						.dst  = dst.reg,
						.arg.rx[0] = tmp,
					});
				return blk;
			} break;
			case DST_CPY: FAILWITH("TODO: DST_CPY (ast_exp_get_len)"); break;
			case DST_REF: FAILWITH("TODO: DST_REF (ast_exp_get_len)"); break;
			case DST_RET: FAILWITH("TODO: ast_exp_literal"); break;
			case DST_NONE: FAILWITH("TODO: DST_NONE (ast_exp_get_len)"); break;
			default: FAILWITH("Unreachable"); break;
			}
		} else if (type_is_array(target->type)) {
			size_t len = 0;
			if (target->type->as.array.is_sized) {
				len = target->type->as.array.size;
			} else {
				FAILWITH("TODO: unsized array");
			}
			switch (dst.tag) {
			case DST_VAL: {
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_loadimm,
						.type = &AST_TYPE_U64,
						.dst  = dst.reg,
						.arg.u32 = len,
					});
				return blk;
			} break;
			case DST_CPY: FAILWITH("TODO: DST_CPY (ast_exp_get_len)"); break;
			case DST_REF: FAILWITH("TODO: DST_REF (ast_exp_get_len)"); break;
			case DST_NONE: FAILWITH("TODO: DST_NONE (ast_exp_get_len)"); break;
			case DST_RET: FAILWITH("TODO: ast_exp_literal"); break;
			default: FAILWITH("Unreachable"); break;
			}
		} else {
			FAILWITH("TODO: ast_exp_get_len");
		}
	} break;
	case ast_exp_get_ptr: {
		struct expression *slice = exp->as.get_ptr;
		if (type_is_slice(slice->type)) {
			switch (dst.tag) {
			case DST_VAL: {
				int slice_reg = ir_proc_new_reg(tl, proc_id);
				blk = ast_compile_expression(slice, DEST_REF(slice_reg), proc_id, blk, scope, tl);
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_load,
						.type = exp->type,
						.dst  = dst.reg,
						.arg.rx[0] = slice_reg,
					});
				return blk;
			} break;
			case DST_CPY: FAILWITH("TODO: DST_CPY (ast_exp_get_ptr)"); break;
			case DST_REF: FAILWITH("TODO: DST_REF (ast_exp_get_ptr)"); break;
			case DST_NONE: FAILWITH("TODO: DST_NONE (ast_exp_get_ptr)"); break;
			case DST_RET: FAILWITH("TODO: ast_exp_literal"); break;
			default: FAILWITH("Unreachable"); break;
			}
		} else {
			FAILWITH("TODO: ast_exp_get_ptr");
		}
	} break;
	case ast_exp_unary: {
		struct unary *una = &exp->as.una;
		switch ((enum unaop)una->op) {
		case unaop_lnot: FAILWITH("TODO: unaop_lnot"); break;
		case unaop_not: FAILWITH("TODO: unaop_not"); break;
		case unaop_pos: {
			switch (dst.tag) {
			case DST_VAL: {
				return ast_compile_expression(una->exp, DEST_REF(dst.reg), proc_id, blk, scope, tl);
			} break;
			case DST_CPY: FAILWITH("TODO: unaop_pos"); break;
			case DST_REF: FAILWITH("TODO: unaop_pos"); break;
			case DST_NONE: FAILWITH("TODO: unaop_pos"); break;
			case DST_RET: FAILWITH("TODO: ast_exp_literal"); break;
			default: FAILWITH("Unreachable"); break;
			}
		} break;
		case unaop_neg: {
			int reg = ir_proc_new_reg(tl, proc_id);
			blk = ast_compile_expression(una->exp, DEST_VAL(reg), proc_id, blk, scope, tl);
			switch (dst.tag) {
			case DST_RET:
			case DST_VAL: {
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_neg,
						.type = exp->type,
						.dst  = dst.reg,
						.arg.rx[0] = reg,
					});
				return blk;
			} break;
			case DST_CPY: {
				int tmp = ir_proc_new_reg(tl, proc_id);
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_neg,
						.type = exp->type,
						.dst  = tmp,
						.arg.rx[0] = reg,
					});
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_store,
						.type = exp->type,
						.dst  = dst.reg,
						.arg.rx[0] = tmp,
					});
				return blk;
			} break;
			case DST_REF: FAILWITH("TODO: DST_REF"); break;
			case DST_NONE: FAILWITH("TODO: DST_NONE"); break;
			default: FAILWITH("Unreachable"); break;
			}
		} break;
		case unaop_address_of: {
			switch (dst.tag) {
			case DST_VAL: {
				return ast_compile_expression(una->exp, DEST_REF(dst.reg), proc_id, blk, scope, tl);
			} break;
			case DST_CPY: FAILWITH("TODO: DST_CPY"); break;
			case DST_REF: FAILWITH("TODO: DST_REF"); break;
			case DST_NONE: FAILWITH("TODO: DST_NONE"); break;
			case DST_RET: FAILWITH("TODO: DST_RET"); break;
			default: FAILWITH("Unreachable"); break;
			}
		} break;
		case unaop_dereference: {
			int reg = ir_proc_new_reg(tl, proc_id);
			blk = ast_compile_expression(una->exp, DEST_VAL(reg), proc_id, blk, scope, tl);
			switch (dst.tag) {
			case DST_RET:
			case DST_VAL: {
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_load,
						.type = exp->type,
						.dst  = dst.reg,
						.arg.rx[0] = reg,
					});
			} break;
			case DST_CPY: {
				int tmp = ir_proc_new_reg(tl, proc_id);
				da_append(&blk->code, (IR_Ins){
						.op        = ir_op_load,
						.type      = exp->type,
						.dst       = tmp,
						.arg.rx[0] = reg,
					});
				da_append(&blk->code, (IR_Ins){
						.op        = ir_op_store,
						.type      = exp->type,
						.dst       = dst.reg,
						.arg.rx[0] = tmp,
					});
			} break;
			case DST_NONE: FAILWITH("TODO: DST_NONE"); break;
			case DST_REF:  FAILWITH("TODO: DST_REF"); break;
			default: FAILWITH("Unreachable"); break;
			}
			return blk;
		} break;
		case unaop_index: {
			struct expression *base = exp->as.idx.exp;
			struct expression *idx =  exp->as.idx.idx;
			switch (dst.tag) {
			case DST_RET:
			case DST_VAL: {
				int base_reg = ir_proc_new_reg(tl, proc_id);
				int index_reg = ir_proc_new_reg(tl, proc_id);
				struct type *base_type = base->type;
				if (type_is_pointer(base->type)) {
					blk = ast_compile_expression(base, DEST_VAL(base_reg), proc_id, blk, scope, tl);
				} else if (type_is_array(base->type)) {
					if (base->is_lvalue) {
						blk = ast_compile_expression(base, DEST_REF(base_reg), proc_id, blk, scope, tl);
					} else {
						int tmp = ir_proc_new_reg(tl, proc_id);
						da_append(&blk->code, (IR_Ins){
								.op   = ir_op_alloca,
								.type = base->type,
								.dst  = base_reg,
								.arg.i32 = 1,
							});
						blk = ast_compile_expression(base, DEST_VAL(tmp), proc_id, blk, scope, tl);
						da_append(&blk->code, (IR_Ins){
								.op   = ir_op_store,
								.type = base->type,
								.dst  = base_reg,
								.arg.rx[0] = tmp,
							});
					}
					base_type = POOL_ALLOC(&tl->data, struct type);
					base_type->tag = ast_type_ptr;
					base_type->as.ptr = base->type;
				} else if (type_is_slice(base->type)) {
					int tmp = ir_proc_new_reg(tl, proc_id);
					blk = ast_compile_expression(base, DEST_REF(base_reg), proc_id, blk, scope, tl);
					base_type = type_slice_to_array_ptr(&tl->data, base->type);
					da_append(&blk->code, (IR_Ins){
							.op   = ir_op_load,
							.type = base_type,
							.dst  = tmp,
							.arg.rx[0] = base_reg,
						});
					base_reg = tmp;
				} else {
					FAILWITH("Unreachable");
				}
				blk = ast_compile_expression(idx, DEST_VAL(index_reg), proc_id, blk, scope, tl);
				int tmp = ir_proc_new_reg(tl, proc_id);
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_getelemptr,
						.type = base_type,
						.dst  = tmp,
						.arg.rx[0] = base_reg,
						.arg.rx[1] = index_reg,
					});
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_load,
						.type = exp->type,
						.dst  = dst.reg,
						.arg.rx[0] = tmp,
					});
				return blk;
			} break;
			case DST_REF: FAILWITH("Unreachable"); break;
			case DST_CPY: FAILWITH("Unreachable"); break;
			case DST_NONE: FAILWITH("TODO: DST_NONE"); break;
			default: FAILWITH("Unreachable"); break;
			}
			FAILWITH("TODO: unaop_index");
		} break;
		case unaop_slice: {
			struct expression *base = exp->as.slice.exp;
			struct expression *idx =  exp->as.slice.idx;
			struct expression *len =  exp->as.slice.len;
			switch (dst.tag) {
			case DST_RET:
			case DST_VAL: {
				int base_reg = ir_proc_new_reg(tl, proc_id);
				int index_reg = ir_proc_new_reg(tl, proc_id);
				int length_reg = ir_proc_new_reg(tl, proc_id);
				struct type *base_type = base->type;
				if (type_is_pointer(base->type)) {
					blk = ast_compile_expression(base, DEST_VAL(base_reg), proc_id, blk, scope, tl);
				} else if (type_is_array(base->type)) {
					blk = ast_compile_expression(base, DEST_REF(base_reg), proc_id, blk, scope, tl);
					base_type = POOL_ALLOC(&tl->data, struct type);
					base_type->tag = ast_type_ptr;
					base_type->as.ptr = base->type;
				} else if (type_is_slice(base->type)) {
					int tmp = ir_proc_new_reg(tl, proc_id);
					blk = ast_compile_expression(base, DEST_REF(base_reg), proc_id, blk, scope, tl);
					base_type = type_slice_to_array_ptr(&tl->data, base->type);
					da_append(&blk->code, (IR_Ins){
							.op   = ir_op_load,
							.type = base_type,
							.dst  = tmp,
							.arg.rx[0] = base_reg,
						});
					base_reg = tmp;
				} else {
					FAILWITH("Unreachable");
				}
				blk = ast_compile_expression(idx, DEST_VAL(index_reg), proc_id, blk, scope, tl);
				blk = ast_compile_expression(len, DEST_VAL(length_reg), proc_id, blk, scope, tl);
				int tmp = ir_proc_new_reg(tl, proc_id);
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_getelemptr,
						.type = base_type,
						.dst  = tmp,
						.arg.rx[0] = base_reg,
						.arg.rx[1] = index_reg,
					});
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_conslice,
						.type = exp->type,
						.dst  = dst.reg,
						.arg.rx[0] = tmp,
						.arg.rx[1] = length_reg,
					});
				return blk;
			} break;
			case DST_CPY: {
				int base_reg = ir_proc_new_reg(tl, proc_id);
				int index_reg = ir_proc_new_reg(tl, proc_id);
				int length_reg = ir_proc_new_reg(tl, proc_id);
				struct type *base_type = base->type;
				if (type_is_pointer(base->type)) {
					blk = ast_compile_expression(base, DEST_VAL(base_reg), proc_id, blk, scope, tl);
				} else if (type_is_array(base->type)) {
					blk = ast_compile_expression(base, DEST_REF(base_reg), proc_id, blk, scope, tl);
					base_type = POOL_ALLOC(&tl->data, struct type);
					base_type->tag = ast_type_ptr;
					base_type->as.ptr = base->type;
				} else if (type_is_slice(base->type)) {
					int tmp = ir_proc_new_reg(tl, proc_id);
					blk = ast_compile_expression(base, DEST_REF(base_reg), proc_id, blk, scope, tl);
					base_type = type_slice_to_array_ptr(&tl->data, base->type);
					da_append(&blk->code, (IR_Ins){
							.op   = ir_op_load,
							.type = base_type,
							.dst  = tmp,
							.arg.rx[0] = base_reg,
						});
					base_reg = tmp;
				} else {
					FAILWITH("Unreachable");
				}
				blk = ast_compile_expression(idx, DEST_VAL(index_reg), proc_id, blk, scope, tl);
				blk = ast_compile_expression(len, DEST_VAL(length_reg), proc_id, blk, scope, tl);
				int tmp_reg = ir_proc_new_reg(tl, proc_id);
				int ptr_reg = ir_proc_new_reg(tl, proc_id);
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_getelemptr,
						.type = base_type,
						.dst  = ptr_reg,
						.arg.rx[0] = base_reg,
						.arg.rx[1] = index_reg,
					});
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_conslice,
						.type = exp->type,
						.dst  = tmp_reg,
						.arg.rx[0] = ptr_reg,
						.arg.rx[1] = length_reg,
					});
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_store,
						.type = exp->type,
						.dst  = dst.reg,
						.arg.rx[0] = tmp_reg,
					});
				return blk;
			} break;
			case DST_REF: FAILWITH("Unreachable"); break;
			case DST_NONE: FAILWITH("TODO: DST_NONE"); break;
			default: FAILWITH("Unreachable"); break;
			}
			FAILWITH("TODO: unaop_slice");
		} break;
		case unaop_call: {
			struct call *call = &exp->as.call;
			struct ir_args args = {0};
			int preg = ir_proc_new_reg(tl, proc_id);
			blk = ast_compile_expression(call->proc, DEST_VAL(preg), proc_id, blk, scope, tl);
			/* compile arg exps */
			for (size_t i = 0; i < call->args.len; ++i) {
				struct expression *arg = call->args.elems[i];
				int reg = ir_proc_new_reg(tl, proc_id);
				blk = ast_compile_expression(arg, DEST_VAL(reg), proc_id, blk, scope, tl);
				da_append(&args, reg);
			}
			/* push args */
			for (int64_t i = (int64_t)args.len - 1; i >= 0; --i) {
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_pushfunarg,
						.type = call->args.elems[i]->type,
						.dst  = args.elems[i],
					});
			}
			switch (dst.tag) {
			case DST_RET:
			case DST_VAL: {
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_call,
						.type = exp->type,
						.dst  = dst.reg,
						.arg.rx[0] = preg,
						.arg.rx[1] = args.len,
					});
			} break;
			case DST_CPY: {
				int tmp = ir_proc_new_reg(tl, proc_id);
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_call,
						.type = exp->type,
						.dst  = tmp,
						.arg.rx[0] = preg,
						.arg.rx[1] = args.len,
					});
				da_append(&blk->code, (IR_Ins){
						.op = ir_op_store,
						.type = exp->type,
						.dst = dst.reg,
						.arg.rx[0] = tmp,
					});
			} break;
			case DST_REF: {
				int tmp = ir_proc_new_reg(tl, proc_id);
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_alloca,
						.type = exp->type,
						.dst  = dst.reg,
						.arg.i32 = 1,
					});
				da_append(&blk->code, (IR_Ins){
						.op        = ir_op_call,
						.type      = exp->type,
						.dst       = tmp,
						.arg.rx[0] = preg,
						.arg.rx[1] = args.len,
					});
				da_append(&blk->code, (IR_Ins){
						.op = ir_op_store,
						.type = exp->type,
						.dst = dst.reg,
						.arg.rx[0] = tmp,
					});
			} break;
			case DST_NONE: FAILWITH("TODO: DST_NONE"); break;
			default: FAILWITH("Unreachable"); break;
			}
			da_free(&args);
			return blk;
		} break;
		case unaop_cast: {
			int reg = ir_proc_new_reg(tl, proc_id);
			blk = ast_compile_expression(exp->as.cast.exp, DEST_VAL(reg), proc_id, blk, scope, tl);
			switch (dst.tag) {
			case DST_VAL: {
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_cast,
						.type = exp->as.cast.type,
						.dst  = dst.reg,
						.arg.rx[0] = reg,
					});
				return blk;
			} break;
			case DST_CPY: {
				int tmp = ir_proc_new_reg(tl, proc_id);
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_cast,
						.type = exp->as.cast.type,
						.dst  = tmp,
						.arg.rx[0] = reg,
					});
				da_append(&blk->code, (IR_Ins){
						.op = ir_op_store,
						.type = exp->type,
						.dst = dst.reg,
						.arg.rx[0] = tmp,
					});
				return blk;
			} break;
			case DST_REF: FAILWITH("TODO: unaop_cast"); break;
			case DST_NONE: FAILWITH("TODO: unaop_cast"); break;
			case DST_RET: FAILWITH("TODO: ast_exp_literal"); break;
			default: FAILWITH("Unreachable"); break;
			}
			FAILWITH("TODO: unaop_cast");
		} break;
		default: FAILWITH("Unreachable"); break;
		}
		FAILWITH("TODO: ast_exp_unary");
	} break;
	case ast_exp_if: {
		struct exp_if *iff = &exp->as.iff;
		int cond_reg = ir_proc_new_reg(tl, proc_id);
		blk = ast_compile_expression(iff->cond, DEST_VAL(cond_reg), proc_id, blk, scope, tl);
		struct ir_blk *join = POOL_ALLOC(&tl->data, struct ir_blk);
		struct ir_blk *tblk = POOL_ALLOC(&tl->data, struct ir_blk);
		blk->term.op = ir_op_if;
		da_append(&blk->term.args, cond_reg);
		struct ir_blk *tb_end;
		struct ast_comp_dest tdst, fdst;
		switch (dst.tag) {
		case DST_RET:
			tdst = DEST_RET(ir_proc_new_reg(tl, proc_id));
			break;
		case DST_VAL:
			tdst = DEST_VAL(ir_proc_new_reg(tl, proc_id));
			break;
		case DST_REF:
			tdst = DEST_REF(ir_proc_new_reg(tl, proc_id));
			break;
		case DST_NONE:
			tdst = DEST_NONE(ir_proc_new_reg(tl, proc_id)); break;
		case DST_CPY:
			tdst = dst;
			break;
		default: FAILWITH("Unreachable"); break;
		}
		tb_end = ast_compile_expression(iff->tb, tdst, proc_id, tblk, scope, tl);
		/* true branch term */
		blk->term.b0 = tblk;
		tb_end->term.op = ir_op_goto;
		da_append(&tb_end->term.args, tdst.reg);
		tb_end->term.b0 = join;
		if (iff->fb) { /* if false branch exists */
			struct ir_blk *fblk = POOL_ALLOC(&tl->data, struct ir_blk);
			struct ir_blk *fb_end;
			switch (dst.tag) {
			case DST_RET:
				fdst = DEST_RET(ir_proc_new_reg(tl, proc_id));
				break;
			case DST_VAL:
				fdst = DEST_VAL(ir_proc_new_reg(tl, proc_id));
				break;
			case DST_REF:
				fdst = DEST_REF(ir_proc_new_reg(tl, proc_id));
				break;
			case DST_NONE:
				fdst = DEST_NONE(ir_proc_new_reg(tl, proc_id));
				break;
			case DST_CPY:
				fdst = dst;
				break;
			default: FAILWITH("Unreachable"); break;
			}
			fb_end = ast_compile_expression(iff->fb, fdst, proc_id, fblk, scope, tl);
			/* false branch term */
			blk->term.b1 = fblk;
			fb_end->term.op = ir_op_goto;
			da_append(&fb_end->term.args, fdst.reg);
			fb_end->term.b0 = join;
		} else {
			blk->term.b1 = join;
		}
		blk->term.j0 = join;
		if (dst.tag == DST_CPY) {
			da_append(&join->args, ir_proc_new_reg(tl, proc_id));
		} else {
			da_append(&join->args, dst.reg);
		}
		return join;
	} break;
	case ast_exp_case: {
		struct exp_case *cc = &exp->as.ccase;
		int val_reg = ir_proc_new_reg(tl, proc_id);
		blk = ast_compile_expression(cc->cexp, DEST_REF(val_reg), proc_id, blk, scope, tl);
		struct ir_blk *join = POOL_ALLOC(&tl->data, struct ir_blk);
		for (size_t i = 0; i < cc->branches.len; ++i) {
			struct case_branch *branch = &cc->branches.elems[i];
			int tag_reg = ir_proc_new_reg(tl, proc_id);
			{ /* load tag value */
				int idx_reg = ir_proc_new_reg(tl, proc_id);
				int tmp_reg = ir_proc_new_reg(tl, proc_id);
				da_append(&blk->code, (IR_Ins){
						.op		 = ir_op_loadimm,
						.type	 = &AST_TYPE_U64,
						.dst	 = idx_reg,
						.arg.u32 = 0,
					});
				da_append(&blk->code, (IR_Ins){
						.op		   = ir_op_getelemptr,
						.type	   = cc->cexp->type,
						.dst	   = tmp_reg,
						.arg.rx[0] = val_reg,
						.arg.rx[1] = idx_reg,
					});
				da_append(&blk->code, (IR_Ins){
						.op        = ir_op_load,
						.type      = &AST_TYPE_UNION_TAG,
						.dst       = tag_reg,
						.arg.rx[0] = tmp_reg,
					});
			}
			int data_reg = ir_proc_new_reg(tl, proc_id);
			{ /* get data ptr */
				int idx_reg = ir_proc_new_reg(tl, proc_id);
				da_append(&blk->code, (IR_Ins){
						.op		 = ir_op_loadimm,
						.type	 = &AST_TYPE_U64,
						.dst	 = idx_reg,
						.arg.u32 = 1,
					});
				da_append(&blk->code, (IR_Ins){
						.op		   = ir_op_getelemptr,
						.type	   = cc->cexp->type,
						.dst	   = data_reg,
						.arg.rx[0] = val_reg,
						.arg.rx[1] = idx_reg,
					});
			}
			{ /* check tag */
				int tmp_reg = ir_proc_new_reg(tl, proc_id);
				int res_reg = ir_proc_new_reg(tl, proc_id);
				da_append(&blk->code, (IR_Ins){
						.op		 = ir_op_loadimm,
						.type	 = &AST_TYPE_UNION_TAG,
						.dst	 = tmp_reg,
						.arg.u32 = branch->tag_val,
					});
				da_append(&blk->code, (IR_Ins){
						.op		   = ir_op_cmpe,
						.type	   = &AST_TYPE_UNION_TAG,
						.dst	   = res_reg,
						.arg.rx[0] = tag_reg,
						.arg.rx[1] = tmp_reg,
					});
				blk->term.op = ir_op_if;
				da_append(&blk->term.args, res_reg);
			}
			struct ir_blk *tblk = POOL_ALLOC(&tl->data, struct ir_blk);
			struct ir_blk *fblk;
			if (i == cc->branches.len - 1 && cc->else_exp == NULL) {
				fblk = join;
			} else {
				fblk = POOL_ALLOC(&tl->data, struct ir_blk);
			}
			/* create binding */
			if (branch->binds_value) {
				struct definition *def = &branch->binding;
				int var_reg = ir_proc_new_reg(tl, proc_id);
				def->ir_symbol = var_reg;
				da_append(&tblk->code, (IR_Ins){
						.op   = ir_op_alloca,
						.type = def->type,
						.dst  = var_reg,
						.arg.i32 = 1,
					});
				if (branch->binding_is_ref) {
					da_append(&tblk->code, (IR_Ins){
							.op		   = ir_op_store,
							.type	   = def->type,
							.dst	   = var_reg,
							.arg.rx[0] = data_reg,
						});
				} else {
					da_append(&tblk->code, (IR_Ins){
							.op		   = ir_op_copy,
							.type	   = def->type,
							.dst	   = var_reg,
							.arg.rx[0] = data_reg,
						});
				}
			}
			if (branch->guard != NULL) {
				FAILWITH("TODO: compile branch guard.");
			}
			struct ast_comp_dest res_dst;
			switch (dst.tag) {
			case DST_RET:
				res_dst = DEST_RET(ir_proc_new_reg(tl, proc_id));
				break;
			case DST_VAL:
				res_dst = DEST_VAL(ir_proc_new_reg(tl, proc_id));
				break;
			case DST_REF:
				res_dst = DEST_REF(ir_proc_new_reg(tl, proc_id));
				break;
			case DST_NONE:
				res_dst = DEST_NONE(ir_proc_new_reg(tl, proc_id));
				break;
			case DST_CPY:
				res_dst = dst;
				break;
			default: FAILWITH("Unreachable"); break;
			}
			struct ir_blk *tb_end = ast_compile_expression(branch->body, res_dst,
														   proc_id, tblk, &branch->scope, tl);
			tb_end->term.op = ir_op_goto;
			da_append(&tb_end->term.args, res_dst.reg);
			tb_end->term.b0 = join;
			blk->term.b0 = tblk;
			blk->term.b1 = fblk;
			blk->term.j0 = join;
			blk = fblk;
		}
		if (cc->else_exp != NULL) {
			FAILWITH("TODO: compile `else` branch.");
		}
		if (dst.tag == DST_CPY) {
			da_append(&join->args, ir_proc_new_reg(tl, proc_id));
		} else {
			da_append(&join->args, dst.reg);
		}
		return join;
	} break;
	case ast_exp_while: {
		struct ir_blk *cond = POOL_ALLOC(&tl->data, struct ir_blk);
		struct ir_blk *body = POOL_ALLOC(&tl->data, struct ir_blk);
		struct ir_blk *join = POOL_ALLOC(&tl->data, struct ir_blk);
		/* jump to loop condition */
		blk->term.op = ir_op_goto;
		blk->term.b0 = cond;
		int cond_reg = ir_proc_new_reg(tl, proc_id);
		cond = ast_compile_expression(exp->as.wloop.cond, DEST_VAL(cond_reg), proc_id, cond, scope, tl);
		cond->term.op = ir_op_if;
		da_append(&cond->term.args, cond_reg);
		cond->term.b0 = body;
		cond->term.b1 = join;
		cond->term.j0 = join;
		body = ast_compile_expression(exp->as.wloop.body, DEST_NONE(), proc_id, body, scope, tl);
		body->term.op = ir_op_goto;
		body->term.b0 = cond;
		return join;
	} break;
	case ast_exp_return: FAILWITH("TODO: ast_exp_return"); break;
	case ast_exp_break: FAILWITH("TODO: ast_exp_break"); break;
	case ast_exp_continue: FAILWITH("TODO: ast_exp_continue"); break;
	case ast_exp_definition: FAILWITH("TODO: ast_exp_definition"); break;
	}
	FAILWITH("Unreachable");
	return NULL;
}

char *generate_mangled_name(struct strview ident, struct type *type)
{
	char *ptr;
	size_t sz;
	FILE *f = open_memstream(&ptr, &sz);
	assert(f != NULL);
	fprintf(f, SV_FMT"#", SV_ARGS(ident));
	ast_type_fprint(type, f);
	fclose(f);
	return ptr;
}

static void ast_create_proc_object(union ir_object *p, struct definition *def)
{
	p->proc.tag = IRO_PROC;
	struct strview id_name = token_to_strview(def->id);
	if (sv_is_equal(id_name, sv_of_cstr("main"))) {
		/* entry point */
		p->proc.link = strndup(id_name.ptr, id_name.len);
		p->proc.is_static = false;
	} else {
		p->proc.link = generate_mangled_name(id_name, def->type);
		p->proc.is_static = true;
	}
	p->proc.type = def->type;
	p->proc.def  = def;
	p->proc.node = &def->exp->as.proc;
	p->proc.regc = 0;
}

struct ir_toplevel ast_compile(struct scope *scope)
{
	struct symtbl *st = &scope->symtbl;
	struct ir_toplevel tl = {0};
	for (size_t i = 0; i < st->len; ++i) {
		if (st->elems[i].tag == SYMTBL_VALCONS) continue;
		assert(st->elems[i].tag == SYMTBL_VAR);
		struct definition *def = st->elems[i].as.variable.def;
		struct expression *exp = def->exp;
		union ir_object p = {0};
		struct strview id_name = token_to_strview(def->id);
		def->ir_symbol = tl.len;
		def->is_global = true;
		if (exp->tag == ast_exp_procedure_literal) {
			if (def->is_polymorphic) {
#if KC_DEBUG
				printf("[Debug] Polymorphic Definition: "SV_FMT" spec count = %u\n",
					   SV_ARGS(id_name), def->specs.len);
#endif
				for (size_t i = 0; i < def->specs.len; ++i) {
					struct definition *spec_def = &def->specs.elems[i];
					assert(!spec_def->is_polymorphic);
					ast_create_proc_object(&p, spec_def);
					spec_def->ir_symbol = tl.len;
					da_append(&tl, p);
				}
			} else {
				ast_create_proc_object(&p, def);
				da_append(&tl, p);
			}
		} else if (exp->tag == ast_exp_extern_symbol) {
			if (type_is_procedure(def->type)) {
				p.tag = IRO_EXTERN_PROC;
			} else {
				p.tag = IRO_EXTERN_DATA;
			}
			struct strview name = token_to_strview(exp->tok);
			p.hddr.link = strndup(name.ptr, name.len);
			da_append(&tl, p);
		} else if (exp->tag == ast_exp_literal) {
			p.tag = IRO_DATA;
			p.hddr.is_static = true;
			p.hddr.link = generate_mangled_name(id_name, def->type);
			switch ((int)exp->as.lit.token->tt) {
			case tt_intlit:
				p.data.size = type_size(def->type);
				p.data.type = def->type;
				p.data.dat = malloc(p.data.size);
				assert(p.data.dat != NULL);
				memcpy(p.data.dat, &exp->as.lit.as.i, p.data.size);
				break;
			default:
				FAILWITH("TODO: tt == %s", token_type_to_str(exp->as.lit.token->tt));
				break;
			}
			da_append(&tl, p);
		} else {
			FAILWITH("TODO: compile toplevel definitions.");
		}
	}
	for (size_t i = 0; i < tl.len; ++i) {
		if (tl.elems[i].tag == IRO_PROC) {
			struct strview id_name = token_to_strview(tl.elems[i].proc.def->id);
#if KC_DEBUG
			printf("[Debug] Compiling proc: "SV_FMT" : ", SV_ARGS(id_name));
			ast_type_fprint(tl.elems[i].proc.def->type, stdout);
			printf("\n");
#endif
			ast_compile_procedure(i, &tl);
		}
	}
	return tl;
}

void ir_ins_fprint(IR_Ins *ins, FILE *file)
{
	fputc('\t', file);
	char *op = NULL;
	switch (ins->op) {
	case ir_op_nop:
		fputs("nop", file);
		break;
	case ir_op_undefined:
		fprintf(file, "%%%d := undefined", ins->dst);
		break;
	case ir_op_mov:
		fprintf(file, "%%%d := %%%d", ins->dst, ins->arg.rx[0]);
		break;
	case ir_op_copy:
		fprintf(file, "copy<");
		ast_type_fprint(ins->type, file);
		fprintf(file, "> %%%d, %%%d", ins->dst, ins->arg.rx[0]);
		break;
	case ir_op_add:	   op = "add";	   goto LBL_BINOP;
	case ir_op_sub:	   op = "sub";	   goto LBL_BINOP;
	case ir_op_mul:	   op = "mul";	   goto LBL_BINOP;
	case ir_op_div:	   op = "div";	   goto LBL_BINOP;
	case ir_op_mod:	   op = "mod";	   goto LBL_BINOP;
	case ir_op_lnot:   op = "lnot";	   goto LBL_BINOP;
	case ir_op_land:   op = "land";	   goto LBL_BINOP;
	case ir_op_lor:	   op = "lor";	   goto LBL_BINOP;
	case ir_op_xor:	   op = "xor";	   goto LBL_BINOP;
	case ir_op_shl:	   op = "shl";	   goto LBL_BINOP;
	case ir_op_shr:	   op = "shr";	   goto LBL_BINOP;
	case ir_op_cmpe:  op = "cmp e ";  goto LBL_BINOP;
	case ir_op_cmpne: op = "cmp ne "; goto LBL_BINOP;
	case ir_op_cmpl:  op = "cmp l ";  goto LBL_BINOP;
	case ir_op_cmpg:  op = "cmp g ";  goto LBL_BINOP;
	case ir_op_cmple: op = "cmp le "; goto LBL_BINOP;
	case ir_op_cmpge: op = "cmp ge "; goto LBL_BINOP;
	LBL_BINOP:
		fprintf(file, "%%%d := %s<", ins->dst, op);
		ast_type_fprint(ins->type, file);
		fprintf(file, "> %%%d, %%%d", ins->arg.rx[0], ins->arg.rx[1]);
		break;
	case ir_op_neg: {
		fprintf(file, "%%%d := neg<", ins->dst);
		ast_type_fprint(ins->type, file);
		fprintf(file, "> %%%d", ins->arg.rx[0]);
	} break;
	case ir_op_cast: {
		fprintf(file, "%%%d := cast<", ins->dst);
		ast_type_fprint(ins->type, file);
		fprintf(file, "> %%%d", ins->arg.rx[0]);
	} break;
	case ir_op_retval: {
		fprintf(file, "%%%d := retval<", ins->dst);
		ast_type_fprint(ins->type, file);
		fputc('>', file);
	} break;
	case ir_op_conslice: {
		fprintf(file, "%%%d := conslice<", ins->dst);
		ast_type_fprint(ins->type, file);
		fprintf(file, "> %%%d, %%%d", ins->arg.rx[0], ins->arg.rx[1]);
	} break;
	case ir_op_alloca: {
		fprintf(file, "%%%d := alloca<", ins->dst);
		ast_type_fprint(ins->type, file);
		fprintf(file, "> %d", ins->arg.u32);
	} break;
	case ir_op_loadglobl: {
		fprintf(file, "%%%d := load_gobl<", ins->dst);
		ast_type_fprint(ins->type, file);
		fprintf(file, "> %d", ins->arg.i32);
	} break;
	case ir_op_loadconst: FAILWITH("TODO: ir_op_loadconst"); break;
	case ir_op_loadimm: {
		fprintf(file, "%%%d := load_imm<", ins->dst);
		ast_type_fprint(ins->type, file);
		fprintf(file, "> %d", ins->arg.i32);
	} break;
	case ir_op_load: {
		fprintf(file, "%%%d := load<", ins->dst);
		ast_type_fprint(ins->type, file);
		fprintf(file, "> %%%d", ins->arg.rx[0]);
	} break;
	case ir_op_store: {
		fputs("store<", file);
		ast_type_fprint(ins->type, file);
		fprintf(file, "> %%%d, %%%d", ins->dst, ins->arg.rx[0]);
	} break;
	case ir_op_getelemptr: {
		fprintf(file, "%%%d := getelemptr<", ins->dst);
		ast_type_fprint(ins->type, file);
		fprintf(file, "> %%%d, %%%d", ins->arg.rx[0], ins->arg.rx[1]);
	} break;
	case ir_op_memzero: {
		fprintf(file, "memzero<");
		ast_type_fprint(ins->type, file);
		fprintf(file, "> %%%d", ins->dst);
	} break;
	case ir_op_pushfunarg: {
		fprintf(file, "pushfunarg<");
		ast_type_fprint(ins->type, file);
		fprintf(file, "> %%%d", ins->dst);
	} break;
	case ir_op_call: {
		fprintf(file, "%%%d := call<", ins->dst);
		ast_type_fprint(ins->type, file);
		fprintf(file, "> %%%d, %d", ins->arg.rx[0], ins->arg.rx[1]);
	} break;
	case IR_OPCODE_COUNT:
	default: FAILWITH("Unreachable"); break;
	}
	fputc('\n', file);
}

bool da_ptr_member_p(void *p, struct da_pointers *ptrs)
{
	da_foreach(ptr, ptrs) {
		if (p == *ptr) return true;
	}
	return false;
}

static void dfs_walk(struct ir_blk *blk, struct da_pointers *order, struct da_pointers *visited)
{
	if (da_ptr_member_p(blk, visited)) return;
	da_append(visited, blk);
	switch (blk->term.op) {
	case ir_op_if:
		/* traverse the false edge first */
		dfs_walk(blk->term.b1, order, visited);
		dfs_walk(blk->term.b0, order, visited);
		break;
	case ir_op_goto:
		dfs_walk(blk->term.b0, order, visited);
		break;
		/* no successors */
	case ir_op_ret:		 break;
	case ir_op_tailcall: break;
	default: FAILWITH("Unreachable"); break;
	}
	da_append(order, blk);
}

struct da_pointers ir_blk_post_order(struct ir_blk *root)
{
	struct da_pointers visited = {0};
	struct da_pointers order = {0};
	dfs_walk(root, &order, &visited);
	da_free(&visited);
	return order;
}

struct da_pointers ir_blk_reverse_post_order(struct ir_blk *root)
{
	struct da_pointers order = ir_blk_post_order(root);
	da_reverse(&order);
	return order;
}

void ir_blk_fprint(struct ir_blk *blk, FILE *file)
{
	fprintf(file, "@%p(", blk);
	if (blk->args.len) {
		fprintf(file, "%%%d", blk->args.elems[0]);
		for (size_t i = 1; i < blk->args.len; ++i) {
			fprintf(file, ", %%%d", blk->args.elems[i]);
		}
	}
	fputs("):\n", file);
	da_foreach(ins, &blk->code) {
		ir_ins_fprint(ins, file);
	}
	struct ir_blk_terminal *term = &blk->term;
	switch (term->op) {
	case ir_op_ret:
		fputs("\tret ", file);
		if (term->args.len) {
			fprintf(file, "%%%d", term->args.elems[0]);
			for (size_t i = 1; i < term->args.len; ++i) {
				fprintf(file, ", %%%d", term->args.elems[i]);
			}
		}
		fputc('\n', file);
		break;
	case ir_op_goto:{
		fprintf(file, "\tgoto @%p(", term->b0);
		if (term->args.len) {
			fprintf(file, "%%%d", term->args.elems[0]);
			for (size_t i = 1; i < term->args.len; ++i) {
				fprintf(file, ", %%%d", term->args.elems[i]);
			}
		}
		fputs(")\n", file);
	} break;
	case ir_op_if: {
		assert(term->args.len == 1);
		fprintf(file, "\tif %%%d then goto @%p else goto @%p\n",
				term->args.elems[0], term->b0, term->b1);
	} break;
	case ir_op_tailcall: FAILWITH("TODO: ir_op_tailcall"); break;
	default: FAILWITH("Unreachable"); break;
	}
}

void ir_proc_fprint(struct ir_proc *proc, FILE *file)
{
	fputs("procedure @", file);
	if (proc->def) {
		struct strview id_name = token_to_strview(proc->def->id);
		fprintf(file, SV_FMT"[%zu]", SV_ARGS(id_name), proc->def->ir_symbol);
	} else {
		fprintf(file, "%p", proc);
	}
	fprintf(file, "(argc=%d, retc=%d, regc=%d) ;; ", proc->argc, proc->retc, proc->regc);
	ast_type_fprint(proc->type, file);
	fputs("\n{\n", file);
	struct da_pointers blks = ir_blk_reverse_post_order(proc->entry);
	da_foreach(blk, &blks) {
		ir_blk_fprint(*blk, file);
	}
	da_free(&blks);
	fputc('\n', file);
	fputs("}\n", file);
}

/* translate ir to x64 gnu asm */
const enum asm_register asm_reg_alloc_ord[ASM_REG_COUNT] = {
	asm_reg_rdi,
	asm_reg_rsi,
	asm_reg_rcx,
	asm_reg_rax,
	asm_reg_rdx,
	asm_reg_r8,
	asm_reg_r9,
	asm_reg_r10,
	asm_reg_r11,
	asm_reg_rbx,
	asm_reg_r12,
	asm_reg_r13,
	asm_reg_r14,
	asm_reg_r15,
};

const enum asm_register asm_arg_regs[ASM_ARG_REG_COUNT] = {
	asm_reg_rdi,
	asm_reg_rsi,
	asm_reg_rdx,
	asm_reg_rcx,
	asm_reg_r8,
	asm_reg_r9,
};

const enum asm_register asm_ret_regs[] = {
	asm_reg_rax,
	asm_reg_rdx,
};

const enum asm_register asm_caller_save_regs[] = {
	asm_reg_rax,
	asm_reg_rdi,
	asm_reg_rsi,
	asm_reg_rdx,
	asm_reg_rcx,
	asm_reg_r8,
	asm_reg_r9,
	asm_reg_r10,
	asm_reg_r11,
};

const enum asm_register asm_callee_save_regs[] = {
	asm_reg_rbx,
	asm_reg_r12,
	asm_reg_r13,
	asm_reg_r14,
	asm_reg_r15,
};

static const char *asm_reg_b_name[ASM_REG_COUNT] = {
	[asm_reg_rax] = "%al",
	[asm_reg_rdx] = "%dl",
	[asm_reg_rdi] = "%dil",
	[asm_reg_rsi] = "%sil",
	[asm_reg_rcx] = "%cl",
	[asm_reg_r8]  = "%r8b",
	[asm_reg_r9]  = "%r9b",
	[asm_reg_r10] = "%r10b",
	[asm_reg_r11] = "%r11b",
	[asm_reg_rbx] = "%bl",
	[asm_reg_r12] = "%r12b",
	[asm_reg_r13] = "%r13b",
	[asm_reg_r14] = "%r14b",
	[asm_reg_r15] = "%r15b",
};

static const char *asm_reg_w_name[ASM_REG_COUNT] = {
	[asm_reg_rax] = "%ax",
	[asm_reg_rdx] = "%dx",
	[asm_reg_rdi] = "%di",
	[asm_reg_rsi] = "%si",
	[asm_reg_rcx] = "%cx",
	[asm_reg_r8]  = "%r8w",
	[asm_reg_r9]  = "%r9w",
	[asm_reg_r10] = "%r10w",
	[asm_reg_r11] = "%r11w",
	[asm_reg_rbx] = "%bx",
	[asm_reg_r12] = "%r12w",
	[asm_reg_r13] = "%r13w",
	[asm_reg_r14] = "%r14w",
	[asm_reg_r15] = "%r15w"
};

static char *asm_reg_d_name[ASM_REG_COUNT] = {
	[asm_reg_rax] = "%eax",
	[asm_reg_rdx] = "%edx",
	[asm_reg_rdi] = "%edi",
	[asm_reg_rsi] = "%esi",
	[asm_reg_rcx] = "%ecx",
	[asm_reg_r8]  = "%r8d",
	[asm_reg_r9]  = "%r9d",
	[asm_reg_r10] = "%r10d",
	[asm_reg_r11] = "%r11d",
	[asm_reg_rbx] = "%ebx",
	[asm_reg_r12] = "%r12d",
	[asm_reg_r13] = "%r13d",
	[asm_reg_r14] = "%r14d",
	[asm_reg_r15] = "%r15d",
};

static const char *asm_reg_q_name[ASM_REG_COUNT] = {
	[asm_reg_rax] = "%rax",
	[asm_reg_rdx] = "%rdx",
	[asm_reg_rdi] = "%rdi",
	[asm_reg_rsi] = "%rsi",
	[asm_reg_rcx] = "%rcx",
	[asm_reg_r8]  = "%r8",
	[asm_reg_r9]  = "%r9",
	[asm_reg_r10] = "%r10",
	[asm_reg_r11] = "%r11",
	[asm_reg_rbx] = "%rbx",
	[asm_reg_r12] = "%r12",
	[asm_reg_r13] = "%r13",
	[asm_reg_r14] = "%r14",
	[asm_reg_r15] = "%r15",
};

const char *asm_addr_tag_to_str(enum asm_addr_tag tag)
{
	switch (tag) {
	case ADDR_NONE:		  return "ADDR_NONE";
	case ADDR_ARGUMENT:	  return "ADDR_ARGUMENT";
	case ADDR_WIDE:	      return "ADDR_WIDE";
	case ADDR_WIDE_ARG:	  return "ADDR_WIDE_ARG";
	case ADDR_TEMP_WIDE:  return "ADDR_TEMP_WIDE";
	case ADDR_STACK_ARG:  return "ADDR_STACK_ARG";
	case ADDR_PUSH_ARG:	  return "ADDR_PUSH_ARG";
	case ADDR_REGISTER:	  return "ADDR_REGISTER";
	case ADDR_TEMP_REG:	  return "ADDR_TEMP_REG";
	case ADDR_IMM_INT:	  return "ADDR_IMM_INT";
	case ADDR_IMM_FLOAT:  return "ADDR_IMM_FLOAT";
	case ADDR_STACK:	  return "ADDR_STACK";
	case ADDR_STACK_LOAD: return "ADDR_STACK_LOAD";
	case ADDR_BLK_ARG:	  return "ADDR_BLK_ARG";
	case ADDR_SYMBOL:	  return "ADDR_SYMBOL";
	case ADDR_FLAGS:	  return "ADDR_FLAGS";
	}
	FAILWITH("Unreachable");
	return NULL;
}

static int count_set_bits(uint64_t n)
{
	int c;
	for (c = 0; n; ++c) n &= n - 1;
	return c;
}

static uintptr_t align_adjust(uintptr_t x, uintptr_t alignment)
{ /* alignment should be a non-zero power of two */
	if (alignment == 1) return x;
	uintptr_t mask = alignment - 1;
	return x & mask ? (x & ~mask) + alignment : x;
}

static void mem_copy_segment_count(size_t sz, uint32_t counts[4])
{
	counts[0] = sz / 8;	// 64 bits
	sz %= 8;
	counts[1] = sz / 4; // 32 bits
	sz %= 4;
	counts[2] = sz / 2; // 16 bits
	sz %= 2;
	counts[3] = sz;     // 8 bits
}

const char *asm_reg_name(enum asm_register reg, struct type *type)
{
	assert(type_is_floating_point(type) == false);
	assert(reg >= 0 && reg < ASM_REG_COUNT);
	size_t sz = type_size(type);
	switch (sz) {
	case 1: return asm_reg_b_name[reg];
	case 2: return asm_reg_w_name[reg];
	case 4: return asm_reg_d_name[reg];
	case 8: return asm_reg_q_name[reg];
	}
	printf("type = ");
	ast_type_fprint(type, stdout);
	printf("\n");
	printf("size = %zu\n", sz);
	FAILWITH("TODO: invalid register size.");
	return NULL;
}

bool asm_is_caller_save(enum asm_register reg)
{
	for (size_t i = 0; i < ARRAY_LENGTH(asm_caller_save_regs); ++i) {
		if (reg == asm_caller_save_regs[i]) return true;
	}
	return false;
}

int asm_suffix(struct type *type)
{
	assert(type_is_floating_point(type) == false);
	switch (type_size(type)) {
	case 1: return 'b';
	case 2: return 'w';
	case 4: return 'l';
	case 8: return 'q';
	}
	FAILWITH("TODO: invalid size.");
	return 0;
}

void asm_context_first_pass(struct ir_blk *blk, struct asm_context *ctx, struct ir_toplevel *tl)
{
	da_foreach(x, &blk->args) {
		if (ctx->vars[*x].tag == ADDR_NONE) {
			ctx->vars[*x].tag = ADDR_TEMP_REG;
		}
	}
	for (size_t i = 0; i < blk->code.len; ++i) {
		IR_Ins *ins = &blk->code.elems[i];
		switch (ins->op) {
		case ir_op_nop: break;
		case ir_op_mov: {
			struct asm_address *addr = &ctx->vars[ins->dst];
			assert(addr->tag == ADDR_NONE);
			addr->tag = ADDR_TEMP_REG;
			addr->type = ctx->vars[ins->arg.rx[0]].type;
		} break;
		case ir_op_undefined: {
			struct asm_address *addr = &ctx->vars[ins->dst];
			assert(addr->tag == ADDR_NONE);
			addr->tag = ADDR_TEMP_REG;
			addr->type = &AST_TYPE_VOID;
		} break;
		case ir_op_add:
		case ir_op_sub:
		case ir_op_mul:
		case ir_op_div:
		case ir_op_mod:
		case ir_op_land:
		case ir_op_lor:
		case ir_op_xor:
		case ir_op_shl:
		case ir_op_shr:
		case ir_op_lnot:
		case ir_op_neg:  {
			struct asm_address *addr = &ctx->vars[ins->dst];
			assert(addr->tag == ADDR_NONE);
			addr->tag = ADDR_TEMP_REG;
			addr->type = ins->type;
		} break;
		case ir_op_cmpe:
		case ir_op_cmpne:
		case ir_op_cmpl:
		case ir_op_cmpg:
		case ir_op_cmple:
		case ir_op_cmpge: {
			struct asm_address *addr = &ctx->vars[ins->dst];
			assert(addr->tag == ADDR_NONE);
			addr->tag = ADDR_FLAGS;
			addr->as.i = ins->op;
			addr->type = ins->type;
		} break;
		case ir_op_cast: {
			struct asm_address *addr = &ctx->vars[ins->dst];
			assert(addr->tag == ADDR_NONE);
			addr->tag = ADDR_TEMP_REG;
			addr->type = ins->type;
		} break;
		case ir_op_getelemptr: {
			struct asm_address *dst = &ctx->vars[ins->dst];
			assert(dst->tag == ADDR_NONE);
			dst->tag = ADDR_TEMP_REG;
			dst->type = ins->type;
		} break;
		case ir_op_retval: {
			struct asm_address *addr = &ctx->vars[ins->dst];
			assert(addr->tag == ADDR_NONE);
			ASSERT(type_size(ins->type) > 16,
				   "(ir_op_retval) dst = %%%d\n"
				   "(ir_op_retval) blk = %p",
				   ins->dst, blk);
			if (ctx->has_large_retval) {
				*addr = ctx->vars[ctx->rv_addr];
			} else {
				ctx->has_large_retval = true;
				ctx->rv_addr = ins->dst;
				addr->tag = ADDR_STACK_LOAD;
				size_t before = ctx->stack_size;
				ctx->stack_size = align_adjust(ctx->stack_size, 8);
				ctx->stack_size += 8;
				addr->as.stack[0] = -ctx->stack_size;
				addr->as.stack[1] = 0;
				size_t after = ctx->stack_size;
				addr->extra.stack_size = after - before;
				addr->type = POOL_ALLOC(&tl->data, struct type);
				addr->type->tag = ast_type_ptr;
				addr->type->as.ptr = ins->type;
			}
		} break;
		case ir_op_conslice: {
			struct asm_address *addr = &ctx->vars[ins->dst];
			assert(addr->tag == ADDR_NONE);
			addr->tag = ADDR_TEMP_WIDE;
			addr->type = ins->type;
		} break;
		case ir_op_alloca: {
			struct asm_address *addr = &ctx->vars[ins->dst];
			assert(addr->tag == ADDR_NONE);
			addr->tag = ADDR_STACK;
			int sz = type_size(ins->type);
			size_t before = ctx->stack_size;
			ctx->stack_size = align_adjust(ctx->stack_size, type_alignment(ins->type));
			ctx->stack_size += sz * ins->arg.i32;
			addr->as.i = -ctx->stack_size;
			size_t after = ctx->stack_size;
			addr->extra.stack_size = after - before;
			addr->type = POOL_ALLOC(&tl->data, struct type);
			addr->type->tag = ast_type_ptr;
			addr->type->as.ptr = ins->type;
		} break;
		case ir_op_load: {
			struct asm_address *dst = &ctx->vars[ins->dst];
			struct asm_address *ptr = &ctx->vars[ins->arg.rx[0]];
			assert(dst->tag == ADDR_NONE);
			if (ptr->tag == ADDR_STACK) {
				dst->tag = ADDR_STACK_LOAD;
				dst->as.stack[0] = ptr->as.i;
				dst->type = ins->type;
			} else {
				size_t ts = type_size(ins->type);
				if (ts <= 8) {
					dst->tag = ADDR_TEMP_REG;
				} else if (ts <= 16) {
					dst->tag = ADDR_TEMP_WIDE;
				} else {
					printf("blk = %p\n", blk);
					printf("ins->dst = %%%d\n", ins->dst);
					FAILWITH("Invalid load. Size cannot fit into register.");
				}
			}
			dst->type = ins->type;
		} break;
		case ir_op_loadglobl: {
			struct asm_address *addr = &ctx->vars[ins->dst];
			assert(addr->tag == ADDR_NONE);
			addr->tag = ADDR_SYMBOL;
			addr->as.i = ins->arg.i32;
			addr->type = ins->type;
		} break;
		case ir_op_loadconst: FAILWITH("TODO: ir_op_loadconst"); break;
		case ir_op_loadimm: {
			struct asm_address *addr = &ctx->vars[ins->dst];
			assert(addr->tag == ADDR_NONE);
			addr->tag = ADDR_IMM_INT;
			addr->as.i = ins->arg.i32;
			addr->type = ins->type;
		} break;
		case ir_op_copy: /* do nothing */ break;
		case ir_op_store: {
			struct asm_address *dst = &ctx->vars[ins->dst];
			struct asm_address *src = &ctx->vars[ins->arg.rx[0]];
			if (dst->tag == ADDR_STACK && src->tag == ADDR_STACK_ARG) {
				ctx->stack_size -= dst->extra.stack_size;
				dst->as.i = src->as.i;
				dst->extra.stack_size = 0;
			}
		} break;
		case ir_op_memzero: /* do nothing */ break;
		case ir_op_pushfunarg: {
			struct asm_address *dst = &ctx->vars[ins->dst];
			assert(dst->tag != ADDR_NONE);
			if (dst->type == NULL) {
				printf("ins->dst = %%%d\n", ins->dst);
			}
			da_append(&ctx->funargs, dst);
		} break;
		case ir_op_call: {
			ctx->is_leaf = false;
			struct asm_address *proc = &ctx->vars[ins->arg.rx[0]];
			assert(proc->tag != ADDR_NONE);
			size_t ts = type_size(ins->type);
			struct asm_address *ret = &ctx->vars[ins->dst];
			ret->type = ins->type;
			/* return register */
			int arg_reg = 0;
			assert(ret->tag == ADDR_NONE);
			if (ts <= 8) {
				ret->tag = ADDR_REGISTER;
				ret->as.i = asm_ret_regs[0];
			} else if (ts <= 16) {
				ret->tag = ADDR_WIDE;
				ret->as.wide[0] = asm_ret_regs[0];
				ret->as.wide[1] = asm_ret_regs[1];
			} else {
				assert(i+1 < blk->code.len);
				IR_Ins *ins_sto = &blk->code.elems[i+1];
				assert(ins_sto->op == ir_op_store);
				struct asm_address *dst = &ctx->vars[ins_sto->dst];
				ret->tag = ADDR_STACK;
				ret->as.i = dst->as.i;
				ret->extra.stack_size = dst->extra.stack_size;
				ret->type = dst->type;
				/* Note: we set arg_reg to 1 so that we can pass the ptr to return value
				 * in register according to abi.
				 */
				arg_reg = 1;
			}
			int argc = ins->arg.rx[1];
			while (argc--) {
				struct asm_address *arg = da_pop(&ctx->funargs);
				assert(arg->tag != ADDR_NONE);
				size_t ts = type_size(arg->type);
				if (ts <= 8 && arg_reg < ASM_ARG_REG_COUNT) {
					arg->tag = ADDR_ARGUMENT;
					arg->as.i = asm_arg_regs[arg_reg++];
				} else if (ts <= 16 && arg_reg < ASM_ARG_REG_COUNT - 1) {
					arg->tag = ADDR_WIDE_ARG;
					arg->as.wide[0] = asm_arg_regs[arg_reg++];
					arg->as.wide[1] = asm_arg_regs[arg_reg++];
				} else {
					arg->tag = ADDR_PUSH_ARG;
				}
			}
		} break;
		case IR_OPCODE_COUNT:
		default: FAILWITH("Unreachable"); break;
		}
	}
	struct ir_blk_terminal *term = &blk->term;
	switch (term->op) {
	case ir_op_ret: {
		if (term->args.len == 1) {
			struct asm_address *addr = &ctx->vars[term->args.elems[0]];
			if (addr->tag == ADDR_STACK) {
				/* do nothing
				 * Move values from stack into appropriate registers later
				 */
			} else if (addr->tag == ADDR_NONE) {
				/* do nothing */
			} else if (addr->tag == ADDR_STACK_LOAD) {
				/* do nothing */
			} else if (addr->tag == ADDR_IMM_INT) {
				/* do nothing */
			} else if (addr->tag == ADDR_TEMP_REG) {
				addr->tag = ADDR_REGISTER;
				addr->as.i = asm_ret_regs[0];
			} else if (addr->tag == ADDR_REGISTER) {
				assert(addr->as.i == asm_ret_regs[0]);
			} else if (addr->tag == ADDR_TEMP_WIDE) {
				addr->tag = ADDR_WIDE;
				addr->as.wide[0] = asm_ret_regs[0];
				addr->as.wide[1] = asm_ret_regs[1];
			} else if (addr->tag == ADDR_WIDE) {
				if ((enum asm_register)addr->as.wide[0] == asm_ret_regs[0]
					&& (enum asm_register)addr->as.wide[1] == asm_ret_regs[1]) {
					/* do nothing */
				} else {
					FAILWITH("TODO:\n"
							 "addr->as.wide[0] = %s\n"
							 "addr->as.wide[1] = %s",
							 asm_reg_q_name[addr->as.wide[0]],
							 asm_reg_q_name[addr->as.wide[1]]);
				}
			} else {
				FAILWITH("TODO: %%%d (addr->tag == %s)",
						 term->args.elems[0],
						 asm_addr_tag_to_str(addr->tag));
			}
		} else {
			assert(term->args.len == 0);
		}
	} break;
	case ir_op_goto: {
		for (size_t i = 0; i < term->args.len; ++i) {
			struct asm_address *addr = &ctx->vars[term->args.elems[i]];
			struct asm_address *formal = &ctx->vars[term->b0->args.elems[i]];
			if (addr->tag == ADDR_NONE) {
				FAILWITH("addr->tag == ADDR_NONE (%%%d)", term->args.elems[i]);
			}
			if (formal->tag == ADDR_REGISTER || formal->tag == ADDR_TEMP_REG) {
				*addr = *formal;
			} else if (addr->tag == ADDR_REGISTER
					   || addr->tag == ADDR_TEMP_REG
					   || formal->tag == ADDR_NONE) {
				*formal = *addr;
			} else if (addr->tag == ADDR_IMM_INT) {
				formal->tag = ADDR_TEMP_REG;
			}
			addr->tag = ADDR_BLK_ARG;
			addr->as.i = term->b0->args.elems[i];
		}
	} break;
	case ir_op_if: {
		/* assert(term->args.len == 1); */
		/* struct asm_address *addr = &ctx->vars[term->args.elems[0]]; */
		/* assert(addr->tag == ADDR_FLAGS); */
	} break;
	case ir_op_tailcall: FAILWITH("TODO: ir_op_tailcall"); break;
	}
}

struct asm_context *
asm_create_context(struct ir_proc *proc, struct da_pointers *blocks,
				   struct ir_toplevel *tl, struct asm_context *ctx)
{
	ctx->varc = proc->regc;
	ctx->vars = calloc(proc->regc, sizeof(struct asm_address));
	ctx->is_leaf = true;
	ctx->arg_stack_size = 16;
	for (int i = 0; i < ASM_REG_COUNT; ++i) {
		ctx->assigned[i] = REG_FREE;
	}
	struct ir_blk *entry = proc->entry;
	struct type *ret_type = proc->def->type->as.proc.ret;
	size_t ret_type_size = type_size(ret_type);
	int arg_reg = ret_type_size <= 16 ? 0 : 1;
	for (size_t arg_num = 0; arg_num < entry->args.len; ++arg_num) {
		struct type *type = proc->node->formals.elems[arg_num].type;
		size_t ts = type_size(type);
		size_t ta = type_alignment(type);
		if (ta < 8) ta = 8;
		int x = entry->args.elems[arg_num];
		ctx->vars[x].type = type;
		if (ts <= 8 && arg_reg < ASM_ARG_REG_COUNT) {
			ctx->vars[x].tag = ADDR_REGISTER;
			ctx->vars[x].as.i = asm_arg_regs[arg_reg++];
		} else if (ts <= 16 && arg_reg < ASM_ARG_REG_COUNT - 1) {
			ctx->vars[x].tag = ADDR_WIDE;
			ctx->vars[x].as.wide[0] = asm_arg_regs[arg_reg++];
			ctx->vars[x].as.wide[1] = asm_arg_regs[arg_reg++];
		} else {
			ctx->vars[x].tag = ADDR_STACK_ARG;
			ctx->vars[x].as.i = ctx->arg_stack_size;
			ctx->arg_stack_size += ts;
			ctx->arg_stack_size = align_adjust(ctx->arg_stack_size, ta);
		}
	}
	da_foreach(blk, blocks) {
		asm_context_first_pass(*blk, ctx, tl);
	}
	assert(ctx->funargs.len == 0);
	return ctx;
}

static int asm_get_register_owner(struct asm_context *ctx, enum asm_register reg);
static bool asm_register_assigned_p(struct asm_context *ctx, enum asm_register reg);
static int asm_reserve_register(struct asm_context *ctx, enum asm_register reg, int local);
static enum asm_register asm_assign_callee_save_register(struct asm_context *ctx, int local);
static enum asm_register asm_emit_move_to_callee_save(struct asm_address *addr, struct asm_procedure *code);
static enum asm_register asm_force_reserve_register(struct asm_context *ctx, struct asm_procedure *code,
													enum asm_register reg, int local);
static enum asm_register asm_assign_register(struct asm_context *ctx, int local);
static void asm_unassign_register(struct asm_context *ctx, enum asm_register reg);

static int asm_get_register_owner(struct asm_context *ctx, enum asm_register reg)
{
	return ctx->assigned[reg];
}

static bool asm_register_assigned_p(struct asm_context *ctx, enum asm_register reg)
{
	return ctx->assigned[reg] != REG_FREE;
}

static int asm_reserve_register(struct asm_context *ctx, enum asm_register reg, int local)
{
	if (ctx->assigned[reg] == local) return reg;
	if (asm_register_assigned_p(ctx, reg)) {
		FAILWITH("Register not free:\n"
				 "Trying to assign %s to %%%d\n"
				 "%s assigned to %%%d\n",
				 asm_reg_q_name[reg],
				 local,
				 asm_reg_q_name[reg],
				 ctx->assigned[reg]);
	}
	ctx->assigned[reg] = local;
	ctx->clobbered |= 1u << reg;
	return reg;
}

static enum asm_register asm_assign_callee_save_register(struct asm_context *ctx, int local)
{
	enum asm_register reg;
	size_t i = 0;
	while (i < ARRAY_LENGTH(asm_callee_save_regs)
		   && asm_register_assigned_p(ctx, reg = asm_callee_save_regs[i])) {
		i++;
	}
	assert(i < ARRAY_LENGTH(asm_callee_save_regs));
	asm_reserve_register(ctx, reg, local);
	return reg;
}

static enum asm_register asm_emit_move_to_callee_save(struct asm_address *addr, struct asm_procedure *code)
{
	if (addr->tag != ADDR_REGISTER) FAILWITH("addr->tag = %s", asm_addr_tag_to_str(addr->tag));
	struct asm_context *ctx = &code->ctx;
	int local = ctx->assigned[addr->as.i];
	enum asm_register reg = asm_assign_callee_save_register(ctx, local);
	append_line(&code->body, fmt_str(
					"\tmovq %s, %s\n",
					asm_reg_q_name[addr->as.i],
					asm_reg_q_name[reg]));
	asm_unassign_register(ctx, addr->as.i);
	ctx->vars[local].as.i = reg;
	return reg;
}

static enum asm_register asm_force_reserve_register(struct asm_context *ctx, struct asm_procedure *code,
													enum asm_register reg, int local)
{
	if (ctx->assigned[reg] == local) return reg;
	if (asm_register_assigned_p(ctx, reg))
		asm_emit_move_to_callee_save(&ctx->vars[ctx->assigned[reg]], code);
	ctx->assigned[reg] = local;
	ctx->clobbered |= 1u << reg;
	return reg;
}

static enum asm_register asm_assign_register(struct asm_context *ctx, int local)
{
	enum asm_register reg;
	size_t i = 0;
	while (i < ARRAY_LENGTH(asm_reg_alloc_ord)
		   && asm_register_assigned_p(ctx, reg = asm_reg_alloc_ord[i])) {
		i++;
	}
	assert(i < ARRAY_LENGTH(asm_reg_alloc_ord));
	asm_reserve_register(ctx, reg, local);
	return reg;
}

static void asm_unassign_register(struct asm_context *ctx, enum asm_register reg)
{
	ctx->assigned[reg] = REG_FREE;
}

static bool can_optimize_red_zone(struct asm_context *ctx)
{
	return ctx->is_leaf && ctx->stack_size <= RED_ZONE_SIZE;
}

static const char *ir_object_link_name(struct ir_toplevel *tl, union ir_object *obj)
{
	static char buf[400];
	if (tl->is_dll && !obj->hddr.is_static && obj->hddr.tag == IRO_EXTERN_DATA) {
		snprintf(buf, sizeof(buf), "\"%s\"@GOTPCREL", obj->hddr.link);
	} else {
		snprintf(buf, sizeof(buf), "\"%s\"", obj->hddr.link);
	}
	return buf;
}

static const char *asm_source_operand_to_str(struct asm_address *src, struct type *type,
											 struct asm_context *ctx, struct ir_toplevel *tl)
{
	static char buf[0xff] = {0};
	const char *s = buf;
	switch (src->tag) {
	case ADDR_REGISTER:
		asm_unassign_register(ctx, src->as.i);
		s = asm_reg_name(src->as.i, type);
		break;
	case ADDR_STACK_ARG: FAILWITH("TODO: ADDR_STACK_ARG");  break;
	case ADDR_WIDE:      FAILWITH("TODO: ADDR_WIDE");  break;
	case ADDR_PUSH_ARG:  FAILWITH("TODO: ADDR_PUSH_ARG");  break;
	case ADDR_IMM_INT:
		snprintf(buf, sizeof(buf), "$%ld", src->as.i);
		break;
	case ADDR_IMM_FLOAT:  FAILWITH("TODO: ADDR_IMM_FLOAT");  break;
	case ADDR_STACK:
		snprintf(buf, sizeof(buf), "%ld(%%rbp)", src->as.i);
		break;
	case ADDR_STACK_LOAD:
		snprintf(buf, sizeof(buf), "%d(%%rbp)", src->as.stack[0] + src->as.stack[1]);
		break;
	case ADDR_SYMBOL:
		/* snprintf(buf, sizeof(buf), "\"%s\"", tl->elems[src->as.i].hddr.link); */
		/* break; */
		return ir_object_link_name(tl, &tl->elems[src->as.i]);
		/* NOTE: These options should be invalid for the source operand */
	case ADDR_TEMP_REG:	  FAILWITH("TODO: ADDR_TEMP_REG");   break;
	case ADDR_BLK_ARG:    FAILWITH("TODO: ADDR_BLK_ARG");    break;
	case ADDR_FLAGS:	  FAILWITH("TODO: ADDR_FLAGS");	     break;
	case ADDR_WIDE_ARG:	  FAILWITH("TODO: ADDR_WIDE_ARG");	 break;
	case ADDR_TEMP_WIDE:  FAILWITH("TODO: ADDR_TEMP_WIDE");	 break;
	case ADDR_NONE:		  FAILWITH("TODO: ADDR_NONE");	     break;
	case ADDR_ARGUMENT:
	default: FAILWITH("Unreachable"); break;
	}
	return s;
}

static void asm_unassign_blk_arg_register(struct asm_context *ctx, int blk_arg)
{
	struct asm_address *b = &ctx->vars[blk_arg];
	assert(b->tag == ADDR_BLK_ARG);
	struct asm_address *a = &ctx->vars[b->as.i];
	if (a->tag == ADDR_REGISTER || a->tag == ADDR_ARGUMENT) {
		asm_unassign_register(ctx, a->as.i);
	} else if (a->tag == ADDR_BLK_ARG) {
		asm_unassign_blk_arg_register(ctx, b->as.i);
	} else if (a->tag == ADDR_WIDE || a->tag == ADDR_WIDE_ARG) {
		asm_unassign_register(ctx, a->as.wide[0]);
		asm_unassign_register(ctx, a->as.wide[1]);
	} else if (a->tag == ADDR_STACK) {
	} else if (a->tag == ADDR_STACK_LOAD) {
	} else {
		FAILWITH("TODO: %s", asm_addr_tag_to_str(a->tag));
	}
}

#define BEGIN_MATCH2(v0, v1)					\
	const struct asm_address *_v0 = v0;				\
	const struct asm_address *_v1 = v1

#define MATCH_ADDR3(_a0, _a1, _a2)				\
	(ctx->vars[ins->dst].tag == (_a0)			\
	 &&	ctx->vars[ins->arg.rx[0]].tag == (_a1)	\
	 &&	ctx->vars[ins->arg.rx[1]].tag == (_a2))

#define MATCH_ADDR2(_a0, _a1) (_v0->tag == (_a0) && _v1->tag == (_a1))

UNUSED static void
emit_mem_cpy_code(struct asm_procedure *code,
				  const char *dst_name, int64_t dst_offset,
				  const char *src_name, int64_t src_offset,
				  struct type *type)
{
	struct asm_context *ctx = &code->ctx;
	enum asm_register tmp = asm_assign_register(ctx, 0);
	int64_t offset = 0;
	uint32_t segcnts[4] = {0};
	mem_copy_segment_count(type_size(type), segcnts);
	append_line(&code->body, "\t/* emit_mem_cpy_code */\n");
	for (; segcnts[0]--; offset += 8) {
		append_line(&code->body, fmt_str(
						"\tmovq %ld(%s), %s\n",
						src_offset + offset,
						src_name,
						asm_reg_q_name[tmp]));
		append_line(&code->body, fmt_str(
						"\tmovq %s, %ld(%s)\n",
						asm_reg_q_name[tmp],
						dst_offset + offset,
						dst_name));
	}
	for (; segcnts[1]--; offset += 4) {
		append_line(&code->body, fmt_str(
						"\tmovl %ld(%s), %s\n",
						src_offset + offset,
						src_name,
						asm_reg_d_name[tmp]));
		append_line(&code->body, fmt_str(
						"\tmovl %s, %ld(%s)\n",
						asm_reg_d_name[tmp],
						dst_offset + offset,
						dst_name));
	}
	for (; segcnts[2]--; offset += 2) {
		append_line(&code->body, fmt_str(
						"\tmovw %ld(%s), %s\n",
						src_offset + offset,
						src_name,
						asm_reg_w_name[tmp]));
		append_line(&code->body, fmt_str(
						"\tmovw %s, %ld(%s)\n",
						asm_reg_w_name[tmp],
						dst_offset + offset,
						dst_name));
	}
	for (; segcnts[3]--; offset += 1) {
		append_line(&code->body, fmt_str(
						"\tmovb %ld(%s), %s\n",
						src_offset + offset,
						src_name,
						asm_reg_b_name[tmp]));
		append_line(&code->body, fmt_str(
						"\tmovb %s, %ld(%s)\n",
						asm_reg_b_name[tmp],
						dst_offset + offset,
						dst_name));
	}
	asm_unassign_register(ctx, tmp);
}

static void emit_op_mov_ADDR_REGISTER__ADDR_STACK(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
{
	assert(ins->op == ir_op_mov);
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
	assert(dst->tag == ADDR_REGISTER || dst->tag == ADDR_ARGUMENT);
	assert(x->tag == ADDR_STACK);
	asm_reserve_register(ctx, dst->as.i, ins->dst);
	append_line(&code->body, fmt_str(
					"\tleaq %ld(%%rbp), %s\n",
					x->as.i,
					asm_reg_q_name[dst->as.i]));
}

static void emit_op_load_ADDR_REGISTER__ADDR_STACK(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
{
	assert(ins->op == ir_op_load);
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
	assert(dst->tag == ADDR_REGISTER || dst->tag == ADDR_ARGUMENT);
	if (x->tag == ADDR_BLK_ARG) x = &ctx->vars[x->as.i];
	assert(x->tag == ADDR_STACK);
	if (dst->tag == ADDR_ARGUMENT)
		asm_force_reserve_register(ctx, code, dst->as.i, ins->dst);
	else
		asm_reserve_register(ctx, dst->as.i, ins->dst);
	append_line(&code->body, fmt_str(
					"\tmov%c %ld(%%rbp), %s\n",
					asm_suffix(ins->type),
					x->as.i,
					asm_reg_name(dst->as.i, ins->type)));
}

static void emit_op_load_ADDR_REGISTER__ADDR_REGISTER(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
{
	assert(ins->op == ir_op_load);
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
	assert(dst->tag == ADDR_REGISTER || dst->tag == ADDR_ARGUMENT);
	assert(x->tag == ADDR_REGISTER);
	asm_unassign_register(ctx, x->as.i);
	if (asm_register_assigned_p(ctx, dst->as.i) && dst->as.i != x->as.i) {
		struct asm_address *addr = &ctx->vars[ctx->assigned[dst->as.i]];
		asm_emit_move_to_callee_save(addr, code);
	}
	asm_reserve_register(ctx, dst->as.i, ins->dst);
	append_line(&code->body, fmt_str(
					"\tmov%c (%s), %s\n",
					asm_suffix(ins->type),
					asm_reg_q_name[x->as.i],
					asm_reg_name(dst->as.i, ins->type)));
}

static void emit_op_load_ADDR_WIDE__ADDR_STACK(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
{
	assert(ins->op == ir_op_load);
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
	assert(dst->tag == ADDR_WIDE || dst->tag == ADDR_WIDE_ARG);
	assert(x->tag == ADDR_STACK);
	size_t ts = type_size(ins->type);
	if (ts == 16) {
		asm_reserve_register(ctx, dst->as.wide[0], ins->dst);
		asm_reserve_register(ctx, dst->as.wide[1], ins->dst);
		append_line(&code->body, fmt_str("\tmovq %ld(%%rbp), %s\n",
										 x->as.i, asm_reg_q_name[dst->as.wide[0]]));
		append_line(&code->body, fmt_str("\tmovq %ld(%%rbp), %s\n",
										 x->as.i + 8, asm_reg_q_name[dst->as.wide[1]]));
	} else {
		FAILWITH("TODO");
	}
}

static void emit_op_load_ADDR_WIDE__ADDR_REGISTER(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
{
	assert(ins->op == ir_op_load);
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
	assert(dst->tag == ADDR_WIDE || dst->tag == ADDR_WIDE_ARG);
	assert(x->tag == ADDR_REGISTER);
	size_t ts = type_size(ins->type);
	if (ts == 16) {
		asm_unassign_register(ctx, x->as.i);
		asm_reserve_register(ctx, dst->as.wide[0], ins->dst);
		asm_reserve_register(ctx, dst->as.wide[1], ins->dst);
		if (dst->as.wide[0] == x->as.i || dst->as.wide[1] == x->as.i) {
			enum asm_register tmp = asm_assign_callee_save_register(ctx, ins->arg.rx[0]);
			append_line(&code->body, fmt_str("\tmovq %s, %s\n",
											 asm_reg_q_name[x->as.i],
											 asm_reg_q_name[tmp]));
			append_line(&code->body, fmt_str("\tmovq (%s), %s\n",
											 asm_reg_q_name[tmp],
											 asm_reg_q_name[dst->as.wide[0]]));
			append_line(&code->body, fmt_str("\tmovq 8(%s), %s\n",
											 asm_reg_q_name[tmp],
											 asm_reg_q_name[dst->as.wide[1]]));
			asm_unassign_register(ctx, tmp);
		} else {
			append_line(&code->body, fmt_str("\tmovq (%s), %s\n",
											 asm_reg_q_name[x->as.i],
											 asm_reg_q_name[dst->as.wide[0]]));
			append_line(&code->body, fmt_str("\tmovq 8(%s), %s\n",
											 asm_reg_q_name[x->as.i],
											 asm_reg_q_name[dst->as.wide[1]]));
		}
	} else {
		FAILWITH("TODO");
	}
}

static void emit_op_load_ADDR_WIDE__GLOBL(IR_Ins *ins, void *dat, struct asm_procedure *code)
{
	assert(ins->op == ir_op_loadglobl);
	struct ir_toplevel *tl = dat;
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	union ir_object *obj = &tl->elems[ins->arg.u32];
	struct type *type = ins->type;
	assert(dst->tag == ADDR_WIDE || dst->tag == ADDR_WIDE_ARG || dst->tag == ADDR_BLK_ARG);
	assert(obj->tag == IRO_DATA);
	assert(type->tag == ast_type_slice || type->tag == ast_type_mut_slice);
	assert(type_size(type) == 16);
	if (dst->tag == ADDR_BLK_ARG) {
		dst = &ctx->vars[dst->as.i];
		assert(dst->tag == ADDR_WIDE || dst->tag == ADDR_WIDE_ARG);
	}
	struct type *base_type = type->as.slice;
	asm_force_reserve_register(ctx, code, dst->as.wide[0], ins->dst);
	asm_force_reserve_register(ctx, code, dst->as.wide[1], ins->dst);
	if (tl->is_dll && !obj->hddr.is_static) {
		append_line(&code->body, fmt_str("\tmovq %s(%%rip), %s\n",
										 ir_object_link_name(tl, obj),
										 asm_reg_q_name[dst->as.wide[0]]));
	} else {
		append_line(&code->body, fmt_str("\tleaq %s(%%rip), %s\n",
										 ir_object_link_name(tl, obj),
										 asm_reg_q_name[dst->as.wide[0]]));
	}
	append_line(&code->body, fmt_str("\tmovq $%zu, %s\n",
									 obj->data.size / type_size(base_type),
									 asm_reg_q_name[dst->as.wide[1]]));
}

static void emit_op_load_ADDR_REGISTER__GLOBL(IR_Ins *ins, void *dat, struct asm_procedure *code)
{
	assert(ins->op == ir_op_loadglobl);
	struct ir_toplevel *tl = dat;
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	union ir_object *obj = &tl->elems[ins->arg.u32];
	struct type *type = ins->type;
	assert(dst->tag == ADDR_REGISTER || dst->tag == ADDR_ARGUMENT);
	assert(type->tag != ast_type_array);
	asm_reserve_register(ctx, dst->as.i, ins->dst);
	if (tl->is_dll && !obj->hddr.is_static) {
		append_line(&code->body, fmt_str("\tmovq %s(%%rip), %s\n",
										 ir_object_link_name(tl, obj),
										 asm_reg_q_name[dst->as.i]));
		append_line(&code->body, fmt_str("\tmov%c (%s), %s\n",
										 asm_suffix(type),
										 asm_reg_q_name[dst->as.i],
										 asm_reg_name(dst->as.i, type)));
	} else {
		append_line(&code->body, fmt_str("\tmov%c %s(%%rip), %s\n",
										 asm_suffix(type),
										 ir_object_link_name(tl, obj),
										 asm_reg_name(dst->as.i, type)));
	}
}

static void emit_op_load_ADDR_PUSH_ARG__ADDR_STACK(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
{
	assert(ins->op == ir_op_load);
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
	assert(dst->tag == ADDR_PUSH_ARG);
	assert(x->tag == ADDR_STACK);
	size_t ts = type_size(ins->type);
	uint32_t counts[4] = {0};
	mem_copy_segment_count(ts, counts);
	int tmp = asm_assign_register(ctx, ins->dst);
	int64_t offset = 0;
	append_line(&code->body, fmt_str("\tsubq $%zu, %%rsp\n", align_adjust(ts, 8)));
	for (uint32_t i = 0; i < counts[0]; ++i) {
		append_line(&code->body, fmt_str("\tmovq %ld(%%rbp), %s\n",
										 x->as.i + offset, asm_reg_q_name[tmp]));
		append_line(&code->body, fmt_str("\tmovq %s, %ld(%%rsp)\n",
										 asm_reg_q_name[tmp], offset));
		offset += 8;
	}
	for (uint32_t i = 0; i < counts[1]; ++i) {
		append_line(&code->body, fmt_str("\tmovl %ld(%%rbp), %s\n",
										 x->as.i + offset, asm_reg_d_name[tmp]));
		append_line(&code->body, fmt_str("\tmovl %s, %ld(%%rsp)\n",
										 asm_reg_d_name[tmp], offset));
		offset += 4;
	}
	for (uint32_t i = 0; i < counts[2]; ++i) {
		append_line(&code->body, fmt_str("\tmovw %ld(%%rbp), %s\n",
										 x->as.i + offset, asm_reg_w_name[tmp]));
		append_line(&code->body, fmt_str("\tmovw %s, %ld(%%rsp)\n",
										 asm_reg_w_name[tmp], offset));
		offset += 2;
	}
	for (uint32_t i = 0; i < counts[3]; ++i) {
		append_line(&code->body, fmt_str("\tmovb %ld(%%rbp), %s\n",
										 x->as.i + offset, asm_reg_b_name[tmp]));
		append_line(&code->body, fmt_str("\tmovb %s, %ld(%%rsp)\n",
										 asm_reg_b_name[tmp], offset));
		offset += 1;
	}
	asm_unassign_register(ctx, tmp);
}

static void emit_op_loadimm_ADDR_REGISTER(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
{
	assert(ins->op == ir_op_loadimm);
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	assert(dst->tag == ADDR_REGISTER || dst->tag == ADDR_ARGUMENT);
	asm_reserve_register(ctx, dst->as.i, ins->dst);
	append_line(&code->body, fmt_str(
					"\tmov%c $%d, %s\n",
					asm_suffix(ins->type),
					ins->arg.i32,
					asm_reg_name(dst->as.i, ins->type)));
}

static void emit_op_loadimm_ADDR_PUSH_ARG(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
{
	assert(ins->op == ir_op_loadimm);
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	assert(dst->tag == ADDR_PUSH_ARG);
	append_line(&code->body, fmt_str("\tpushq $%d\n", ins->arg.i32));
}

static void emit_op_neg_ADDR_REGISTER__ADDR_IMM_INT(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
{
	assert(ins->op == ir_op_neg);
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
	assert(dst->tag == ADDR_REGISTER || dst->tag == ADDR_ARGUMENT);
	assert(x->tag == ADDR_IMM_INT);
	asm_reserve_register(ctx, dst->as.i, ins->dst);
	append_line(&code->body, fmt_str(
					"\tmov%c $%ld, %s\n",
					asm_suffix(ins->type),
					-x->as.i,
					asm_reg_name(dst->as.i, ins->type)));
}

static void emit_op_cast_ADDR_REGISTER__ADDR_STACK_LOAD(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
{
	assert(ins->op == ir_op_cast);
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
	assert(dst->tag == ADDR_REGISTER || dst->tag == ADDR_ARGUMENT);
	assert(x->tag == ADDR_STACK_LOAD);
	asm_reserve_register(ctx, dst->as.i, ins->dst);
	if (type_equiv(NULL, x->type, ins->type, NULL)) {
		append_line(&code->body, fmt_str(
						"\tmov%c %d(%%rbp), %s\n",
						asm_suffix(ins->type),
						x->as.stack[0] + x->as.stack[1],
						asm_reg_name(dst->as.i, ins->type)));
	} else if (type_is_integer(x->type) && ins->type->tag == ast_type_i8) {
		append_line(&code->body, fmt_str(
						"\tmov%c %d(%%rbp), %s\n",
						asm_suffix(ins->type),
						x->as.stack[0] + x->as.stack[1],
						asm_reg_name(dst->as.i, ins->type)));
	} else if (x->type->tag == ast_type_i32 && ins->type->tag == ast_type_i64) {
		append_line(&code->body, fmt_str(
						"\tmovslq %d(%%rbp), %s\n",
						x->as.stack[0] + x->as.stack[1],
						asm_reg_name(dst->as.i, ins->type)));
	} else if (x->type->tag == ast_type_i8 && ins->type->tag == ast_type_i64) {
		append_line(&code->body, fmt_str(
						"\tmovsbq %d(%%rbp), %s\n",
						x->as.stack[0] + x->as.stack[1],
						asm_reg_name(dst->as.i, ins->type)));
	} else {
		FAILWITH("TODO: cast %s -> %s", ast_type_to_str(x->type), ast_type_to_str(ins->type));
	}
}

static void emit_op_cast_ADDR_REGISTER__ADDR_IMM_INT(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
{
	assert(ins->op == ir_op_cast);
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
	assert(dst->tag == ADDR_REGISTER || dst->tag == ADDR_ARGUMENT);
	assert(x->tag == ADDR_IMM_INT);
	asm_reserve_register(ctx, dst->as.i, ins->dst);
	if (type_is_integer(ins->type) || type_is_pointer(ins->type)) {
		append_line(&code->body, fmt_str(
						"\tmov%c $%ld, %s\n",
						asm_suffix(ins->type),
						x->as.i,
						asm_reg_name(dst->as.i, ins->type)));
	} else {
		FAILWITH("TODO: cast %s -> %s", ast_type_to_str(x->type), ast_type_to_str(ins->type));
	}
}

static void emit_op_cast_ADDR_REGISTER__ADDR_REGISTER(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
{
	assert(ins->op == ir_op_cast);
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
	assert(dst->tag == ADDR_REGISTER || dst->tag == ADDR_ARGUMENT);
	assert(x->tag == ADDR_REGISTER);
	asm_unassign_register(ctx, x->as.i);
	asm_reserve_register(ctx, dst->as.i, ins->dst);
	if (type_equiv(NULL, x->type, ins->type, NULL)) {
		append_line(&code->body, fmt_str(
						"\tmov%c %s, %s\n",
						asm_suffix(ins->type),
						asm_reg_name(x->as.i, ins->type),
						asm_reg_name(dst->as.i, ins->type)));
	} else if (type_is_integer(x->type) && ins->type->tag == ast_type_i8) {
		append_line(&code->body, fmt_str(
						"\tmov%c %s, %s\n",
						asm_suffix(ins->type),
						asm_reg_name(x->as.i, ins->type),
						asm_reg_name(dst->as.i, ins->type)));
	} else if (x->type->tag == ast_type_i32 && ins->type->tag == ast_type_i64) {
		append_line(&code->body, fmt_str(
						"\tmovslq %s, %s\n",
						asm_reg_name(x->as.i, ins->type),
						asm_reg_name(dst->as.i, ins->type)));
	} else if (x->type->tag == ast_type_i8 && ins->type->tag == ast_type_i64) {
		append_line(&code->body, fmt_str(
						"\tmovsbq %s, %s\n",
						asm_reg_name(x->as.i, ins->type),
						asm_reg_name(dst->as.i, ins->type)));
	} else {
		FAILWITH("TODO: cast %s -> %s", ast_type_to_str(x->type), ast_type_to_str(ins->type));
	}
}

static void emit_op_conslice_ADDR_WIDE__ADDR_REGISTER__ADDR_IMM_INT(
	IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
{
	assert(ins->op == ir_op_conslice);
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
	struct asm_address *y   = &ctx->vars[ins->arg.rx[1]];
	assert(dst->tag == ADDR_WIDE || dst->tag == ADDR_WIDE_ARG);
	assert(x->tag == ADDR_REGISTER);
	assert(y->tag == ADDR_IMM_INT);
	size_t ts = type_size(ins->type);
	if (ts == 16) {
		asm_unassign_register(ctx, x->as.i);
		asm_reserve_register(ctx, dst->as.wide[0], ins->dst);
		asm_reserve_register(ctx, dst->as.wide[1], ins->dst);
		if (dst->as.wide[0] == x->as.i) {
			append_line(&code->body, fmt_str("\tmovq $%ld, %s\n",
											 y->as.i,
											 asm_reg_q_name[dst->as.wide[1]]));
		} else {
			append_line(&code->body, fmt_str("\tmovq %s, %s\n",
											 asm_reg_q_name[x->as.i],
											 asm_reg_q_name[dst->as.wide[0]]));
			append_line(&code->body, fmt_str("\tmovq $%ld, %s\n",
											 y->as.i,
											 asm_reg_q_name[dst->as.wide[1]]));
		}
	} else {
		FAILWITH("TODO");
	}
}

static void emit_op_conslice_ADDR_WIDE__ADDR_REGISTER__ADDR_REGISTER(
	IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
{
	assert(ins->op == ir_op_conslice);
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
	struct asm_address *y   = &ctx->vars[ins->arg.rx[1]];
	assert(dst->tag == ADDR_WIDE || dst->tag == ADDR_WIDE_ARG);
	assert(x->tag == ADDR_REGISTER);
	assert(y->tag == ADDR_REGISTER);
	asm_unassign_register(ctx, x->as.i);
	asm_unassign_register(ctx, y->as.i);
	asm_reserve_register(ctx, dst->as.wide[0], ins->dst);
	asm_reserve_register(ctx, dst->as.wide[1], ins->dst);
	if (dst->as.wide[0] != x->as.i || dst->as.wide[1] != y->as.i) {
		append_line(&code->body, fmt_str("\tpushq %s\n", asm_reg_q_name[x->as.i]));
		append_line(&code->body, fmt_str("\tpushq %s\n", asm_reg_q_name[y->as.i]));
		append_line(&code->body, fmt_str("\tpopq %s\n",  asm_reg_q_name[dst->as.wide[1]]));
		append_line(&code->body, fmt_str("\tpopq %s\n",  asm_reg_q_name[dst->as.wide[0]]));
	}
}

static void emit_op_conslice_ADDR_WIDE__ADDR_REGISTER__ADDR_STACK_LOAD(
	IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
{
	assert(ins->op == ir_op_conslice);
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
	struct asm_address *y   = &ctx->vars[ins->arg.rx[1]];
	assert(dst->tag == ADDR_WIDE || dst->tag == ADDR_WIDE_ARG);
	assert(x->tag == ADDR_REGISTER);
	assert(y->tag == ADDR_STACK_LOAD);
	asm_unassign_register(ctx, x->as.i);
	asm_reserve_register(ctx, dst->as.wide[0], ins->dst);
	asm_reserve_register(ctx, dst->as.wide[1], ins->dst);
	if (dst->as.wide[0] != x->as.i) {
		append_line(&code->body, fmt_str(
						"\tmovq %s, %s\n",
						asm_reg_q_name[x->as.i],
						asm_reg_q_name[dst->as.wide[0]]));
	}
	append_line(&code->body, fmt_str(
					"\tmovq %d(%%rbp), %s\n",
					y->as.stack[0] + y->as.stack[1],
					asm_reg_q_name[dst->as.wide[1]]));
}

static void emit_op_conslice_ADDR_WIDE__ADDR_STACK__ADDR_IMM_INT(
	IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
{
	assert(ins->op == ir_op_conslice);
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
	struct asm_address *y   = &ctx->vars[ins->arg.rx[1]];
	assert(dst->tag == ADDR_WIDE || dst->tag == ADDR_WIDE_ARG);
	assert(x->tag == ADDR_STACK);
	assert(y->tag == ADDR_IMM_INT);
	asm_reserve_register(ctx, dst->as.wide[0], ins->dst);
	asm_reserve_register(ctx, dst->as.wide[1], ins->dst);
	append_line(&code->body, "/* emit_op_conslice_ADDR_WIDE__ADDR_STACK__ADDR_IMM_INT */\n");
	append_line(&code->body, fmt_str(
					"\tleaq %ld(%%rbp), %s\n",
					x->as.i,
					asm_reg_q_name[dst->as.wide[0]]));
	append_line(&code->body, fmt_str(
					"\tmovq $%ld, %s\n",
					y->as.i,
					asm_reg_q_name[dst->as.wide[1]]));
}

static void emit_basic_op_ADDR_REGISTER__ADDR_STACK_LOAD__ADDR_IMM_INT(IR_Ins *ins, void *dat,
																	   struct asm_procedure *code)
{
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
	struct asm_address *y   = &ctx->vars[ins->arg.rx[1]];
	struct type *type = ins->type;
	char *asm_op = dat;
	assert(dst->tag == ADDR_REGISTER || dst->tag == ADDR_ARGUMENT);
	assert(x->tag == ADDR_STACK_LOAD);
	assert(y->tag == ADDR_IMM_INT);
	int dst_reg = asm_reserve_register(ctx, dst->as.i, ins->dst);
	const char *dst_name = asm_reg_name(dst_reg, type);
	append_line(&code->body, fmt_str(
					"\tmov%c %d(%%rbp), %s\n",
					asm_suffix(type),
					x->as.stack[0] + x->as.stack[1],
					dst_name));
	append_line(&code->body, fmt_str(
					"\t%s%c $%ld, %s\n",
					asm_op,
					asm_suffix(type),
					y->as.i,
					dst_name));
}

static void defer_emit_div_mod(IR_Ins *ins, void *dat, struct asm_procedure *code);

void asm_emit_div_mod(IR_Ins *ins, struct asm_procedure *code, bool remainder_p)
{
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
	struct asm_address *y   = &ctx->vars[ins->arg.rx[1]];
	enum asm_register result = remainder_p ? asm_reg_rdx : asm_reg_rax;
	char *op = NULL;
	char *conv = NULL;
	/* The div instruction requires the use of RAX and RDX for the dst argument.
	 * If RAX or RDX are in use, push them to the stack temporarily.
	 */
	bool rax_in_use = asm_register_assigned_p(ctx, asm_reg_rax);
	bool rdx_in_use = asm_register_assigned_p(ctx, asm_reg_rdx);
	if (rax_in_use) append_line(&code->body, strdup("\tpushq %rax\n"));
	if (rdx_in_use) append_line(&code->body, strdup("\tpushq %rdx\n"));
	if (type_is_signed_integer(ins->type)) {
		op = "idiv";
	} else if (type_is_unsigned_integer(ins->type)) {
		op = "div";
	} else {
		FAILWITH("TODO: division is unimplemented for type.");
	}
	if (MATCH_ADDR3(ADDR_REGISTER, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
		asm_reserve_register(ctx, dst->as.i, ins->dst);
		goto fallthrough_ADDR_TEMP_REG_ADDR_STACK_LOAD_ADDR_IMM_INT;
	} else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
		dst->tag = ADDR_REGISTER;
		dst->as.i = asm_assign_register(ctx, ins->dst);
	fallthrough_ADDR_TEMP_REG_ADDR_STACK_LOAD_ADDR_IMM_INT:
		if (!type_is_integer(ins->type))
			FAILWITH("TODO: implement division for non integer types.");
		enum asm_register tmp = asm_assign_register(ctx, ins->dst);
		int suffix = asm_suffix(ins->type);
		int ext = type_is_signed_integer(ins->type) ? 's' : 'z';
		switch (type_size(ins->type)) {
		case 1:
			conv = "cltd";
			append_line(&code->body, fmt_str(
							"\tmov%cbl %d(%%rbp), %s\n",
							ext,
							x->as.stack[0] + x->as.stack[1],
							asm_reg_d_name[asm_reg_rax]));
			append_line(&code->body, fmt_str(
							"\tmovl $%ld, %s\n",
							y->as.i,
							asm_reg_d_name[tmp]));
			append_line(&code->body, fmt_str("\t%s\n", conv));
			append_line(&code->body, fmt_str(
							"\t%sl %s\n",
							op,
							asm_reg_d_name[tmp]));
			append_line(&code->body, fmt_str(
							"\tmov%c %s, %s\n",
							asm_suffix(ins->type),
							asm_reg_name(result, ins->type),
							asm_reg_name(dst->as.i, ins->type)));
			asm_unassign_register(ctx, tmp);
			break;
		case 2:
			conv = "cltd";
			append_line(&code->body, fmt_str(
							"\tmov%cwl %d(%%rbp), %s\n",
							ext,
							x->as.stack[0] + x->as.stack[1],
							asm_reg_d_name[asm_reg_rax]));
			append_line(&code->body, fmt_str(
							"\tmovl $%ld, %s\n",
							y->as.i,
							asm_reg_d_name[tmp]));
			append_line(&code->body, fmt_str("\t%s\n", conv));
			append_line(&code->body, fmt_str(
							"\t%sl %s\n",
							op,
							asm_reg_d_name[tmp]));
			append_line(&code->body, fmt_str(
							"\tmov%c %s, %s\n",
							asm_suffix(ins->type),
							asm_reg_name(result, ins->type),
							asm_reg_name(dst->as.i, ins->type)));
			asm_unassign_register(ctx, tmp);
			break;
		case 4:
			conv = "cltd";
			goto CASE8;
		case 8:
			conv = "cqto";
		CASE8:
			append_line(&code->body, fmt_str(
							"\tmov%c %d(%%rbp), %s\n",
							suffix,
							x->as.stack[0] + x->as.stack[1],
							asm_reg_name(asm_reg_rax, ins->type)));
			append_line(&code->body, fmt_str(
							"\tmov%c $%ld, %s\n",
							suffix,
							y->as.i,
							asm_reg_name(tmp, ins->type)));
			append_line(&code->body, fmt_str("\t%s\n", conv));
			append_line(&code->body, fmt_str(
							"\t%s%c %s\n",
							op, suffix,
							asm_reg_name(tmp, ins->type)));
			append_line(&code->body, fmt_str(
							"\tmov%c %s, %s\n",
							asm_suffix(ins->type),
							asm_reg_name(result, ins->type),
							asm_reg_name(dst->as.i, ins->type)));
			asm_unassign_register(ctx, tmp);
			break;
		default: FAILWITH("TODO: invalid type size."); break;
		}
	} else if (MATCH_ADDR3(ADDR_ARGUMENT, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
		dst->extra.defered.fun = defer_emit_div_mod;
		dst->extra.defered.ins = ins;
		dst->extra.defered.dat = (void*)remainder_p;
	} else {
		printf("dst = %%%d\n", ins->dst);
		FAILWITH("Unhandled case: MATCH_ADDR3(%s, %s, %s)",
				 asm_addr_tag_to_str(dst->tag),
				 asm_addr_tag_to_str(x->tag),
				 asm_addr_tag_to_str(y->tag));
	}
	if (rdx_in_use) append_line(&code->body, strdup("\tpopq %rdx\n"));
	if (rax_in_use) append_line(&code->body, strdup("\tpopq %rax\n"));
}

static void defer_emit_div_mod(IR_Ins *ins, void *dat, struct asm_procedure *code)
{
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	bool remainder_p = (bool)dat;
	assert(dst->tag == ADDR_ARGUMENT);
	dst->tag = ADDR_REGISTER; // temporarily change tag so that correct path executes.
	asm_emit_div_mod(ins, code, remainder_p);
	dst->tag = ADDR_ARGUMENT;
}

void asm_emit_basic_op(const char *asm_op, IR_Ins *ins, struct asm_address *dst,
					   struct asm_address *x, struct asm_address *y,
					   struct ir_toplevel *tl, struct asm_procedure *code)
{
	struct asm_context *ctx = &code->ctx;
	struct type *type = ins->type;
	if (MATCH_ADDR3(ADDR_REGISTER, ADDR_IMM_INT, ADDR_IMM_INT)) {
		enum asm_register dst_reg = dst->as.i;
		const char *dst_name = asm_reg_name(dst_reg, type);
		asm_reserve_register(ctx, dst_reg, ins->dst);
		append_line(&code->body, fmt_str(
						"\tmov%c $%ld, %s\n",
						asm_suffix(type),
						x->as.i,
						dst_name));
		append_line(&code->body, fmt_str(
						"\t%s%c $%ld, %s\n",
						asm_op,
						asm_suffix(type),
						y->as.i,
						dst_name));
	} else if (MATCH_ADDR3(ADDR_REGISTER, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
		emit_basic_op_ADDR_REGISTER__ADDR_STACK_LOAD__ADDR_IMM_INT(ins, NULL, code);
	} else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK_LOAD, ADDR_IMM_INT)
			   || MATCH_ADDR3(ADDR_FLAGS, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
		int dst_reg = asm_assign_register(ctx, ins->dst);
		dst->tag = ADDR_REGISTER;
		dst->as.i = dst_reg;
		const char *dst_name = asm_reg_name(dst_reg, type);
		append_line(&code->body, fmt_str(
						"\tmov%c %d(%%rbp), %s\n",
						asm_suffix(type),
						x->as.stack[0] + x->as.stack[1],
						dst_name));
		const char *src_name = asm_source_operand_to_str(y, type, ctx, tl);
		append_line(&code->body, fmt_str(
						"\t%s%c %s, %s\n",
						asm_op,
						asm_suffix(type),
						src_name,
						dst_name));
	} else if (MATCH_ADDR3(ADDR_BLK_ARG, ADDR_REGISTER, ADDR_REGISTER)) {
		struct asm_address *arg_addr = &ctx->vars[dst->as.i]; // lookup the addr for the formal block arg
		if (arg_addr->tag == ADDR_TEMP_REG) {
			arg_addr->tag = ADDR_REGISTER;
			arg_addr->as.i = asm_assign_callee_save_register(ctx, ins->dst);
		}
		assert(arg_addr->tag == ADDR_REGISTER);
		dst = arg_addr;
		bool move_result_p = false;
		int dst_reg = dst->as.i;
		if (asm_register_assigned_p(ctx, dst_reg)) {
			if (x->as.i == dst_reg) {
				const char *dst_name = asm_reg_name(dst_reg, type);
				const char *src_name = asm_source_operand_to_str(y, type, ctx, tl);
				append_line(&code->body, fmt_str(
								"\t%s%c %s, %s\n",
								asm_op,
								asm_suffix(type),
								src_name,
								dst_name));
				return;
			} else if (y->as.i == dst_reg) {
				move_result_p = true;
				dst_reg = asm_assign_register(ctx, ins->dst);
			} else {
				/* spill to callee save */
				dst_reg = asm_emit_move_to_callee_save(dst, code);
			}
		} else {
			asm_reserve_register(ctx, dst_reg, ins->dst);
		}
		const char *dst_name = asm_reg_name(dst_reg, type);
		if (dst->as.i != x->as.i) {
			append_line(&code->body, fmt_str(
							"\tmov%c %s, %s\n",
							asm_suffix(type),
							asm_reg_name(x->as.i, type),
							dst_name));
		}
		const char *src_name = asm_source_operand_to_str(y, type, ctx, tl);
		append_line(&code->body, fmt_str(
						"\t%s%c %s, %s\n",
						asm_op,
						asm_suffix(type),
						src_name,
						dst_name));
		if (move_result_p) {
			asm_unassign_register(ctx, dst_reg);
			append_line(&code->body, fmt_str(
							"\tmov%c %s, %s\n",
							asm_suffix(type),
							asm_reg_name(dst_reg, type),
							asm_reg_name(dst->as.i, type)));
		}
	} else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK_LOAD, ADDR_STACK_LOAD)) {
		int dst_reg = asm_assign_register(ctx, ins->dst);
		dst->tag = ADDR_REGISTER;
		dst->as.i = dst_reg;
		const char *dst_name = asm_reg_name(dst_reg, type);
		const char *src_name = asm_source_operand_to_str(y, type, ctx, tl);
		append_line(&code->body, fmt_str(
						"\tmov%c %d(%%rbp), %s\n",
						asm_suffix(type),
						x->as.stack[0] + x->as.stack[1],
						dst_name));
		append_line(&code->body, fmt_str(
						"\t%s%c %s, %s\n",
						asm_op,
						asm_suffix(type),
						src_name,
						dst_name));
	} else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_REGISTER, ADDR_STACK_LOAD)) {
		asm_unassign_register(ctx, x->as.i);
		dst->as.i = asm_reserve_register(ctx, x->as.i, ins->dst);
		dst->tag = ADDR_REGISTER;
		append_line(&code->body, fmt_str(
						"\t%s%c %d(%%rbp), %s\n",
						asm_op,
						asm_suffix(type),
						y->as.stack[0] + y->as.stack[1],
						asm_reg_name(dst->as.i, type)));
	} else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK_LOAD, ADDR_REGISTER)) {
		dst->as.i = asm_assign_register(ctx, ins->dst);
		dst->tag = ADDR_REGISTER;
		append_line(&code->body, fmt_str(
						"\tmov%c %d(%%rbp), %s\n",
						asm_suffix(type),
						x->as.stack[0] + x->as.stack[1],
						asm_reg_name(dst->as.i, type)));
		append_line(&code->body, fmt_str(
						"\t%s%c %s, %s\n",
						asm_op,
						asm_suffix(type),
						asm_reg_name(y->as.i, type),
						asm_reg_name(dst->as.i, type)));
		asm_unassign_register(ctx, y->as.i);
	} else if (MATCH_ADDR3(ADDR_REGISTER, ADDR_REGISTER, ADDR_STACK_LOAD)) {
		asm_reserve_register(ctx, dst->as.i, ins->dst);
		const char *d_name = asm_reg_name(dst->as.i, type);
		const char *y_name = asm_source_operand_to_str(y, type, ctx, tl);
		int suffix = asm_suffix(type);
		if (dst->as.i != x->as.i) {
			const char *x_name = asm_reg_name(x->as.i, type);
			append_line(&code->body, fmt_str("\tmov%c %s, %s\n", suffix, x_name, d_name));
			asm_unassign_register(ctx, x->as.i);
		}
		append_line(&code->body, fmt_str("\t%s%c %s, %s\n",
										 asm_op, suffix, y_name, d_name));
	} else if (MATCH_ADDR3(ADDR_ARGUMENT, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
		dst->extra.defered.fun = emit_basic_op_ADDR_REGISTER__ADDR_STACK_LOAD__ADDR_IMM_INT;
		dst->extra.defered.ins = ins;
		dst->extra.defered.dat = (void*)asm_op;
	} else if (MATCH_ADDR3(ADDR_REGISTER, ADDR_STACK_LOAD, ADDR_STACK_LOAD)) {
		asm_reserve_register(ctx, dst->as.i, ins->dst);
		const char *d_name = asm_reg_name(dst->as.i, type);
		const char *x_name = asm_source_operand_to_str(x, type, ctx, tl);
		int suffix = asm_suffix(type);
		append_line(&code->body, fmt_str("\tmov%c %s, %s\n", suffix, x_name, d_name));
		const char *y_name = asm_source_operand_to_str(y, type, ctx, tl);
		append_line(&code->body, fmt_str("\t%s%c %s, %s\n", asm_op, suffix, y_name, d_name));
	} else if (MATCH_ADDR3(ADDR_REGISTER, ADDR_REGISTER, ADDR_REGISTER)) {
		asm_unassign_register(ctx, x->as.i);
		asm_unassign_register(ctx, y->as.i);
		asm_reserve_register(ctx, dst->as.i, ins->dst);
		int suffix = asm_suffix(type);
		if (dst->as.i == x->as.i) {
			append_line(&code->body, fmt_str(
							"\t%s%c %s, %s\n",
							asm_op,
							suffix,
							asm_reg_name(y->as.i, type),
							asm_reg_name(dst->as.i, type)));
		} else {
			append_line(&code->body, fmt_str(
							"\t%s%c %s, %s\n",
							asm_op,
							suffix,
							asm_reg_name(y->as.i, type),
							asm_reg_name(x->as.i, type)));
			append_line(&code->body, fmt_str(
							"\tmov%c %s, %s\n",
							suffix,
							asm_reg_name(x->as.i, type),
							asm_reg_name(dst->as.i, type)));
		}
	} else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_REGISTER, ADDR_IMM_INT)) {
		asm_unassign_register(ctx, x->as.i);
		int dst_reg = dst->as.i = asm_reserve_register(ctx, x->as.i, ins->dst);
		dst->tag = ADDR_REGISTER;
		const char *dst_name = asm_reg_name(dst_reg, type);
		const char *src_name = asm_source_operand_to_str(y, type, ctx, tl);
		append_line(&code->body, fmt_str(
						"\t%s%c %s, %s\n",
						asm_op,
						asm_suffix(type),
						src_name,
						dst_name));
	} else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_BLK_ARG, ADDR_IMM_INT)) {
		x = &ctx->vars[x->as.i];
		int dst_reg = dst->as.i = asm_assign_register(ctx, ins->dst);
		dst->tag = ADDR_REGISTER;
		const char *dst_name = asm_reg_name(dst_reg, type);
		const char *src_name = asm_source_operand_to_str(y, type, ctx, tl);
		if (x->tag == ADDR_STACK) {
			append_line(&code->body, fmt_str(
							"\tmov%c %ld(%%rbp), %s\n",
							asm_suffix(type),
							x->as.i,
							dst_name));
			append_line(&code->body, fmt_str(
							"\t%s%c %s, %s\n",
							asm_op,
							asm_suffix(type),
							src_name,
							dst_name));
		} else if (x->tag == ADDR_REGISTER) {
			append_line(&code->body, fmt_str(
							"\tmov%c %s, %s\n",
							asm_suffix(type),
							asm_reg_name(x->as.i, ins->type),
							dst_name));
			append_line(&code->body, fmt_str(
							"\t%s%c %s, %s\n",
							asm_op,
							asm_suffix(type),
							src_name,
							dst_name));
		} else {
			FAILWITH("Unhandled case: `%s`; (x->tag == %s)", asm_op, asm_addr_tag_to_str(x->tag));
		}
		append_line(&code->body, fmt_str(
						"\t%s%c %s, %s\n",
						asm_op,
						asm_suffix(type),
						src_name,
						dst_name));
	} else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_REGISTER, ADDR_REGISTER)) {
		asm_unassign_register(ctx, x->as.i);
		append_line(&code->body, fmt_str(
						"\t%s%c %s, %s\n",
						asm_op,
						asm_suffix(type),
						asm_reg_name(y->as.i, type),
						asm_reg_name(x->as.i, type)));
		asm_unassign_register(ctx, y->as.i);
		dst->as.i = asm_reserve_register(ctx, x->as.i, ins->dst);
		dst->tag = ADDR_REGISTER;
	} else {
		FAILWITH("Unhandled case: `%s`; MATCH_ADDR3(%s, %s, %s)",
				 asm_op,
				 asm_addr_tag_to_str(dst->tag),
				 asm_addr_tag_to_str(x->tag),
				 asm_addr_tag_to_str(y->tag));
	}
}

void asm_emit_block(struct da_pointers *blocks, size_t blk_id, struct ir_proc *proc,
					struct ir_toplevel *tl, struct asm_procedure *code)
{
	struct asm_context *ctx = &code->ctx;
	struct ir_blk *blk = blocks->elems[blk_id];
	append_line(&code->body, fmt_str(".L%p:\n", blk));
	for (size_t i = 0; i < blk->code.len; ++i) {
		IR_Ins *ins = &blk->code.elems[i];
		char *asm_op;
		switch (ins->op) {
		case ir_op_nop: break;
		case ir_op_mov: {
			struct asm_address *dst = &ctx->vars[ins->dst];
			struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
			BEGIN_MATCH2(dst, x);
			if (MATCH_ADDR2(ADDR_REGISTER, ADDR_STACK)) {
				emit_op_mov_ADDR_REGISTER__ADDR_STACK(ins, NULL, code);
			} else if (MATCH_ADDR2(ADDR_REGISTER, ADDR_REGISTER)) {
				asm_reserve_register(ctx, dst->as.i, ins->dst);
				append_line(&code->body, fmt_str(
								"\tmovq %s, %s\n",
								asm_reg_q_name[x->as.i],
								asm_reg_q_name[dst->as.i]));
			} else if (MATCH_ADDR2(ADDR_TEMP_REG, ADDR_STACK)) {
				dst->tag = ADDR_REGISTER;
				dst->as.i = asm_assign_register(ctx, ins->dst);
				append_line(&code->body, fmt_str(
								"\tleaq %s, %s\n",
								asm_source_operand_to_str(x, NULL, ctx, tl),
								asm_reg_q_name[dst->as.i]));
			} else if (MATCH_ADDR2(ADDR_TEMP_REG, ADDR_REGISTER)) {
				dst->tag = ADDR_REGISTER;
				dst->as.i = asm_assign_register(ctx, ins->dst);
				append_line(&code->body, fmt_str(
								"\tmovq %s, %s\n",
								asm_reg_q_name[x->as.i],
								asm_reg_q_name[dst->as.i]));
				asm_unassign_register(ctx, x->as.i);
			} else if (MATCH_ADDR2(ADDR_ARGUMENT, ADDR_STACK)) {
				dst->extra.defered.fun = emit_op_mov_ADDR_REGISTER__ADDR_STACK;
				dst->extra.defered.ins = ins;
			} else {
				FAILWITH("Unhandled case: MATCH_ADDR2(%s, %s)",
						 asm_addr_tag_to_str(dst->tag),
						 asm_addr_tag_to_str(x->tag));
			}
		} break;
		case ir_op_copy: {
			struct asm_address *dst = &ctx->vars[ins->dst];
			struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
			BEGIN_MATCH2(dst, x);
			if (MATCH_ADDR2(ADDR_STACK, ADDR_REGISTER)) {
				emit_mem_cpy_code(code, "%rbp", dst->as.i, asm_reg_q_name[x->as.i], 0, ins->type);
				asm_unassign_register(ctx, x->as.i);
			} else {
				FAILWITH("Unhandled case (ir_op_copy): MATCH_ADDR2(%s, %s)",
						 asm_addr_tag_to_str(dst->tag),
						 asm_addr_tag_to_str(x->tag));
			}
		} break;
		case ir_op_undefined: break;
		case ir_op_add:
			if (type_is_floating_point(ins->type)) {
				asm_op = "fadd";
			} else {
				assert(type_is_integer(ins->type));
				asm_op = "add";
			}
			goto LBL_basic;
		case ir_op_sub:
			if (type_is_floating_point(ins->type)) {
				asm_op = "fsub";
			} else {
				assert(type_is_integer(ins->type));
				asm_op = "sub";
			}
			goto LBL_basic;
		case ir_op_mul:
			if (type_is_floating_point(ins->type)) {
				asm_op = "fmul";
			} else {
				assert(type_is_integer(ins->type));
				asm_op = "imul";
			}
		LBL_basic:
			asm_emit_basic_op(asm_op, ins, &ctx->vars[ins->dst],
							  &ctx->vars[ins->arg.rx[0]], &ctx->vars[ins->arg.rx[1]], tl, code);
			break;
		case ir_op_cmpe:
		case ir_op_cmpne:
		case ir_op_cmpl:
		case ir_op_cmpg:
		case ir_op_cmple:
		case ir_op_cmpge: {
			asm_op = type_is_floating_point(ins->type) ? "fcmp" : "cmp";
			assert(ctx->vars[ins->dst].tag == ADDR_FLAGS);
			ctx->vars[ins->dst].tag = ADDR_TEMP_REG;
			struct asm_address dst = {.tag=ADDR_TEMP_REG};
			asm_emit_basic_op(asm_op, ins, &dst, &ctx->vars[ins->arg.rx[0]],
							  &ctx->vars[ins->arg.rx[1]], tl, code);
			ctx->vars[ins->dst].tag = ADDR_FLAGS;
			assert(dst.tag == ADDR_REGISTER);
			asm_unassign_register(ctx, dst.as.i);
		} break;
		case ir_op_div: asm_emit_div_mod(ins, code, false); break;
		case ir_op_mod: asm_emit_div_mod(ins, code, true); break;
		case ir_op_lnot:		FAILWITH("TODO: ir_op_lnot");		break;
		case ir_op_land:		FAILWITH("TODO: ir_op_land");		break;
		case ir_op_lor:			FAILWITH("TODO: ir_op_lor");		break;
		case ir_op_xor:			FAILWITH("TODO: ir_op_xor");		break;
		case ir_op_shl: asm_op = type_is_signed_integer(ins->type) ? "sal" : "shl"; goto LBL_shift;
		case ir_op_shr: asm_op = type_is_signed_integer(ins->type) ? "sar" : "shr"; goto LBL_shift;
		LBL_shift:
		{
			struct asm_address *dst = &ctx->vars[ins->dst];
			struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
			struct asm_address *y   = &ctx->vars[ins->arg.rx[1]];
			if (MATCH_ADDR3(ADDR_REGISTER, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
				const char *dst_name = asm_reg_name(dst->as.i, ins->type);
				int suffix = asm_suffix(ins->type);
				append_line(&code->body, fmt_str(
								"\tmov%c %d(%%rbp), %s\n",
								suffix,
								x->as.stack[0] + x->as.stack[1],
								dst_name));
				append_line(&code->body, fmt_str(
								"\t%s%c $%ld, %s\n",
								asm_op,
								suffix,
								y->as.i,
								dst_name));
			} else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
				dst->tag = ADDR_REGISTER;
				dst->as.i = asm_assign_register(ctx, ins->dst);
				const char *dst_name = asm_reg_name(dst->as.i, ins->type);
				int suffix = asm_suffix(ins->type);
				append_line(&code->body, fmt_str(
								"\tmov%c %d(%%rbp), %s\n",
								suffix,
								x->as.stack[0] + x->as.stack[1],
								dst_name));
				append_line(&code->body, fmt_str(
								"\t%s%c $%ld, %s\n",
								asm_op,
								suffix,
								y->as.i,
								dst_name));
			} else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK_LOAD, ADDR_STACK_LOAD)) {
				asm_reserve_register(ctx, asm_reg_rcx, ins->arg.rx[1]);
				dst->tag = ADDR_REGISTER;
				dst->as.i = asm_assign_register(ctx, ins->dst);
				int suffix = asm_suffix(ins->type);
				const char *dst_name = asm_reg_name(dst->as.i, ins->type);
				append_line(&code->body, fmt_str(
								"\tmov%c %d(%%rbp), %s\n",
								suffix,
								x->as.stack[0] + x->as.stack[1],
								dst_name));
				append_line(&code->body, fmt_str(
								"\tmov%c %d(%%rbp), %s\n",
								suffix,
								y->as.stack[0] + y->as.stack[1],
								asm_reg_name(asm_reg_rcx, ins->type)));
				append_line(&code->body, fmt_str(
								"\t%s%c %s, %s\n",
								asm_op,
								suffix,
								asm_reg_b_name[asm_reg_rcx],
								dst_name));
				asm_unassign_register(ctx, asm_reg_rcx);
			} else {
				FAILWITH("Unhandled case: MATCH_ADDR3(%s, %s, %s)",
						 asm_addr_tag_to_str(dst->tag),
						 asm_addr_tag_to_str(x->tag),
						 asm_addr_tag_to_str(y->tag));
			}
		} break;
		case ir_op_neg: {
			struct asm_address *dst = &ctx->vars[ins->dst];
			struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
			BEGIN_MATCH2(dst, x);
			if (MATCH_ADDR2(ADDR_BLK_ARG, ADDR_STACK_LOAD)) {
				struct asm_address *arg_addr = &ctx->vars[dst->as.i];
				if (arg_addr->tag == ADDR_TEMP_REG) {
					arg_addr->tag = ADDR_REGISTER;
					arg_addr->as.i = asm_assign_callee_save_register(ctx, ins->dst);
				}
				assert(arg_addr->tag == ADDR_REGISTER);
				dst = arg_addr;
				append_line(&code->body, fmt_str(
								"\tmov%c %d(%%rbp), %s\n",
								asm_suffix(ins->type),
								x->as.stack[0] + x->as.stack[1],
								asm_reg_name(dst->as.i, ins->type)));
				append_line(&code->body, fmt_str(
								"\tneg%c %s\n",
								asm_suffix(ins->type),
								asm_reg_name(dst->as.i, ins->type)));
			} else if (MATCH_ADDR2(ADDR_TEMP_REG, ADDR_STACK_LOAD)) {
				dst->tag = ADDR_REGISTER;
				dst->as.i = asm_assign_register(ctx, ins->dst);
				append_line(&code->body, fmt_str(
								"\tmov%c %d(%%rbp), %s\n",
								asm_suffix(ins->type),
								x->as.stack[0] + x->as.stack[1],
								asm_reg_name(dst->as.i, ins->type)));
				append_line(&code->body, fmt_str(
								"\tneg%c %s\n",
								asm_suffix(ins->type),
								asm_reg_name(dst->as.i, ins->type)));
			} else if (MATCH_ADDR2(ADDR_ARGUMENT, ADDR_IMM_INT)) {
				dst->extra.defered.fun = emit_op_neg_ADDR_REGISTER__ADDR_IMM_INT;
				dst->extra.defered.ins = ins;
			} else {
				FAILWITH("Unhandled case: MATCH_ADDR2(%s, %s)",
						 asm_addr_tag_to_str(dst->tag),
						 asm_addr_tag_to_str(x->tag));
			}
		} break;
		case ir_op_cast: {
			struct asm_address *dst = &ctx->vars[ins->dst];
			struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
			BEGIN_MATCH2(dst, x);
			if (MATCH_ADDR2(ADDR_ARGUMENT, ADDR_STACK_LOAD)) {
				dst->extra.defered.fun = emit_op_cast_ADDR_REGISTER__ADDR_STACK_LOAD;
				dst->extra.defered.ins = ins;
			} else if (MATCH_ADDR2(ADDR_TEMP_REG, ADDR_STACK_LOAD)) {
				dst->tag = ADDR_REGISTER;
				dst->as.i = asm_assign_register(ctx, ins->dst);
				emit_op_cast_ADDR_REGISTER__ADDR_STACK_LOAD(ins, NULL, code);
			} else if (MATCH_ADDR2(ADDR_TEMP_REG, ADDR_IMM_INT)) {
				dst->tag = ADDR_REGISTER;
				dst->as.i = asm_assign_register(ctx, ins->dst);
				emit_op_cast_ADDR_REGISTER__ADDR_IMM_INT(ins, NULL, code);
			} else if (MATCH_ADDR2(ADDR_ARGUMENT, ADDR_IMM_INT)) {
				dst->extra.defered.fun = emit_op_cast_ADDR_REGISTER__ADDR_IMM_INT;
				dst->extra.defered.ins = ins;
			} else if (MATCH_ADDR2(ADDR_ARGUMENT, ADDR_REGISTER)) {
				dst->extra.defered.fun = emit_op_cast_ADDR_REGISTER__ADDR_REGISTER;
				dst->extra.defered.ins = ins;
			} else {
				FAILWITH("Unhandled case: MATCH_ADDR2(%s, %s)",
						 asm_addr_tag_to_str(dst->tag),
						 asm_addr_tag_to_str(x->tag));
			}
		} break;
		case ir_op_conslice: {
			struct asm_address *dst = &ctx->vars[ins->dst];
			struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
			struct asm_address *y   = &ctx->vars[ins->arg.rx[1]];
			if (MATCH_ADDR3(ADDR_TEMP_WIDE, ADDR_REGISTER, ADDR_IMM_INT)) {
				dst->tag = ADDR_WIDE;
				dst->as.wide[0] = asm_assign_register(ctx, ins->dst);
				dst->as.wide[1] = asm_assign_register(ctx, ins->dst);
				emit_op_conslice_ADDR_WIDE__ADDR_REGISTER__ADDR_IMM_INT(ins, NULL, code);
			} else if (MATCH_ADDR3(ADDR_WIDE_ARG, ADDR_REGISTER, ADDR_IMM_INT)) {
				dst->extra.defered.fun = emit_op_conslice_ADDR_WIDE__ADDR_REGISTER__ADDR_IMM_INT;
				dst->extra.defered.ins = ins;
			} else if (MATCH_ADDR3(ADDR_WIDE_ARG, ADDR_REGISTER, ADDR_REGISTER)) {
				dst->extra.defered.fun = emit_op_conslice_ADDR_WIDE__ADDR_REGISTER__ADDR_REGISTER;
				dst->extra.defered.ins = ins;
			} else if (MATCH_ADDR3(ADDR_TEMP_WIDE, ADDR_STACK, ADDR_IMM_INT)) {
				dst->tag = ADDR_WIDE;
				dst->as.wide[0] = asm_assign_register(ctx, ins->dst);
				dst->as.wide[1] = asm_assign_register(ctx, ins->dst);
				emit_op_conslice_ADDR_WIDE__ADDR_STACK__ADDR_IMM_INT(ins, NULL, code);
			} else if (MATCH_ADDR3(ADDR_WIDE_ARG, ADDR_STACK, ADDR_IMM_INT)) {
				dst->extra.defered.fun = emit_op_conslice_ADDR_WIDE__ADDR_STACK__ADDR_IMM_INT;
				dst->extra.defered.ins = ins;
			} else if (MATCH_ADDR3(ADDR_WIDE, ADDR_REGISTER, ADDR_IMM_INT)) {
				emit_op_conslice_ADDR_WIDE__ADDR_REGISTER__ADDR_IMM_INT(ins, NULL, code);
			} else if (MATCH_ADDR3(ADDR_WIDE, ADDR_STACK, ADDR_IMM_INT)) {
				emit_op_conslice_ADDR_WIDE__ADDR_STACK__ADDR_IMM_INT(ins, NULL, code);
			} else if (MATCH_ADDR3(ADDR_WIDE, ADDR_REGISTER, ADDR_STACK_LOAD)) {
				emit_op_conslice_ADDR_WIDE__ADDR_REGISTER__ADDR_STACK_LOAD(ins, NULL, code);
			} else {
				FAILWITH("Unhandled case: MATCH_ADDR3(%s, %s, %s)",
						 asm_addr_tag_to_str(dst->tag),
						 asm_addr_tag_to_str(x->tag),
						 asm_addr_tag_to_str(y->tag));
			}
		} break;
		case ir_op_getelemptr: {
			struct asm_address *dst  = &ctx->vars[ins->dst];
			struct asm_address *base = &ctx->vars[ins->arg.rx[0]];
			struct asm_address *idx  = &ctx->vars[ins->arg.rx[1]];
			if (type_is_array_ptr(ins->type)) {
				struct type *base_type = ins->type->as.ptr->as.array.base;
				size_t scale = type_size(base_type);
				if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK, ADDR_IMM_INT)) {
					dst->tag = ADDR_REGISTER;
					dst->as.i = asm_assign_register(ctx, ins->dst);
					append_line(&code->body, fmt_str(
									"\tleaq %ld(%%rbp), %s\n",
									base->as.i + (idx->as.i * scale),
									asm_reg_q_name[dst->as.i]));
				} else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_REGISTER, ADDR_IMM_INT)) {
					asm_unassign_register(ctx, base->as.i);
					dst->tag = ADDR_REGISTER;
					dst->as.i = asm_assign_register(ctx, ins->dst);
					append_line(&code->body, fmt_str(
									"\tleaq %ld(%s), %s\n",
									idx->as.i * scale,
									asm_reg_q_name[base->as.i],
									asm_reg_q_name[dst->as.i]));
				} else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_REGISTER, ADDR_STACK_LOAD)) {
					assert(idx->type);
					dst->tag = ADDR_REGISTER;
					dst->as.i = asm_assign_register(ctx, ins->dst);
					/* load index into dst */
					append_line(&code->body, fmt_str(
									"\tmov%c %d(%%rbp), %s\n",
									asm_suffix(idx->type),
									idx->as.stack[0] + idx->as.stack[1],
									asm_reg_name(dst->as.i, idx->type)));
					if (type_is_scalar(base_type)) {
						append_line(&code->body, fmt_str(
										"\tleaq (%s, %s, %zu), %s\n",
										asm_reg_q_name[base->as.i],
										asm_reg_q_name[dst->as.i],
										scale,
										asm_reg_q_name[dst->as.i]));
					} else {
						FAILWITH("TODO: index array of non-scalar type");
					}
					asm_unassign_register(ctx, base->as.i);
				} else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
					dst->tag = ADDR_REGISTER;
					dst->as.i = asm_assign_register(ctx, ins->dst);
					const char *dst_name = asm_reg_q_name[dst->as.i];
					append_line(&code->body, fmt_str(
									"\tmov%c %d(%%rbp), %s\n",
									asm_suffix(base->type),
									base->as.stack[0] + base->as.stack[1],
									asm_reg_name(dst->as.i, base->type)));
					append_line(&code->body, fmt_str(
									"\tleaq %ld(%s), %s\n",
									idx->as.i * scale,
									dst_name,
									dst_name));
				} else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK_LOAD, ADDR_STACK_LOAD)) {
					dst->tag = ADDR_REGISTER;
					dst->as.i = asm_assign_register(ctx, ins->dst);
					int tmp = asm_assign_register(ctx, ins->arg.rx[1]);
					const char *dst_name = asm_reg_q_name[dst->as.i];
					const char *tmp_name = asm_reg_name(tmp, idx->type);
					append_line(&code->body, fmt_str(
									"\tmov%c %d(%%rbp), %s\n",
									asm_suffix(base->type),
									base->as.stack[0] + base->as.stack[1],
									asm_reg_name(dst->as.i, base->type)));
					append_line(&code->body, fmt_str(
									"\tmov%c %d(%%rbp), %s\n",
									asm_suffix(idx->type),
									idx->as.stack[0] + idx->as.stack[1],
									tmp_name));
					append_line(&code->body, fmt_str(
									"\tleaq (%s, %s, %zu), %s\n",
									dst_name,
									tmp_name,
									scale,
									dst_name));
					asm_unassign_register(ctx, tmp);
				} else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_BLK_ARG, ADDR_IMM_INT)) {
					dst->tag = ADDR_REGISTER;
					dst->as.i = asm_assign_register(ctx, ins->dst);
					const char *dst_name = asm_reg_q_name[dst->as.i];
					base = &ctx->vars[base->as.i];
					if (base->tag == ADDR_STACK_LOAD) {
						append_line(&code->body, fmt_str(
										"\tmov%c %d(%%rbp), %s\n",
										asm_suffix(base->type),
										base->as.stack[0] + base->as.stack[1],
										asm_reg_name(dst->as.i, base->type)));
						append_line(&code->body, fmt_str(
										"\tleaq %ld(%s), %s\n",
										idx->as.i * scale,
										dst_name,
										dst_name));
					} else {
						FAILWITH("Unhandled case: (base->tag == %s)", asm_addr_tag_to_str(base->tag));
					}
				} else {
					printf("dst = %%%d\n", ins->dst);
					FAILWITH("Unhandled case: MATCH_ADDR3(%s, %s, %s)",
							 asm_addr_tag_to_str(dst->tag),
							 asm_addr_tag_to_str(base->tag),
							 asm_addr_tag_to_str(idx->tag));
				}
			} else if (type_is_struct_ptr(ins->type, NULL)) {
				if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK, ADDR_IMM_INT)) {
					dst->tag = ADDR_REGISTER;
					dst->as.i = asm_assign_register(ctx, ins->dst);
					size_t offset = struct_member_offset(&ins->type->as.ptr->as.struct_t, idx->as.i);
					append_line(&code->body, fmt_str(
									"\tleaq %ld(%%rbp), %s\n",
									base->as.i + offset,
									asm_reg_q_name[dst->as.i]));
				} else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
					dst->tag = ADDR_REGISTER;
					dst->as.i = asm_assign_register(ctx, ins->dst);
					size_t offset = struct_member_offset(&ins->type->as.ptr->as.struct_t, idx->as.i);
					append_line(&code->body, fmt_str(
									"\t/* getelemptr */\n"
									"\tmovq %d(%%rbp), %s\n",
									base->as.stack[0] + base->as.stack[1],
									asm_reg_q_name[dst->as.i]));
					if (offset > 0) {
						append_line(&code->body, fmt_str(
										"\tleaq %zu(%s), %s\n",
										offset,
										asm_reg_q_name[dst->as.i],
										asm_reg_q_name[dst->as.i]));
					}
				} else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_REGISTER, ADDR_IMM_INT)) {
					asm_unassign_register(ctx, base->as.i);
					dst->tag = ADDR_REGISTER;
					dst->as.i = asm_assign_register(ctx, ins->dst);
					size_t offset = struct_member_offset(&ins->type->as.ptr->as.struct_t, idx->as.i);
					append_line(&code->body, fmt_str(
									"\tleaq %ld(%s), %s\n",
									offset,
									asm_reg_q_name[base->as.i],
									asm_reg_q_name[dst->as.i]));
				} else {
					FAILWITH("Unhandled case: MATCH_ADDR3(%s, %s, %s)",
							 asm_addr_tag_to_str(dst->tag),
							 asm_addr_tag_to_str(base->tag),
							 asm_addr_tag_to_str(idx->tag));
				}
			} else if (type_is_slice(ins->type)) {
				if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_REGISTER, ADDR_IMM_INT)) {
					asm_unassign_register(ctx, base->as.i);
					dst->tag = ADDR_REGISTER;
					dst->as.i = asm_assign_register(ctx, ins->dst);
					append_line(&code->body, fmt_str(
									"\tleaq %ld(%s), %s\n",
									idx->as.i * 8,
									asm_reg_q_name[base->as.i],
									asm_reg_q_name[dst->as.i]));
				} else {
					FAILWITH("Unhandled case: MATCH_ADDR3(%s, %s, %s)",
							 asm_addr_tag_to_str(dst->tag),
							 asm_addr_tag_to_str(base->tag),
							 asm_addr_tag_to_str(idx->tag));
				}
			} else if (ins->type->tag == ast_type_union) {
				if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_REGISTER, ADDR_IMM_INT)) {
					asm_unassign_register(ctx, base->as.i);
					dst->tag = ADDR_REGISTER;
					dst->as.i = asm_assign_register(ctx, ins->dst);
					append_line(&code->body, fmt_str(
									"\tleaq %ld(%s), %s\n",
									idx->as.i * 8,
									asm_reg_q_name[base->as.i],
									asm_reg_q_name[dst->as.i]));
				} else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_BLK_ARG, ADDR_IMM_INT)) {
					dst->tag = ADDR_REGISTER;
					dst->as.i = asm_assign_register(ctx, ins->dst);
					base = &ctx->vars[base->as.i];
					if (base->tag == ADDR_STACK) {
						append_line(&code->body, fmt_str(
										"\tleaq %ld(%%rbp), %s\n",
										base->as.i + idx->as.i * 8,
										asm_reg_q_name[dst->as.i]));
					} else if (base->tag == ADDR_STACK_LOAD) {
						append_line(&code->body, fmt_str(
										"\tmovq %d(%%rbp), %s\n",
										base->as.stack[0] + base->as.stack[1],
										asm_reg_q_name[dst->as.i]));
						append_line(&code->body, fmt_str(
										"\tleaq %ld(%s), %s\n",
										idx->as.i * 8,
										asm_reg_q_name[dst->as.i],
										asm_reg_q_name[dst->as.i]));
					} else {
						FAILWITH("Unhandled case: (base->tag == %s)", asm_addr_tag_to_str(base->tag));
					}
				} else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK, ADDR_IMM_INT)) {
					dst->tag = ADDR_REGISTER;
					dst->as.i = asm_assign_register(ctx, ins->dst);
					append_line(&code->body, fmt_str(
									"\tleaq %ld(%%rbp), %s\n",
									base->as.i + idx->as.i * 8,
									asm_reg_q_name[dst->as.i]));
				} else {
					FAILWITH("Unhandled case (ir_op_getelemptr %%%d): MATCH_ADDR3(%s, %s, %s)",
							 ins->dst,
							 asm_addr_tag_to_str(dst->tag),
							 asm_addr_tag_to_str(base->tag),
							 asm_addr_tag_to_str(idx->tag));
				}
			} else {
				printf("ins->type = %s\n", ast_type_to_str(ins->type));
				FAILWITH("TODO: ir_op_getelemptr");
			}
		} break;
		case ir_op_retval: {
			struct asm_address *dst = &ctx->vars[ins->dst];
			if (dst->tag == ADDR_BLK_ARG) dst = &ctx->vars[dst->as.i];
			assert(dst->tag == ADDR_STACK_LOAD);
		} break;
		case ir_op_alloca: /* nothing to do */ break;
		case ir_op_load: {
			struct asm_address *dst = &ctx->vars[ins->dst];
			struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
			BEGIN_MATCH2(dst, x);
			if (dst->tag == ADDR_STACK_LOAD) {
				/* do nothing */
			} else if (MATCH_ADDR2(ADDR_REGISTER, ADDR_REGISTER)) {
				append_line(&code->body, fmt_str(
								"\tmov%c (%s), %s\n",
								asm_suffix(ins->type),
								asm_reg_q_name[x->as.i],
								asm_reg_name(dst->as.i, ins->type)));
			} else if (MATCH_ADDR2(ADDR_BLK_ARG, ADDR_REGISTER)) {
				dst = &ctx->vars[dst->as.i];
				if (dst->tag == ADDR_TEMP_REG) {
					dst->tag = ADDR_REGISTER;
					dst->as.i = asm_assign_callee_save_register(ctx, ins->dst);
				} else {
					assert(dst->tag == ADDR_REGISTER);
					if (asm_register_assigned_p(ctx, dst->as.i)) {
						/* check if register was already assigned in another branch */
						struct asm_address *owner = &ctx->vars[asm_get_register_owner(ctx, dst->as.i)];
						assert(owner->tag == ADDR_BLK_ARG);
					} else {
						asm_reserve_register(ctx, dst->as.i, ins->dst);
					}
				}
				/* ADDR_REGISTER */
				append_line(&code->body, fmt_str(
								"\tmov%c (%s), %s\n",
								asm_suffix(ins->type),
								asm_reg_q_name[x->as.i],
								asm_reg_name(dst->as.i, ins->type)));
			} else if (MATCH_ADDR2(ADDR_BLK_ARG, ADDR_STACK)) {
				dst = &ctx->vars[dst->as.i];
				if (dst->tag == ADDR_STACK_LOAD) {
					size_t ts = type_size(ins->type);
					if (ts <= 8) {
						dst->tag = ADDR_REGISTER;
						dst->as.i = asm_assign_register(ctx, ins->dst);
						append_line(&code->body, fmt_str(
										"\tmov%c %ld(%%rbp), %s\n",
										asm_suffix(ins->type),
										x->as.i,
										asm_reg_name(dst->as.i, ins->type)));
					} else if (ts <= 16) {
						dst->tag = ADDR_WIDE;
						dst->as.wide[0] = asm_assign_register(ctx, ins->dst);
						dst->as.wide[1] = asm_assign_register(ctx, ins->dst);
						append_line(&code->body, fmt_str(
										"\tmovq %ld(%%rbp), %s\n",
										x->as.i,
										asm_reg_q_name[dst->as.wide[0]]));
						append_line(&code->body, fmt_str(
										"\tmovq %ld(%%rbp), %s\n",
										x->as.i + 8,
										asm_reg_q_name[dst->as.wide[1]]));
					} else {
						FAILWITH("TODO");
					}
				} else if (dst->tag == ADDR_WIDE) {
					if (asm_register_assigned_p(ctx, dst->as.wide[0])) {
						/* check if register was already assigned in another branch */
						int owner_var = asm_get_register_owner(ctx, dst->as.wide[0]);
						struct asm_address *owner = &ctx->vars[owner_var];
						assert(owner->tag == ADDR_BLK_ARG);
					} else {
						asm_reserve_register(ctx, dst->as.wide[0], ins->dst);
						asm_reserve_register(ctx, dst->as.wide[1], ins->dst);
					}
					append_line(&code->body, fmt_str(
									"\tmovq %ld(%%rbp), %s\n",
									x->as.i,
									asm_reg_q_name[dst->as.wide[0]]));
					append_line(&code->body, fmt_str(
									"\tmovq %ld(%%rbp), %s\n",
									x->as.i + 8,
									asm_reg_q_name[dst->as.wide[1]]));
				} else if (dst->tag == ADDR_TEMP_REG) {
					dst->tag = ADDR_REGISTER;
					dst->as.i = asm_assign_callee_save_register(ctx, ins->dst);
					append_line(&code->body, fmt_str(
									"\tmov%c %ld(%%rbp), %s\n",
									asm_suffix(ins->type),
									x->as.i,
									asm_reg_name(dst->as.i, ins->type)));
				} else if (dst->tag == ADDR_REGISTER) {
					if (asm_register_assigned_p(ctx, dst->as.i)) {
						/* check if register was already assigned in another branch */
						struct asm_address *owner = &ctx->vars[asm_get_register_owner(ctx, dst->as.i)];
						assert(owner->tag == ADDR_BLK_ARG);
					} else {
						asm_reserve_register(ctx, dst->as.i, ins->dst);
					}
					append_line(&code->body, fmt_str(
									"\tmov%c %ld(%%rbp), %s\n",
									asm_suffix(ins->type),
									x->as.i,
									asm_reg_name(dst->as.i, ins->type)));
				} else {
					printf("reg = %%%d\n", ins->dst);
					FAILWITH("Unhandled case: (dst->tag == %s)", asm_addr_tag_to_str(dst->tag));
				}
			} else if (MATCH_ADDR2(ADDR_REGISTER, ADDR_STACK)) {
				emit_op_load_ADDR_REGISTER__ADDR_STACK(ins, NULL, code);
			} else if (MATCH_ADDR2(ADDR_PUSH_ARG, ADDR_STACK)) {
				dst->extra.defered.fun = emit_op_load_ADDR_PUSH_ARG__ADDR_STACK;
				dst->extra.defered.ins = ins;
			} else if (MATCH_ADDR2(ADDR_WIDE, ADDR_STACK)) {
				emit_op_load_ADDR_WIDE__ADDR_STACK(ins, NULL, code);
			} else if (MATCH_ADDR2(ADDR_WIDE_ARG, ADDR_STACK)) {
				dst->extra.defered.fun = emit_op_load_ADDR_WIDE__ADDR_STACK;
				dst->extra.defered.ins = ins;
			} else if (MATCH_ADDR2(ADDR_WIDE_ARG, ADDR_REGISTER)) {
				dst->extra.defered.fun = emit_op_load_ADDR_WIDE__ADDR_REGISTER;
				dst->extra.defered.ins = ins;
			} else if (MATCH_ADDR2(ADDR_ARGUMENT, ADDR_STACK)) {
				dst->extra.defered.fun = emit_op_load_ADDR_REGISTER__ADDR_STACK;
				dst->extra.defered.ins = ins;
			} else if (MATCH_ADDR2(ADDR_ARGUMENT, ADDR_REGISTER)) {
				dst->extra.defered.fun = emit_op_load_ADDR_REGISTER__ADDR_REGISTER;
				dst->extra.defered.ins = ins;
			} else if (MATCH_ADDR2(ADDR_TEMP_REG, ADDR_REGISTER)) {
				dst->tag = ADDR_REGISTER;
				dst->as.i = asm_assign_register(ctx, ins->dst);
				if (type_size(ins->type) > 8) {
					FAILWITH("invalid load: dst = %%%d, x = %%%d",
							 ins->dst,
							 ins->arg.rx[0]);
				}
				append_line(&code->body, fmt_str(
								"\tmov%c (%s), %s\n",
								asm_suffix(ins->type),
								asm_reg_q_name[x->as.i],
								asm_reg_name(dst->as.i, ins->type)));
				asm_unassign_register(ctx, x->as.i);
			} else if (MATCH_ADDR2(ADDR_TEMP_WIDE, ADDR_REGISTER)) {
				dst->tag = ADDR_WIDE;
				dst->as.wide[0] = asm_assign_register(ctx, ins->dst);
				dst->as.wide[1] = asm_assign_register(ctx, ins->dst);
				append_line(&code->body, fmt_str(
								"\tmovq (%s), %s\n",
								asm_reg_q_name[x->as.i],
								asm_reg_q_name[dst->as.wide[0]]));
				append_line(&code->body, fmt_str(
								"\tmovq 8(%s), %s\n",
								asm_reg_q_name[x->as.i],
								asm_reg_q_name[dst->as.wide[1]]));
				asm_unassign_register(ctx, x->as.i);
			} else if (MATCH_ADDR2(ADDR_ARGUMENT, ADDR_BLK_ARG)) {
				x = &ctx->vars[x->as.i];
				switch ((int)x->tag) {
				case ADDR_STACK: {
					dst->extra.defered.fun = emit_op_load_ADDR_REGISTER__ADDR_STACK;
					dst->extra.defered.ins = ins;
				} break;
				default:
					FAILWITH("Unhandled case %s:", asm_addr_tag_to_str(x->tag));
					break;
				}
			} else {
				printf("ins->dst = %%%d\n", ins->dst);
				FAILWITH("Unhandled case: MATCH_ADDR2(%s, %s)",
						 asm_addr_tag_to_str(dst->tag),
						 asm_addr_tag_to_str(x->tag));
			}
		} break;
		case ir_op_loadglobl: {
			struct asm_address *dst = &ctx->vars[ins->dst];
			if (dst->tag == ADDR_SYMBOL) {
				/* do nothing */
			} else if (dst->tag == ADDR_WIDE_ARG) {
				dst->extra.defered.fun = emit_op_load_ADDR_WIDE__GLOBL;
				dst->extra.defered.ins = ins;
				dst->extra.defered.dat = tl;
			} else if (dst->tag == ADDR_ARGUMENT) {
				dst->extra.defered.fun = emit_op_load_ADDR_REGISTER__GLOBL;
				dst->extra.defered.ins = ins;
				dst->extra.defered.dat = tl;
			} else if (dst->tag == ADDR_BLK_ARG) {
				struct asm_address *arg_addr = &ctx->vars[dst->as.i]; // lookup the addr for the formal block arg
				if (arg_addr->tag == ADDR_TEMP_REG) {
					arg_addr->tag = ADDR_REGISTER;
					arg_addr->as.i = asm_assign_callee_save_register(ctx, ins->dst);
				}
				if (arg_addr->tag == ADDR_WIDE_ARG) {
					dst = arg_addr;
					union ir_object *obj = &tl->elems[ins->arg.u32];
					struct type *base_type = ins->type->as.slice;
					asm_reserve_register(ctx, dst->as.wide[0], ins->dst);
					asm_reserve_register(ctx, dst->as.wide[1], ins->dst);
					if (tl->is_dll && !obj->hddr.is_static) {
						append_line(&code->body, fmt_str("\tmovq %s(%%rip), %s\n",
														 ir_object_link_name(tl, obj),
														 asm_reg_q_name[dst->as.wide[0]]));
					} else {
						append_line(&code->body, fmt_str("\tleaq %s(%%rip), %s\n",
														 ir_object_link_name(tl, obj),
														 asm_reg_q_name[dst->as.wide[0]]));
					}
					append_line(&code->body, fmt_str("\tmovq $%zu, %s\n",
													 obj->data.size / type_size(base_type),
													 asm_reg_q_name[dst->as.wide[1]]));
				} else {
					FAILWITH("TODO: unhandled case arg_addr->tag == %s", asm_addr_tag_to_str(arg_addr->tag));
				}
			}
		} break;
		case ir_op_loadconst: FAILWITH("TODO: ir_op_loadconst"); break;
		case ir_op_loadimm: {
			struct asm_address *dst = &ctx->vars[ins->dst];
			if (dst->tag == ADDR_IMM_INT) {
				/* Do nothing */
			} else if (dst->tag == ADDR_REGISTER) {
				emit_op_loadimm_ADDR_REGISTER(ins, NULL, code);
			} else if (dst->tag == ADDR_ARGUMENT) {
				dst->extra.defered.fun = emit_op_loadimm_ADDR_REGISTER;
				dst->extra.defered.ins = ins;
			} else if (dst->tag == ADDR_PUSH_ARG) {
				dst->extra.defered.fun = emit_op_loadimm_ADDR_PUSH_ARG;
				dst->extra.defered.ins = ins;
			} else if (dst->tag == ADDR_BLK_ARG) {
				struct asm_address *arg_addr = &ctx->vars[dst->as.i]; // lookup the addr for the formal block arg
				if (arg_addr->tag == ADDR_TEMP_REG) {
					arg_addr->tag = ADDR_REGISTER;
					arg_addr->as.i = asm_assign_callee_save_register(ctx, ins->dst);
				}
				assert(arg_addr->tag == ADDR_REGISTER || arg_addr->tag == ADDR_ARGUMENT);
				dst = arg_addr;
				append_line(&code->body, fmt_str(
								"\tmov%c $%d, %s\n",
								asm_suffix(ins->type),
								ins->arg.i32,
								asm_reg_name(dst->as.i, ins->type)));
			} else {
				FAILWITH("TODO: unhandled case dst->tag == %s", asm_addr_tag_to_str(dst->tag));
			}
		} break;
		case ir_op_store: {
			struct asm_address *dst = &ctx->vars[ins->dst];
			struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
			if (dst->tag == ADDR_BLK_ARG) dst = &ctx->vars[dst->as.i];
			if (x->tag   == ADDR_BLK_ARG) x   = &ctx->vars[x->as.i];
			BEGIN_MATCH2(dst, x);
			if (MATCH_ADDR2(ADDR_REGISTER, ADDR_IMM_INT)) {
				append_line(&code->body, fmt_str(
								"\tmov%c $%ld, (%s)\n",
								asm_suffix(ins->type),
								x->as.i,
								asm_reg_q_name[dst->as.i]));
				asm_unassign_register(ctx, dst->as.i);
			} else if (MATCH_ADDR2(ADDR_REGISTER, ADDR_WIDE)) {
				append_line(&code->body, fmt_str(
								"\tmovq %s, (%s)\n",
								asm_reg_q_name[x->as.wide[0]],
								asm_reg_q_name[dst->as.i]));
				append_line(&code->body, fmt_str(
								"\tmovq %s, 8(%s)\n",
								asm_reg_q_name[x->as.wide[1]],
								asm_reg_q_name[dst->as.i]));
				asm_unassign_register(ctx, dst->as.i);
				asm_unassign_register(ctx, x->as.wide[0]);
				asm_unassign_register(ctx, x->as.wide[1]);
			} else if (MATCH_ADDR2(ADDR_STACK, ADDR_REGISTER)) {
				append_line(&code->body, fmt_str(
								"\tmov%c %s, %ld(%%rbp)\n",
								asm_suffix(ins->type),
								asm_reg_name(x->as.i, ins->type),
								dst->as.i));
				asm_unassign_register(ctx, x->as.i);
			} else if (MATCH_ADDR2(ADDR_STACK, ADDR_STACK_ARG)) {
				/* do nothing */
			} else if (MATCH_ADDR2(ADDR_REGISTER, ADDR_STACK_LOAD)) {
				enum asm_register tmp = asm_assign_register(ctx, ins->arg.rx[0]);
				append_line(&code->body, fmt_str(
								"\tmov%c %d(%%rbp), %s\n",
								asm_suffix(ins->type),
								x->as.stack[0] + x->as.stack[1],
								asm_reg_name(tmp, ins->type)));
				append_line(&code->body, fmt_str(
								"\tmov%c %s, (%s)\n",
								asm_suffix(ins->type),
								asm_reg_name(tmp, ins->type),
								asm_reg_q_name[dst->as.i]));
				asm_unassign_register(ctx, tmp);
				asm_unassign_register(ctx, dst->as.i);
			} else if (MATCH_ADDR2(ADDR_STACK, ADDR_WIDE)) {
				size_t ts = type_size(ins->type);
				if (ts == 16) {
					append_line(&code->body, fmt_str(
									"\tmovq %s, %ld(%%rbp)\n",
									asm_reg_q_name[x->as.wide[0]],
									dst->as.i));
					append_line(&code->body, fmt_str(
									"\tmovq %s, %ld(%%rbp)\n",
									asm_reg_q_name[x->as.wide[1]],
									dst->as.i + 8));
					asm_unassign_register(ctx, x->as.wide[0]);
					asm_unassign_register(ctx, x->as.wide[1]);
				} else {
					FAILWITH("TODO");
				}
			} else if (MATCH_ADDR2(ADDR_STACK, ADDR_WIDE_ARG)) {
				size_t ts = type_size(ins->type);
				if (ts == 16) {
					/* TODO: BUG */
					append_line(&code->body, fmt_str("/* DEBUG %s:%d */\n", __FILE__, __LINE__));
					append_line(&code->body, fmt_str(
									"\tmovq %s, %ld(%%rbp)\n",
									asm_reg_q_name[x->as.wide[0]],
									dst->as.i));
					append_line(&code->body, fmt_str(
									"\tmovq %s, %ld(%%rbp)\n",
									asm_reg_q_name[x->as.wide[1]],
									dst->as.i + 8));
				} else {
					FAILWITH("TODO");
				}
			} else if (MATCH_ADDR2(ADDR_STACK, ADDR_IMM_INT)) {
				append_line(&code->body, fmt_str(
								"\tmov%c $%ld, %ld(%%rbp)\n",
								asm_suffix(ins->type),
								x->as.i,
								dst->as.i));
			} else if (MATCH_ADDR2(ADDR_REGISTER, ADDR_SYMBOL)) {
				union ir_object *obj = &tl->elems[x->as.i];
				assert(obj->tag == IRO_DATA);
				if (type_is_slice(ins->type)) {
					struct type *base_type = ins->type->as.slice;
					enum asm_register tmp = asm_assign_register(ctx, ins->arg.rx[0]);
					if (tl->is_dll && !obj->hddr.is_static) {
						append_line(&code->body, fmt_str("\tmovq %s(%%rip), %s\n",
														 ir_object_link_name(tl, obj),
														 asm_reg_q_name[tmp]));
					} else {
						append_line(&code->body, fmt_str("\tleaq %s(%%rip), %s\n",
														 ir_object_link_name(tl, obj),
														 asm_reg_q_name[tmp]));
					}
					append_line(&code->body, fmt_str("\tmovq %s, (%s)\n",
													 asm_reg_q_name[tmp],
													 asm_reg_q_name[dst->as.i]));
					append_line(&code->body, fmt_str("\tmovq $%zu, 8(%s)\n",
													 obj->data.size / type_size(base_type),
													 asm_reg_q_name[dst->as.i]));
					asm_unassign_register(ctx, dst->as.i);
					asm_unassign_register(ctx, tmp);
				} else {
					FAILWITH("TODO: MATCH_ADDR2(ADDR_REGISTER, ADDR_SYMBOL)");
				}
			} else if (MATCH_ADDR2(ADDR_STACK, ADDR_SYMBOL)) {
				union ir_object *obj = &tl->elems[x->as.i];
				assert(obj->tag == IRO_DATA);
				if (type_is_slice(ins->type)) {
					struct type *base_type = ins->type->as.slice;
					enum asm_register tmp = asm_assign_register(ctx, ins->arg.rx[0]);
					if (tl->is_dll && !obj->hddr.is_static) {
						append_line(&code->body, fmt_str("\tmovq %s(%%rip), %s\n",
														 ir_object_link_name(tl, obj),
														 asm_reg_q_name[tmp]));
					} else {
						append_line(&code->body, fmt_str("\tleaq %s(%%rip), %s\n",
														 ir_object_link_name(tl, obj),
														 asm_reg_q_name[tmp]));
					}
					append_line(&code->body, fmt_str("\tmovq %s, %ld(%%rbp)\n",
													 asm_reg_q_name[tmp],
													 dst->as.i));
					append_line(&code->body, fmt_str("\tmovq $%zu, %ld(%%rbp)\n",
													 obj->data.size / type_size(base_type),
													 dst->as.i + 8));
					asm_unassign_register(ctx, tmp);
				} else {
					FAILWITH("TODO: MATCH_ADDR2(ADDR_REGISTER, ADDR_SYMBOL)");
				}
			} else if (MATCH_ADDR2(ADDR_STACK, ADDR_FLAGS)) {
				char *cc;
				switch (x->as.stack[0]) {
				case ir_op_cmpe:  cc = "e";  break;
				case ir_op_cmpne: cc = "ne"; break;
				case ir_op_cmpl:  cc = type_is_signed_scalar(x->type) ? "l" : "b";   break;
				case ir_op_cmpg:  cc = type_is_signed_scalar(x->type) ? "g" : "a";   break;
				case ir_op_cmple: cc = type_is_signed_scalar(x->type) ? "le" : "be"; break;
				case ir_op_cmpge: cc = type_is_signed_scalar(x->type) ? "ge" : "ae"; break;
				default: FAILWITH("Unreachable"); break;
				}
				append_line(&code->body, fmt_str("\tset%s %ld(%%rbp)\n", cc, dst->as.i));
			} else if (MATCH_ADDR2(ADDR_STACK, ADDR_STACK)) {
				/* Nothing to do? */
			} else if (MATCH_ADDR2(ADDR_STACK_LOAD, ADDR_IMM_INT)) {
				enum asm_register tmp = asm_assign_register(ctx, ins->dst);
				append_line(&code->body, fmt_str(
								"\tmovq %d(%%rbp), %s\n",
								dst->as.stack[0] + dst->as.stack[1],
								asm_reg_q_name[tmp]));
				append_line(&code->body, fmt_str(
								"\tmovq $%ld, (%s)\n",
								x->as.i,
								asm_reg_q_name[tmp]));
				asm_unassign_register(ctx, tmp);
			} else if (MATCH_ADDR2(ADDR_STACK, ADDR_STACK_LOAD)) {
				enum asm_register tmp = asm_assign_register(ctx, ins->dst);
				append_line(&code->body, fmt_str(
								"\tmov%c %d(%%rbp), %s\n",
								asm_suffix(ins->type),
								x->as.stack[0] + x->as.stack[1],
								asm_reg_name(tmp, ins->type)));
				append_line(&code->body, fmt_str(
								"\tmov%c %s, %ld(%%rbp)\n",
								asm_suffix(ins->type),
								asm_reg_name(tmp, ins->type),
								dst->as.i));
				asm_unassign_register(ctx, tmp);
			} else {
				printf("dst = %%%d\n", ins->dst);
				printf("x   = %%%d\n", ins->arg.rx[0]);
				FAILWITH("Unhandled case: MATCH_ADDR2(%s, %s)",
						 asm_addr_tag_to_str(dst->tag),
						 asm_addr_tag_to_str(x->tag));
			}
		} break;
		case ir_op_memzero: {
			struct asm_address *dst = &ctx->vars[ins->dst];
			if (dst->tag == ADDR_STACK) {
				size_t ts = type_size(ins->type);
				uint32_t counts[4] = {0};
				mem_copy_segment_count(ts, counts);
				uint32_t offset = 0;
				for (uint32_t i = 0; i < counts[0]; ++i) {
					append_line(&code->body, fmt_str("\tmovq $0, %ld(%%rbp)\n", dst->as.i + offset));
					offset += 8;
				}
				for (uint32_t i = 0; i < counts[1]; ++i) {
					append_line(&code->body, fmt_str("\tmovl $0, %ld(%%rbp)\n", dst->as.i + offset));
					offset += 4;
				}
				for (uint32_t i = 0; i < counts[2]; ++i) {
					append_line(&code->body, fmt_str("\tmovw $0, %ld(%%rbp)\n", dst->as.i + offset));
					offset += 2;
				}
				for (uint32_t i = 0; i < counts[3]; ++i) {
					append_line(&code->body, fmt_str("\tmovb $0, %ld(%%rbp)\n", dst->as.i + offset));
					offset += 1;
				}
			} else {
				FAILWITH("Unhandled case: dst->tag == %s",
						 asm_addr_tag_to_str(dst->tag));
			}
		} break;
		case ir_op_pushfunarg: {
			struct asm_address *arg = &ctx->vars[ins->dst];
			da_append(&ctx->funargs, arg);
			switch ((int)arg->tag) {
			case ADDR_ARGUMENT:
			case ADDR_WIDE_ARG:
			case ADDR_PUSH_ARG:
				if (arg->extra.defered.fun) {
					arg->extra.defered.fun(arg->extra.defered.ins, arg->extra.defered.dat, code);
				}
				break;
			default:
				FAILWITH("TODO: unhandled case arg->tag == %s (%%%d)",
						 asm_addr_tag_to_str(arg->tag), ins->dst);
				break;
			}
		} break;
		case ir_op_call: {
			size_t stack_size = 0;
			int argc = ins->arg.rx[1];
			while (argc--) {
				struct asm_address *arg = da_pop(&ctx->funargs);
				assert(arg->tag != ADDR_NONE);
				switch ((int)arg->tag) {
				case ADDR_REGISTER:
				case ADDR_ARGUMENT:
					asm_unassign_register(ctx, arg->as.i);
					break;
				case ADDR_WIDE_ARG:
					asm_unassign_register(ctx, arg->as.wide[0]);
					asm_unassign_register(ctx, arg->as.wide[1]);
					break;
				case ADDR_PUSH_ARG:
					stack_size += type_size(arg->type);
					stack_size = align_adjust(stack_size, 8);
					break;
				default:
					FAILWITH("TODO: unhandled case arg->tag == %s", asm_addr_tag_to_str(arg->tag));
					break;
				}
			}
			struct asm_address *ret = &ctx->vars[ins->dst];
			if (ret->tag == ADDR_STACK) {
				assert(asm_register_assigned_p(ctx, asm_arg_regs[0]) == false);
				append_line(&code->body, fmt_str(
								"\tleaq %ld(%%rbp), %s\n",
								ret->as.i,
								asm_reg_q_name[asm_arg_regs[0]]));
			}
			for (size_t i = 0; i < ARRAY_LENGTH(asm_caller_save_regs); ++i) {
				enum asm_register reg = asm_caller_save_regs[i];
				if (asm_register_assigned_p(ctx, reg)) {
					int var = ctx->assigned[reg];
					struct asm_address *addr = &ctx->vars[var];
					asm_emit_move_to_callee_save(addr, code);
				}
			}
			struct asm_address *p = &ctx->vars[ins->arg.rx[0]];
			if (p->tag == ADDR_SYMBOL) {
				append_line(&code->body, fmt_str("\tcall %s\n", ir_object_link_name(tl, &tl->elems[p->as.i])));
			} else {
				FAILWITH("TODO: unhandled case p->tag == %s", asm_addr_tag_to_str(p->tag));
			}
			if (stack_size) {
				append_line(&code->body, fmt_str("\taddq $%zu, %%rsp\n", stack_size));
				ctx->setup_frame = true;
			}
			if (!type_is_void(ins->type)) {
				switch ((int)ret->tag) {
				case ADDR_REGISTER: {
					asm_reserve_register(ctx, ret->as.i, ins->dst);
				} break;
				case ADDR_ARGUMENT: {
					asm_unassign_register(ctx, asm_arg_regs[0]);
					asm_reserve_register(ctx, ret->as.i, ins->dst);
					append_line(&code->body, fmt_str("\tmovq %s, %s\n",
													 asm_reg_q_name[asm_ret_regs[0]],
													 asm_reg_q_name[ret->as.i]));
					ret->extra.defered.fun = NULL;
				} break;
				case ADDR_WIDE_ARG: {
					asm_unassign_register(ctx, asm_arg_regs[0]);
					asm_unassign_register(ctx, asm_arg_regs[1]);
					asm_reserve_register(ctx, ret->as.wide[0], ins->dst);
					asm_reserve_register(ctx, ret->as.wide[1], ins->dst);
					append_line(&code->body, fmt_str("\tpushq %s\n", asm_reg_q_name[asm_ret_regs[0]]));
					append_line(&code->body, fmt_str("\tpushq %s\n", asm_reg_q_name[asm_ret_regs[1]]));
					append_line(&code->body, fmt_str("\tpopq %s\n",  asm_reg_q_name[ret->as.wide[1]]));
					append_line(&code->body, fmt_str("\tpopq %s\n",  asm_reg_q_name[ret->as.wide[0]]));
					ret->extra.defered.fun = NULL;
				} break;
				case ADDR_WIDE: {
					asm_reserve_register(ctx, ret->as.wide[0], ins->dst);
					asm_reserve_register(ctx, ret->as.wide[1], ins->dst);
				} break;
				case ADDR_STACK: /* do nothing */ break;
				case ADDR_BLK_ARG: {
					ret = &ctx->vars[ret->as.i];
					if (ret->tag == ADDR_REGISTER) {
						assert(ret->as.i == asm_ret_regs[0]);
						asm_reserve_register(ctx, ret->as.i, ins->dst);
					} else {
						FAILWITH("TODO: unhandled case ret->tag == %s", asm_addr_tag_to_str(ret->tag));
					}
				} break;
				default: {
					FAILWITH("TODO: unhandled case ret->tag == %s", asm_addr_tag_to_str(ret->tag));
				} break;
				}
			}
		} break;
		case IR_OPCODE_COUNT:
		default: FAILWITH("Unreachable"); break;
		}
	}
	struct ir_blk_terminal *term = &blk->term;
	switch (term->op) {
	case ir_op_ret: {
		if (term->args.len > 0) {
			struct asm_address *ret = &ctx->vars[term->args.elems[0]];
			if (ret->tag == ADDR_STACK) {
				assert(type_is_pointer(ret->type));
				size_t ts = type_size(ret->type->as.ptr);
				if (ts < 16) {
					FAILWITH("TODO");
				} else if (ts == 16) {
					append_line(&code->body, fmt_str(
									"\tmovq %ld(%%rbp), %s\n",
									ret->as.i,
									asm_reg_q_name[asm_ret_regs[0]]));
					append_line(&code->body, fmt_str(
									"\tmovq %ld(%%rbp), %s\n",
									ret->as.i + 8,
									asm_reg_q_name[asm_ret_regs[1]]));
				} else {
					FAILWITH("Unreachable");
				}
			} else if (ret->tag == ADDR_STACK_LOAD) {
				size_t ts = type_size(ret->type);
				if (ts <= 8) {
					append_line(&code->body, fmt_str(
									"\tmovq %d(%%rbp), %s\n",
									ret->as.stack[0] + ret->as.stack[1],
									asm_reg_q_name[asm_ret_regs[0]]));
				} else if (ts <= 16) {
					append_line(&code->body, fmt_str(
									"\tmovq %d(%%rbp), %s\n",
									ret->as.stack[0] + ret->as.stack[1],
									asm_reg_q_name[asm_ret_regs[0]]));
					append_line(&code->body, fmt_str(
									"\tmovq %d(%%rbp), %s\n",
									ret->as.stack[0] + ret->as.stack[1] + 8,
									asm_reg_q_name[asm_ret_regs[1]]));
				} else {
					FAILWITH("Unreachable blk = %p, ts = %zu, ret = %%%d",
							 blk, ts, term->args.elems[0]);
				}
			} else if (ret->tag == ADDR_REGISTER) {
				if (ret->as.i != asm_ret_regs[0]) {
					append_line(&code->body, fmt_str(
									"\tmovq %s, %s\n",
									asm_reg_q_name[ret->as.i],
									asm_reg_q_name[asm_ret_regs[0]]));
				}
			} else if (ret->tag == ADDR_WIDE) {
				if (ret->as.wide[0] != (int)asm_ret_regs[0] || ret->as.wide[1] != (int)asm_ret_regs[1]) {
					append_line(&code->body, fmt_str("\tpushq %s\n", asm_reg_q_name[ret->as.wide[0]]));
					append_line(&code->body, fmt_str("\tpushq %s\n", asm_reg_q_name[ret->as.wide[1]]));
					append_line(&code->body, fmt_str("\tpopq %s\n",  asm_reg_q_name[asm_ret_regs[1]]));
					append_line(&code->body, fmt_str("\tpopq %s\n",  asm_reg_q_name[asm_ret_regs[2]]));
				}
			} else if (ret->tag == ADDR_IMM_INT) {
				append_line(&code->body, fmt_str(
								"\tmov%c $%ld, %s\n",
								asm_suffix(ret->type),
								ret->as.i,
								asm_reg_name(asm_ret_regs[0], ret->type)));
			} else if (ret->tag == ADDR_NONE) {
				/* do nothing */
			} else {
				FAILWITH("TODO: ret->tag == %s", asm_addr_tag_to_str(ret->tag));
			}
		}
		if (blk_id < blocks->len - 1) {
			struct strview id_name = token_to_strview(proc->def->id);
			append_line(&code->body, fmt_str("\tjmp \".LR"SV_FMT"\"\n", SV_ARGS(id_name)));
		}
	} break;
	case ir_op_goto: {
		for (size_t i = 0; i < term->args.len; ++i) {
			asm_unassign_blk_arg_register(ctx, term->args.elems[i]);
		}
		if (blk_id < blocks->len - 1
			&& term->b0 == blocks->elems[blk_id + 1]) {
			/* fall through to block */
		} else {
			append_line(&code->body, fmt_str("\tjmp .L%p\n", term->b0));
		}
	} break;
	case ir_op_if: {
		assert(term->args.len == 1);
		struct asm_address *cond = &ctx->vars[term->args.elems[0]];
		if (cond->tag == ADDR_FLAGS) {
			char *cc;
			/* We want to jump to the false branch (and fallthrough to the true branch),
			 * so we logically negate the comparison.
			 */
			switch (cond->as.stack[0]) {
			case ir_op_cmpe:  cc = "ne";  break;
			case ir_op_cmpne: cc = "e"; break;
			case ir_op_cmpl:  cc = type_is_signed_scalar(cond->type) ? "ge" : "ae"; break;
			case ir_op_cmpg:  cc = type_is_signed_scalar(cond->type) ? "le" : "be"; break;
			case ir_op_cmple: cc = type_is_signed_scalar(cond->type) ? "g" : "a";   break;
			case ir_op_cmpge: cc = type_is_signed_scalar(cond->type) ? "l" : "b";   break;
			default: FAILWITH("Unreachable"); break;
			}
			append_line(&code->body, fmt_str("\tj%s .L%p\n", cc, term->b1));
		} else if (cond->tag == ADDR_STACK_LOAD) {
			append_line(&code->body, fmt_str(
							"\tcmpb $0, %d(%%rbp)\n",
							cond->as.stack[0] + cond->as.stack[1]));
			append_line(&code->body, fmt_str("\tje .L%p\n", term->b1));
		} else if (cond->tag == ADDR_REGISTER) {
			append_line(&code->body, fmt_str(
							"\tcmpb $0, %s\n",
							asm_reg_b_name[cond->as.i]));
			append_line(&code->body, fmt_str("\tje .L%p\n", term->b1));
		} else {
			FAILWITH("TODO: cond->tag == %s", asm_addr_tag_to_str(cond->tag));
		}
	} break;
	case ir_op_tailcall: FAILWITH("TODO: ir_op_tailcall"); break;
	default: FAILWITH("Unreachable"); break;
	}
}

void asm_emit_callee_save_code(size_t offset, struct asm_procedure *code)
{
	struct asm_context *ctx = &code->ctx;
	uint64_t saved = ctx->clobbered & REGC_CALLEE_SAVED;
	if (saved == 0) return;
	for (size_t i = 0; i < ARRAY_LENGTH(asm_reg_q_name); ++i) {
		if (saved & (1lu << i)) {
			offset += 8;
			append_line(&code->prologue, fmt_str("\tmovq %s, -%zu(%%rbp)\n", asm_reg_q_name[i], offset));
			append_line(&code->epilogue, fmt_str("\tmovq -%zu(%%rbp), %s\n", offset, asm_reg_q_name[i]));
		}
	}
}

void asm_emit_prologue_and_epilogue(struct ir_proc *proc, struct asm_procedure *code)
{
	struct asm_context *ctx = &code->ctx;
	if (!proc->is_static) {
		append_line(&code->prologue, fmt_str("\t.globl \"%s\"\n", proc->link));
	}
	append_line(&code->prologue, strdup("\t.text\n"));
	append_line(&code->prologue, fmt_str("\"%s\":\n", proc->link));
	/* procedure entry */
	bool setup_frame = ctx->setup_frame;
	bool alloc_frame = false;
	size_t callee_save_offset = ctx->stack_size = align_adjust(ctx->stack_size, 8);
	ctx->stack_size += count_set_bits(ctx->clobbered & REGC_CALLEE_SAVED) * 8;
	ctx->stack_size = align_adjust(ctx->stack_size, 16);
	if (ctx->stack_size > 0) setup_frame = true;
	if (setup_frame) {
		append_line(&code->prologue, strdup("\tpushq %rbp\n"));
		append_line(&code->prologue, strdup("\tmovq %rsp, %rbp\n"));
	}
	if (setup_frame && ctx->stack_size && !can_optimize_red_zone(ctx)) {
		alloc_frame = true;
		append_line(&code->prologue, fmt_str("\tsubq $%zu, %%rsp\n", ctx->stack_size));
	}
	if (ctx->has_large_retval) {
		struct asm_address *addr = &ctx->vars[ctx->rv_addr];
		if (addr->tag == ADDR_BLK_ARG) addr = &ctx->vars[addr->as.i];
		assert(addr->tag == ADDR_STACK_LOAD);
		append_line(&code->prologue, fmt_str(
						"\tmovq %s, %d(%%rbp)\n",
						asm_reg_q_name[asm_arg_regs[0]],
						addr->as.stack[0] + addr->as.stack[1]));
	}
	/* epilogue label */
	append_line(&code->epilogue, fmt_str("\".LR%s\":\n", proc->link));
	/* callee save registers */
	asm_emit_callee_save_code(callee_save_offset, code);
	/* procedure exit */
	if (alloc_frame) {
		append_line(&code->epilogue, fmt_str("\taddq $%zu, %%rsp\n", ctx->stack_size));
		append_line(&code->epilogue, strdup("\tpopq %rbp\n"));
	} else if (setup_frame) {
		append_line(&code->epilogue, strdup("\tpopq %rbp\n"));
	}
	append_line(&code->epilogue, strdup("\tret\n"));
}

void asm_emit_procedure(struct ir_proc *proc, struct ir_toplevel *tl, struct asm_procedure *code)
{
	struct da_pointers blks = ir_blk_reverse_post_order(proc->entry);
	asm_create_context(proc, &blks, tl, &code->ctx);
	for (size_t i = 0; i < blks.len; ++i) {
		asm_emit_block(&blks, i, proc, tl, code);
	}
	asm_emit_prologue_and_epilogue(proc, code);
	da_free(&blks);
}

void asm_emit_datum(struct ir_data *data, struct asm_datum *asm_data)
{
	if (!data->is_static) {
		append_line(&asm_data->body, fmt_str("\t.globl \"%s\"\n", data->link));
	}
	append_line(&asm_data->body, strdup("\t.data\n"));
	append_line(&asm_data->body, strdup("\t.align 16\n"));
	append_line(&asm_data->body, fmt_str("\"%s\":\n", data->link));
	size_t size = data->size;
	size_t q = size / 8;
	size %= 8;
	size_t d = size / 4;
	size %= 4;
	size_t b = size;
	uint8_t *ptr = data->dat;
	while (q--) {
		append_line(&asm_data->body, fmt_str("\t.quad %lu\n", *(uint64_t *)ptr));
		ptr += 8;
	}
	while (d--) {
		append_line(&asm_data->body, fmt_str("\t.long %u\n", *(uint32_t *)ptr));
		ptr += 4;
	}
	while (b--) {
		append_line(&asm_data->body, fmt_str("\t.byte %u\n", *(uint8_t *)ptr));
		ptr += 1;
	}
}

void dump_lines(struct lines *asm_lines, FILE *file)
{
	da_foreach(line, asm_lines) {
		fputs(*line, file);
	}
}

void asm_dump_procedure(struct asm_procedure *proc, FILE *file)
{
	dump_lines(&proc->prologue, file);
	dump_lines(&proc->body, file);
	dump_lines(&proc->epilogue, file);
}

void asm_dump_data(struct asm_datum *data, FILE *file)
{
	dump_lines(&data->body, file);
}


void asm_dump_module(struct asm_module *mod, FILE *file)
{
	for (size_t i = 0; i < mod->data.len; ++i) {
		asm_dump_data(&mod->data.elems[i], file);
	}
	for (size_t i = 0; i < mod->procs.len; ++i) {
		asm_dump_procedure(&mod->procs.elems[i], file);
	}
}

struct asm_module *compile_file(const char *filename, struct asm_module *asm_mod, bool is_dll)
{
	struct strview sv = {0};
	if (sv_open_file(filename, &sv) == false) {
		fprintf(stderr, "[Error] %s: %s\n", filename, strerror(errno));
		EXIT(1);
	}
	if (sv.len == 0) {
		fprintf(stderr, "[Error] %s: %s\n", filename, "Empty file");
		EXIT(1);
	}
	Parser parser = {
		.lexer = {
			.filename = filename,
			.text     = sv.ptr,
			.length   = sv.len,
			.line     = 1,
		}
	};
	tokenize(&parser.lexer, &parser.tokens);
	struct expression_stack tl = {0};
	struct expression *exp = NULL;
	struct scope sc = {0};
	while (!parser_is_at_end(&parser)) {
		exp = parse_toplevel_expression(&parser, &sc);
		if (exp != NULL) da_append(&tl, exp);
	}
	/* Type check */
	struct typing_context ctx = {.scope = &sc};
	for (size_t i = 0; i < tl.len; ++i) {
		infer_type(&parser, ctx, tl.elems[i]);
	}
	/* Desugar */
	for (size_t i = 0; i < tl.len; ++i) {
		tl.elems[i] = ast_desugar(&parser, tl.elems[i], &sc);
	}
	/* AST => IR */
	struct ir_toplevel ir = ast_compile(&sc);
#if KC_DEBUG
	puts("[Debug]");
	for (size_t i = 0; i < ir.len; ++i) {
		if (ir.elems[i].tag == IRO_PROC) {
			ir_proc_fprint(&ir.elems[i].proc, stdout);
		}
	}
#endif
	/* IR => ASM */
	ir.is_dll = is_dll;
	for (size_t i = 0; i < ir.len; ++i) {
		switch (ir.elems[i].tag) {
		case IRO_PROC:
			asm_emit_procedure(&ir.elems[i].proc, &ir, da_allot(&asm_mod->procs));
			break;
		case IRO_DATA:
			asm_emit_datum(&ir.elems[i].data, da_allot(&asm_mod->data));
			break;
		case IRO_EXTERN_DATA: /* Do nothing */ break;
		case IRO_EXTERN_PROC: /* Do nothing */ break;
		default: FAILWITH("Unreachable"); break;
		}
	}
	return asm_mod;
}

int run_cmd(struct lines *args)
{
	char *cmd_str = concat_lines(args, " ");
	printf("[Info] %s\n", cmd_str);
	int c = system(cmd_str);
	free(cmd_str);
	return c;
}

// as --gdwarf-5 --64 syntax.s -o syntax.o
// ld -dynamic-linker /lib/ld-linux-x86-64.so.2 /usr/lib/crt1.o /usr/lib/crti.o -lc syntax.o /usr/lib/crtn.o
int assemble_module(struct asm_module *mod, char *target, bool debug)
{
	struct lines cmd = {0};
	da_append(&cmd, "as");
	if (debug) da_append(&cmd, "--gdwarf-5");
	da_append(&cmd, "--64");
	da_append(&cmd, "-o");
	da_append(&cmd, target);
	char *cmd_str = concat_lines(&cmd, " ");
	printf("[Info] %s\n", cmd_str);
	FILE *assembler = popen(cmd_str, "w");
	assert(assembler != NULL);
	asm_dump_module(mod, assembler);
	int c = pclose(assembler);
	free(cmd_str);
	da_free(&cmd);
	return c;
}

int assemble_file(char *filename, char *target, bool debug)
{
	struct lines cmd = {0};
	da_append(&cmd, "as");
	if (debug) da_append(&cmd, "--gdwarf-5");
	da_append(&cmd, "--64");
	da_append(&cmd, "-o");
	da_append(&cmd, target);
	da_append(&cmd, filename);
	char *cmd_str = concat_lines(&cmd, " ");
	int c = run_cmd(&cmd);
	free(cmd_str);
	da_free(&cmd);
	return c;
}

int link_object_files(struct lines *obj_files, char *target, bool is_dll)
{
	struct lines cmd = {0};
	da_append(&cmd, "ld");
	if (is_dll) da_append(&cmd, "-shared");
	da_append(&cmd, "-dynamic-linker /lib/ld-linux-x86-64.so.2");
	da_append(&cmd, "/usr/lib/crt1.o");
	da_append(&cmd, "/usr/lib/crti.o");
	da_append(&cmd, "-lc");
	da_copy(&cmd, obj_files);
	da_append(&cmd, "/usr/lib/crtn.o");
	da_append(&cmd, "-o");
	da_append(&cmd, target);
	int c = run_cmd(&cmd);
	da_free(&cmd);
	return c;
}

int run(char *source_file, const char *dll_file)
{
	void *handle = dlopen(dll_file, RTLD_NOW);
	if (handle == NULL) {
		fprintf(stderr, "[Error] %s\n", dlerror());
		return -1;
	}
	int(*fn)(int, char**) = dlsym(handle, "main");
	if (fn == NULL) {
		fprintf(stderr, "[Error] %s\n", dlerror());
		return -1;
	}
	char *args[] = {source_file, NULL};
	int c = fn(1, args);
	dlclose(handle);
	return c;
}

char *shift(int *argc, char ***argv)
{
	(*argc)--;
	(*argv)++;
	return 0[*argv];
}

int main(int argc, char **argv)
{
	static char input[0x1000] = {0};
	static char cwd[PATH_MAX] = {0};
	struct lines input_files = {0};
	struct lines asm_files = {0};
	struct lines obj_files = {0};
	struct asm_modules asm_modules = {0};
	char *target_file = "a.out";
	bool output_asm_p = false;
	bool run_p = false;
	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		fprintf(stderr, "[Error] %s\n", strerror(errno));
		return 1;
	}
	/* process args and compile input files */
	for (char *arg = shift(&argc, &argv); argc; arg = shift(&argc, &argv)) {
		if (strcmp(arg, "-run") == 0) {
			run_p = true;
		} else if (strcmp(arg, "-S") == 0) {
			output_asm_p = true;
		} else if (strcmp(arg, "-o") == 0) {
			target_file = shift(&argc, &argv);
		} else if (sscanf(arg, "%s.k", input) != 1) {
			fprintf(stderr, "[Error] Invalid input file %s\n", arg);
			return 1;
		} else {
			da_append(&input_files, strdup(input));
		}
	}
	for (size_t i = 0; i < input_files.len; ++i) {
		compile_file(input_files.elems[i], da_allot(&asm_modules), run_p);
		da_append(&asm_files, subst_file_suffix(input_files.elems[i], "s"));
	}
	if (output_asm_p) {
		for (size_t i = 0; i < asm_files.len; ++i) {
			FILE *file = fopen(asm_files.elems[i], "w");
			assert(file != NULL);
			asm_dump_module(&asm_modules.elems[i], file);
			fclose(file);
		}
	}
	for (size_t i = 0; i < input_files.len; ++i) {
		char *obj_file = subst_file_suffix(input_files.elems[i], "o");
		if (output_asm_p) {
			assemble_file(asm_files.elems[i], obj_file, true);
		} else {
			assemble_module(&asm_modules.elems[i], obj_file, true);
		}
		da_append(&obj_files, obj_file);
	}
	if (run_p) {
		char *dlname = strjoin(cwd, "dlrun.so", "/");
		link_object_files(&obj_files, dlname, true);
		printf("[Info] Running program...\n");
		printf("[Info] Program exited with error code: %d\n", run(input_files.elems[0], dlname));
	} else {
		link_object_files(&obj_files, target_file, false);
	}
	return 0;
}
