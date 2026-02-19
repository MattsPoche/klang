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

static uintptr_t align_adjust(uintptr_t x, uintptr_t alignment);

/* --- TYPE-CHECKER --- */
bool type_coerce(Parser *p, struct type *t, struct type *u, struct token *tok);
bool type_eq(Parser *p, struct type *t, struct type *u, struct expression *exp,
			 char *debug_file, int debug_line);
struct expression *infer_type(Parser *p, struct expression *exp, struct type *ret, struct scope *scope);

#define TYPE_EQ(t, u, exp) type_eq(p, t, u, exp, __FILE__, __LINE__)

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
	case ast_type_intlit:
		return true;
	case ast_type_void:
	case ast_type_bool:
	case ast_type_f32:
	case ast_type_f64:
	case ast_type_floatlit:
	case ast_type_alias:
	case ast_type_cons:
	case ast_type_ptr:
	case ast_type_mut_ptr:
	case ast_type_slice:
	case ast_type_mut_slice:
	case ast_type_array:
	case ast_type_struct:
	case ast_type_vector:
	case ast_type_proc:
	case ast_type_noreturn:
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
	case ast_type_intlit:
		return true;
	case ast_type_u8:
	case ast_type_u16:
	case ast_type_u32:
	case ast_type_u64:
	case ast_type_void:
	case ast_type_bool:
	case ast_type_f32:
	case ast_type_f64:
	case ast_type_floatlit:
	case ast_type_alias:
	case ast_type_cons:
	case ast_type_ptr:
	case ast_type_mut_ptr:
	case ast_type_slice:
	case ast_type_mut_slice:
	case ast_type_array:
	case ast_type_struct:
	case ast_type_vector:
	case ast_type_proc:
	case ast_type_noreturn:
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
	case ast_type_intlit:
		return true;
	case ast_type_i8:
	case ast_type_i16:
	case ast_type_i32:
	case ast_type_i64:
	case ast_type_void:
	case ast_type_bool:
	case ast_type_f32:
	case ast_type_f64:
	case ast_type_floatlit:
	case ast_type_alias:
	case ast_type_cons:
	case ast_type_ptr:
	case ast_type_mut_ptr:
	case ast_type_slice:
	case ast_type_mut_slice:
	case ast_type_array:
	case ast_type_struct:
	case ast_type_vector:
	case ast_type_proc:
	case ast_type_noreturn:
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
	case ast_type_intlit:
	case ast_type_void:
	case ast_type_bool:
	case ast_type_alias:
	case ast_type_cons:
	case ast_type_ptr:
	case ast_type_mut_ptr:
	case ast_type_slice:
	case ast_type_mut_slice:
	case ast_type_array:
	case ast_type_struct:
	case ast_type_vector:
	case ast_type_proc:
	case ast_type_noreturn:
		return false;
	case ast_type_f32:
	case ast_type_f64:
	case ast_type_floatlit:
		return true;
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
	case ast_type_intlit:
	case ast_type_f32:
	case ast_type_f64:
	case ast_type_floatlit:
		return true;
	case ast_type_void:
	case ast_type_bool:
	case ast_type_alias:
	case ast_type_cons:
	case ast_type_ptr:
	case ast_type_mut_ptr:
	case ast_type_slice:
	case ast_type_mut_slice:
	case ast_type_array:
	case ast_type_struct:
	case ast_type_vector:
	case ast_type_proc:
	case ast_type_noreturn:
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

bool type_is_struct(struct type *t)
{
	return t->tag == ast_type_struct;
}

bool type_is_struct_ptr(struct type *t)
{
	return type_is_pointer(t)
		&& type_is_struct(t->as.ptr);
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

bool type_has_length(struct type *t)
{
	if (type_is_array(t)) {
		return t->as.array.stag == AT_LITERAL
			|| t->as.array.stag == AT_EXPRESSION;
	}
	if (type_is_array_ptr(t)) {
		return t->as.ptr->as.array.stag == AT_LITERAL
			|| t->as.ptr->as.array.stag == AT_EXPRESSION;
	}
	return type_is_slice(t)
		|| type_is_slice_ptr(t);
}


bool type_is_scalar(struct type *t)
{
	return type_is_numeric_scalar(t) || type_is_pointer(t) || type_is_bool(t);
}

struct type *slice_type_to_array_ptr_type(struct mem_arena *pool, struct type *t)
{
	struct type *ptr = POOL_ALLOC(pool, struct type);
	struct type *array = POOL_ALLOC(pool, struct type);
	array->tag = ast_type_array;
	array->as.array.stag = AT_UNSIZED;
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

bool type_coerce(Parser *p, struct type *t, struct type *u, struct token *tok)
{
	if (u->tag == ast_type_void) return true;
	switch (t->tag) {
	case ast_type_void: return true;
	case ast_type_noreturn: return false;
	case ast_type_bool: return u->tag == ast_type_bool;
	case ast_type_i8:
	case ast_type_i16:
	case ast_type_i32:
	case ast_type_i64:
	case ast_type_u8:
	case ast_type_u16:
	case ast_type_u32:
	case ast_type_u64:
		if (u->tag == ast_type_intlit) u->tag = t->tag;
		return t->tag == u->tag;
	case ast_type_intlit:
		return type_is_integer(u) ? t->tag = u->tag, true : false;
	case ast_type_f32:
	case ast_type_f64:
		if (u->tag == ast_type_intlit) u->tag = t->tag;
		return t->tag == u->tag;
	case ast_type_floatlit:
		return type_is_floating_point(u) ? t->tag = u->tag, true : false;
	case ast_type_alias: FAILWITH("TODO: ast_type_alias"); break;
	case ast_type_cons: FAILWITH("TODO: ast_type_cons"); break;
	case ast_type_mut_ptr:
		return u->tag == ast_type_mut_ptr
			&& type_coerce(p, t->as.ptr, u->as.ptr, tok);
	case ast_type_ptr:
		return (u->tag == ast_type_ptr || u->tag == ast_type_mut_ptr)
			&& type_coerce(p, t->as.ptr, u->as.ptr, tok);
	case ast_type_mut_slice:
		return u->tag == ast_type_mut_slice
			&& type_coerce(p, t->as.slice, u->as.slice, tok);
	case ast_type_slice:
		return u->tag == ast_type_slice
			&& type_coerce(p, t->as.slice, u->as.slice, tok);
	case ast_type_array: {
		if (u->tag != ast_type_array) return false;
		if (!type_coerce(p, t->as.array.base, u->as.array.base, tok)) return false;
		if (t->as.array.stag == AT_UNSIZED) return true;
		if (u->as.array.stag == AT_UNSIZED) return true;
		assert(t->as.array.stag == AT_LITERAL);
		assert(u->as.array.stag == AT_LITERAL);
		return t->as.array.size.sz == u->as.array.size.sz;
	} break;
	case ast_type_struct: FAILWITH("TODO: ast_type_struct"); break;
	case ast_type_vector: FAILWITH("TODO: ast_type_vector"); break;
	case ast_type_proc:
		if (u->tag == ast_type_proc
			&& type_coerce(p, t->as.proc.ret, u->as.proc.ret, tok)
			&& t->as.proc.args.len == u->as.proc.args.len) {
			for (size_t i = 0; i < t->as.proc.args.len; ++i) {
				struct type *arg_t = t->as.proc.args.elems[i];
				struct type *arg_u = u->as.proc.args.elems[i];
				if (!type_coerce(p, arg_t, arg_u, tok)) {
					return false;
				}
				if (arg_t->tag == ast_type_array && arg_t->as.array.stag == AT_UNSIZED) {
					log_error_and_die(p->lexer.filename, p->lexer.contents, tok->sv, tok->loc,
									  "Unable to determine the size of array parameter.");
					return false;
				}
			}
			return true;
		}
		return false;
	default:
		FAILWITH("Unreachable condition");
		break;
	}
	FAILWITH("Unreachable condition");
	return false;
}

bool type_eq(Parser *p, struct type *t, struct type *u, struct expression *exp,
			 char *debug_file, int debug_line)
{
	if (!type_coerce(p, t, u, exp->tok)) {
		fflush(stdout);
		struct strview msg = {0};
		FILE *stream = open_memstream(&msg.ptr, &msg.len);
		fputs("Type error. Type ", stream);
		ast_type_fprint(t, stream);
		fputs(" is incompatible with type ", stream);
		ast_type_fprint(u, stream);
		fclose(stream);
		log_error_impl(p->lexer.filename, p->lexer.contents, exp->tok->sv, exp->tok->loc,
					   debug_file, debug_line, msg.ptr);
		exit(1);
	}
	return true;
}

bool def_coerce(Parser *p, struct type *def_type, struct expression *exp)
{
	switch (def_type->tag) {
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
	case ast_type_intlit:
	case ast_type_f32:
	case ast_type_f64:
	case ast_type_floatlit:
	case ast_type_ptr:
	case ast_type_mut_ptr:
	case ast_type_proc:
	case ast_type_slice:
	case ast_type_mut_slice:
		return TYPE_EQ(def_type, exp->type, exp);
	case ast_type_array: {
		struct array_type *array_type = &def_type->as.array;
		if (exp->tag == ast_exp_zero_initializer) {
			switch (array_type->stag) {
			case AT_UNSIZED:    FAILWITH("TODO: Unsized array used with zero initializer"); break;
			case AT_EXPRESSION: FAILWITH("TODO: AT_EXPRESSION"); break;
			case AT_LITERAL:
				assert(exp->type == NULL);
				exp->type = def_type;
				return true;
			}
		} else if (exp->tag == ast_exp_initializer) {
			assert(exp->type == NULL);
			struct expression_stack *init_exps = &exp->as.init;
			struct type *base = array_type->base;
			da_foreach(exp, init_exps) {
				TYPE_EQ(base, (*exp)->type, *exp);
			}
			switch (array_type->stag) {
			case AT_UNSIZED:
				array_type->stag = AT_LITERAL;
				array_type->size.sz = init_exps->len;
				break;
			case AT_EXPRESSION: FAILWITH("TODO: AT_EXPRESSION"); break;
			case AT_LITERAL:
				if (init_exps->len != array_type->size.sz) {
					log_error_and_die(p->lexer.filename, p->lexer.contents, exp->tok->sv, exp->tok->loc,
									  "Invalid size of array initializer.");
				}
				break;
			}
			exp->type = def_type;
			return true;
		} else if (exp->tag == ast_exp_named_initializer) {
			FAILWITH("TODO: type error");
		} else {
			assert(array_type->stag != AT_UNSIZED);
			assert(exp->type != NULL);
			return TYPE_EQ(def_type, exp->type, exp);
		}
	} break;
	case ast_type_struct: {
		struct struct_type *struct_type = &def_type->as.strct;
		if (exp->tag == ast_exp_zero_initializer) {
			assert(exp->type == NULL);
			exp->type = def_type;
			return true;
		} else if (exp->tag == ast_exp_initializer) {
			assert(exp->type == NULL);
			struct expression_stack *init_exps = &exp->as.init;
			if (struct_type->len != init_exps->len) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, exp->tok->sv, exp->tok->loc,
								  "Expected type does not match aggregate initializer.");
			}
			for (size_t i = 0; i < struct_type->len; ++i) {
				struct struct_member *mem = &struct_type->elems[i];
				struct expression *init = init_exps->elems[i];
				TYPE_EQ(mem->type, init->type, init);
			}
			exp->type = def_type;
			return true;
		} else if (exp->tag == ast_exp_named_initializer) {
			FAILWITH("TODO: ast_exp_named_initializer");
		} else {
			FAILWITH("TODO");
			/* assert(array_type->stag != AT_UNSIZED); */
			/* assert(exp->type != NULL); */
			/* return TYPE_EQ(def_type, exp->type, exp); */
		}
	} break;

	case ast_type_alias: FAILWITH("TODO: ast_type_alias"); break;
	case ast_type_cons: FAILWITH("TODO: ast_type_cons"); break;
	case ast_type_vector: FAILWITH("TODO: ast_type_vector"); break;
	}
	FAILWITH("TODO: def_type_coerce");
	return false;
}

