#pragma once

#include "common.h"
#include "ir.h"

static union ir_object *
get_toplevel_obj(struct ir_toplevel *tl, size_t id)
{
	assert(id < tl->len);
	return &tl->elems[id];
}

static struct ir_proc *
get_toplevel_proc(struct ir_toplevel *tl, size_t id)
{
	union ir_object *obj = get_toplevel_obj(tl, id);
	assert(obj->tag == IRO_PROC);
	return &obj->proc;
}

static int
ir_proc_new_reg(struct ir_toplevel *tl, size_t proc_id)
{
	struct ir_proc *proc = get_toplevel_proc(tl, proc_id);
	assert(proc->regc < UINT16_MAX);
	return proc->regc++;
}

static void
ast_compile_procedure(size_t proc_id, struct ir_toplevel *tl)
{
	struct ir_proc *proc = get_toplevel_proc(tl, proc_id);
	if (proc->node->ret->tag == ast_type_void) {
		proc->retc = 0;
	}
	struct def_array *formals = &proc->node->formals;
	struct scope *scope = &proc->node->scope;
	struct ir_blk *entry = MEM_ALLOC(struct ir_blk);
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
		KCType *base_type = MEM_ALLOC(KCType);
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
		KCType *array_type = MEM_ALLOC(KCType);
		array_type->tag = ast_type_array;
		array_type->as.array.base = exp->type->as.slice;
		array_type->as.array.is_sized = true;
		array_type->as.array.size = exp->as.init.len;
		KCType *base_type = MEM_ALLOC(KCType);
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
		KCType *base_type = MEM_ALLOC(KCType);
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

static struct ir_blk *
dst_cpy_named_initializer(struct expression *exp, struct ast_comp_dest dst, size_t proc_id,
						  struct ir_blk *blk, struct scope *scope, struct ir_toplevel *tl)
{
	assert(dst.tag == DST_CPY);
	assert(exp->tag == ast_exp_named_struct_initializer);
	KCType *base_type = MEM_ALLOC(KCType);
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

static struct ir_blk *
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
	case ast_exp_array_initializer:
		printf("Here!! %s\n", ast_type_to_str(exp->type));
		[[fallthrough]];
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
			printf("Here!! %s\n", ast_type_to_str(exp->type));
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
		assert(entry->tag == SYMTBL_VARIABL);
		struct definition *def = entry->as.variable.def;
		assert(def != NULL);
		size_t ir_symbol;
		if (type_is_polymorphic(def->type)) {
			struct type_spec *instance = lookup_poly_proc_spec(NULL, def, exp->type, scope);
			if (instance == NULL) {
				FAILWITH(SV_FMT"; %s", SV_ARGS(token_to_strview(exp->tok)), ast_type_to_str(exp->type));
			}
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
				KCType *base_type = base->type;
				if (type_is_struct_ptr(base->type, scope)) {
					blk = ast_compile_expression(base, DEST_VAL(base_reg), proc_id, blk, scope, tl);
				} else {
					assert(base->type->tag == ast_type_struct);
					blk = ast_compile_expression(base, DEST_REF(base_reg), proc_id, blk, scope, tl);
					base_type = MEM_ALLOC(KCType);
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
				assert(entry->tag == SYMTBL_VARIABL);
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
				KCType *base_type = base->type;
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
					base_type = MEM_ALLOC(KCType);
					base_type->tag = ast_type_ptr;
					base_type->as.ptr = base->type;
				} else if (type_is_slice(base->type)) {
					int tmp = ir_proc_new_reg(tl, proc_id);
					blk = ast_compile_expression(base, DEST_REF(base_reg), proc_id, blk, scope, tl);
					base_type = type_slice_to_array_ptr(base->type);
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
				KCType *base_type = base->type;
				if (type_is_pointer(base->type)) {
					blk = ast_compile_expression(base, DEST_VAL(base_reg), proc_id, blk, scope, tl);
				} else if (type_is_array(base->type)) {
					blk = ast_compile_expression(base, DEST_REF(base_reg), proc_id, blk, scope, tl);
					base_type = MEM_ALLOC(KCType);
					base_type->tag = ast_type_ptr;
					base_type->as.ptr = base->type;
				} else if (type_is_slice(base->type)) {
					int tmp = ir_proc_new_reg(tl, proc_id);
					blk = ast_compile_expression(base, DEST_REF(base_reg), proc_id, blk, scope, tl);
					base_type = type_slice_to_array_ptr(base->type);
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
				KCType *base_type = base->type;
				if (type_is_pointer(base->type)) {
					blk = ast_compile_expression(base, DEST_VAL(base_reg), proc_id, blk, scope, tl);
				} else if (type_is_array(base->type)) {
					blk = ast_compile_expression(base, DEST_REF(base_reg), proc_id, blk, scope, tl);
					base_type = MEM_ALLOC(KCType);
					base_type->tag = ast_type_ptr;
					base_type->as.ptr = base->type;
				} else if (type_is_slice(base->type)) {
					int tmp = ir_proc_new_reg(tl, proc_id);
					blk = ast_compile_expression(base, DEST_REF(base_reg), proc_id, blk, scope, tl);
					base_type = type_slice_to_array_ptr(base->type);
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
			case DST_RET: [[fallthrough]];
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
		struct ir_blk *join = MEM_ALLOC(struct ir_blk);
		struct ir_blk *tblk = MEM_ALLOC(struct ir_blk);
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
			struct ir_blk *fblk = MEM_ALLOC(struct ir_blk);
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
		struct ir_blk *join = MEM_ALLOC(struct ir_blk);
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
			struct ir_blk *tblk = MEM_ALLOC(struct ir_blk);
			struct ir_blk *fblk;
			if (i == cc->branches.len - 1 && cc->else_exp == NULL) {
				fblk = join;
			} else {
				fblk = MEM_ALLOC(struct ir_blk);
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
		struct ir_blk *cond = MEM_ALLOC(struct ir_blk);
		struct ir_blk *body = MEM_ALLOC(struct ir_blk);
		struct ir_blk *join = MEM_ALLOC(struct ir_blk);
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

static char *
generate_mangled_name(struct strview ident, KCType *type)
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

static void
ast_create_proc_object(union ir_object *p, struct definition *def, KCType *type, struct expression *exp)
{
	p->proc.tag = IRO_PROC;
	struct strview id_name = token_to_strview(def->id);
	if (sv_is_equal(id_name, sv_of_cstr("main"))) {
		/* entry point */
		p->proc.link = strndup(id_name.ptr, id_name.len);
		p->proc.is_static = false;
	} else {
		p->proc.link = generate_mangled_name(id_name, type);
		p->proc.is_static = true;
	}
	p->proc.type = type;
	p->proc.def  = def;
	p->proc.node = &exp->as.proc;
	p->proc.regc = 0;
}

static struct ir_toplevel
ast_compile(struct scope *scope)
{
	struct symtbl *st = &scope->symtbl;
	struct ir_toplevel tl = {0};
	for (size_t i = 0; i < st->len; ++i) {
		if (st->elems[i].tag == SYMTBL_VALCONS) continue;
		assert(st->elems[i].tag == SYMTBL_VARIABL);
		struct definition *def = st->elems[i].as.variable.def;
		struct expression *exp = def->exp;
		union ir_object p = {0};
		struct strview id_name = token_to_strview(def->id);
		def->ir_symbol = tl.len;
		def->is_global = true;
		if (exp->tag == ast_exp_procedure_literal) {
			if (type_is_polymorphic(def->type)) {
#if KC_DEBUG
				printf("[Debug] Polymorphic Definition: "SV_FMT" spec count = %u\n",
					   SV_ARGS(id_name), def->specs.len);
#endif
				for (size_t i = 0; i < def->specs.len; ++i) {
					struct type_spec *spec_def = &def->specs.elems[i];
					if (type_is_polymorphic(spec_def->type)) continue;
					ast_create_proc_object(&p, def, spec_def->type, spec_def->exp);
					spec_def->ir_symbol = tl.len;
					da_append(&tl, p);
				}
			} else {
				ast_create_proc_object(&p, def, def->type, def->exp);
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

static void
ir_ins_fprint(IR_Ins *ins, FILE *file)
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
		fprintf(file, "%%%d := load_globl<", ins->dst);
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

static bool
da_ptr_member_p(void *p, struct da_pointers *ptrs)
{
	da_foreach(ptr, ptrs) {
		if (p == *ptr) return true;
	}
	return false;
}

static void
dfs_walk(struct ir_blk *blk, struct da_pointers *order, struct da_pointers *visited)
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

static struct da_pointers
ir_blk_post_order(struct ir_blk *root)
{
	struct da_pointers visited = {0};
	struct da_pointers order = {0};
	dfs_walk(root, &order, &visited);
	da_free(&visited);
	return order;
}

static struct da_pointers
ir_blk_reverse_post_order(struct ir_blk *root)
{
	struct da_pointers order = ir_blk_post_order(root);
	da_reverse(&order);
	return order;
}

static void
ir_blk_fprint(struct ir_blk *blk, FILE *file)
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

static void
ir_proc_fprint(struct ir_proc *proc, FILE *file)
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

static struct expression *
ast_desugar(Parser *p, struct expression *exp, struct scope *scope)
{
	if (exp->type == NULL) {
		log_error_and_die(p->lexer.filename, exp->tok, "Expression has no type.");
	}
	{
		KCType *t = resolve_type(p, exp->type, scope);
		if (t == NULL)
			log_error_and_die(p->lexer.filename, exp->tok,
							  "Failed to resolve type of expression `%s`.",
							  ast_type_to_str(exp->type));
		exp->type = t;
	}
	switch (exp->tag) {
	case ast_exp_definition: {
		struct definition *def = &exp->as.def;
		if (type_is_polymorphic(def->type)) {
			for (size_t i = 0; i < def->specs.len; ++i) {
				struct type_spec *spec_def = &def->specs.elems[i];
				if (!type_is_polymorphic(type_recursive_find(spec_def->type)) && spec_def->exp != NULL) {
					spec_def->type = resolve_type(p, spec_def->type, scope);
					spec_def->exp = ast_desugar(p, spec_def->exp, scope);
				}
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
		switch ((int)exp->as.bin.op) {
		case binop_and: {
			struct expression *left = ast_desugar(p, exp->as.bin.left, scope);
			struct expression *right = ast_desugar(p, exp->as.bin.right, scope);
			struct expression *iff = MEM_ALLOC(struct expression);
			struct expression *fb  = MEM_ALLOC(struct expression);
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
			exp->as.bin.left = ast_desugar(p, exp->as.bin.left, scope);
			exp->as.bin.right->type = &AST_TYPE_U64;
			if (exp->as.bin.right->tag == ast_exp_literal) {
				return exp;
			} else {
				assert(exp->as.bin.right->tag == ast_exp_ident);
				struct struct_type *mems = struct_type_members(p, exp->as.bin.left->type, scope);
				for (size_t i = 0; i < mems->len; ++i) {
					if (sv_is_equal(token_to_strview(mems->elems[i].name),
									token_to_strview(exp->as.bin.right->tok))) {
						exp->as.bin.right->tag = ast_exp_literal;
						exp->as.bin.right->as.lit.as.i = i;
						return exp;
					}
				}
				FAILWITH("Unreachable");
			}
		} break;
		case binop_add_assign: exp->as.bin.op = op_add;                 goto desugar_assignment;
		case binop_and_assign: exp->as.bin.op = op_and;                 goto desugar_assignment;
		case binop_lor_assign: exp->as.bin.op = op_lor;                 goto desugar_assignment;
		case binop_xor_assign: exp->as.bin.op = op_xor;                 goto desugar_assignment;
		case binop_sub_assign: exp->as.bin.op = op_sub;                 goto desugar_assignment;
		case binop_mul_assign: exp->as.bin.op = op_mul;                 goto desugar_assignment;
		case binop_div_assign: exp->as.bin.op = op_div;                 goto desugar_assignment;
		case binop_mod_assign: exp->as.bin.op = op_mod;                 goto desugar_assignment;
		case binop_shift_left_assign:  exp->as.bin.op = op_shift_left;  goto desugar_assignment;
		case binop_shift_right_assign: exp->as.bin.op = op_shift_right; goto desugar_assignment;
		desugar_assignment:
		{
			struct expression *assign = MEM_ALLOC(struct expression);
			*assign = *exp;
			assign->as.bin.op = op_assign;
			assign->as.bin.left = exp->as.bin.left;
			assign->as.bin.right = exp;
			return ast_desugar(p, assign, scope);
		} break;
		default: {
			struct expression *left = ast_desugar(p, exp->as.bin.left, scope);
			struct expression *right = ast_desugar(p, exp->as.bin.right, scope);
			exp->as.bin.left = left;
			exp->as.bin.right = right;
			return exp;
		}
		}
	} break;
	default: FAILWITH("Unreachable");
	}
	return NULL;
}
