#include "common.h"

KC_PRIVATE const enum asm_register asm_reg_alloc_ord[ASM_REG_COUNT] = {
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

KC_PRIVATE const enum asm_register asm_arg_regs[ASM_ARG_REG_COUNT] = {
	asm_reg_rdi,
	asm_reg_rsi,
	asm_reg_rdx,
	asm_reg_rcx,
	asm_reg_r8,
	asm_reg_r9,
};

KC_PRIVATE const enum asm_register asm_ret_regs[] = {
	asm_reg_rax,
	asm_reg_rdx,
};

KC_PRIVATE const enum asm_register asm_caller_save_regs[] = {
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

KC_PRIVATE const enum asm_register asm_callee_save_regs[] = {
	asm_reg_rbx,
	asm_reg_r12,
	asm_reg_r13,
	asm_reg_r14,
	asm_reg_r15,
};

KC_PRIVATE const char *asm_reg_b_name[ASM_REG_COUNT] = {
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

KC_PRIVATE const char *asm_reg_w_name[ASM_REG_COUNT] = {
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

KC_PRIVATE char *asm_reg_d_name[ASM_REG_COUNT] = {
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

KC_PRIVATE const char *asm_reg_q_name[ASM_REG_COUNT] = {
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

KC_PRIVATE int asm_get_register_owner(struct asm_context *ctx, enum asm_register reg);
KC_PRIVATE bool asm_register_assigned_p(struct asm_context *ctx, enum asm_register reg);
KC_PRIVATE int asm_reserve_register(struct asm_context *ctx, enum asm_register reg, int local);
KC_PRIVATE enum asm_register asm_assign_callee_save_register(struct asm_context *ctx, int local);
KC_PRIVATE enum asm_register asm_emit_move_to_callee_save(struct asm_address *addr, struct asm_procedure *code);
KC_PRIVATE enum asm_register asm_force_reserve_register(struct asm_context *ctx, struct asm_procedure *code,
													enum asm_register reg, int local);
KC_PRIVATE enum asm_register asm_assign_register(struct asm_context *ctx, int local);
KC_PRIVATE void asm_unassign_register(struct asm_context *ctx, enum asm_register reg);

KC_PRIVATE const char *
asm_addr_tag_to_str(enum asm_addr_tag tag)
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

KC_PRIVATE int
count_set_bits(uint64_t n)
{
	int c;
	for (c = 0; n; ++c) n &= n - 1;
	return c;
}

KC_PRIVATE void
mem_copy_segment_count(size_t sz, uint32_t counts[4])
{
	counts[0] = sz / 8;	// 64 bits
	sz %= 8;
	counts[1] = sz / 4; // 32 bits
	sz %= 4;
	counts[2] = sz / 2; // 16 bits
	sz %= 2;
	counts[3] = sz;     // 8 bits
}

KC_PRIVATE const char *
asm_reg_name(enum asm_register reg, KCType *type)
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

UNUSED KC_PRIVATE bool
asm_is_caller_save(enum asm_register reg)
{
	for (size_t i = 0; i < ARRAY_LENGTH(asm_caller_save_regs); ++i) {
		if (reg == asm_caller_save_regs[i]) return true;
	}
	return false;
}

KC_PRIVATE int
asm_suffix(KCType *type)
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

KC_PRIVATE void
asm_context_first_pass(struct ir_blk *blk, struct asm_context *ctx)
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
				addr->type = MEM_ALLOC(KCType);
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
			addr->type = MEM_ALLOC(KCType);
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

KC_PRIVATE struct asm_context *
asm_create_context(struct ir_proc *proc, struct da_pointers *blocks, struct asm_context *ctx)
{
	ctx->varc = proc->regc;
	ctx->vars = calloc(proc->regc, sizeof(struct asm_address));
	ctx->is_leaf = true;
	ctx->arg_stack_size = 16;
	for (int i = 0; i < ASM_REG_COUNT; ++i) {
		ctx->assigned[i] = REG_FREE;
	}
	struct ir_blk *entry = proc->entry;
	KCType *ret_type = proc->def->type->as.proc.ret;
	size_t ret_type_size = type_size(ret_type);
	int arg_reg = ret_type_size <= 16 ? 0 : 1;
	for (size_t arg_num = 0; arg_num < entry->args.len; ++arg_num) {
		KCType *type = proc->node->formals.elems[arg_num].type;
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
		asm_context_first_pass(*blk, ctx);
	}
	assert(ctx->funargs.len == 0);
	return ctx;
}

KC_PRIVATE int
asm_get_register_owner(struct asm_context *ctx, enum asm_register reg)
{
	return ctx->assigned[reg];
}

KC_PRIVATE bool
asm_register_assigned_p(struct asm_context *ctx, enum asm_register reg)
{
	return ctx->assigned[reg] != REG_FREE;
}

KC_PRIVATE int
asm_reserve_register(struct asm_context *ctx, enum asm_register reg, int local)
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

KC_PRIVATE enum asm_register
asm_assign_callee_save_register(struct asm_context *ctx, int local)
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

KC_PRIVATE enum asm_register
asm_emit_move_to_callee_save(struct asm_address *addr, struct asm_procedure *code)
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

KC_PRIVATE enum asm_register
asm_force_reserve_register(struct asm_context *ctx, struct asm_procedure *code, enum asm_register reg, int local)
{
	if (ctx->assigned[reg] == local) return reg;
	if (asm_register_assigned_p(ctx, reg))
		asm_emit_move_to_callee_save(&ctx->vars[ctx->assigned[reg]], code);
	ctx->assigned[reg] = local;
	ctx->clobbered |= 1u << reg;
	return reg;
}

KC_PRIVATE enum asm_register
asm_assign_register(struct asm_context *ctx, int local)
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

KC_PRIVATE void
asm_unassign_register(struct asm_context *ctx, enum asm_register reg)
{
	ctx->assigned[reg] = REG_FREE;
}

KC_PRIVATE bool
can_optimize_red_zone(struct asm_context *ctx)
{
	return ctx->is_leaf && ctx->stack_size <= RED_ZONE_SIZE;
}

KC_PRIVATE const char *
ir_object_link_name(struct ir_toplevel *tl, union ir_object *obj)
{
	static char buf[400];
	if (tl->is_dll && !obj->hddr.is_static && obj->hddr.tag == IRO_EXTERN_DATA) {
		snprintf(buf, sizeof(buf), "\"%s\"@GOTPCREL", obj->hddr.link);
	} else {
		snprintf(buf, sizeof(buf), "\"%s\"", obj->hddr.link);
	}
	return buf;
}

KC_PRIVATE const char *
asm_source_operand_to_str(struct asm_address *src, KCType *type,
						  struct asm_context *ctx, struct ir_toplevel *tl)
{
	KC_PRIVATE char buf[0xff] = {0};
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

KC_PRIVATE void
asm_unassign_blk_arg_register(struct asm_context *ctx, int blk_arg)
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

KC_PRIVATE void
emit_mem_cpy_code(struct asm_procedure *code,
				  const char *dst_name, int64_t dst_offset,
				  const char *src_name, int64_t src_offset,
				  KCType *type)
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

KC_PRIVATE void
emit_op_mov_ADDR_REGISTER__ADDR_STACK(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
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

KC_PRIVATE void
emit_op_load_ADDR_REGISTER__ADDR_STACK(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
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

KC_PRIVATE void
emit_op_load_ADDR_REGISTER__ADDR_REGISTER(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
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

KC_PRIVATE void
emit_op_load_ADDR_WIDE__ADDR_STACK(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
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

KC_PRIVATE void
emit_op_load_ADDR_WIDE__ADDR_REGISTER(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
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

KC_PRIVATE void
emit_op_load_ADDR_WIDE__GLOBL(IR_Ins *ins, void *dat, struct asm_procedure *code)
{
	assert(ins->op == ir_op_loadglobl);
	struct ir_toplevel *tl = dat;
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	union ir_object *obj = &tl->elems[ins->arg.u32];
	KCType *type = ins->type;
	assert(dst->tag == ADDR_WIDE || dst->tag == ADDR_WIDE_ARG || dst->tag == ADDR_BLK_ARG);
	assert(obj->tag == IRO_DATA);
	assert(type->tag == ast_type_slice || type->tag == ast_type_mut_slice);
	assert(type_size(type) == 16);
	if (dst->tag == ADDR_BLK_ARG) {
		dst = &ctx->vars[dst->as.i];
		assert(dst->tag == ADDR_WIDE || dst->tag == ADDR_WIDE_ARG);
	}
	KCType *base_type = type->as.slice;
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

KC_PRIVATE void
emit_op_load_ADDR_REGISTER__GLOBL(IR_Ins *ins, void *dat, struct asm_procedure *code)
{
	assert(ins->op == ir_op_loadglobl);
	struct ir_toplevel *tl = dat;
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	union ir_object *obj = &tl->elems[ins->arg.u32];
	KCType *type = ins->type;
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

KC_PRIVATE void
emit_op_load_ADDR_PUSH_ARG__ADDR_STACK(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
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

KC_PRIVATE void
emit_op_loadimm_ADDR_REGISTER(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
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

KC_PRIVATE void
emit_op_loadimm_ADDR_PUSH_ARG(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
{
	assert(ins->op == ir_op_loadimm);
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	assert(dst->tag == ADDR_PUSH_ARG);
	append_line(&code->body, fmt_str("\tpushq $%d\n", ins->arg.i32));
}

KC_PRIVATE void
emit_op_neg_ADDR_REGISTER__ADDR_IMM_INT(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
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

KC_PRIVATE void
emit_op_cast_ADDR_REGISTER__ADDR_STACK_LOAD(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
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

KC_PRIVATE void
emit_op_cast_ADDR_REGISTER__ADDR_IMM_INT(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
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

KC_PRIVATE void
emit_op_cast_ADDR_REGISTER__ADDR_REGISTER(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
{
	assert(ins->op == ir_op_cast);
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
	assert(dst->tag == ADDR_REGISTER || dst->tag == ADDR_ARGUMENT);
	assert(x->tag == ADDR_REGISTER);
	asm_unassign_register(ctx, x->as.i);
	if (type_is_void(ins->type)) return; /* do nothing */
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

KC_PRIVATE void
emit_op_conslice_ADDR_WIDE__ADDR_REGISTER__ADDR_IMM_INT(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
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

KC_PRIVATE void
emit_op_conslice_ADDR_WIDE__ADDR_REGISTER__ADDR_REGISTER(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
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

KC_PRIVATE void
emit_op_conslice_ADDR_WIDE__ADDR_REGISTER__ADDR_STACK_LOAD(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
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

KC_PRIVATE void
emit_op_conslice_ADDR_WIDE__ADDR_STACK__ADDR_IMM_INT(IR_Ins *ins, UNUSED void *dat, struct asm_procedure *code)
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

KC_PRIVATE void
emit_basic_op_ADDR_REGISTER__ADDR_STACK_LOAD__ADDR_IMM_INT(IR_Ins *ins, void *dat, struct asm_procedure *code)
{
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	struct asm_address *x   = &ctx->vars[ins->arg.rx[0]];
	struct asm_address *y   = &ctx->vars[ins->arg.rx[1]];
	KCType *type = ins->type;
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

KC_PRIVATE void defer_emit_div_mod(IR_Ins *ins, void *dat, struct asm_procedure *code);

KC_PRIVATE void
asm_emit_div_mod(IR_Ins *ins, struct asm_procedure *code, bool remainder_p)
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

KC_PRIVATE void
defer_emit_div_mod(IR_Ins *ins, void *dat, struct asm_procedure *code)
{
	struct asm_context *ctx = &code->ctx;
	struct asm_address *dst = &ctx->vars[ins->dst];
	bool remainder_p = (bool)dat;
	assert(dst->tag == ADDR_ARGUMENT);
	dst->tag = ADDR_REGISTER; // temporarily change tag so that correct path executes.
	asm_emit_div_mod(ins, code, remainder_p);
	dst->tag = ADDR_ARGUMENT;
}

KC_PRIVATE void
asm_emit_basic_op(const char *asm_op, IR_Ins *ins, struct asm_address *dst,
				  struct asm_address *x, struct asm_address *y,
				  struct ir_toplevel *tl, struct asm_procedure *code)
{
	struct asm_context *ctx = &code->ctx;
	KCType *type = ins->type;
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

KC_PRIVATE void
asm_emit_block(struct da_pointers *blocks, size_t blk_id, struct ir_proc *proc,
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
			} else if (MATCH_ADDR2(ADDR_REGISTER, ADDR_REGISTER)) {
				emit_op_cast_ADDR_REGISTER__ADDR_REGISTER(ins, NULL, code);
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
				KCType *base_type = ins->type->as.ptr->as.array.base;
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
					KCType *base_type = ins->type->as.slice;
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
					KCType *base_type = ins->type->as.slice;
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
					KCType *base_type = ins->type->as.slice;
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

KC_PRIVATE void
asm_emit_callee_save_code(size_t offset, struct asm_procedure *code)
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

KC_PRIVATE void
asm_emit_prologue_and_epilogue(struct ir_proc *proc, struct asm_procedure *code)
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

KC_PUBLIC void
asm_emit_procedure(struct ir_proc *proc, struct ir_toplevel *tl, struct asm_procedure *code)
{
	struct da_pointers blks = ir_blk_reverse_post_order(proc->entry);
	asm_create_context(proc, &blks, &code->ctx);
	for (size_t i = 0; i < blks.len; ++i) {
		asm_emit_block(&blks, i, proc, tl, code);
	}
	asm_emit_prologue_and_epilogue(proc, code);
	da_free(&blks);
}

KC_PUBLIC void
asm_emit_datum(struct ir_data *data, struct asm_datum *asm_data)
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

KC_PRIVATE void
dump_lines(struct lines *asm_lines, FILE *file)
{
	da_foreach(line, asm_lines) {
		fputs(*line, file);
	}
}

KC_PUBLIC void
asm_dump_procedure(struct asm_procedure *proc, FILE *file)
{
	dump_lines(&proc->prologue, file);
	dump_lines(&proc->body, file);
	dump_lines(&proc->epilogue, file);
}

KC_PRIVATE void
asm_dump_data(struct asm_datum *data, FILE *file)
{
	dump_lines(&data->body, file);
}


KC_PUBLIC void
asm_dump_module(struct asm_module *mod, FILE *file)
{
	for (size_t i = 0; i < mod->data.len; ++i) {
		asm_dump_data(&mod->data.elems[i], file);
	}
	for (size_t i = 0; i < mod->procs.len; ++i) {
		asm_dump_procedure(&mod->procs.elems[i], file);
	}
}