struct expression *infer_type(Parser *p, struct expression *exp, struct type *ret, struct scope *scope)
{
	assert(exp->type == NULL);
	struct expression *lhs, *rhs;
	switch (exp->tag) {
	case ast_exp_definition: {
		assert(exp->as.def.type != NULL);
		if (exp->as.def.exp->tag == ast_exp_extern_symbol) {
			exp->as.def.exp->type = exp->as.def.type;
		} else {
			assert(def_coerce(p, exp->as.def.type, infer_type(p, exp->as.def.exp, ret, scope)));
		}
		exp->is_addressable = false;
		exp->is_mutable = false;
	} break;
	case ast_exp_let: {
		struct let *let = &exp->as.let;
		/* type check definition */
		assert(exp->as.def.type != NULL);
		assert(def_coerce(p, exp->as.def.type, infer_type(p, exp->as.def.exp, ret, scope)));
		/* type check body */
		let->scope.parent = scope;
		da_append(&let->scope.symtbl, &let->def);
		rhs = infer_type(p, let->body, ret, &let->scope);
		exp->type = rhs->type;
		exp->is_addressable = rhs->is_addressable;
		exp->is_mutable = rhs->is_mutable;
	} break;
	case ast_exp_procedure_literal: {
		struct procedure *proc = &exp->as.proc;
		proc->scope.parent = scope;
		/* add arguments to procedure scope */
		da_foreach(formal, &proc->formals) {
			da_append(&proc->scope.symtbl, formal);
		}
		rhs = infer_type(p, proc->body, proc->ret, &proc->scope);
		assert(def_coerce(p, proc->ret, rhs));
		exp->type = POOL_ALLOC(&p->data, struct type);
		exp->type->tag = ast_type_proc;
		exp->type->as.proc = procedure_type(proc);
		exp->is_addressable = false;
		exp->is_mutable = false;
	} break;
	case ast_exp_literal: {
		struct literal *lit = &exp->as.lit;
		exp->is_addressable = false;
		exp->is_mutable = false;
		if (lit->token->tt == tt_intlit || lit->token->tt == tt_hexlit) {
			exp->type = POOL_ALLOC(&p->data, struct type);
			exp->type->tag = ast_type_intlit;
		} else if (lit->token->tt == tt_floatlit) {
			exp->type = POOL_ALLOC(&p->data, struct type);
			exp->type->tag = ast_type_floatlit;
		} else if (lit->token->tt == tt_true || lit->token->tt == tt_false) {
			exp->type = POOL_ALLOC(&p->data, struct type);
			exp->type->tag = ast_type_bool;
		} else {
			FAILWITH("TODO: ast_exp_literal");
		}
	} break;
	case ast_exp_extern_symbol: FAILWITH("TODO: ast_exp_extern_symbol"); break;
	case ast_exp_get_ptr: {
		struct expression *obj = infer_type(p, exp->as.get_ptr, ret, scope);
		if (obj->type->tag == ast_type_slice) {
			exp->is_addressable = false;
			exp->is_mutable = false;
			exp->type = slice_type_to_array_ptr_type(&p->data, obj->type);
		} else if (obj->type->tag == ast_type_mut_slice) {
			exp->is_addressable = false;
			exp->is_mutable = true;
			exp->type = slice_type_to_array_ptr_type(&p->data, obj->type);
		} else {
			log_error_and_die(p->lexer.filename, p->lexer.contents, exp->tok->sv, exp->tok->loc,
							  "Invalid argument type for operator `#ptr`. Expected slice.");
		}
	} break;
	case ast_exp_get_len: {
		struct expression *obj = infer_type(p, exp->as.get_ptr, ret, scope);
		exp->is_addressable = false;
		exp->is_mutable = false;
		exp->type = &AST_TYPE_U64;
		if (!type_has_length(obj->type)) {
			log_error_and_die(p->lexer.filename, p->lexer.contents, exp->tok->sv, exp->tok->loc,
							  "Invalid argument type for operator `#len`.");
		}
	} break;
	case ast_exp_string: {
		exp->is_addressable = false;
		exp->is_mutable = false;
		exp->type = &AST_TYPE_STRING;
	} break;
	case ast_exp_ident: {
		struct definition *def = lookup_definition(scope, exp->as.id->sv);
		if (def == NULL) {
			log_error_and_die(p->lexer.filename, p->lexer.contents, exp->tok->sv, exp->tok->loc,
							  "Reference to undefined symbol `"SV_FMT"`", SV_ARGS(&exp->as.id->sv));
		}
		exp->type = def->type;
		exp->is_addressable = true;
		exp->is_mutable = def->is_mut;
	} break;
	case ast_exp_undefined: {
		exp->type = &AST_TYPE_VOID;
		exp->is_addressable = false;
		exp->is_mutable = false;
	} break;
	case ast_exp_binary: {
		struct binary *bin = &exp->as.bin;
		switch ((enum binop)bin->op) {
		case binop_sequence: {
			lhs = infer_type(p, bin->left, ret, scope);
			rhs = infer_type(p, bin->right, ret, scope);
			exp->type = rhs->type;
			exp->is_addressable = rhs->is_addressable;
			exp->is_mutable = rhs->is_mutable;
		} break;
		case binop_assign: {
			lhs = infer_type(p, bin->left, ret, scope);
			rhs = infer_type(p, bin->right, ret, scope);
			if (!lhs->is_addressable) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, lhs->tok->sv, lhs->tok->loc,
								  "Unable to assign. Expression is not addressable.");
			}
			if (!lhs->is_mutable) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, lhs->tok->sv, lhs->tok->loc,
								  "Cannot assign to immutable expression.");
			}
			TYPE_EQ(lhs->type, rhs->type, exp);
			exp->type = lhs->type;
			exp->is_addressable = false;
			exp->is_mutable = false;
		} break;
		case binop_add:
		case binop_sub:
		case binop_mul:
		case binop_div: {
			lhs = infer_type(p, bin->left, ret, scope);
			rhs = infer_type(p, bin->right, ret, scope);
			if (!type_is_numeric_scalar(lhs->type)) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, lhs->tok->sv, lhs->tok->loc,
								  "Type error. Operator is undefined for type.");
			}
			if (!type_is_numeric_scalar(rhs->type)) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, rhs->tok->sv, rhs->tok->loc,
								  "Type error. Operator is undefined for type.");
			}
			TYPE_EQ(lhs->type, rhs->type, exp);
			exp->type = lhs->type;
			exp->is_addressable = false;
			exp->is_mutable = false;
		} break;
		case binop_equal:
		case binop_less_than:
		case binop_more_than:
		case binop_not_equal:
		case binop_less_equal:
		case binop_more_equal: {
			lhs = infer_type(p, bin->left, ret, scope);
			rhs = infer_type(p, bin->right, ret, scope);
			if (!type_is_numeric_scalar(lhs->type)) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, lhs->tok->sv, lhs->tok->loc,
								  "Type error. Operator is undefined for type.");
			}
			if (!type_is_numeric_scalar(rhs->type)) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, rhs->tok->sv, rhs->tok->loc,
								  "Type error. Operator is undefined for type.");
			}
			TYPE_EQ(lhs->type, rhs->type, exp);
			exp->type = &AST_TYPE_BOOL;
			exp->is_addressable = false;
			exp->is_mutable = false;
		} break;
		case binop_add_assign:
		case binop_sub_assign:
		case binop_mul_assign:
		case binop_div_assign: {
			lhs = infer_type(p, bin->left, ret, scope);
			rhs = infer_type(p, bin->right, ret, scope);
			if (!lhs->is_addressable) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, lhs->tok->sv, lhs->tok->loc,
								  "Unable to assign. Expression is not addressable.");
			}
			if (!lhs->is_mutable) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, lhs->tok->sv, lhs->tok->loc,
								  "Cannot assign to immutable expression.");
			}
			if (!type_is_numeric_scalar(lhs->type)) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, lhs->tok->sv, lhs->tok->loc,
								  "Type error. Operator is undefined for type.");
			}
			if (!type_is_numeric_scalar(rhs->type)) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, rhs->tok->sv, rhs->tok->loc,
								  "Type error. Operator is undefined for type.");
			}
			TYPE_EQ(lhs->type, rhs->type, exp);
			exp->type = lhs->type;
			exp->is_addressable = false;
			exp->is_mutable = false;
		} break;
		case binop_mod:
		case binop_xor:
		case binop_land:
		case binop_lor: {
			lhs = infer_type(p, bin->left, ret, scope);
			rhs = infer_type(p, bin->right, ret, scope);
			if (!type_is_integer(lhs->type)) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, lhs->tok->sv, lhs->tok->loc,
								  "Type error. Operator is undefined for type.");
			}
			if (!type_is_integer(rhs->type)) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, rhs->tok->sv, rhs->tok->loc,
								  "Type error. Operator is undefined for type.");
			}
			TYPE_EQ(lhs->type, rhs->type, exp);
			exp->type = lhs->type;
			exp->is_addressable = false;
			exp->is_mutable = false;
		} break;
		case binop_shift_left:
		case binop_shift_right: {
			lhs = infer_type(p, bin->left, ret, scope);
			rhs = infer_type(p, bin->right, ret, scope);
			if (!type_is_integer(lhs->type)) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, lhs->tok->sv, lhs->tok->loc,
								  "Type error. Operator is undefined for type.");
			}
			if (!type_is_integer(rhs->type)) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, rhs->tok->sv, rhs->tok->loc,
								  "Type error. Operator is undefined for type.");
			}
			TYPE_EQ(lhs->type, rhs->type, exp);
			exp->type = lhs->type;
			exp->is_addressable = false;
			exp->is_mutable = false;
		} break;
		case binop_mod_assign:
		case binop_and_assign:
		case binop_lor_assign:
		case binop_xor_assign: {
			lhs = infer_type(p, bin->left, ret, scope);
			rhs = infer_type(p, bin->right, ret, scope);
			if (!lhs->is_addressable) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, lhs->tok->sv, lhs->tok->loc,
								  "Unable to assign. Expression is not addressable.");
			}
			if (!lhs->is_mutable) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, lhs->tok->sv, lhs->tok->loc,
								  "Cannot assign to immutable expression.");
			}
			if (!type_is_integer(lhs->type)) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, lhs->tok->sv, lhs->tok->loc,
								  "Type error. Operator is undefined for type.");
			}
			if (!type_is_integer(rhs->type)) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, rhs->tok->sv, rhs->tok->loc,
								  "Type error. Operator is undefined for type.");
			}
			TYPE_EQ(lhs->type, rhs->type, exp);
			exp->type = lhs->type;
			exp->is_addressable = false;
			exp->is_mutable = false;
		} break;
		case binop_shift_left_assign:
		case binop_shift_right_assign: {
			lhs = infer_type(p, bin->left, ret, scope);
			rhs = infer_type(p, bin->right, ret, scope);
			if (!lhs->is_addressable) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, lhs->tok->sv, lhs->tok->loc,
								  "Unable to assign. Expression is not addressable.");
			}
			if (!lhs->is_mutable) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, lhs->tok->sv, lhs->tok->loc,
								  "Cannot assign to immutable expression.");
			}
			if (!type_is_integer(lhs->type)) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, lhs->tok->sv, lhs->tok->loc,
								  "Type error. Operator is undefined for type.");
			}
			if (!type_is_integer(rhs->type)) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, rhs->tok->sv, rhs->tok->loc,
								  "Type error. Operator is undefined for type.");
			}
			TYPE_EQ(lhs->type, rhs->type, exp);
			exp->type = lhs->type;
			exp->is_addressable = false;
			exp->is_mutable = false;
		} break;
		case binop_or:
		case binop_and: {
			lhs = infer_type(p, bin->left, ret, scope);
			rhs = infer_type(p, bin->right, ret, scope);
			if (lhs->type->tag != ast_type_bool) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, lhs->tok->sv, lhs->tok->loc,
								  "Type error. Expected expression of type bool.");
			}
			if (rhs->type->tag != ast_type_bool) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, rhs->tok->sv, rhs->tok->loc,
								  "Type error. Expected expression of type bool.");
			}
			exp->type = lhs->type;
			exp->is_addressable = false;
			exp->is_mutable = false;
		} break;
		case binop_member: {
			lhs = infer_type(p, bin->left, ret, scope);
			rhs = bin->right;
			struct struct_type *st;
			if (type_is_struct(lhs->type)) {
				st = &lhs->type->as.strct;
			} else if (type_is_struct_ptr(lhs->type)) {
				st = &lhs->type->as.ptr->as.strct;
			} else {
				log_error_and_die(p->lexer.filename, p->lexer.contents, lhs->tok->sv, lhs->tok->loc,
								  "Type error. Expected expression of struct type.");
			}
			if (rhs->tag == ast_exp_literal) {
				rhs = infer_type(p, bin->right, ret, scope);
				if (!type_is_unsigned_integer(rhs->type)) {
					log_error_and_die(p->lexer.filename, p->lexer.contents, rhs->tok->sv, rhs->tok->loc,
									  "Type error. Member accessor must be a positive integer literal.");
				}
				rhs->type = &AST_TYPE_U64;
				size_t index = rhs->as.lit.as.i;
				if (index >= st->len) {
					log_error_and_die(p->lexer.filename, p->lexer.contents, rhs->tok->sv, rhs->tok->loc,
									  "Invalid member accessor.");
				}
				struct struct_member *mem = &st->elems[index];
				if (mem->name != NULL) {
					log_error_and_die(p->lexer.filename, p->lexer.contents, rhs->tok->sv, rhs->tok->loc,
									  "Integer member accessor is only valid for anonymous structs.");
				}
				exp->type = mem->type;
				exp->is_addressable = lhs->is_addressable;
				exp->is_mutable = lhs->is_mutable;
			} else {
				FAILWITH("TODO: binop_member");
			}
		} break;
		}
	} break;
	case ast_exp_unary: {
		struct unary *una = &exp->as.una;
		switch ((enum unaop)una->op) {
		case unaop_neg:
		case unaop_pos: {
			rhs = infer_type(p, una->exp, ret, scope);
			if (!type_is_numeric_scalar(rhs->type)) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, rhs->tok->sv, rhs->tok->loc,
								  "Type error. Operator is undefined for type.");
			}
			exp->type = rhs->type;
			exp->is_addressable = false;
			exp->is_mutable = false;
		} break;
		case unaop_lnot: {
			rhs = infer_type(p, una->exp, ret, scope);
			if (!type_is_integer(rhs->type)) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, rhs->tok->sv, rhs->tok->loc,
								  "Type error. Operator is undefined for type.");
			}
			exp->type = rhs->type;
			exp->is_addressable = false;
			exp->is_mutable = false;
		} break;
		case unaop_not: {
			rhs = infer_type(p, una->exp, ret, scope);
			if (rhs->type->tag != ast_type_bool) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, rhs->tok->sv, rhs->tok->loc,
								  "Type error. Expected expression of type bool.");
			}
			exp->type = rhs->type;
			exp->is_addressable = false;
			exp->is_mutable = false;
		} break;
		case unaop_address_of: {
			rhs = infer_type(p, una->exp, ret, scope);
			if (!rhs->is_addressable) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, rhs->tok->sv, rhs->tok->loc,
								  "Expression is not addressable.");
			}
			struct type *ptr_type = POOL_ALLOC(&p->data, struct type);
			ptr_type->as.ptr = rhs->type;
			exp->is_mutable = rhs->is_mutable;
			exp->is_addressable = false;
			exp->type = ptr_type;
			ptr_type->tag = rhs->is_mutable ? ast_type_mut_ptr : ast_type_ptr;
		} break;
		case unaop_dereference: {
			rhs = infer_type(p, una->exp, ret, scope);
			if (rhs->type->tag == ast_type_mut_ptr) {
				exp->type = rhs->type->as.mut_ptr;
				exp->is_addressable = true;
				exp->is_mutable = true;
			} else if (rhs->type->tag == ast_type_ptr) {
				exp->type = rhs->type->as.ptr;
				exp->is_addressable = true;
				exp->is_mutable = false;
			} else {
				log_error_and_die(p->lexer.filename, p->lexer.contents, rhs->tok->sv, rhs->tok->loc,
								  "Unable to dereference. Expression is not a pointer.");
			}
		} break;
		case unaop_index: {
			lhs = infer_type(p, exp->as.idx.exp, ret, scope); // base
			rhs = infer_type(p, exp->as.idx.idx, ret, scope); // index
			if (!type_is_integer(rhs->type)) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, rhs->tok->sv, rhs->tok->loc,
								  "Index expression is not integer type.");
			}
			if (rhs->type->tag == ast_type_intlit) {
				rhs->type->tag = ast_type_i64;
			}
			switch ((int)lhs->type->tag) {
			case ast_type_mut_ptr:
			case ast_type_ptr: {
				struct type *ptr_base = lhs->type->as.ptr;
				if (ptr_base->tag == ast_type_array) {
					exp->type = ptr_base->as.array.base;
					exp->is_addressable = true;
					exp->is_mutable = lhs->type->tag == ast_type_mut_ptr;
				} else {
					log_error_and_die(p->lexer.filename, p->lexer.contents, lhs->tok->sv, lhs->tok->loc,
									  "Expression cannot be indexed.");
				}
			} break;
			case ast_type_mut_slice:
			case ast_type_slice:
				exp->type = lhs->type->as.slice;
				exp->is_addressable = true;
				exp->is_mutable = lhs->is_mutable;
				break;
			case ast_type_array: {
				exp->type = lhs->type->as.array.base;
				exp->is_addressable = true;
				exp->is_mutable = lhs->is_mutable;
			} break;
			default:
				log_error_and_die(p->lexer.filename, p->lexer.contents, lhs->tok->sv, lhs->tok->loc,
								  "Expression cannot be indexed.");
				break;
			}
		} break;
		case unaop_call: {
			struct call *call = &exp->as.call;
			struct expression *proc = infer_type(p, call->proc, ret, scope);
			if (proc->type->tag != ast_type_proc) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, proc->tok->sv, proc->tok->loc,
								  "Expression is not a procedure.");
			}
			struct proc_type *pt = &proc->type->as.proc;
			if (call->args.len != pt->args.len) {
				log_error_and_die(p->lexer.filename, p->lexer.contents, exp->tok->sv, exp->tok->loc,
								  "Arity mismatch. Procedure expected %d arguments, but received %d.",
								  pt->args.len, call->args.len);
			}
			for (size_t i = 0; i < call->args.len; ++i) {
				struct expression *arg = infer_type(p, call->args.elems[i], pt->args.elems[i], scope);
				TYPE_EQ(pt->args.elems[i], arg->type, arg);
			}
			exp->type = pt->ret;
			exp->is_mutable = true;
			exp->is_addressable = false;
		} break;
		case unaop_cast: {
			struct expression *from = infer_type(p, exp->as.cast.exp, ret, scope);
			assert(type_is_numeric_scalar(from->type));
			assert(type_is_numeric_scalar(exp->as.cast.type));
			exp->type = exp->as.cast.type;
			exp->is_addressable = false;
			exp->is_mutable = false;
		} break;
		default: FAILWITH("Unreachable"); break;
		}
	} break;
	case ast_exp_zero_initializer: {
		exp->is_mutable = true;
		exp->is_addressable = false;
	} break;
	case ast_exp_initializer: {
		struct expression_stack *init_exps = &exp->as.init;
		da_foreach(e, init_exps) {
			infer_type(p, *e, ret, scope);
		}
		exp->is_mutable = true;
		exp->is_addressable = false;
	} break;
	case ast_exp_named_initializer: {
		struct expression_stack *init_exps = &exp->as.named_init.exps;
		da_foreach(e, init_exps) {
			infer_type(p, *e, ret, scope);
		}
		exp->is_mutable = true;
		exp->is_addressable = false;
	} break;
	case ast_exp_if: {
		struct exp_if *br = &exp->as.iff;
		struct expression *cond = infer_type(p, br->cond, ret, scope);
		if (!type_is_bool(cond->type)) {
			log_error_and_die(p->lexer.filename, p->lexer.contents, cond->tok->sv, cond->tok->loc,
							  "If condition must be a boolean expression.");
		}
		lhs = infer_type(p, br->tb, ret, scope);
		if (br->fb) {
			rhs = infer_type(p, br->fb, ret, scope);
			TYPE_EQ(lhs->type, rhs->type, exp);
			exp->type = rhs->type = lhs->type;
			exp->is_addressable = lhs->is_addressable && rhs->is_addressable;
			exp->is_mutable = lhs->is_mutable && rhs->is_mutable;
		} else {
			exp->type = lhs->type;
			exp->is_addressable = lhs->is_addressable;
			exp->is_mutable = lhs->is_mutable;
		}
	} break;
	case ast_exp_while: {
		struct expression *cond = infer_type(p, exp->as.wloop.cond, ret, scope);
		if (!type_is_bool(cond->type)) {
			log_error_and_die(p->lexer.filename, p->lexer.contents, cond->tok->sv, cond->tok->loc,
							  "While loop condition must be a boolean expression.");
		}
		infer_type(p, exp->as.wloop.body, ret, scope);
		exp->type = &AST_TYPE_VOID;
		exp->is_addressable = false;
		exp->is_mutable = false;
	} break;
	case ast_exp_case: {
		struct exp_case *c = &exp->as.ccase;
		struct expression *cond = infer_type(p, c->cexp, ret, scope);
		struct expression *branch = NULL;
		bool is_addressable, is_mutable;
		da_foreach(cb, &c->branches) {
			da_foreach(match, &cb->matches) {
				infer_type(p, *match, NULL, scope);
				TYPE_EQ(cond->type, (*match)->type, *match);
			}
			infer_type(p, cb->exp, NULL, scope);
			if (branch == NULL) {
				branch = cb->exp;
				is_addressable = branch->is_addressable;
				is_mutable = branch->is_mutable;
			} else {
				TYPE_EQ(branch->type, cb->exp->type, cb->exp);
				is_addressable = is_addressable && branch->is_addressable;
				is_mutable = is_mutable && branch->is_mutable;
			}
		}
		assert(branch != NULL);
		if (c->else_exp) {
			infer_type(p, c->else_exp, NULL, scope);
			TYPE_EQ(branch->type, c->else_exp->type, c->else_exp);
			is_addressable = is_addressable && c->else_exp->is_addressable;
			is_mutable = is_mutable && c->else_exp->is_mutable;
		}
		exp->is_addressable = is_addressable;
		exp->is_mutable = is_mutable;
		exp->type = branch->type;
	} break;
	case ast_exp_return: FAILWITH("TODO: ast_exp_return"); break;
	case ast_exp_break: FAILWITH("TODO: ast_exp_break"); break;
	case ast_exp_continue: FAILWITH("TODO: ast_exp_continue"); break;
	default: FAILWITH("Unreachable condition."); break;
	}
	return exp;
}

