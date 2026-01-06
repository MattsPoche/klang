#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "common.h"
#define MEM_H_IMPLEMENTATION
#include "mem.h"
#define STRVIEW_IMPLEMENTATION
#include "strview.h"
#include "lex.h"
#include "ast.h"
#include "lex.c"
#include "parse.h"
#include "parse.c"

struct da_pointers {
	uint32_t len, cap;
	void **elems;
};

/* --- TYPE-CHECKER --- */
bool type_eq(struct type *t, struct type *u);
struct expression *infer_type(Parser *p, struct expression *exp, struct type *ret, struct scope *scope);

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

bool type_is_bool(struct type *t)
{
	return t->tag == ast_type_bool;
}

bool type_is_pointer(struct type *t)
{
	return t->tag == ast_type_ptr
		|| t->tag == ast_type_mut_ptr
		|| t->tag == ast_type_proc;
}


bool type_eq(struct type *t, struct type *u)
{
	switch (t->tag) {
	case ast_type_void: return false;
	case ast_type_noreturn: return false;
	case ast_type_bool: return u->tag == ast_type_bool;
	case ast_type_i8:
		return u->tag == ast_type_i8
			|| u->tag == ast_type_intlit;
	case ast_type_i16:
		return u->tag == ast_type_i16
			|| u->tag == ast_type_intlit;
	case ast_type_i32:
		return u->tag == ast_type_i32
			|| u->tag == ast_type_intlit;
	case ast_type_i64:
		return u->tag == ast_type_i64
			|| u->tag == ast_type_intlit;
	case ast_type_u8:
		return u->tag == ast_type_i64
			|| u->tag == ast_type_intlit;
	case ast_type_u16:
		return u->tag == ast_type_i64
			|| u->tag == ast_type_intlit;
	case ast_type_u32:
		return u->tag == ast_type_i64
			|| u->tag == ast_type_intlit;
	case ast_type_u64:
		return u->tag == ast_type_i64
			|| u->tag == ast_type_intlit;
	case ast_type_intlit:
		return type_is_integer(u);
	case ast_type_f32:
		return u->tag == ast_type_f32
			|| u->tag == ast_type_floatlit;
	case ast_type_f64:
		return u->tag == ast_type_f64
			|| u->tag == ast_type_floatlit;
	case ast_type_floatlit:
		return type_is_floating_point(u);
	case ast_type_alias: FAILWITH("TODO: ast_type_alias"); break;
	case ast_type_cons: FAILWITH("TODO: ast_type_cons"); break;
	case ast_type_ptr:
		return (u->tag == ast_type_ptr || u->tag == ast_type_mut_ptr)
			&& type_eq(t->as.ptr, u->as.ptr);
	case ast_type_mut_ptr:
		return u->tag == ast_type_mut_ptr
			&& type_eq(t->as.mut_ptr, u->as.mut_ptr);
	case ast_type_slice: FAILWITH("TODO: ast_type_slice"); break;
	case ast_type_mut_slice: FAILWITH("TODO: ast_type_mut_slice"); break;
	case ast_type_array:
		return u->tag == ast_type_array
			&& type_eq(t->as.array.base, u->as.array.base);
	case ast_type_struct: FAILWITH("TODO: ast_type_struct"); break;
	case ast_type_vector: FAILWITH("TODO: ast_type_vector"); break;
	case ast_type_proc:
		if (u->tag == ast_type_proc
			&& type_eq(t->as.proc.ret, u->as.proc.ret)
			&& t->as.proc.args.len == u->as.proc.args.len) {
			for (size_t i = 0; i < t->as.proc.args.len; ++i) {
				if (!type_eq(t->as.proc.args.elems[i], u->as.proc.args.elems[i])) {
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
	ast_type_fprint(t, stdout);
	putchar('\n');
	ast_type_fprint(u, stdout);
	putchar('\n');
	FAILWITH("TODO: type_eq");
	return 0;
}

struct type *ident_type(struct token *name, struct scope *scope)
{
	return lookup_definition(name, scope)->type;
}

bool type_coerce(struct type *t, struct type *u)
{
	switch (t->tag) {
	case ast_type_void: return false;
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
	case ast_type_ptr:
		return (u->tag == ast_type_ptr || u->tag == ast_type_mut_ptr)
			&& type_eq(t->as.ptr, u->as.ptr);
	case ast_type_mut_ptr:
		return u->tag == ast_type_mut_ptr
			&& type_eq(t->as.mut_ptr, u->as.mut_ptr);
	case ast_type_slice: FAILWITH("TODO: ast_type_slice"); break;
	case ast_type_mut_slice: FAILWITH("TODO: ast_type_mut_slice"); break;
	case ast_type_array:
		return u->tag == ast_type_array
			&& type_eq(t->as.array.base, u->as.array.base);
	case ast_type_struct: FAILWITH("TODO: ast_type_struct"); break;
	case ast_type_vector: FAILWITH("TODO: ast_type_vector"); break;
	case ast_type_proc:
		if (u->tag == ast_type_proc
			&& type_eq(t->as.proc.ret, u->as.proc.ret)
			&& t->as.proc.args.len == u->as.proc.args.len) {
			for (size_t i = 0; i < t->as.proc.args.len; ++i) {
				if (!type_eq(t->as.proc.args.elems[i], u->as.proc.args.elems[i])) {
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
	return false;
}

bool def_coerce(UNUSED Parser *p, struct definition *def, struct expression *exp)
{
	switch (def->type->tag) {
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
		return type_coerce(def->type, exp->type);
	case ast_type_array: {
		struct array_type *array_type = &def->type->as.array;
		if (exp->tag == ast_exp_initializer) {
			assert(exp->type == NULL);
			struct expression_stack *init_exps = &exp->as.init;
			struct type *base = array_type->base;
			da_foreach(exp, init_exps) {
				assert(type_eq(base, (*exp)->type));
			}
			switch (array_type->stag) {
			case AT_UNSIZED:
				array_type->stag = AT_LITERAL;
				array_type->size.sz = init_exps->len;
				break;
			case AT_EXPRESSION: FAILWITH("TODO: AT_EXPRESSION"); break;
			case AT_LITERAL:
				assert(init_exps->len <= array_type->size.sz);
				break;
			}
			exp->type = def->type;
			return true;
		} else if (exp->tag == ast_exp_named_initializer) {
			FAILWITH("TODO: type error");
		} else {
			assert(array_type->stag != AT_UNSIZED);
			return type_eq(def->type, exp->type);
		}
	} break;
	case ast_type_alias: FAILWITH("TODO: ast_type_alias"); break;
	case ast_type_cons: FAILWITH("TODO: ast_type_cons"); break;
	case ast_type_slice: FAILWITH("TODO: ast_type_slice"); break;
	case ast_type_mut_slice: FAILWITH("TODO: ast_type_mut_slice"); break;
	case ast_type_struct: FAILWITH("TODO: ast_type_struct"); break;
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
		assert(def_coerce(p, &exp->as.def, infer_type(p, exp->as.def.exp, ret, scope)));
		exp->is_addressable = false;
		exp->is_mutable = false;
	} break;
	case ast_exp_let: {
		struct let *let = &exp->as.let;
		/* type check definition */
		assert(exp->as.def.type != NULL);
		assert(def_coerce(p, &exp->as.def, infer_type(p, exp->as.def.exp, ret, scope)));
		/* type check body */
		let->scope.parent = scope;
		da_append(&let->scope.symtbl, &let->def);
		rhs = infer_type(p, let->body, ret, &let->scope);
		exp->type = rhs->type;
		exp->is_addressable = rhs->is_addressable;
		exp->is_mutable = rhs->is_mutable;
	} break;
	case ast_exp_literal: {
		struct literal *lit = &exp->as.lit;
		exp->is_addressable = false;
		exp->is_mutable = false;
		if (lit->token->tt == tt_intlit) {
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
	case ast_exp_procedure_literal: {
		struct procedure *proc = &exp->as.proc;
		proc->scope.parent = scope;
		/* add arguments to procedure scope */
		da_foreach(formal, &proc->formals) {
			da_append(&proc->scope.symtbl, formal);
		}
		rhs = infer_type(p, proc->body, proc->ret, &proc->scope);
		assert(type_coerce(proc->ret, rhs->type));
		exp->type = POOL_ALLOC(&p->data, struct type);
		exp->type->tag = ast_type_proc;
		exp->type->as.proc = procedure_type(proc);
		exp->is_addressable = false;
		exp->is_mutable = false;
	} break;
	case ast_exp_ident: {
		struct definition *def = lookup_definition(exp->as.id, scope);
		exp->type = def->type;
		exp->is_addressable = true;
		exp->is_mutable = def->is_mut;
	} break;
	case ast_exp_undefined: FAILWITH("TODO: ast_exp_undefined"); break;
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
			assert(lhs->is_addressable && lhs->is_mutable);
			assert(type_coerce(lhs->type, rhs->type));
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
			assert(type_is_numeric_scalar(lhs->type));
			assert(type_is_numeric_scalar(rhs->type));
			assert(type_coerce(lhs->type, rhs->type));
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
			assert(type_is_numeric_scalar(lhs->type));
			assert(type_is_numeric_scalar(rhs->type));
			assert(type_coerce(lhs->type, rhs->type));
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
			assert(lhs->is_addressable && lhs->is_mutable);
			assert(type_is_numeric_scalar(lhs->type));
			assert(type_is_numeric_scalar(rhs->type));
			assert(type_coerce(lhs->type, rhs->type));
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
			assert(type_is_integer(lhs->type));
			assert(type_is_integer(rhs->type));
			assert(type_coerce(lhs->type, rhs->type));
			exp->type = lhs->type;
			exp->is_addressable = false;
			exp->is_mutable = false;
		} break;
		case binop_shift_left:
		case binop_shift_right: {
			lhs = infer_type(p, bin->left, ret, scope);
			rhs = infer_type(p, bin->right, ret, scope);
			assert(type_is_integer(lhs->type));
			assert(type_is_integer(rhs->type));
			assert(type_coerce(lhs->type, rhs->type));
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
			assert(lhs->is_addressable && lhs->is_mutable);
			assert(type_is_integer(lhs->type));
			assert(type_is_integer(rhs->type));
			assert(type_coerce(lhs->type, rhs->type));
			exp->type = lhs->type;
			exp->is_addressable = false;
			exp->is_mutable = false;
		} break;
		case binop_shift_left_assign:
		case binop_shift_right_assign: {
			lhs = infer_type(p, bin->left, ret, scope);
			rhs = infer_type(p, bin->right, ret, scope);
			assert(lhs->is_addressable && lhs->is_mutable);
			assert(type_is_integer(lhs->type));
			assert(type_is_integer(rhs->type));
			assert(type_coerce(lhs->type, rhs->type));
			exp->type = lhs->type;
			exp->is_addressable = false;
			exp->is_mutable = false;
		} break;
		case binop_or:
		case binop_and: {
			lhs = infer_type(p, bin->left, ret, scope);
			rhs = infer_type(p, bin->right, ret, scope);
			assert(lhs->type->tag == ast_type_bool);
			assert(rhs->type->tag == ast_type_bool);
			exp->type = lhs->type;
			exp->is_addressable = false;
			exp->is_mutable = false;
		} break;
		case binop_member: FAILWITH("TODO: binop_member"); break;
		}
	} break;
	case ast_exp_unary: {
		struct unary *una = &exp->as.una;
		switch ((enum unaop)una->op) {
		case unaop_neg:
		case unaop_pos: {
			rhs = infer_type(p, una->exp, ret, scope);
			assert(type_is_numeric_scalar(rhs->type));
			exp->type = rhs->type;
			exp->is_addressable = false;
			exp->is_mutable = false;
		} break;
		case unaop_lnot: {
			rhs = infer_type(p, una->exp, ret, scope);
			assert(type_is_integer(rhs->type));
			exp->type = rhs->type;
			exp->is_addressable = false;
			exp->is_mutable = false;
		} break;
		case unaop_not: {
			rhs = infer_type(p, una->exp, ret, scope);
			assert(rhs->type->tag == ast_type_bool);
			exp->type = rhs->type;
			exp->is_addressable = false;
			exp->is_mutable = false;
		} break;
		case unaop_address_of: {
			rhs = infer_type(p, una->exp, ret, scope);
			assert(rhs->is_addressable);
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
				FAILWITH("TODO: ERROR: type is not a pointer.");
			}
		} break;
		case unaop_index: {
			lhs = infer_type(p, exp->as.idx.exp, ret, scope); // base
			rhs = infer_type(p, exp->as.idx.idx, ret, scope); // index
			assert(type_is_integer(rhs->type));
			switch ((int)lhs->type->tag) {
			case ast_type_ptr: FAILWITH("TODO: ast_type_ptr.");	break;
			case ast_type_mut_ptr: FAILWITH("TODO: ast_type_mut_ptr.");	break;
			case ast_type_array: {
				exp->type = lhs->type->as.array.base;
				exp->is_addressable = true;
				exp->is_mutable = lhs->is_mutable;
			} break;
			default: FAILWITH("TODO: type error."); break;
			}
		} break;
		case unaop_call: {
			struct call *call = &exp->as.call;
			struct expression *proc = infer_type(p, call->proc, ret, scope);
			assert(proc->type->tag == ast_type_proc);
			struct proc_type *pt = &proc->type->as.proc;
			assert(call->args.len == pt->args.len);
			for (size_t i = 0; i < call->args.len; ++i) {
				struct expression *arg = infer_type(p, call->args.elems[i], ret, scope);
				assert(type_coerce(pt->args.elems[i], arg->type));
			}
			exp->type = pt->ret;
			exp->is_mutable = true;
			exp->is_addressable = false;
		} break;
		default: FAILWITH("Unreachable"); break;
		}
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
		assert(type_is_bool(cond->type));
		lhs = infer_type(p, br->tb, ret, scope);
		rhs = infer_type(p, br->fb, ret, scope);
		assert(type_coerce(lhs->type, rhs->type));
		exp->type = rhs->type = lhs->type;
		exp->is_addressable = lhs->is_addressable && rhs->is_addressable;
		exp->is_mutable = lhs->is_mutable && rhs->is_mutable;
	} break;
	case ast_exp_case: {
		struct exp_case *c = &exp->as.ccase;
		struct expression *cond = infer_type(p, c->cexp, ret, scope);
		struct expression *branch = NULL;
		bool is_addressable, is_mutable;
		da_foreach(cb, &c->branches) {
			da_foreach(match, &cb->matches) {
				infer_type(p, *match, NULL, scope);
				assert(type_coerce(cond->type, (*match)->type));
			}
			infer_type(p, cb->exp, NULL, scope);
			if (branch == NULL) {
				branch = cb->exp;
				is_addressable = branch->is_addressable;
				is_mutable = branch->is_mutable;
			} else {
				assert(type_coerce(branch->type, cb->exp->type));
				is_addressable = is_addressable && branch->is_addressable;
				is_mutable = is_mutable && branch->is_mutable;
			}
		}
		assert(branch != NULL);
		if (c->else_exp) {
			infer_type(p, c->else_exp, NULL, scope);
			assert(type_coerce(branch->type, c->else_exp->type));
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

enum ir_opcode {
	ir_op_nop,
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
	ir_op_cmp_e,
	ir_op_cmp_ne,
	ir_op_cmp_l,
	ir_op_cmp_g,
	ir_op_cmp_le,
	ir_op_cmp_ge,
	ir_op_alloca,
	ir_op_load,
	ir_op_load_globl,
	ir_op_load_const,
	ir_op_load_imm,
	ir_op_store,
	ir_op_memcpy,
	ir_op_memset,
	IR_OPCODE_COUNT,
};

static_assert(IR_OPCODE_COUNT <= UINT8_MAX);

enum ir_opterm {
	ir_op_ret,
	ir_op_goto,
	ir_op_if,
	ir_op_call,
	ir_op_tailcall,
};

enum ir_type_bits {
	IR_VOID  = 0,
	IR_I8,
	IR_I16,
	IR_I32,
	IR_I64,
	IR_PTR,
	IR_SIGN	 = 1 << 7,
	IR_FLOAT = 1 << 8,
};

#define IR_TYPE_MASK (IR_SIGN - 1)

typedef struct ir_ins {
	enum ir_opcode op : 8;
	uint8_t type;
	uint16_t dst;
	union {
		uint32_t u32;
		int32_t  i32;
		uint16_t rx[2];
	} arg;
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
};

struct ir_blk {
	struct ir_args args;
	struct ir_code code;
	struct ir_blk_terminal term;
};

struct ir_proc {
	struct definition *def;
	struct procedure *node;
	struct ir_blk *entry;
	uint16_t regc;
	uint16_t argc;
	uint16_t retc;
};

struct ir_toplevel {
	uint32_t len, cap;
	struct ir_proc *elems;
	struct mem_arena data;
};

struct lines {
	uint32_t len, cap;
	char **elems;
};

struct strview sv_vfmt(const char *fmt, va_list ap)
{
	struct strview sv = {0};
	FILE *stream = open_memstream(&sv.ptr, &sv.len);
	vfprintf(stream, fmt, ap);
	fclose(stream);
	return sv;
}

__attribute__ ((format(printf, 1, 2)))
struct strview sv_fmt(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	struct strview sv = sv_vfmt(fmt, ap);
	va_end(ap);
	return sv;
}

__attribute__ ((format(printf, 1, 2)))
char *fmt_str(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	struct strview sv = sv_vfmt(fmt, ap);
	va_end(ap);
	return sv.ptr;
}

void append_line(struct lines *lines, char *str)
{
	da_append(lines, str);
}

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
	case ast_type_alias: FAILWITH("TODO: ast_type_alias"); break;
	case ast_type_cons: FAILWITH("TODO: ast_type_cons"); break;
	case ast_type_struct: FAILWITH("TODO: ast_type_struct"); break;
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
	case ast_type_alias: FAILWITH("TODO: ast_type_alias"); break;
	case ast_type_cons: FAILWITH("TODO: ast_type_cons"); break;
	case ast_type_struct: FAILWITH("TODO: ast_type_struct"); break;
	case ast_type_vector: FAILWITH("TODO: ast_type_vector"); break;
	default: FAILWITH("Unreachable"); break;
	}
}

int ir_typeof(struct type *t)
{
	assert(t != NULL);
	switch (t->tag) {
	case ast_type_void:
	case ast_type_noreturn:
		return IR_VOID;
	case ast_type_i8:  return IR_SIGN|IR_I8;
	case ast_type_bool:
	case ast_type_u8:  return IR_I8;
	case ast_type_i16: return IR_SIGN|IR_I16;
	case ast_type_u16: return IR_I16;
	case ast_type_i32: return IR_SIGN|IR_I32;
	case ast_type_u32: return IR_I32;
	case ast_type_i64: return IR_SIGN|IR_I64;
	case ast_type_u64: return IR_I64;
	case ast_type_f32: return IR_FLOAT|IR_I32;
	case ast_type_f64: return IR_FLOAT|IR_I32;
	case ast_type_ptr:
	case ast_type_mut_ptr:
	case ast_type_proc:
		return IR_PTR;
	case ast_type_slice:
	case ast_type_mut_slice:
		FAILWITH("TODO: type slice.");
		break;
	case ast_type_array:
		FAILWITH("TODO: type array.");
		break;
	case ast_type_alias: FAILWITH("TODO: ast_type_alias"); break;
	case ast_type_cons: FAILWITH("TODO: ast_type_cons"); break;
	case ast_type_struct: FAILWITH("TODO: ast_type_struct"); break;
	case ast_type_vector: FAILWITH("TODO: ast_type_vector"); break;
	case ast_type_floatlit: FAILWITH("TODO: ERROR: unresolved type (floatlit)."); break;
	case ast_type_intlit: FAILWITH("TODO: ERROR: unresolved type (intlit)."); break;
	default: FAILWITH("Unreachable"); break;
	}
	FAILWITH("TODO: ERROR: unresolved type.");
	return -1;
}

typedef struct ast_compile_result {
	struct ir_blk *blk;
	int rx[2];
} AST_COMPILE_RESULT;

struct ir_toplevel ast_compile(struct expression_stack *toplevel, struct scope *scope);
AST_COMPILE_RESULT ast_compile_expression(struct expression *exp, struct ir_proc *proc,
										  struct ir_blk *blk, struct scope *scope, struct ir_toplevel *tl);
void ast_compile_procedure(struct ir_proc *proc, struct ir_toplevel *tl);

int ir_proc_new_reg(struct ir_proc *proc)
{
	return proc->regc++;
}

void ast_compile_procedure(struct ir_proc *proc, struct ir_toplevel *tl)
{
	/* TODO: For now, only handle cases where return type can fit into a single register */
	if (proc->node->ret) {
		assert(type_size(proc->node->ret) <= 8);
		proc->retc = 1;
	} else {
		proc->retc = 0;
	}
	struct formals *formals = &proc->node->formals;
	struct scope *scope = &proc->node->scope;
	struct ir_blk *entry = POOL_ALLOC(&tl->data, struct ir_blk);
	proc->entry = entry;
	//for (size_t i = 0; i < formals->len; ++i) {
	da_foreach(formal, formals) {
		assert(formal->type != NULL);
		/* TODO: For now, only handle cases where type can fit into a single register */
		assert(type_size(formal->type) <= 8);
		int reg = ir_proc_new_reg(proc);
		int sym = ir_proc_new_reg(proc);
		int type = ir_typeof(formal->type);
		da_append(&entry->args, reg);
		formal->ir_symbol = sym;
		/* push registers to stack */
		da_append(&entry->code, (IR_Ins){
				.op   = ir_op_alloca,
				.type = type,
				.dst  = sym,
				.arg.i32 = 1,
			});
		da_append(&entry->code, (IR_Ins){
				.op = ir_op_store,
				.type = type,
				.dst = sym,
				.arg.rx[0] = 0,
				.arg.rx[1] = reg,
			});
	}
	proc->argc = entry->args.len;
	auto res = ast_compile_expression(proc->node->body, proc, entry, scope, tl);
	da_append(&res.blk->term.args, res.rx[0]);
	res.blk->term.op = ir_op_ret;
}

AST_COMPILE_RESULT ast_compile_expression(struct expression *exp, struct ir_proc *proc,
										  struct ir_blk *blk, struct scope *scope, struct ir_toplevel *tl)
{
	switch (exp->tag) {
	case ast_exp_let: {
		struct let *let = &exp->as.let;
		struct definition *def = &let->def;
		int var  = ir_proc_new_reg(proc);
		int type = ir_typeof(def->type);
		def->ir_symbol = var;
		da_append(&blk->code, (IR_Ins){
				.op   = ir_op_alloca,
				.type = type,
				.dst  = var,
				.arg.i32 = 1,
			});
		auto res = ast_compile_expression(def->exp, proc, blk, scope, tl);
		da_append(&blk->code, (IR_Ins){
				.op   = ir_op_store,
				.type = type,
				.dst  = var,
				.arg.rx[0] = 0,
				.arg.rx[1] = res.rx[0],
			});
		return ast_compile_expression(let->body, proc, blk, &let->scope, tl);
	} break;
	case ast_exp_literal: {
		struct literal *lit = &exp->as.lit;
		int reg = ir_proc_new_reg(proc);
		if (type_is_integer(exp->type)) {
			assert(lit->as.i <= INT32_MAX && lit->as.i >= INT32_MIN);
			da_append(&blk->code, (IR_Ins){
					.op = ir_op_load_imm,
					.type = ir_typeof(exp->type),
					.dst = reg,
					.arg.i32 = lit->as.i,
				});
		} else {
			FAILWITH("TODO: ast_exp_literal");
		}
		return (AST_COMPILE_RESULT){.rx[0]=reg, .blk=blk};
	} break;
	case ast_exp_initializer: FAILWITH("TODO: ast_exp_initializer"); break;
	case ast_exp_named_initializer: FAILWITH("TODO: ast_exp_named_initializer"); break;
	case ast_exp_procedure_literal: FAILWITH("TODO: ast_exp_procedure_literal"); break;
	case ast_exp_undefined: FAILWITH("TODO: ast_exp_undefined"); break;
	case ast_exp_ident: {
		struct definition *def = lookup_definition(exp->as.id, scope);
		int dst = -1;
		if (type_is_numeric_scalar(def->type)
			|| type_is_bool(def->type)
			|| type_is_pointer(def->type)) {
			dst = ir_proc_new_reg(proc);
			if (def->is_global) {
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_load_globl,
						.type = ir_typeof(def->type),
						.dst  = dst,
						.arg.u32 = def->ir_symbol,
					});
			} else {
				da_append(&blk->code, (IR_Ins){
						.op   = ir_op_load,
						.type = ir_typeof(def->type),
						.dst  = dst,
						.arg.rx[0] = def->ir_symbol,
						.arg.rx[1] = 0,
					});
			}
		} else {
			FAILWITH("TODO: ast_exp_ident");
		}
		return (AST_COMPILE_RESULT){.rx[0]=dst, .blk=blk};
	} break;
	case ast_exp_binary: {
		struct binary *bin = &exp->as.bin;
		enum ir_opcode op;
		switch ((enum binop)bin->op) {
		case binop_sequence: {
			auto left  = ast_compile_expression(bin->left, proc, blk, scope, tl);
			return ast_compile_expression(bin->right, proc, left.blk, scope, tl);
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
			auto left  = ast_compile_expression(bin->left, proc, blk, scope, tl);
			auto right = ast_compile_expression(bin->right, proc, left.blk, scope, tl);
			int dst = ir_proc_new_reg(proc);
			da_append(&right.blk->code, (IR_Ins){
					.op   = op,
					.type = ir_typeof(exp->type),
					.dst  = dst,
					.arg.rx[0] = left.rx[0],
					.arg.rx[1] = right.rx[0],
				});
			return (AST_COMPILE_RESULT){.rx[0]=dst, .blk=right.blk};
		} break;
		case binop_equal:      op = ir_op_cmp_e;  goto LBL_cmp;
		case binop_not_equal:  op = ir_op_cmp_ne; goto LBL_cmp;
		case binop_less_than:  op = ir_op_cmp_l;  goto LBL_cmp;
		case binop_more_than:  op = ir_op_cmp_g;  goto LBL_cmp;
		case binop_less_equal: op = ir_op_cmp_le; goto LBL_cmp;
		case binop_more_equal: op = ir_op_cmp_ge; goto LBL_cmp;
		{
		LBL_cmp:
			auto left  = ast_compile_expression(bin->left, proc, blk, scope, tl);
			auto right = ast_compile_expression(bin->right, proc, left.blk, scope, tl);
			int dst = ir_proc_new_reg(proc);
			da_append(&right.blk->code, (IR_Ins){
					.op   = op,
					.type = ir_typeof(bin->left->type),
					.dst  = dst,
					.arg.rx[0] = left.rx[0],
					.arg.rx[1] = right.rx[0],
				});
			return (AST_COMPILE_RESULT){.rx[0]=dst, .blk=right.blk};
		}
		case binop_member: FAILWITH("TODO: binop_member"); break;
		case binop_assign: FAILWITH("TODO: binop_assign"); break;
		case binop_and_assign: FAILWITH("TODO: binop_and_assign"); break;
		case binop_lor_assign: FAILWITH("TODO: binop_lor_assign"); break;
		case binop_xor_assign: FAILWITH("TODO: binop_xor_assign"); break;
		case binop_add_assign: FAILWITH("TODO: binop_add_assign"); break;
		case binop_sub_assign: FAILWITH("TODO: binop_sub_assign"); break;
		case binop_mul_assign: FAILWITH("TODO: binop_mul_assign"); break;
		case binop_div_assign: FAILWITH("TODO: binop_div_assign"); break;
		case binop_mod_assign: FAILWITH("TODO: binop_mod_assign"); break;
		case binop_shift_left_assign: FAILWITH("TODO: binop_shift_left_assign"); break;
		case binop_shift_right_assign: FAILWITH("TODO: binop_shift_right_assign"); break;
		case binop_or: {
			struct ir_blk *tblk = POOL_ALLOC(&tl->data, struct ir_blk);
			struct ir_blk *fblk = POOL_ALLOC(&tl->data, struct ir_blk);
			struct ir_blk *rblk = POOL_ALLOC(&tl->data, struct ir_blk);
			struct ir_blk *join = POOL_ALLOC(&tl->data, struct ir_blk);
			auto left  = ast_compile_expression(bin->left, proc, blk, scope, tl);
			auto right = ast_compile_expression(bin->right, proc, rblk, scope, tl);
			/* left */
			left.blk->term.op = ir_op_if;
			da_append(&left.blk->term.args, left.rx[0]);
			left.blk->term.b0 = tblk;
			left.blk->term.b1 = rblk;
			/* right */
			right.blk->term.op = ir_op_if;
			da_append(&right.blk->term.args, right.rx[0]);
			right.blk->term.b0 = tblk;
			right.blk->term.b1 = fblk;
			/* tblk */
			int reg = ir_proc_new_reg(proc);
			da_append(&tblk->code, (IR_Ins){
					.op   = ir_op_load_imm,
					.type = ir_typeof(exp->type),
					.dst  = reg,
					.arg.i32 = 1,
				});
			tblk->term.op = ir_op_goto;
			da_append(&tblk->term.args, reg);
			tblk->term.b0 = join;
			/* fblk */
			reg = ir_proc_new_reg(proc);
			da_append(&fblk->code, (IR_Ins){
					.op   = ir_op_load_imm,
					.type = ir_typeof(exp->type),
					.dst  = reg,
					.arg.i32 = 0,
				});
			fblk->term.op = ir_op_goto;
			da_append(&fblk->term.args, reg);
			fblk->term.b0 = join;
			/* join */
			reg = ir_proc_new_reg(proc);
			da_append(&join->args, reg);
			return (AST_COMPILE_RESULT){.rx[0]=reg, .blk=join};
		} break;
		case binop_and: FAILWITH("TODO: binop_and"); break;
		}
		FAILWITH("TODO: ast_exp_binary");
	} break;
	case ast_exp_unary: {
		struct unary *una = &exp->as.una;
		switch ((enum unaop)una->op) {
		case unaop_lnot: FAILWITH("TODO: unaop_lnot"); break;
		case unaop_not: FAILWITH("TODO: unaop_not"); break;
		case unaop_neg: FAILWITH("TODO: unaop_neg"); break;
		case unaop_pos: FAILWITH("TODO: unaop_pos"); break;
		case unaop_address_of: FAILWITH("TODO: unaop_address_of"); break;
		case unaop_dereference: FAILWITH("TODO: unaop_dereference"); break;
		case unaop_index: FAILWITH("TODO: unaop_index"); break;
		case unaop_call: {
			struct call *call = &exp->as.call;
			typeof(blk->term.args) args = {0};
			auto res = ast_compile_expression(call->proc, proc, blk, scope, tl);
			da_append(&args, res.rx[0]);
			da_foreach(arg, &call->args) {
				res = ast_compile_expression(*arg, proc, res.blk, scope, tl);
				da_append(&args, res.rx[0]);
				blk = res.blk;
			}
			struct ir_blk *ret = POOL_ALLOC(&tl->data, struct ir_blk);
			blk = res.blk;
			blk->term.op = ir_op_call;
			blk->term.args = args;
			blk->term.b0 = ret;
			int dst = ir_proc_new_reg(proc);
			da_append(&ret->args, dst);
			return (AST_COMPILE_RESULT){.rx[0]=dst, .blk=ret};
		} break;
		default: FAILWITH("Unreachable"); break;
		}
		FAILWITH("TODO: ast_exp_unary");
	} break;
	case ast_exp_if: {
		struct exp_if *iff = &exp->as.iff;
		auto cond = ast_compile_expression(iff->cond, proc, blk, scope, tl);
		struct ir_blk *tblk = POOL_ALLOC(&tl->data, struct ir_blk);
		struct ir_blk *fblk = POOL_ALLOC(&tl->data, struct ir_blk);
		struct ir_blk *join = POOL_ALLOC(&tl->data, struct ir_blk);
		cond.blk->term.op = ir_op_if;
		da_append(&cond.blk->term.args, cond.rx[0]);
		auto tres = ast_compile_expression(iff->tb, proc, tblk, scope, tl);
		auto fres = ast_compile_expression(iff->fb, proc, fblk, scope, tl);
		cond.blk->term.b0 = tblk;
		cond.blk->term.b1 = fblk;
		/* true branch term */
		tres.blk->term.op = ir_op_goto;
		da_append(&tres.blk->term.args, tres.rx[0]);
		tres.blk->term.b0 = join;
		/* false branch term */
		fres.blk->term.op = ir_op_goto;
		da_append(&fres.blk->term.args, fres.rx[0]);
		fres.blk->term.b0 = join;
		int dst = ir_proc_new_reg(proc);
		da_append(&join->args, dst);
		return (AST_COMPILE_RESULT){.rx[0]=dst, .blk=join};
	} break;
	case ast_exp_case: FAILWITH("TODO: ast_exp_case"); break;
	case ast_exp_return: FAILWITH("TODO: ast_exp_return"); break;
	case ast_exp_break: FAILWITH("TODO: ast_exp_break"); break;
	case ast_exp_continue: FAILWITH("TODO: ast_exp_continue"); break;
	case ast_exp_definition: FAILWITH("TODO: ast_exp_definition"); break;
	}
	FAILWITH("Unreachable");
}

struct ir_toplevel ast_compile(UNUSED struct expression_stack *toplevel, struct scope *scope)
{
	struct symtbl *st = &scope->symtbl;
	struct ir_toplevel tl = {0};
	da_foreach(def, st) {
		struct expression *exp = (*def)->exp;
		if (exp->tag == ast_exp_procedure_literal) {
			(*def)->ir_symbol = tl.len;
			(*def)->is_global = true;
			struct ir_proc p = {
				.def  = *def,
				.node = &exp->as.proc,
				.regc = 0,
			};
			da_append(&tl, p);
		} else {
			FAILWITH("TODO: compile toplevel definitions.");
		}
	}
	da_foreach(e, &tl) {
		ast_compile_procedure(e, &tl);
	}
	return tl;
}

void ir_type_fprint(int t, FILE *file)
{
	if (t == IR_VOID) {
		fputs("VOID", file);
		return;
	}
	if (t == IR_PTR) {
		fputs("PTR", file);
		return;
	}
	if (t & IR_FLOAT) {
		fputc('F', file);
	} else if (t & IR_SIGN) {
		fputc('S', file);
	} else {
		fputc('U', file);
	}
	switch (t & IR_TYPE_MASK) {
	case IR_I8:  fputs("8",  file); break;
	case IR_I16: fputs("16", file); break;
	case IR_I32: fputs("32", file); break;
	case IR_I64: fputs("64", file); break;
	default: FAILWITH("Unreachable"); break;
	}
}

void ir_ins_fprint(IR_Ins *ins, FILE *file)
{
	fputc('\t', file);
	char *op = NULL;
	switch (ins->op) {
	case ir_op_nop:
		fputs("nop", file);
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
	case ir_op_cmp_e:  op = "cmp e ";  goto LBL_BINOP;
	case ir_op_cmp_ne: op = "cmp ne "; goto LBL_BINOP;
	case ir_op_cmp_l:  op = "cmp l ";  goto LBL_BINOP;
	case ir_op_cmp_g:  op = "cmp g ";  goto LBL_BINOP;
	case ir_op_cmp_le: op = "cmp le "; goto LBL_BINOP;
	case ir_op_cmp_ge: op = "cmp ge "; goto LBL_BINOP;
	LBL_BINOP:
		fprintf(file, "%%%d := %s<", ins->dst, op);
		ir_type_fprint(ins->type, file);
		fprintf(file, "> %%%d, %%%d", ins->arg.rx[0], ins->arg.rx[1]);
		break;
	case ir_op_neg: FAILWITH("TODO: ir_op_neg"); break;
	case ir_op_alloca: {
		fprintf(file, "%%%d := alloca<", ins->dst);
		ir_type_fprint(ins->type, file);
		fprintf(file, "> %d", ins->arg.u32);
	} break;
	case ir_op_load_globl: {
		fprintf(file, "%%%d := load_gobl<", ins->dst);
		ir_type_fprint(ins->type, file);
		fprintf(file, "> %d", ins->arg.i32);
	} break;
	case ir_op_load_const: FAILWITH("TODO: ir_op_load_const"); break;
	case ir_op_load_imm: {
		fprintf(file, "%%%d := load_imm<", ins->dst);
		ir_type_fprint(ins->type, file);
		fprintf(file, "> %d", ins->arg.i32);
	} break;
	case ir_op_load: {
		fprintf(file, "%%%d := load<", ins->dst);
		ir_type_fprint(ins->type, file);
		fprintf(file, "> %d(%%%d)", ins->arg.rx[1], ins->arg.rx[0]);
	} break;
	case ir_op_store: {
		fputs("store<", file);
		ir_type_fprint(ins->type, file);
		fprintf(file, "> %d(%%%d), %%%d", ins->arg.rx[0], ins->dst, ins->arg.rx[1]);
	} break;
	case ir_op_memcpy: FAILWITH("TODO: ir_op_memcpy"); break;
	case ir_op_memset: FAILWITH("TODO: ir_op_memset"); break;
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
		dfs_walk(blk->term.b0, order, visited);
		dfs_walk(blk->term.b1, order, visited);
		break;
	case ir_op_call:
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
		da_foreach(arg, &blk->args) {
			fprintf(file, ", %%%d", *arg);
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
	case ir_op_call: {
		assert(term->args.len >= 1);
		fprintf(file, "\tcall %%%d(", term->args.elems[0]);
		if (term->args.len > 1) {
			fprintf(file, "%%%d", term->args.elems[1]);
			for (size_t i = 2; i < term->args.len; ++i) {
				fprintf(file, ", %%%d", term->args.elems[i]);
			}
		}
		fprintf(file, ") -> @%p\n", term->b0);
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
enum asm_register {
	/* caller save */
	asm_reg_rax,
	asm_reg_rdi,
	asm_reg_rsi,
	asm_reg_rdx,
	asm_reg_rcx,
	asm_reg_r8,
	asm_reg_r9,
	asm_reg_r10,  // static chain pointer
	asm_reg_r11,
	/* callee save */
	asm_reg_rbx,
	asm_reg_r12,
	asm_reg_r13,
	asm_reg_r14,
	asm_reg_r15,
	ASM_REG_COUNT,
};

enum asm_register_bits {
	BIT_RAX = 1u << asm_reg_rax,
	BIT_RDI = 1u << asm_reg_rdi,
	BIT_RSI = 1u << asm_reg_rsi,
	BIT_RDX = 1u << asm_reg_rdx,
	BIT_RCX = 1u << asm_reg_rcx,
	BIT_R8  = 1u << asm_reg_r8,
	BIT_R9  = 1u << asm_reg_r9,
	BIT_R10 = 1u << asm_reg_r10,
	BIT_R11 = 1u << asm_reg_r11,
	BIT_RBX = 1u << asm_reg_rbx,
	BIT_R12 = 1u << asm_reg_r12,
	BIT_R13 = 1u << asm_reg_r13,
	BIT_R14 = 1u << asm_reg_r14,
	BIT_R15 = 1u << asm_reg_r15,
};

enum asm_register_class {
	REGC_ARG          = BIT_RDI|BIT_RSI|BIT_RDX|BIT_RCX|BIT_R8|BIT_R9,
	REGC_RET          = BIT_RAX|BIT_RDX,
	REGC_CALLEE_SAVED = BIT_RBX|BIT_R12|BIT_R13|BIT_R14|BIT_R15,
	REGC_CALLER_SAVED = BIT_RAX|BIT_RDI|BIT_RSI|BIT_RDX|BIT_RCX|BIT_R8|BIT_R9|BIT_R10|BIT_R11,
	REGC_STATIC_CHAIN = BIT_R10,
};

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

enum asm_addr_tag {
	ADDR_NONE,
	ADDR_REGISTER,
	ADDR_TEMP_REG,
	ADDR_IMM_INT,
	ADDR_IMM_FLOAT,
	ADDR_STACK,
	ADDR_STACK_LOAD,
	ADDR_BLK_ARG,
	ADDR_SYMBOL,
	ADDR_FLAGS,
};

struct asm_address {
	enum asm_addr_tag tag;
	union {
		int64_t i;
		double d;
		float f;
		int32_t sta[2]; /* stack address */
	} as;
};

struct asm_context {
	uint64_t clobbered;
	size_t stack_size;
	size_t localc;
	struct asm_address *locals;
	int assigned[ASM_REG_COUNT];
	bool is_leaf;
};

struct asm_procedure {
	struct lines prologue;
	struct lines body;
	struct lines epilogue;
	struct asm_context ctx;
};

struct asm_module {
	struct asm_procedures {
		uint32_t len, cap;
		struct asm_procedure *elems;
	} procs;
};

#define REG_FREE -1
#define RED_ZONE_SIZE 128

static const enum asm_register asm_reg_alloc_ord[ASM_REG_COUNT] = {
	asm_reg_rdi,
	asm_reg_rsi,
	asm_reg_rcx,
	asm_reg_r8,
	asm_reg_r9,
	asm_reg_r10,
	asm_reg_r11,
	asm_reg_rbx,
	asm_reg_r12,
	asm_reg_r13,
	asm_reg_r14,
	asm_reg_r15,
	asm_reg_rax,
	asm_reg_rdx,
};

static const enum asm_register asm_arg_regs[] = {
	asm_reg_rdi,
	asm_reg_rsi,
	asm_reg_rdx,
	asm_reg_rcx,
	asm_reg_r8,
	asm_reg_r9,
};

static const enum asm_register asm_ret_regs[] = {
	asm_reg_rax,
	asm_reg_rdx,
};

static const enum asm_register asm_callee_save_regs[] = {
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
	[asm_reg_r15] = "%r15w",
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

const char *asm_reg_name(enum asm_register reg, int ir_type)
{
	assert(!(ir_type & IR_FLOAT));
	assert(reg >= 0 && reg < ASM_REG_COUNT);
	switch (ir_type & IR_TYPE_MASK) {
	case IR_VOID: FAILWITH("TODO: invalid register size.");
	case IR_I8:  return asm_reg_b_name[reg];
	case IR_I16: return asm_reg_w_name[reg];
	case IR_I32: return asm_reg_d_name[reg];
	case IR_I64:
	case IR_PTR: return asm_reg_q_name[reg];
	default:
		FAILWITH("TODO: invalid register size.");
		break;
	}
}

bool ir_type_float_p(int ir_type)
{
	return ir_type & IR_FLOAT;
}

bool ir_type_signed_p(int ir_type)
{
	return ir_type & IR_SIGN;
}

int asm_suffix(int ir_type)
{
	assert(!(ir_type & IR_FLOAT));
	switch (ir_type & IR_TYPE_MASK) {
	case IR_VOID: FAILWITH("TODO: invalid register size.");
	case IR_I8:  return 'b';
	case IR_I16: return 'w';
	case IR_I32: return 'l';
	case IR_I64:
	case IR_PTR: return 'q';
	default:
		FAILWITH("TODO: invalid size.");
		break;
	}
}

int ir_type_size(int ir_type)
{
	switch (ir_type & IR_TYPE_MASK) {
	case IR_VOID: FAILWITH("TODO: invalid type size.");
	case IR_I8:  return 1;
	case IR_I16: return 2;
	case IR_I32: return 4;
	case IR_I64:
	case IR_PTR: return 8;
	default:
		FAILWITH("TODO: invalid size.");
		break;
	}
}

void asm_context_block(struct asm_context *ctx, struct ir_proc *proc, struct ir_blk *blk,
					   struct ir_toplevel *tl, struct da_pointers *visited)
{
	if (da_ptr_member_p(blk, visited)) return;
	da_append(visited, blk);
	da_foreach(x, &blk->args) {
		if (ctx->locals[*x].tag == ADDR_NONE) {
			ctx->locals[*x].tag = ADDR_TEMP_REG;
		}
	}
	da_foreach(ins, &blk->code) {
		switch (ins->op) {
		case ir_op_nop: break;
		case ir_op_add:
		case ir_op_sub:
		case ir_op_mul:
		case ir_op_land:
		case ir_op_lor:
		case ir_op_xor:
		case ir_op_shl:
		case ir_op_shr: {
			struct asm_address *addr = &ctx->locals[ins->dst];
			assert(addr->tag == ADDR_NONE);
			addr->tag = ADDR_TEMP_REG;
		} break;
		case ir_op_div:  FAILWITH("TODO: ir_op_div"); break;
		case ir_op_mod:  FAILWITH("TODO: ir_op_mod"); break;
		case ir_op_lnot: FAILWITH("TODO: ir_op_lnot"); break;
		case ir_op_neg:  FAILWITH("TODO: ir_op_neg"); break;
		case ir_op_cmp_e:
		case ir_op_cmp_ne:
		case ir_op_cmp_l:
		case ir_op_cmp_g:
		case ir_op_cmp_le:
		case ir_op_cmp_ge: {
			struct asm_address *addr = &ctx->locals[ins->dst];
			assert(addr->tag == ADDR_NONE);
			addr->tag = ADDR_FLAGS;
			addr->as.sta[0] = ins->op;
			addr->as.sta[1] = ins->type;
		} break;
		case ir_op_alloca: {
			struct asm_address *addr = &ctx->locals[ins->dst];
			assert(addr->tag == ADDR_NONE);
			addr->tag = ADDR_STACK;
			int sz = ir_type_size(ins->type);
			ctx->stack_size = align_adjust(ctx->stack_size, sz);
			ctx->stack_size += sz * ins->arg.i32;
			addr->as.i = -ctx->stack_size;
		} break;
		case ir_op_load: {
			struct asm_address *dst = &ctx->locals[ins->dst];
			struct asm_address *ptr = &ctx->locals[ins->arg.rx[0]];
			assert(dst->tag == ADDR_NONE);
			if (ptr->tag == ADDR_STACK) {
				dst->tag = ADDR_STACK_LOAD;
				dst->as.sta[0] = ptr->as.i;
				dst->as.sta[1] = ins->arg.rx[1];
			} else {
				dst->tag = ADDR_TEMP_REG;
			}
		} break;
		case ir_op_load_globl: {
			struct asm_address *addr = &ctx->locals[ins->dst];
			assert(addr->tag == ADDR_NONE);
			addr->tag = ADDR_SYMBOL;
			addr->as.i = ins->arg.i32;
		} break;
		case ir_op_load_const: FAILWITH("TODO: ir_op_load_const"); break;
		case ir_op_load_imm: {
			struct asm_address *addr = &ctx->locals[ins->dst];
			assert(addr->tag == ADDR_NONE);
			addr->tag = ADDR_IMM_INT;
			addr->as.i = ins->arg.i32;
		} break;
		case ir_op_store: /* do nothing */ break;
		case ir_op_memcpy: FAILWITH("TODO: ir_op_memcpy"); break;
		case ir_op_memset: FAILWITH("TODO: ir_op_memset"); break;
		case IR_OPCODE_COUNT:
		default: FAILWITH("Unreachable"); break;
		}
	}
	struct ir_blk_terminal *term = &blk->term;
	switch (term->op) {
	case ir_op_ret: {
		assert(term->args.len <= 2);
		for (size_t i = 0; i < term->args.len; ++i) {
			struct asm_address *addr = &ctx->locals[term->args.elems[i]];
			assert(addr->tag != ADDR_NONE);
			addr->tag = ADDR_REGISTER;
			addr->as.i = asm_ret_regs[i];
		}
	} break;
	case ir_op_goto: {
		for (size_t i = 0; i < term->args.len; ++i) {
			struct asm_address *addr = &ctx->locals[term->args.elems[i]];
			assert(addr->tag != ADDR_NONE);
			addr->tag = ADDR_BLK_ARG;
			addr->as.i = term->b0->args.elems[i];
		}
		asm_context_block(ctx, proc, term->b0, tl, visited);
	} break;
	case ir_op_if: {
		assert(term->args.len == 1);
		struct asm_address *addr = &ctx->locals[term->args.elems[0]];
		assert(addr->tag == ADDR_FLAGS);
		asm_context_block(ctx, proc, term->b0, tl, visited);
		asm_context_block(ctx, proc, term->b1, tl, visited);
	} break;
	case ir_op_call: {
		assert(term->args.len >= 1);
		assert(term->args.len - 1 <= ARRAY_LENGTH(asm_arg_regs));
		ctx->is_leaf = false;
		assert(ctx->locals[term->args.elems[0]].tag != ADDR_NONE);
		for (size_t i = 1; i < term->args.len; ++i) {
			struct asm_address *addr = &ctx->locals[term->args.elems[i]];
			addr->tag = ADDR_REGISTER;
			addr->as.i = asm_arg_regs[i - 1];
		}
		struct ir_blk *ret_blk = term->b0;
		assert(ret_blk->args.len <= 2);
		for (size_t i = 0; i < ret_blk->args.len; ++i) {
			struct asm_address *addr = &ctx->locals[ret_blk->args.elems[i]];
			addr->tag = ADDR_REGISTER;
			addr->as.i = asm_ret_regs[i];
		}
		asm_context_block(ctx, proc, term->b0, tl, visited);
	} break;
	case ir_op_tailcall: FAILWITH("TODO: ir_op_tailcall"); break;
	}
}

struct asm_context *asm_create_context(struct ir_proc *proc, struct ir_toplevel *tl, struct asm_context *ctx)
{
	ctx->localc = proc->regc;
	ctx->locals = calloc(proc->regc, sizeof(struct asm_address));
	ctx->is_leaf = true;
	for (int i = 0; i < ASM_REG_COUNT; ++i) {
		ctx->assigned[i] = REG_FREE;
	}
	assert(proc->argc <= ARRAY_LENGTH(asm_arg_regs));
	struct ir_blk *entry = proc->entry;
	for (size_t i = 0; i < entry->args.len; ++i) {
		int x = entry->args.elems[i];
		ctx->locals[x].tag = ADDR_REGISTER;
		ctx->locals[x].as.i = asm_arg_regs[i];
	}
	struct da_pointers visited = {0};
	asm_context_block(ctx, proc, entry, tl, &visited);
	da_free(&visited);
	return ctx;
}

bool asm_register_assigned_p(struct asm_context *ctx, enum asm_register reg)
{
	return ctx->assigned[reg] != REG_FREE;
}

void asm_reserve_register(struct asm_context *ctx, enum asm_register reg, int local)
{
	ctx->assigned[reg] = local;
	ctx->clobbered |= 1u << reg;
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

bool can_optimize_red_zone(struct asm_context *ctx)
{
	return ctx->is_leaf && ctx->stack_size <= RED_ZONE_SIZE;
}

bool must_alloc_stack_frame(struct asm_context *ctx)
{
	return ctx->stack_size > 0 && can_optimize_red_zone(ctx) == false;
}

const char *asm_source_operand_to_str(struct asm_address *src, int type,
									  struct asm_context *ctx, struct ir_toplevel *tl)
{
	static char buf[0xff] = {0};
	const char *s = buf;
	switch (src->tag) {
	case ADDR_REGISTER:
		asm_unassign_register(ctx, src->as.i);
		s = asm_reg_name(src->as.i, type);
		break;
	case ADDR_IMM_INT:
		snprintf(buf, sizeof(buf), "$%ld", src->as.i);
		break;
	case ADDR_IMM_FLOAT:  FAILWITH("TODO: ADDR_IMM_FLOAT");  break;
	case ADDR_STACK:
		snprintf(buf, sizeof(buf), "%ld(%%rbp)", src->as.i);
		break;
	case ADDR_STACK_LOAD:
		snprintf(buf, sizeof(buf), "%d(%%rbp)", src->as.sta[0] + src->as.sta[1]);
		break;
	case ADDR_SYMBOL:
		snprintf(buf, sizeof(buf), SV_FMT, SV_ARGS(&tl->elems[src->as.i].def->id->sv));
		break;
		/* NOTE: These options should be invalid for the source operand */
	case ADDR_TEMP_REG:	  FAILWITH("TODO: ADDR_TEMP_REG");   break;
	case ADDR_BLK_ARG:    FAILWITH("TODO: ADDR_BLK_ARG");    break;
	case ADDR_FLAGS:	  FAILWITH("TODO: ADDR_FLAGS");	     break;
	case ADDR_NONE:		  FAILWITH("TODO: ADDR_NONE");	     break;
	default: FAILWITH("Unreachable"); break;
	}
	return s;
}

enum asm_register asm_emit_move_to_callee_save(struct asm_address *addr, struct asm_procedure *code)
{
	struct asm_context *ctx = &code->ctx;
	int local = ctx->assigned[addr->as.i];
	assert(ctx->locals[local].tag == ADDR_REGISTER);
	enum asm_register reg = asm_assign_callee_save_register(ctx, local);
	ctx->locals[local].as.i = reg;
	append_line(&code->body, fmt_str(
					"\tmovq %s, %s\n",
					asm_reg_q_name[addr->as.i],
					asm_reg_q_name[reg]));
	return reg;
}

void asm_emit_basic_op(const char *asm_op, IR_Ins *ins, struct asm_address *dst,
					   struct asm_address *x, struct asm_address *y,
					   struct ir_toplevel *tl, struct asm_procedure *code)
{
	struct asm_context *ctx = &code->ctx;
	int type = ins->type;
	enum asm_register dst_reg;
	switch (dst->tag) {
	case ADDR_BLK_ARG: {
		struct asm_address *arg_addr = &ctx->locals[dst->as.i]; // lookup the addr for the formal block arg */
		if (arg_addr->tag == ADDR_TEMP_REG) {
			arg_addr->tag = ADDR_REGISTER;
			arg_addr->as.i = asm_assign_register(ctx, ins->dst);
		}
		assert(arg_addr->tag == ADDR_REGISTER);
		dst = arg_addr;
	} [[fallthrough]];
	case ADDR_REGISTER: {
		bool move_result_p = false;
		dst_reg = dst->as.i;
		if (asm_register_assigned_p(ctx, dst_reg)) {
			if (x->tag == ADDR_REGISTER && x->as.i == dst_reg) {
				const char *dst_name = asm_reg_name(dst_reg, type);
				const char *src_name = asm_source_operand_to_str(y, type, ctx, tl);
				append_line(&code->body, fmt_str(
								"\t%s%c %s, %s\n",
								asm_op,
								asm_suffix(type),
								src_name,
								dst_name));
				break;
			} else if (y->tag == ADDR_REGISTER && y->as.i == dst_reg) {
				move_result_p = true;
				dst_reg = asm_assign_register(ctx, ins->dst);
			} else {
				/* spill to callee save */
				dst_reg = asm_emit_move_to_callee_save(dst, code);
			}
		} else {
			asm_reserve_register(ctx, dst_reg, ins->dst);
		}
		switch (x->tag) {
		case ADDR_REGISTER: {
			const char *dst_name = asm_reg_name(dst_reg, type);
			if (dst->as.i != x->as.i) {
				append_line(&code->body, fmt_str(
								"\tmov%c %s, %s /* HERE */\n",
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
		} break;
		case ADDR_TEMP_REG:	  FAILWITH("TODO: ADDR_TEMP_REG");  break;
		case ADDR_IMM_INT: {
			const char *dst_name = asm_reg_name(dst_reg, type);
			append_line(&code->body, fmt_str(
							"\tmov%c $%ld, %s\n",
							asm_suffix(type),
							x->as.i,
							dst_name));
			const char *src_name = asm_source_operand_to_str(y, type, ctx, tl);
			append_line(&code->body, fmt_str(
							"\t%s%c %s, %s\n",
							asm_op,
							asm_suffix(type),
							src_name,
							dst_name));
		} break;
		case ADDR_IMM_FLOAT:  FAILWITH("TODO: ADDR_IMM_FLOAT"); break;
		case ADDR_STACK:      FAILWITH("TODO: ADDR_STACK");     break;
		case ADDR_STACK_LOAD: {
			/* setup dst operand */
			const char *dst_name = asm_reg_name(dst_reg, type);
			append_line(&code->body, fmt_str(
							"\tmov%c %d(%%rbp), %s\n",
							asm_suffix(type),
							x->as.sta[0] + x->as.sta[1],
							dst_name));
			const char *src_name = asm_source_operand_to_str(y, type, ctx, tl);
			append_line(&code->body, fmt_str(
							"\t%s%c %s, %s\n",
							asm_op,
							asm_suffix(type),
							src_name,
							dst_name));
		} break;
		case ADDR_BLK_ARG:    FAILWITH("TODO: ADDR_BLK_ARG");   break;
		case ADDR_SYMBOL:	  FAILWITH("TODO: ADDR_SYMBOL");    break;
		case ADDR_FLAGS:	  FAILWITH("TODO: ADDR_FLAGS");	    break;
		case ADDR_NONE:		  FAILWITH("TODO: ADDR_NONE");	    break;
		default: FAILWITH("Unreachable"); break;
		}
		if (move_result_p) {
			asm_unassign_register(ctx, dst_reg);
			append_line(&code->body, fmt_str(
							"\tmov%c %s, %s\n",
							asm_suffix(type),
							asm_reg_name(dst_reg, type),
							asm_reg_name(dst->as.i, type)));
		}
	} break;
	case ADDR_TEMP_REG: {
		switch (x->tag) {
		case ADDR_REGISTER: {
			dst_reg = x->as.i;
			dst->tag = ADDR_REGISTER;
			dst->as.i = dst_reg;
			asm_reserve_register(ctx, dst_reg, ins->dst);
			const char *dst_name = asm_reg_name(dst_reg, type);
			const char *src_name = asm_source_operand_to_str(y, type, ctx, tl);
			append_line(&code->body, fmt_str(
							"\t%s%c %s, %s\n",
							asm_op,
							asm_suffix(type),
							src_name,
							dst_name));
		} break;
		case ADDR_TEMP_REG:	  FAILWITH("TODO: ADDR_TEMP_REG");   break;
		case ADDR_IMM_INT:	  FAILWITH("TODO: ADDR_IMM_INT");    break;
		case ADDR_IMM_FLOAT:  FAILWITH("TODO: ADDR_IMM_FLOAT");  break;
		case ADDR_STACK:      FAILWITH("TODO: ADDR_STACK");      break;
		case ADDR_STACK_LOAD: {
			/* setup dst operand */
			dst_reg = asm_assign_register(ctx, ins->dst);
			dst->tag = ADDR_REGISTER;
			dst->as.i = dst_reg;
			const char *dst_name = asm_reg_name(dst_reg, type);
			append_line(&code->body, fmt_str(
							"\tmov%c %d(%%rbp), %s\n",
							asm_suffix(type),
							x->as.sta[0] + x->as.sta[1],
							dst_name));
			const char *src_name = asm_source_operand_to_str(y, type, ctx, tl);
			append_line(&code->body, fmt_str(
							"\t%s%c %s, %s\n",
							asm_op,
							asm_suffix(type),
							src_name,
							dst_name));
		} break;
		case ADDR_BLK_ARG:    FAILWITH("TODO: ADDR_BLK_ARG");   break;
		case ADDR_SYMBOL:	  FAILWITH("TODO: ADDR_SYMBOL");     break;
		case ADDR_FLAGS:	  FAILWITH("TODO: ADDR_FLAGS");	     break;
		case ADDR_NONE:		  FAILWITH("TODO: ADDR_NONE");	     break;
		default: FAILWITH("Unreachable"); break;
		}
	} break;
	case ADDR_IMM_INT:	  FAILWITH("TODO: ADDR_IMM_INT");   break;
	case ADDR_IMM_FLOAT:  FAILWITH("TODO: ADDR_IMM_FLOAT"); break;
	case ADDR_STACK:      FAILWITH("TODO: ADDR_STACK");     break;
	case ADDR_STACK_LOAD: FAILWITH("TODO: ADDR_STACK");     break;
	case ADDR_SYMBOL:	  FAILWITH("TODO: ADDR_SYMBOL");    break;
	case ADDR_FLAGS:      FAILWITH("TODO: ADDR_FLAGS");     break;
	case ADDR_NONE:		  FAILWITH("TODO: ADDR_NONE");	    break;
	default: FAILWITH("Unreachable"); break;
	}
}

void asm_emit_block(struct da_pointers *blocks, size_t blk_id, struct ir_proc *proc,
					struct ir_toplevel *tl, struct asm_procedure *code)
{
	struct asm_context *ctx = &code->ctx;
	struct ir_blk *blk = blocks->elems[blk_id];
	append_line(&code->body, fmt_str(".L%p:\n", blk));
	da_foreach(ins, &blk->code) {
		char *asm_op;
		switch (ins->op) {
		case ir_op_nop:	break;
		case ir_op_add:
			if (ir_type_float_p(ins->type)) {
				asm_op = "fadd";
			} else {
				asm_op = "add";
			}
			goto LBL_basic;
		case ir_op_sub:
			if (ir_type_float_p(ins->type)) {
				asm_op = "fsub";
			} else {
				asm_op = "sub";
			}
			goto LBL_basic;
		case ir_op_mul:
			if (ir_type_float_p(ins->type)) {
				asm_op = "fmul";
			} else if (ir_type_signed_p(ins->type)) {
				asm_op = "imul";
			} else {
				asm_op = "mul";
			}
		LBL_basic:
			asm_emit_basic_op(asm_op, ins, &ctx->locals[ins->dst],
							  &ctx->locals[ins->arg.rx[0]], &ctx->locals[ins->arg.rx[1]], tl, code);
			break;
		case ir_op_cmp_e:
		case ir_op_cmp_ne:
		case ir_op_cmp_l:
		case ir_op_cmp_g:
		case ir_op_cmp_le:
		case ir_op_cmp_ge: {
			asm_op = ir_type_float_p(ins->type) ? "fcmp" : "cmp";
			assert(ctx->locals[ins->dst].tag == ADDR_FLAGS);
			struct asm_address dst = {.tag=ADDR_TEMP_REG};
			asm_emit_basic_op(asm_op, ins, &dst, &ctx->locals[ins->arg.rx[0]],
							  &ctx->locals[ins->arg.rx[1]], tl, code);
			assert(dst.tag == ADDR_REGISTER);
			asm_unassign_register(ctx, dst.as.i);
		} break;
		case ir_op_div:			FAILWITH("TODO: ir_op_div");		break;
		case ir_op_mod:			FAILWITH("TODO: ir_op_mod");		break;
		case ir_op_lnot:		FAILWITH("TODO: ir_op_lnot");		break;
		case ir_op_land:		FAILWITH("TODO: ir_op_land");		break;
		case ir_op_lor:			FAILWITH("TODO: ir_op_lor");		break;
		case ir_op_xor:			FAILWITH("TODO: ir_op_xor");		break;
		case ir_op_shl: asm_op = ir_type_signed_p(ins->type) ? "sal" : "shl"; goto LBL_shift;
		case ir_op_shr: asm_op = ir_type_signed_p(ins->type) ? "sar" : "shr"; goto LBL_shift;
		LBL_shift:
		{
			struct asm_address *dst = &ctx->locals[ins->dst];
			struct asm_address *x   = &ctx->locals[ins->arg.rx[0]];
			struct asm_address *y   = &ctx->locals[ins->arg.rx[1]];
			switch (dst->tag) {
			case ADDR_REGISTER: {
				enum asm_register dst_reg = dst->as.i;
				if (asm_register_assigned_p(ctx, dst_reg)) {
					FAILWITH("TODO: resolve register conflict.");
				} else {
					asm_reserve_register(ctx, dst_reg, ins->dst);
				}
				/* make sure rcx is available for shift */
				if (asm_register_assigned_p(ctx, asm_reg_rcx)) {
					assert((y->tag == ADDR_REGISTER && y->as.i == asm_reg_rcx)
						   || (y->tag == ADDR_IMM_INT));
				} else {
					asm_reserve_register(ctx, asm_reg_rcx, ins->arg.rx[1]);
				}
				switch (x->tag) {
				case ADDR_REGISTER:   FAILWITH("TODO: ADDR_REGISTER");  break;
				case ADDR_TEMP_REG:	  FAILWITH("TODO: ADDR_TEMP_REG");  break;
				case ADDR_IMM_INT:    FAILWITH("TODO: ADDR_IMM_INT");   break;
				case ADDR_IMM_FLOAT:  FAILWITH("TODO: ADDR_IMM_FLOAT"); break;
				case ADDR_STACK:      FAILWITH("TODO: ADDR_STACK");     break;
				case ADDR_STACK_LOAD: {
					const char *dst_name = asm_reg_name(dst_reg, ins->type);
					append_line(&code->body, fmt_str(
									"\tmov%c %d(%%rbp), %s\n",
									asm_suffix(ins->type),
									x->as.sta[0] + x->as.sta[1],
									dst_name));
					/* special case if value is already in rcx */
					switch (y->tag) {
					case ADDR_REGISTER: {
						if (y->as.i != asm_reg_rcx) {
							append_line(&code->body, fmt_str(
											"\tmov%c %s, %s\n",
											asm_suffix(ins->type),
											asm_reg_name(y->as.i, ins->type),
											asm_reg_name(asm_reg_rcx, ins->type)));
						}
						append_line(&code->body, fmt_str(
										"\t%s%c %%cl, %s\n",
										asm_op,
										asm_suffix(ins->type),
										dst_name));
					} break;
					case ADDR_IMM_INT: {
						append_line(&code->body, fmt_str(
										"\t%s%c $%ld, %s\n",
										asm_op,
										asm_suffix(ins->type),
										y->as.i,
										dst_name));
					} break;
					case ADDR_STACK: FAILWITH("TODO: ADDR_STACK"); break;
					case ADDR_STACK_LOAD: {
						append_line(&code->body, fmt_str(
										"\tmov%c %d(%%rbp), %s\n",
										asm_suffix(ins->type),
										y->as.sta[0] + y->as.sta[1],
										asm_reg_name(asm_reg_rcx, ins->type)));
						append_line(&code->body, fmt_str(
										"\t%s%c %%cl, %s\n",
										asm_op,
										asm_suffix(ins->type),
										dst_name));
					} break;
					case ADDR_SYMBOL:     FAILWITH("TODO: ADDR_SYMBOL");     break;
					case ADDR_TEMP_REG:	  FAILWITH("TODO: ADDR_TEMP_REG");   break;
					case ADDR_BLK_ARG:    FAILWITH("TODO: ADDR_BLK_ARG");    break;
					case ADDR_FLAGS:	  FAILWITH("TODO: ADDR_FLAGS");	     break;
					case ADDR_NONE:		  FAILWITH("TODO: ADDR_NONE");	     break;
					case ADDR_IMM_FLOAT:  FAILWITH("TODO: ADDR_IMM_FLOAT");  break;
					default: FAILWITH("Unreachable"); break;
					}
				} break;
				case ADDR_BLK_ARG:    FAILWITH("TODO: ADDR_BLK_ARG");   break;
				case ADDR_SYMBOL:	  FAILWITH("TODO: ADDR_SYMBOL");    break;
				case ADDR_FLAGS:	  FAILWITH("TODO: ADDR_FLAGS");	    break;
				case ADDR_NONE:		  FAILWITH("TODO: ADDR_NONE");	    break;
				default: FAILWITH("Unreachable"); break;
				}
				asm_unassign_register(ctx, asm_reg_rcx);
			} break;
			case ADDR_TEMP_REG:   FAILWITH("TODO: ADDR_TEMP_REG");  break;
			case ADDR_IMM_INT:	  FAILWITH("TODO: ADDR_IMM_INT");   break;
			case ADDR_IMM_FLOAT:  FAILWITH("TODO: ADDR_IMM_FLOAT"); break;
			case ADDR_STACK:      FAILWITH("TODO: ADDR_STACK");     break;
			case ADDR_STACK_LOAD: FAILWITH("TODO: ADDR_STACK_LOAD"); break;
			case ADDR_BLK_ARG:    FAILWITH("TODO: ADDR_BLK_ARG");   break;
			case ADDR_SYMBOL:	  FAILWITH("TODO: ADDR_SYMBOL");    break;
			case ADDR_FLAGS:      FAILWITH("TODO: ADDR_FLAGS");     break;
			case ADDR_NONE:		  FAILWITH("TODO: ADDR_NONE");	    break;
			default: FAILWITH("Unreachable"); break;
			}
		} break;
		case ir_op_neg:			FAILWITH("TODO: ir_op_neg");		break;
		case ir_op_alloca: /* nothing to do */ break;
		case ir_op_load: {
			struct asm_address *dst = &ctx->locals[ins->dst];
			struct asm_address *x   = &ctx->locals[ins->arg.rx[0]];
			switch (dst->tag) {
			case ADDR_BLK_ARG: {
				struct asm_address *arg_addr = &ctx->locals[dst->as.i];
				if (arg_addr->tag == ADDR_TEMP_REG) {
					arg_addr->tag = ADDR_REGISTER;
					arg_addr->as.i = asm_assign_register(ctx, ins->dst);
				}
				assert(arg_addr->tag == ADDR_REGISTER);
				dst = arg_addr;
			} [[fallthrough]];
			case ADDR_REGISTER: {
				switch (x->tag) {
				case ADDR_REGISTER:   FAILWITH("TODO: ADDR_REGISTER");   break;
				case ADDR_TEMP_REG:	  FAILWITH("TODO: ADDR_TEMP_REG");   break;
				case ADDR_IMM_INT:    FAILWITH("TODO: ADDR_IMM_INT");    break;
				case ADDR_IMM_FLOAT:  FAILWITH("TODO: ADDR_IMM_FLOAT");  break;
				case ADDR_STACK: {
					int offset = ins->arg.rx[1];
					append_line(&code->body, fmt_str(
									"\tmov%c %ld(%%rbp), %s\n",
									asm_suffix(ins->type),
									x->as.i + offset,
									asm_reg_name(dst->as.i, ins->type)));
				} break;
				case ADDR_STACK_LOAD: FAILWITH("TODO: ADDR_STACK_LOAD"); break;
				case ADDR_BLK_ARG:    FAILWITH("TODO: ADDR_BLK_ARG");    break;
				case ADDR_SYMBOL:	  FAILWITH("TODO: ADDR_SYMBOL");	 break;
				case ADDR_FLAGS:	  FAILWITH("TODO: ADDR_FLAGS");	     break;
				case ADDR_NONE:		  FAILWITH("TODO: ADDR_NONE");	     break;
				default: FAILWITH("Unreachable"); break;
				}
			} break;
			case ADDR_TEMP_REG:	  FAILWITH("TODO: ADDR_TEMP_REG");  break;
			case ADDR_IMM_INT:	  FAILWITH("TODO: ADDR_IMM_INT");   break;
			case ADDR_IMM_FLOAT:  FAILWITH("TODO: ADDR_IMM_FLOAT"); break;
			case ADDR_STACK:      FAILWITH("TODO: ADDR_STACK");     break;
			case ADDR_STACK_LOAD: /* do nothing */ break;
			case ADDR_SYMBOL:	  FAILWITH("TODO: ADDR_SYMBOL");    break;
			case ADDR_FLAGS:	  FAILWITH("TODO: ADDR_FLAGS");	    break;
			case ADDR_NONE:		  FAILWITH("TODO: ADDR_NONE");	    break;
			default: FAILWITH("Unreachable"); break;
			}
		} break;
		case ir_op_load_globl: assert(ctx->locals[ins->dst].tag == ADDR_SYMBOL); break;
		case ir_op_load_const: FAILWITH("TODO: ir_op_load_const"); break;
		case ir_op_load_imm: {
			struct asm_address *dst = &ctx->locals[ins->dst];
			if (dst->tag != ADDR_IMM_INT) {
				assert(dst->tag == ADDR_REGISTER);
				assert(!asm_register_assigned_p(ctx, dst->as.i));
				asm_reserve_register(ctx, dst->as.i, ins->dst);
				append_line(&code->body, fmt_str(
								"\tmov%c $%d, %s\n",
								asm_suffix(ins->type),
								ins->arg.i32,
								asm_reg_name(dst->as.i, ins->type)));
			}
		} break;
		case ir_op_store: {
			struct asm_address *dst = &ctx->locals[ins->dst];
			struct asm_address *x   = &ctx->locals[ins->arg.rx[1]];
			int offset = ins->arg.rx[0];
			switch (dst->tag) {
			case ADDR_REGISTER:	 FAILWITH("TODO: ADDR_REGISTER");  break;
			case ADDR_TEMP_REG:	 FAILWITH("TODO: ADDR_TEMP_REG");  break;
			case ADDR_IMM_INT:	 FAILWITH("TODO: ADDR_IMM_INT");   break;
			case ADDR_IMM_FLOAT: FAILWITH("TODO: ADDR_IMM_FLOAT"); break;
			case ADDR_STACK: {
				switch (x->tag) {
				case ADDR_REGISTER: {
					append_line(&code->body, fmt_str(
									"\tmov%c %s, %ld(%%rbp)\n",
									asm_suffix(ins->type),
									asm_reg_name(x->as.i, ins->type),
									dst->as.i + offset));
					asm_unassign_register(ctx, x->as.i);
				} break;
				case ADDR_TEMP_REG:	  FAILWITH("TODO: ADDR_TEMP_REG");   break;
				case ADDR_IMM_INT: {
					append_line(&code->body, fmt_str(
									"\tmov%c $%ld, %ld(%%rbp)\n",
									asm_suffix(ins->type),
									x->as.i,
									dst->as.i + offset));
				} break;
				case ADDR_IMM_FLOAT:  FAILWITH("TODO: ADDR_IMM_FLOAT");  break;
				case ADDR_STACK:	  FAILWITH("TODO: ADDR_STACK");	     break;
				case ADDR_STACK_LOAD: FAILWITH("TODO: ADDR_STACK_LOAD"); break;
				case ADDR_BLK_ARG:    FAILWITH("TODO: ADDR_BLK_ARG");    break;
				case ADDR_SYMBOL:	  FAILWITH("TODO: ADDR_SYMBOL");	 break;
				case ADDR_FLAGS:	  FAILWITH("TODO: ADDR_FLAGS");	     break;
				case ADDR_NONE:		  FAILWITH("TODO: ADDR_NONE");	     break;
				default: FAILWITH("Unreachable"); break;
				}
			} break;
			case ADDR_STACK_LOAD: FAILWITH("TODO: ADDR_STACK_LOAD"); break;
			case ADDR_BLK_ARG:    FAILWITH("TODO: ADDR_BLK_ARG");    break;
			case ADDR_SYMBOL:	 FAILWITH("TODO: ADDR_SYMBOL");	   break;
			case ADDR_FLAGS:	 FAILWITH("TODO: ADDR_FLAGS");	   break;
			case ADDR_NONE:		 FAILWITH("TODO: ADDR_NONE");	   break;
			default: FAILWITH("Unreachable"); break;
			}
		} break;
		case ir_op_memcpy:		FAILWITH("TODO: ir_op_memcpy");		break;
		case ir_op_memset:		FAILWITH("TODO: ir_op_memset");		break;
		case IR_OPCODE_COUNT:
		default: FAILWITH("Unreachable"); break;
		}
	}
	struct ir_blk_terminal *term = &blk->term;
	switch (term->op) {
	case ir_op_ret: {
		if (blk_id < blocks->len - 1) {
			append_line(&code->body, fmt_str("\tjmp .R"SV_FMT"\n", SV_ARGS(&proc->def->id->sv)));
		}
	} break;
	case ir_op_goto: {
		if (blk_id < blocks->len - 1
			&& term->b0 == blocks->elems[blk_id + 1]) {
			/* fall through to block */
		} else {
			append_line(&code->body, fmt_str("\tjmp .L%p\n", term->b0));
		}
	} break;
	case ir_op_if: {
		assert(term->args.len == 1);
		struct asm_address *cond = &ctx->locals[term->args.elems[0]];
		assert(cond->tag == ADDR_FLAGS);
		char *cc;
		switch (cond->as.sta[0]) {
		case ir_op_cmp_e:  cc = "e";  break;
		case ir_op_cmp_ne: cc = "ne"; break;
		case ir_op_cmp_l:  cc = ir_type_signed_p(cond->as.sta[1]) ? "l" : "b";   break;
		case ir_op_cmp_g:  cc = ir_type_signed_p(cond->as.sta[1]) ? "g" : "b";   break;
		case ir_op_cmp_le: cc = ir_type_signed_p(cond->as.sta[1]) ? "le" : "be"; break;
		case ir_op_cmp_ge: cc = ir_type_signed_p(cond->as.sta[1]) ? "ge" : "be"; break;
		default: FAILWITH("Unreachable"); break;
		}
		append_line(&code->body, fmt_str("\tj%s .L%p\n", cc, term->b0));
	} break;
	case ir_op_call: {
		assert(term->args.len >= 1);
		{
			struct asm_address *arg = &ctx->locals[term->args.elems[0]];
			if (arg->tag == ADDR_REGISTER) {
				assert(asm_register_assigned_p(ctx, arg->as.i));
				asm_unassign_register(ctx, arg->as.i);
			}
		}
		for (size_t i = 1; i < term->args.len; ++i) {
			struct asm_address *arg = &ctx->locals[term->args.elems[i]];
			assert(arg->tag == ADDR_REGISTER);
			assert(asm_register_assigned_p(ctx, arg->as.i));
			asm_unassign_register(ctx, arg->as.i);
		}
		struct ir_blk *ret_blk = term->b0;
		for (size_t i = 0; i < ret_blk->args.len; ++i) {
			int arg_id = ret_blk->args.elems[i];
			struct asm_address *arg = &ctx->locals[arg_id];
			assert(arg->tag == ADDR_REGISTER);
			if (asm_register_assigned_p(ctx, arg->as.i)) {
				/* spill to callee save */
				asm_emit_move_to_callee_save(arg, code);
			}
			asm_reserve_register(ctx, arg->as.i, arg_id);
		}
		struct asm_address *p = &ctx->locals[term->args.elems[0]];
		assert(p->tag == ADDR_SYMBOL);
		struct ir_proc *proc = &tl->elems[p->as.i];
		append_line(&code->body, fmt_str("\tcall "SV_FMT"\n", SV_ARGS(&proc->def->id->sv)));
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
	struct definition *def = proc->def;
	append_line(&code->prologue, fmt_str(".globl "SV_FMT"\n", SV_ARGS(&def->id->sv)));
	append_line(&code->prologue, fmt_str(SV_FMT":\n", SV_ARGS(&def->id->sv)));
	/* procedure entry */
	bool setup_frame = false;
	bool alloc_frame = false;
	size_t callee_save_offset = ctx->stack_size = align_adjust(ctx->stack_size, 8);
	ctx->stack_size += count_set_bits(ctx->clobbered & REGC_CALLEE_SAVED) * 8;
	ctx->stack_size = align_adjust(ctx->stack_size, 16);
	if (ctx->stack_size > 0) {
		setup_frame = true;
		append_line(&code->prologue, strdup("\tpushq %rbp\n"));
		append_line(&code->prologue, strdup("\tmovq %rsp, %rbp\n"));
	}
	if (setup_frame && !can_optimize_red_zone(ctx)) {
		alloc_frame = true;
		append_line(&code->prologue, fmt_str("\tsubq $%zu, %%rsp\n", ctx->stack_size));
	}
	/* epilogue label */
	append_line(&code->epilogue, fmt_str(".R"SV_FMT":\n", SV_ARGS(&def->id->sv)));
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
	asm_create_context(proc, tl, &code->ctx);
	struct da_pointers blks = ir_blk_reverse_post_order(proc->entry);
	for (size_t i = 0; i < blks.len; ++i) {
		asm_emit_block(&blks, i, proc, tl, code);
	}
	asm_emit_prologue_and_epilogue(proc, code);
	da_free(&blks);
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

void asm_dump_module(struct asm_module *mod, FILE *file)
{
	fputs(".text\n", file);
	da_foreach(proc, &mod->procs) {
		asm_dump_procedure(proc, file);
	}
}

int main(void)
{
	const char *filename = "examples/fibonacci.k";
	struct strview sv = {0};
	if (sv_open_file(filename, &sv) == false) {
		fprintf(stderr, "[Error] %s: %s\n", filename, strerror(errno));
		return 1;
	}
	Parser parser = {
		.lexer = {
			.filename = filename,
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
	da_foreach(exp, &tl) {
		infer_type(&parser, *exp, NULL, &sc);
		ast_fprint(*exp, stdout);
		putchar('\n');
	}
	struct ir_toplevel ir = ast_compile(&tl, &sc);
	da_foreach(e, &ir) {
		ir_proc_fprint(e, stdout);
	}
	struct asm_module asm_mod = {0};
	da_foreach(p, &ir) {
		asm_emit_procedure(p, &ir, da_allot(&asm_mod.procs));
	}
	FILE *file = fopen("test.s", "w");
	asm_dump_module(&asm_mod, stdout);
	asm_dump_module(&asm_mod, file);
	fclose(file);
	return 0;
}