struct expression *ast_desugar(Parser *p, struct expression *exp)
{
	if (exp == NULL) return exp;
	switch (exp->tag) {
	case ast_exp_definition:
		exp->as.def.exp = ast_desugar(p, exp->as.def.exp);
		return exp;
	case ast_exp_let:
		exp->as.let.def.exp = ast_desugar(p, exp->as.let.def.exp);
		exp->as.let.body = ast_desugar(p, exp->as.let.body);
		return exp;
	case ast_exp_while:
		exp->as.wloop.cond = ast_desugar(p, exp->as.wloop.cond);
		exp->as.wloop.body = ast_desugar(p, exp->as.wloop.body);
		return exp;
	case ast_exp_string: return exp;
	case ast_exp_initializer: {
		struct expression_stack *exps = &exp->as.init;
		for (size_t i = 0; i < exps->len; ++i) {
			exps->elems[i] = ast_desugar(p, exps->elems[i]);
		}
		return exp;
	} break;
	case ast_exp_named_initializer: FAILWITH("TODO: ast_exp_named_initializer"); break;
	case ast_exp_zero_initializer:	FAILWITH("TODO: ast_exp_zero_initializer"); break;
	case ast_exp_procedure_literal:
		exp->as.proc.body = ast_desugar(p, exp->as.proc.body);
		return exp;
	case ast_exp_undefined:			FAILWITH("TODO: ast_exp_undefined"); break;
	case ast_exp_unary:
		if ((enum unaop)exp->as.una.op == unaop_call) {
			exp->as.call.proc = ast_desugar(p, exp->as.call.proc);
			for (size_t i = 0; i < exp->as.call.args.len; ++i) {
				exp->as.call.args.elems[i] = ast_desugar(p, exp->as.call.args.elems[i]);
			}
		} else if ((enum unaop)exp->as.una.op == unaop_index) {
			struct index *idx = &exp->as.idx;
			idx->exp = ast_desugar(p, idx->exp);
			idx->idx = ast_desugar(p, idx->idx);
		} else {
			exp->as.una.exp = ast_desugar(p, exp->as.una.exp);
		}
		return exp;
	case ast_exp_if:
		exp->as.iff.cond = ast_desugar(p, exp->as.iff.cond);
		exp->as.iff.tb = ast_desugar(p, exp->as.iff.tb);
		exp->as.iff.fb = ast_desugar(p, exp->as.iff.fb);
		return exp;
	case ast_exp_case:				FAILWITH("TODO: ast_exp_case"); break;
	case ast_exp_return:			FAILWITH("TODO: ast_exp_return"); break;
	case ast_exp_break:				FAILWITH("TODO: ast_exp_break"); break;
	case ast_exp_continue:			FAILWITH("TODO: ast_exp_continue"); break;
	case ast_exp_literal:
	case ast_exp_ident:
	case ast_exp_extern_symbol: return exp;
	case ast_exp_get_ptr:
		exp->as.get_ptr = ast_desugar(p, exp->as.get_ptr);
		return exp;
	case ast_exp_get_len:
		exp->as.get_len = ast_desugar(p, exp->as.get_len);
		return exp;
	case ast_exp_binary: {
		struct expression *left = ast_desugar(p, exp->as.bin.left);
		struct expression *right = ast_desugar(p, exp->as.bin.right);
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
	ir_op_alloca,
	ir_op_load,
	ir_op_loadglobl,
	ir_op_loadconst,
	ir_op_loadimm,
	ir_op_store,
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

#define IR_OBJ_HDDR \
	enum ir_obj_tag tag; \
	char *link;			 \
	bool is_static

struct ir_obj_hddr {
	IR_OBJ_HDDR;
};

struct ir_proc {
	IR_OBJ_HDDR;
	struct definition *def;
	struct procedure *node;
	struct ir_blk *entry;
	uint16_t regc;
	uint16_t argc;
	uint16_t retc;
};

struct ir_extern {
	IR_OBJ_HDDR;
};

struct ir_data {
	IR_OBJ_HDDR;
	size_t size;
	void *dat;
};

union ir_object {
	enum ir_obj_tag tag;
	struct ir_obj_hddr hddr;
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
	case ast_type_floatlit:
	case ast_type_intlit:
	case ast_type_ptr:
	case ast_type_mut_ptr:
	case ast_type_proc:
		return 8;
	case ast_type_slice:
	case ast_type_mut_slice:
		return 16;
	case ast_type_array:
		switch (t->as.array.stag) {
		case AT_UNSIZED:
			FAILWITH("TODO: Error: Cannot determine size of type unsized array.");
			break;
		case AT_EXPRESSION:
			FAILWITH("TODO: AT_EXPRESSION (type_size)");
			break;
		case AT_LITERAL:
			return type_size(t->as.array.base) * t->as.array.size.sz;
		default: FAILWITH("Unreachable"); break;
		}
		break;
	case ast_type_struct: {
		struct struct_type *st = &t->as.strct;
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
	case ast_type_alias: FAILWITH("TODO: ast_type_alias"); break;
	case ast_type_cons: FAILWITH("TODO: ast_type_cons"); break;
	case ast_type_vector: FAILWITH("TODO: ast_type_vector"); break;
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
	case ast_type_floatlit:
	case ast_type_intlit:
	case ast_type_ptr:
	case ast_type_mut_ptr:
	case ast_type_proc:
	case ast_type_slice:
	case ast_type_mut_slice:
		return 8;
	case ast_type_array:
		return type_alignment(t->as.array.base);
	case ast_type_struct: {
		struct struct_type *st = &t->as.strct;
		size_t alignment = 1;
		for (size_t i = 0; i < st->len; ++i) {
			size_t a = type_alignment(st->elems[i].type);
			alignment = MAX(alignment, a);
		}
		return alignment;
	} break;
	case ast_type_alias: FAILWITH("TODO: ast_type_alias"); break;
	case ast_type_cons: FAILWITH("TODO: ast_type_cons"); break;
	case ast_type_vector: FAILWITH("TODO: ast_type_vector"); break;
	}
	FAILWITH("Unreachable");
	return 0;
}

size_t struct_member_offset(struct struct_type *st, size_t index)
{
	assert(index < st->len);
	size_t offset = 0;
	for (size_t i = 0; i < index; ++i) {
		offset = align_adjust(offset, type_alignment(st->elems[i].type));
		offset += type_size(st->elems[i].type);
	}
	return align_adjust(offset, type_alignment(st->elems[index].type));
}

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

struct ast_comp_dest {
	enum {
		DST_NONE,
		DST_VAL,
		DST_REF,
		DST_CPY,
	} tag;
	uint16_t reg;
};

#define DEST_VAL(_reg) ((struct ast_comp_dest){.tag = DST_VAL, .reg = (_reg)})
#define DEST_REF(_reg) ((struct ast_comp_dest){.tag = DST_REF, .reg = (_reg)})
#define DEST_CPY(_reg) ((struct ast_comp_dest){.tag = DST_CPY, .reg = (_reg)})
#define DEST_NONE()    ((struct ast_comp_dest){.tag = DST_NONE})

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
	struct formals *formals = &proc->node->formals;
	struct scope *scope = &proc->node->scope;
	struct ir_blk *entry = POOL_ALLOC(&tl->data, struct ir_blk);
	proc->entry = entry;
	size_t argn = 0;
	for (; argn < formals->len; ++argn) {
		struct definition *formal = &formals->elems[argn];
		assert(formal->type != NULL);
		/* TODO: For now, only handle cases where type can fit into a single register */
		//assert(type_size(formal->type) <= 8);
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
	struct ir_blk *res = ast_compile_expression(proc->node->body, DEST_VAL(reg), proc_id, entry, scope, tl);
	da_append(&res->term.args, reg);
	res->term.op = ir_op_ret;
}

struct ir_blk *ast_compile_expression(struct expression *exp, struct ast_comp_dest dst, size_t proc_id,
									  struct ir_blk *blk, struct scope *scope, struct ir_toplevel *tl)
{
	switch (exp->tag) {
	case ast_exp_let: {
		struct let *let = &exp->as.let;
		struct definition *def = &let->def;
		int var  = ir_proc_new_reg(tl, proc_id);
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
			default:
				FAILWITH("TODO: ast_exp_literal");
				break;
			}
		} else {
			FAILWITH("TODO: ast_exp_literal");
		}
		return blk;
	} break;
	case ast_exp_zero_initializer: {
		assert(dst.tag == DST_CPY);
		da_append(&blk->code, (IR_Ins){
				.op = ir_op_memzero,
				.type = exp->type,
				.dst = dst.reg,
			});
		return blk;
	} break;
	case ast_exp_initializer: {
		switch (dst.tag) {
		case DST_VAL: {
			da_append(&blk->code, (IR_Ins){
					.op   = ir_op_retval,
					.type = exp->type,
					.dst  = dst.reg,
				});
		} [[fallthrough]];
		case DST_CPY: {
			if (exp->type->tag == ast_type_array) {
				struct type *base_type = POOL_ALLOC(&tl->data, struct type);
				base_type->tag = ast_type_ptr;
				base_type->as.ptr = exp->type;
				struct array_type *at = &exp->type->as.array;
				for (size_t i = 0; i < exp->as.init.len; ++i) {
					int tmp = ir_proc_new_reg(tl, proc_id);
					blk = ast_compile_expression(exp->as.init.elems[i], DEST_VAL(tmp), proc_id, blk, scope, tl);
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
					da_append(&blk->code, (IR_Ins){
							.op = ir_op_store,
							.type = at->base,
							.dst = ptr,
							.arg.rx[0] = tmp,
						});
				}
				return blk;
			} else if (exp->type->tag == ast_type_struct) {
				struct struct_type *st = &exp->type->as.strct;
				struct type *base_type = POOL_ALLOC(&tl->data, struct type);
				base_type->tag = ast_type_ptr;
				base_type->as.ptr = exp->type;
				for (size_t i = 0; i < st->len; ++i) {
					struct struct_member *mem = &st->elems[i];
					int tmp = ir_proc_new_reg(tl, proc_id);
					blk = ast_compile_expression(exp->as.init.elems[i], DEST_VAL(tmp), proc_id, blk, scope, tl);
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
					da_append(&blk->code, (IR_Ins){
							.op = ir_op_store,
							.type = mem->type,
							.dst = ptr,
							.arg.rx[0] = tmp,
						});
				}
				return blk;
			} else {
				FAILWITH("TODO: ast_exp_initializer");
			}
		} break;
		case DST_REF: FAILWITH("TODO: DST_REF"); break;
		case DST_NONE: FAILWITH("TODO: DST_NONE"); break;
		default: FAILWITH("Unreachable"); break;
		}
	} break;
	case ast_exp_named_initializer: FAILWITH("TODO: ast_exp_named_initializer"); break;
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
		case DST_CPY: FAILWITH("TODO: ast_exp_undefined"); break;
		case DST_REF: FAILWITH("TODO: ast_exp_undefined"); break;
		case DST_NONE: FAILWITH("TODO: ast_exp_undefined"); break;
		default:      FAILWITH("Unreachable"); break;
		}
		return blk; /* do nothing */
	} break;
	case ast_exp_string: {
		char *str = NULL;
		size_t length = sv_unescape_string(exp->tok->sv, &str);
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
		case DST_NONE: FAILWITH("TODO: ast_exp_string"); break;
		default:      FAILWITH("Unreachable"); break;
		}
		return blk;
	} break;
	case ast_exp_extern_symbol: FAILWITH("TODO: ast_exp_extern_symbol"); break;
	case ast_exp_ident: {
		struct definition *def = lookup_definition(scope, exp->as.id->sv);
		assert(def != NULL);
		switch (dst.tag) {
		case DST_VAL: {
			if (def->is_global) {
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_loadglobl,
						.type = def->type,
						.dst  = dst.reg,
						.arg.u32 = def->ir_symbol,
					});
			} else {
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_load,
						.type = def->type,
						.dst  = dst.reg,
						.arg.rx[0] = def->ir_symbol,
					});
			}
		} break;
		case DST_CPY: FAILWITH("TODO: ast_exp_ident"); break;
		case DST_REF: {
			if (def->is_global) {
				FAILWITH("TODO: ast_exp_ident");
			} else {
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_mov,
						.dst  = dst.reg,
						.arg.rx[0] = def->ir_symbol,
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
		{
		LBL_binop:
			int left_reg = ir_proc_new_reg(tl, proc_id);
			int right_reg = ir_proc_new_reg(tl, proc_id);
			struct ir_blk *left  = ast_compile_expression(bin->left, DEST_VAL(left_reg), proc_id, blk, scope, tl);
			struct ir_blk *right = ast_compile_expression(bin->right, DEST_VAL(right_reg), proc_id, left, scope, tl);
			switch (dst.tag) {
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
			default: FAILWITH("Unreachable"); break;
			}
			return right;
		}
		case binop_member: {
			struct expression *base = bin->left;
			struct expression *idx = bin->right;
			switch (dst.tag) {
			case DST_VAL: {
				int base_reg = ir_proc_new_reg(tl, proc_id);
				int index_reg = ir_proc_new_reg(tl, proc_id);
				struct type *base_type = base->type;
				if (type_is_struct_ptr(base->type)) {
					blk = ast_compile_expression(base, DEST_VAL(base_reg), proc_id, blk, scope, tl);
				} else {
					assert(base->type->tag == ast_type_struct);
					blk = ast_compile_expression(base, DEST_REF(base_reg), proc_id, blk, scope, tl);
					base_type = POOL_ALLOC(&tl->data, struct type);
					base_type->tag = ast_type_ptr;
					base_type->as.ptr = base->type;
				}
				if (idx->tag == ast_exp_literal) {
					assert(type_is_integer(idx->type));
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
				struct definition *def = lookup_definition(scope, bin->left->as.id->sv);
				assert(def != NULL);
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
			case DST_NONE: FAILWITH("TODO: DST_NONE (ast_exp_get_len)"); break;
			default: FAILWITH("Unreachable"); break;
			}
		} else if (type_is_array(target->type)) {
			size_t len = 0;
			switch (target->type->as.array.stag) {
			case AT_LITERAL: len = target->type->as.array.size.sz; break;
			case AT_EXPRESSION: FAILWITH("TODO: AT_EXPRESSION"); break;
			case AT_UNSIZED: FAILWITH("TODO: AT_UNSIZED"); break;
			default: FAILWITH("Unreachable"); break;
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
			default: FAILWITH("Unreachable"); break;
			}
		} break;
		case unaop_neg: {
			int reg = ir_proc_new_reg(tl, proc_id);
			blk = ast_compile_expression(una->exp, DEST_VAL(reg), proc_id, blk, scope, tl);
			switch (dst.tag) {
			case DST_VAL: {
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_neg,
						.type = exp->type,
						.dst  = dst.reg,
						.arg.rx[0] = reg,
					});
				return blk;
			} break;
			case DST_CPY: FAILWITH("TODO: unaop_neg"); break;
			case DST_REF: FAILWITH("TODO: unaop_neg"); break;
			case DST_NONE: FAILWITH("TODO: unaop_neg"); break;
			default: FAILWITH("Unreachable"); break;
			}
		} break;
		case unaop_address_of: {
			switch (dst.tag) {
			case DST_VAL: {
				return ast_compile_expression(una->exp, DEST_REF(dst.reg), proc_id, blk, scope, tl);
			} break;
			case DST_CPY: FAILWITH("TODO: unaop_address_of"); break;
			case DST_REF: FAILWITH("TODO: unaop_address_of"); break;
			case DST_NONE: FAILWITH("TODO: unaop_address_of"); break;
			default: FAILWITH("Unreachable"); break;
			}
		} break;
		case unaop_dereference: {
			int reg = ir_proc_new_reg(tl, proc_id);
			blk = ast_compile_expression(una->exp, DEST_REF(reg), proc_id, blk, scope, tl);
			if (dst.tag == DST_VAL) {
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_load,
						.type = exp->type,
						.dst  = dst.reg,
						.arg.rx[0] = reg,
					});
			} else {
				FAILWITH("TODO: unaop_dereference");
			}
			return blk;
		} break;
		case unaop_index: {
			struct expression *base = exp->as.idx.exp;
			struct expression *idx =  exp->as.idx.idx;
			switch (dst.tag) {
			case DST_VAL: {
				int base_reg = ir_proc_new_reg(tl, proc_id);
				int index_reg = ir_proc_new_reg(tl, proc_id);
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
					base_type = slice_type_to_array_ptr_type(&tl->data, base->type);
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
			case DST_REF:
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
			case DST_CPY: FAILWITH("TODO: unaop_cast"); break;
			case DST_REF: FAILWITH("TODO: unaop_cast"); break;
			case DST_NONE: FAILWITH("TODO: unaop_cast"); break;
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
		int treg = ir_proc_new_reg(tl, proc_id);
		int freg = ir_proc_new_reg(tl, proc_id);
		struct ir_blk *tb_end = ast_compile_expression(iff->tb, DEST_VAL(treg), proc_id, tblk, scope, tl);
		/* true branch term */
		blk->term.b0 = tblk;
		tb_end->term.op = ir_op_goto;
		da_append(&tb_end->term.args, treg);
		tb_end->term.b0 = join;
		if (iff->fb) { /* if false branch exists */
			struct ir_blk *fblk = POOL_ALLOC(&tl->data, struct ir_blk);
			struct ir_blk *fb_end = ast_compile_expression(iff->fb, DEST_VAL(freg), proc_id, fblk, scope, tl);
			/* false branch term */
			blk->term.b1 = fblk;
			fb_end->term.op = ir_op_goto;
			da_append(&fb_end->term.args, freg);
			fb_end->term.b0 = join;
		} else {
			blk->term.b1 = join;
		}
		blk->term.j0 = join;
		switch (dst.tag) {
		case DST_VAL: {
			da_append(&join->args, dst.reg);
		} break;
		case DST_CPY: {
			int tmp = ir_proc_new_reg(tl, proc_id);
			da_append(&join->args, tmp);
			da_append(&join->code, (IR_Ins){
					.op = ir_op_store,
					.type = exp->type,
					.dst = dst.reg,
					.arg.rx[0] = tmp,
				});
		} break;
		case DST_REF: FAILWITH("TODO: DST_REF"); break;
		case DST_NONE: FAILWITH("TODO: DST_NONE"); break;
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
	case ast_exp_case: FAILWITH("TODO: ast_exp_case"); break;
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
	fprintf(f, SV_FMT"#", SV_ARGS(&ident));
	ast_type_fprint(type, f);
	fclose(f);
	return ptr;
}

struct ir_toplevel ast_compile(struct scope *scope)
{
	struct symtbl *st = &scope->symtbl;
	struct ir_toplevel tl = {0};
	for (size_t i = 0; i < st->len; ++i) {
		struct definition *def = st->elems[i];
		struct expression *exp = def->exp;
		union ir_object p = {0};
		def->ir_symbol = tl.len;
		def->is_global = true;
		if (exp->tag == ast_exp_procedure_literal) {
			p.proc.tag  = IRO_PROC;
			if (sv_is_equal(def->id->sv, sv_of_cstr("main"))) {
				/* entry point */
				p.proc.link = strndup(def->id->sv.ptr, def->id->sv.len);
			} else {
				p.proc.link = generate_mangled_name(def->id->sv, def->type);
			}
			p.proc.def  = def;
			p.proc.node = &exp->as.proc;
			p.proc.regc = 0;
			da_append(&tl, p);
		} else if (exp->tag == ast_exp_extern_symbol) {
			if (type_is_procedure(def->type)) {
				p.ext.tag = IRO_EXTERN_PROC;
			} else {
				p.ext.tag = IRO_EXTERN_DATA;
			}
			p.ext.link = strndup(def->id->sv.ptr, def->id->sv.len);
			da_append(&tl, p);
		} else {
			FAILWITH("TODO: compile toplevel definitions.");
		}
	}
	for (size_t i = 0; i < tl.len; ++i) {
		if (tl.elems[i].tag == IRO_PROC) {
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
		fprintf(file, SV_FMT"[%zu]", SV_ARGS(&proc->def->id->sv), proc->def->ir_symbol);
	} else {
		fprintf(file, "%p", proc);
	}
	fprintf(file, "(argc=%d, retc=%d, regc=%d)", proc->argc, proc->retc, proc->regc);
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
	switch (type_size(type)) {
	case 1: return asm_reg_b_name[reg];
	case 2: return asm_reg_w_name[reg];
	case 4: return asm_reg_d_name[reg];
	case 8: return asm_reg_q_name[reg];
	}
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
			if (type_size(ins->type) <= 16) {
				addr->tag = ADDR_STACK;
				int sz = type_size(ins->type);
				size_t before = ctx->stack_size;
				ctx->stack_size = align_adjust(ctx->stack_size, type_alignment(ins->type));
				ctx->stack_size += sz;
				addr->as.i = -ctx->stack_size;
				size_t after = ctx->stack_size;
				addr->extra.stack_size = after - before;
				addr->type = POOL_ALLOC(&tl->data, struct type);
				addr->type->tag = ast_type_ptr;
				addr->type->as.ptr = ins->type;
			} else {
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
				dst->tag = ADDR_TEMP_REG;
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
				ctx->stack_size = align_adjust(ctx->stack_size, type_alignment(ins->type));
				ctx->stack_size += type_size(ins->type);
				ret->extra.offset = -ctx->stack_size;
			} else {
				ret->tag = ADDR_STACK;
				size_t before = ctx->stack_size;
				ctx->stack_size = align_adjust(ctx->stack_size, type_alignment(ins->type));
				ctx->stack_size += type_size(ins->type);
				ret->as.i = -ctx->stack_size;
				size_t after = ctx->stack_size;
				ret->extra.stack_size = after - before;
				ret->type = POOL_ALLOC(&tl->data, struct type);
				ret->type->tag = ast_type_ptr;
				ret->type->as.ptr = ins->type;
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
			} else if (addr->tag == ADDR_STACK_LOAD) {
				/* do nothing */
			} else if (addr->tag == ADDR_IMM_INT) {
				/* do nothing */
			} else if (addr->tag == ADDR_TEMP_REG) {
				addr->tag = ADDR_REGISTER;
				addr->as.i = asm_ret_regs[0];
			} else if (addr->tag == ADDR_REGISTER) {
				assert(addr->as.i == asm_ret_regs[0]);
			} else {
				FAILWITH("TODO: addr->tag == %s", asm_addr_tag_to_str(addr->tag));
			}
		} else {
			assert(term->args.len == 0);
		}
	} break;
	case ir_op_goto: {
		for (size_t i = 0; i < term->args.len; ++i) {
			struct asm_address *addr = &ctx->vars[term->args.elems[i]];
			assert(addr->tag != ADDR_NONE);
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

struct asm_context *asm_create_context(struct ir_proc *proc,
									   struct da_pointers *blocks,
									   struct ir_toplevel *tl,
									   struct asm_context *ctx)
{
	ctx->varc = proc->regc;
	ctx->vars = calloc(proc->regc, sizeof(struct asm_address));
	ctx->is_leaf = true;
	ctx->arg_stack_size = 16;
	for (int i = 0; i < ASM_REG_COUNT; ++i) {
		ctx->assigned[i] = REG_FREE;
	}
	struct ir_blk *entry = proc->entry;
	//proc->
	struct type *ret_type = proc->def->type->as.proc.ret;
	size_t ret_type_size = type_size(ret_type);
	int arg_reg = ret_type_size <= 16 ? 0 : 1;
	for (size_t arg_num = 0; arg_num < entry->args.len; ++arg_num) {
		struct type *type = proc->def->type->as.proc.args.elems[arg_num];
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

int asm_get_register_owner(struct asm_context *ctx, enum asm_register reg)
{
	return ctx->assigned[reg];
}

bool asm_register_assigned_p(struct asm_context *ctx, enum asm_register reg)
{
	return ctx->assigned[reg] != REG_FREE;
}

int asm_reserve_register(struct asm_context *ctx, enum asm_register reg, int local)
{
	if (ctx->assigned[reg] == local) return reg;
	assert(asm_register_assigned_p(ctx, reg) == false);
	ctx->assigned[reg] = local;
	ctx->clobbered |= 1u << reg;
	return reg;
}

enum asm_register asm_emit_move_to_callee_save(struct asm_address *addr, struct asm_procedure *code);

int asm_reserve_register_safe_mov(struct asm_context *ctx, enum asm_register reg, int dst, int src,
								  struct asm_procedure *code)
{
	if (ctx->assigned[reg] == dst) return reg;
	if (ctx->assigned[reg] == src) {
		asm_emit_move_to_callee_save(&ctx->vars[src], code);
	}
	return asm_reserve_register(ctx, reg, dst);
}

enum asm_register asm_assign_callee_save_register(struct asm_context *ctx, int local)
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

enum asm_register asm_assign_register(struct asm_context *ctx, int local)
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

void asm_unassign_register(struct asm_context *ctx, enum asm_register reg)
{
	ctx->assigned[reg] = REG_FREE;
}

void asm_unassign_local(struct asm_context *ctx, int local)
{
	struct asm_address *addr = &ctx->vars[local];
	assert(addr->tag == ADDR_REGISTER);
	if (ctx->assigned[addr->as.i] == local) {
		asm_unassign_register(ctx, addr->as.i);
	}
}

bool can_optimize_red_zone(struct asm_context *ctx)
{
	return ctx->is_leaf && ctx->stack_size <= RED_ZONE_SIZE;
}

bool must_alloc_stack_frame(struct asm_context *ctx)
{
	return ctx->stack_size > 0 && can_optimize_red_zone(ctx) == false;
}

const char *ir_object_link_name(struct ir_toplevel *tl, union ir_object *obj)
{
	static char buf[400];
	if (tl->is_dll && !obj->hddr.is_static && obj->hddr.tag == IRO_EXTERN_DATA) {
		snprintf(buf, sizeof(buf), "\"%s\"@GOTPCREL", obj->hddr.link);
	} else {
		snprintf(buf, sizeof(buf), "\"%s\"", obj->hddr.link);
	}
	return buf;
}

const char *asm_source_operand_to_str(struct asm_address *src, struct type *type,
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
	case ADDR_NONE:		  FAILWITH("TODO: ADDR_NONE");	     break;
	case ADDR_ARGUMENT:
	default: FAILWITH("Unreachable"); break;
	}
	return s;
}

enum asm_register asm_emit_move_to_callee_save(struct asm_address *addr, struct asm_procedure *code)
{
	assert(addr->tag == ADDR_REGISTER);
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

enum asm_register asm_get_blk_arg_register(struct asm_context *ctx, int blk_arg)
{
	struct asm_address *b = &ctx->vars[blk_arg];
	assert(b->tag == ADDR_BLK_ARG);
	struct asm_address *a = &ctx->vars[b->as.i];
	if (a->tag == ADDR_REGISTER) {
		return a->as.i;
	} else if (a->tag == ADDR_BLK_ARG) {
		return asm_get_blk_arg_register(ctx, b->as.i);
	}
	FAILWITH("Unreachable");
	return 0;
}

#define MATCH_ADDR3(_a0, _a1, _a2)				\
	(ctx->vars[ins->dst].tag == (_a0)			\
	 &&	ctx->vars[ins->arg.rx[0]].tag == (_a1)	\
	 &&	ctx->vars[ins->arg.rx[1]].tag == (_a2))

#define MATCH_ADDR2(_a0, _a1)					\
	(ctx->vars[ins->dst].tag == (_a0)			\
	 &&	ctx->vars[ins->arg.rx[0]].tag == (_a1))

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
	assert(x->tag == ADDR_STACK);
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
		asm_reserve_register_safe_mov(ctx, dst->as.wide[0], ins->dst, ins->arg.rx[0], code);
		asm_reserve_register_safe_mov(ctx, dst->as.wide[1], ins->dst, ins->arg.rx[0], code);
		append_line(&code->body, fmt_str("\tmovq (%s), %s\n",
										 asm_reg_q_name[x->as.i],
										 asm_reg_q_name[dst->as.wide[0]]));
		append_line(&code->body, fmt_str("\tmovq 8(%s), %s\n",
										 asm_reg_q_name[x->as.i],
										 asm_reg_q_name[dst->as.wide[1]]));
		asm_unassign_local(ctx, ins->arg.rx[0]);
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
	assert(dst->tag == ADDR_WIDE || dst->tag == ADDR_WIDE_ARG);
	assert(obj->tag == IRO_DATA);
	assert(type->tag == ast_type_slice || type->tag == ast_type_mut_slice);
	assert(type_size(type) == 16);
	struct type *base_type = type->as.slice;
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
	uint32_t offset = 0;
	append_line(&code->body, fmt_str("\tsubq $%zu, %%rsp\n", align_adjust(ts, 8)));
	for (uint32_t i = 0; i < counts[0]; ++i) {
		append_line(&code->body, fmt_str("\tmovq %ld(%%rbp), %s\n",
										 x->as.i + offset, asm_reg_q_name[tmp]));
		append_line(&code->body, fmt_str("\tmovq %s, %u(%%rsp)\n",
										 asm_reg_q_name[tmp], offset));
		offset += 8;
	}
	for (uint32_t i = 0; i < counts[1]; ++i) {
		append_line(&code->body, fmt_str("\tmovl %ld(%%rbp), %s\n",
										 x->as.i + offset, asm_reg_d_name[tmp]));
		append_line(&code->body, fmt_str("\tmovl %s, %u(%%rsp)\n",
										 asm_reg_d_name[tmp], offset));
		offset += 4;
	}
	for (uint32_t i = 0; i < counts[2]; ++i) {
		append_line(&code->body, fmt_str("\tmovw %ld(%%rbp), %s\n",
										 x->as.i + offset, asm_reg_w_name[tmp]));
		append_line(&code->body, fmt_str("\tmovw %s, %u(%%rsp)\n",
										 asm_reg_w_name[tmp], offset));
		offset += 2;
	}
	for (uint32_t i = 0; i < counts[3]; ++i) {
		append_line(&code->body, fmt_str("\tmovb %ld(%%rbp), %s\n",
										 x->as.i + offset, asm_reg_b_name[tmp]));
		append_line(&code->body, fmt_str("\tmovb %s, %u(%%rsp)\n",
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
	if (asm_register_assigned_p(ctx, dst->as.i)) {
		printf("ins->dst = %%%d\n", ins->dst);
		printf("owner = %%%d\n", asm_get_register_owner(ctx, dst->as.i));
	}
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
	if (type_is_integer(x->type) && ins->type->tag == ast_type_i8) {
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
	} else {
		FAILWITH("TODO: cast %s -> %s", ast_type_to_str(x->type), ast_type_to_str(ins->type));
	}
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
	switch (type_size(ins->type)) {
	case 1: FAILWITH("TODO: invalid type size."); break;
	case 2: FAILWITH("TODO: invalid type size."); break;
	case 4: conv = "cltd"; break;
	case 8: conv = "cqto"; break;
	default: FAILWITH("TODO: invalid type size."); break;
	}
	if (MATCH_ADDR3(ADDR_REGISTER, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
		asm_reserve_register(ctx, dst->as.i, ins->dst);
		goto fallthrough_ADDR_TEMP_REG_ADDR_STACK_LOAD_ADDR_IMM_INT;
	} else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
		dst->tag = ADDR_REGISTER;
		dst->as.i = asm_assign_register(ctx, ins->dst);
	fallthrough_ADDR_TEMP_REG_ADDR_STACK_LOAD_ADDR_IMM_INT:
		enum asm_register tmp = asm_assign_register(ctx, ins->dst);
		int suffix = asm_suffix(ins->type);
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
	} else if (MATCH_ADDR3(ADDR_ARGUMENT, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
		dst->extra.defered.fun = defer_emit_div_mod;
		dst->extra.defered.ins = ins;
		dst->extra.defered.dat = (void*)remainder_p;
	} else {
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
		const char *d_name = asm_reg_name(dst->as.i, type);
		asm_reserve_register(ctx, dst->as.i, ins->dst);
		int suffix = asm_suffix(type);
		if (dst->as.i == x->as.i) {
			append_line(&code->body, fmt_str(
							"\t%s%c %s, %s\n",
							asm_op,
							suffix,
							asm_reg_name(y->as.i, type),
							d_name));
		} else {
			append_line(&code->body, fmt_str(
							"\tmov%c %s, %s\n",
							suffix,
							asm_reg_name(x->as.i, type),
							d_name));
			append_line(&code->body, fmt_str(
							"\t%s%c %s, %s\n",
							asm_op,
							suffix,
							asm_reg_name(y->as.i, type),
							d_name));
			asm_unassign_register(ctx, x->as.i);
		}
		asm_unassign_register(ctx, y->as.i);
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
			if (MATCH_ADDR2(ADDR_ARGUMENT, ADDR_STACK_LOAD)) {
				dst->extra.defered.fun = emit_op_cast_ADDR_REGISTER__ADDR_STACK_LOAD;
				dst->extra.defered.ins = ins;
			} else {
				FAILWITH("Unhandled case: MATCH_ADDR2(%s, %s)",
						 asm_addr_tag_to_str(dst->tag),
						 asm_addr_tag_to_str(x->tag));
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
				} else {
					FAILWITH("Unhandled case: MATCH_ADDR3(%s, %s, %s)",
							 asm_addr_tag_to_str(dst->tag),
							 asm_addr_tag_to_str(base->tag),
							 asm_addr_tag_to_str(idx->tag));
				}
			} else if (type_is_struct_ptr(ins->type)) {
				if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK, ADDR_IMM_INT)) {
					dst->tag = ADDR_REGISTER;
					dst->as.i = asm_assign_register(ctx, ins->dst);
					size_t offset = struct_member_offset(&ins->type->as.ptr->as.strct, idx->as.i);
					append_line(&code->body, fmt_str(
									"\tleaq %ld(%%rbp), %s\n",
									base->as.i + offset,
									asm_reg_q_name[dst->as.i]));
				} else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_REGISTER, ADDR_IMM_INT)) {
					dst->tag = ADDR_REGISTER;
					dst->as.i = asm_assign_register(ctx, ins->dst);
					size_t offset = struct_member_offset(&ins->type->as.ptr->as.strct, idx->as.i);
					append_line(&code->body, fmt_str(
									"\tleaq %ld(%s), %s\n",
									offset,
									asm_reg_q_name[base->as.i],
									asm_reg_q_name[dst->as.i]));
					asm_unassign_register(ctx, base->as.i);
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
			} else {
				printf("ins->type = %s\n", ast_type_to_str(ins->type));
				FAILWITH("TODO: ir_op_getelemptr");
			}
		} break;
		case ir_op_retval: {
			struct asm_address *dst = &ctx->vars[ins->dst];
			if (dst->tag == ADDR_STACK) {
			} else if (dst->tag == ADDR_STACK_LOAD) {
				append_line(&code->body, fmt_str(
								"\tmovq %s, %d(%%rbp)\n",
								asm_reg_q_name[asm_arg_regs[0]],
								dst->as.stack[0] + dst->as.stack[1]));
				asm_unassign_register(ctx, asm_arg_regs[0]);
			} else {
				FAILWITH("Unhandled case: (dst->tag == %s)",
						 asm_addr_tag_to_str(dst->tag));
			}
		} break;
		case ir_op_alloca: /* nothing to do */ break;
		case ir_op_load: {
			struct asm_address *dst = &ctx->vars[ins->dst];
			struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
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
				/* ADDR_STACK */
				append_line(&code->body, fmt_str(
								"\tmov%c %ld(%%rbp), %s\n",
								asm_suffix(ins->type),
								x->as.i,
								asm_reg_name(dst->as.i, ins->type)));
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
				append_line(&code->body, fmt_str(
								"\tmov%c (%s), %s\n",
								asm_suffix(ins->type),
								asm_reg_q_name[x->as.i],
								asm_reg_name(dst->as.i, ins->type)));
				asm_unassign_register(ctx, x->as.i);
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
			} else {
				FAILWITH("TODO: unhandled case dst->tag == %s", asm_addr_tag_to_str(dst->tag));
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
				assert(arg_addr->tag == ADDR_REGISTER);
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
			if (MATCH_ADDR2(ADDR_REGISTER, ADDR_IMM_INT)) {
				append_line(&code->body, fmt_str(
								"\tmov%c $%ld, (%s)\n",
								asm_suffix(ins->type),
								x->as.i,
								asm_reg_q_name[dst->as.i]));
				asm_unassign_register(ctx, dst->as.i);
			} else if (MATCH_ADDR2(ADDR_STACK, ADDR_REGISTER)) {
				append_line(&code->body, fmt_str(
								"\tmov%c %s, %ld(%%rbp)\n",
								asm_suffix(ins->type),
								asm_reg_name(x->as.i, ins->type),
								dst->as.i));
				asm_unassign_register(ctx, x->as.i);
			} else if (MATCH_ADDR2(ADDR_STACK, ADDR_STACK_ARG)) {
				/* do nothing */
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
			} else {
				//printf("dst = %%%d\n", ins->dst);
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
				arg->extra.defered.fun(arg->extra.defered.ins, arg->extra.defered.dat, code);
				break;
			default:
				FAILWITH("TODO: unhandled case arg->tag == %s", asm_addr_tag_to_str(arg->tag));
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
					printf("var = %%%d\n", var);
					printf("addr->tag = %s\n", asm_addr_tag_to_str(addr->tag));
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
				case ADDR_WIDE: {
					size_t ts = type_size(ins->type);
					if (ts == 16) {
						append_line(&code->body, fmt_str(
										"\tmovq %s, %ld(%%rbp)\n",
										asm_reg_q_name[ret->as.wide[0]],
										ret->extra.offset));
						append_line(&code->body, fmt_str(
										"\tmovq %s, %ld(%%rbp)\n",
										asm_reg_q_name[ret->as.wide[1]],
										ret->extra.offset + 8));
						ret->tag = ADDR_STACK;
						ret->as.i = ret->extra.offset;
						ret->type = POOL_ALLOC(&tl->data, struct type);
						ret->type->tag = ast_type_ptr;
						ret->type->as.ptr = ins->type;
					} else {
						FAILWITH("TODO");
					}
				} break;
				case ADDR_STACK: /* do nothing */ break;
				default:
					FAILWITH("TODO: unhandled case ret->tag == %s", asm_addr_tag_to_str(ret->tag));
					break;
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
					append_line(&code->body, fmt_str(
									"\tmovq %d(%%rbp), %s\n",
									ret->as.stack[0] + ret->as.stack[1],
									asm_reg_q_name[asm_ret_regs[0]]));
			} else if (ret->tag == ADDR_REGISTER) {
				if (ret->as.i != asm_ret_regs[0]) {
					append_line(&code->body, fmt_str(
									"\tmovq %s, %s\n",
									asm_reg_q_name[ret->as.i],
									asm_reg_q_name[asm_ret_regs[0]]));
				}
			} else if (ret->tag == ADDR_IMM_INT) {
				append_line(&code->body, fmt_str(
								"\tmov%c $%ld, %s\n",
								asm_suffix(ret->type),
								ret->as.i,
								asm_reg_name(asm_ret_regs[0], ret->type)));
			} else {
				FAILWITH("TODO: ret->tag == %s", asm_addr_tag_to_str(ret->tag));
			}
		}
		if (blk_id < blocks->len - 1) {
			append_line(&code->body, fmt_str("\tjmp \".LR"SV_FMT"\"\n", SV_ARGS(&proc->def->id->sv)));
		}
	} break;
	case ir_op_goto: {
		for (size_t i = 0; i < term->args.len; ++i) {
			asm_unassign_register(ctx, asm_get_blk_arg_register(ctx, term->args.elems[i]));
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
		exit(1);
	}
	if (sv.len == 0) {
		fprintf(stderr, "[Error] %s: %s\n", filename, "Empty file");
		exit(1);
	}
	Parser parser = {
		.lexer = {
			.filename = filename,
			.contents = sv,
			.sv = sv,
			.loc.line   = 1,
			.loc.column = 0,
		}
	};
	tokenize(&parser.lexer, &parser.tokens);
	struct expression_stack tl = {0};
	struct expression *exp = NULL;
	struct symtbl symtbl = {0};
	while ((exp = parse_toplevel_expression(&parser, &symtbl)) != NULL) {
		da_append(&tl, exp);
	}
	struct scope sc = {
		.symtbl = symtbl,
	};
	/* Type check */
	for (size_t i = 0; i < tl.len; ++i) {
		infer_type(&parser, tl.elems[i], NULL, &sc);
	}
	/* Desugar */
	for (size_t i = 0; i < tl.len; ++i) {
		tl.elems[i] = ast_desugar(&parser, tl.elems[i]);
	}
	struct ir_toplevel ir = ast_compile(&sc);
	puts("[Debug Info]");
	for (size_t i = 0; i < ir.len; ++i) {
		if (ir.elems[i].tag == IRO_PROC) {
			ir_proc_fprint(&ir.elems[i].proc, stdout);
		}
	}
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

int run(char *dll)
{
	void *handle = dlopen(dll, RTLD_NOW);
	if (handle == NULL) {
		fprintf(stderr, "[Error] %s\n", dlerror());
		return -1;
	}
	int(*fn)(void) = dlsym(handle, "main");
	if (fn == NULL) {
		fprintf(stderr, "[Error] %s\n", dlerror());
		return -1;
	}
	int c = fn();
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
		assemble_module(&asm_modules.elems[i], obj_file, true);
		da_append(&obj_files, obj_file);
	}
	if (run_p) {
		char *dlname = strjoin(cwd, "dlrun.so", "/");
		link_object_files(&obj_files, dlname, true);
		printf("%d\n", run(dlname));
	} else {
		link_object_files(&obj_files, target_file, false);
	}
	return 0;
}
