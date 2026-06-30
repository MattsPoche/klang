#include <dlfcn.h>
#include "common.h"

KC_PRIVATE const enum asm_register cg_reg_alloc_ord[CG_REG_COUNT] = {
    RDI,
    RSI,
    RCX,
    RAX,
    RDX,
    R8,
    R9,
    R10,
    R11,
    RBX,
    R12,
    R13,
    R14,
    R15,
};

KC_PRIVATE const enum asm_register cg_arg_regs[CG_ARG_REG_COUNT] = {
    RDI,
    RSI,
    RDX,
    RCX,
    R8,
    R9,
};

KC_PRIVATE const enum asm_register cg_ret_regs[] = {
    RAX,
    RDX,
};

KC_PRIVATE const enum asm_register cg_caller_save_regs[] = {
    RAX,
    RDI,
    RSI,
    RDX,
    RCX,
    R8,
    R9,
    R10,
    R11,
};

KC_PRIVATE const enum asm_register cg_callee_save_regs[] = {
    RBX,
    R12,
    R14,
    R15,
};

UNUSED KC_PRIVATE const char *cg_reg_b_name[16] = {
    [RAX] = "%al",
    [RDX] = "%dl",
    [RDI] = "%dil",
    [RSI] = "%sil",
    [RCX] = "%cl",
    [R8]  = "%r8b",
    [R9]  = "%r9b",
    [R10] = "%r10b",
    [R11] = "%r11b",
    [RBX] = "%bl",
    [R12] = "%r12b",
    [R13] = "%r13b",
    [R14] = "%r14b",
    [R15] = "%r15b",
};

UNUSED KC_PRIVATE const char *cg_reg_w_name[16] = {
    [RAX] = "%ax",
    [RDX] = "%dx",
    [RDI] = "%di",
    [RSI] = "%si",
    [RCX] = "%cx",
    [R8]  = "%r8w",
    [R9]  = "%r9w",
    [R10] = "%r10w",
    [R11] = "%r11w",
    [RBX] = "%bx",
    [R12] = "%r12w",
    [R13] = "%r13w",
    [R14] = "%r14w",
    [R15] = "%r15w"
};

UNUSED KC_PRIVATE char *cg_reg_d_name[16] = {
    [RAX] = "%eax",
    [RDX] = "%edx",
    [RDI] = "%edi",
    [RSI] = "%esi",
    [RCX] = "%ecx",
    [R8]  = "%r8d",
    [R9]  = "%r9d",
    [R10] = "%r10d",
    [R11] = "%r11d",
    [RBX] = "%ebx",
    [R12] = "%r12d",
    [R13] = "%r13d",
    [R14] = "%r14d",
    [R15] = "%r15d",
};

KC_PRIVATE const char *cg_reg_q_name[16] = {
    [RAX] = "%rax",
    [RDX] = "%rdx",
    [RDI] = "%rdi",
    [RSI] = "%rsi",
    [RCX] = "%rcx",
    [R8]  = "%r8",
    [R9]  = "%r9",
    [R10] = "%r10",
    [R11] = "%r11",
    [RBX] = "%rbx",
    [R12] = "%r12",
    [R13] = "%r13",
    [R14] = "%r14",
    [R15] = "%r15",
};

KC_PRIVATE int cg_get_register_owner(struct cg_context *ctx, enum asm_register reg);
KC_PRIVATE bool cg_register_assigned_p(struct cg_context *ctx, enum asm_register reg);
KC_PRIVATE int cg_reserve_register(struct cg_context *ctx, enum asm_register reg, int local);
KC_PRIVATE enum asm_register cg_assign_callee_save_register(struct cg_context *ctx, int local);
KC_PRIVATE enum asm_register cg_emit_move_to_callee_save(struct cg_address *addr, struct cg_procedure *code);
KC_PRIVATE enum asm_register cg_force_reserve_register(struct cg_context *ctx, struct cg_procedure *code,
                                                       enum asm_register reg, int local);
KC_PRIVATE enum asm_register cg_assign_register(struct cg_context *ctx, int local);
KC_PRIVATE void cg_unassign_register(struct cg_context *ctx, enum asm_register reg);

KC_PRIVATE const char *
cg_addr_tag_to_str(enum cg_addr_tag tag)
{
    switch (tag) {
    case ADDR_NONE:          return "ADDR_NONE";
    case ADDR_ARGUMENT:      return "ADDR_ARGUMENT";
    case ADDR_WIDE:          return "ADDR_WIDE";
    case ADDR_WIDE_ARG:      return "ADDR_WIDE_ARG";
    case ADDR_TEMP_WIDE:  return "ADDR_TEMP_WIDE";
    case ADDR_STACK_ARG:  return "ADDR_STACK_ARG";
    case ADDR_PUSH_ARG:      return "ADDR_PUSH_ARG";
    case ADDR_REGISTER:      return "ADDR_REGISTER";
    case ADDR_TEMP_REG:      return "ADDR_TEMP_REG";
    case ADDR_IMM_INT:      return "ADDR_IMM_INT";
    case ADDR_IMM_FLOAT:  return "ADDR_IMM_FLOAT";
    case ADDR_STACK:      return "ADDR_STACK";
    case ADDR_STACK_LOAD: return "ADDR_STACK_LOAD";
    case ADDR_BLK_ARG:      return "ADDR_BLK_ARG";
    case ADDR_SYMBOL:      return "ADDR_SYMBOL";
    case ADDR_FLAGS:      return "ADDR_FLAGS";
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
    counts[0] = sz / 8;    // 64 bits
    sz %= 8;
    counts[1] = sz / 4; // 32 bits
    sz %= 4;
    counts[2] = sz / 2; // 16 bits
    sz %= 2;
    counts[3] = sz;     // 8 bits
}

UNUSED KC_PRIVATE bool
cg_is_caller_save(enum asm_register reg)
{
    for (size_t i = 0; i < ARRAY_LENGTH(cg_caller_save_regs); ++i) {
        if (reg == cg_caller_save_regs[i]) return true;
    }
    return false;
}

KC_PRIVATE enum asm_op_size
cg_suffix(KCType *type)
{
    assert(type_is_floating_point(type) == false);
    switch (type_size(type)) {
    case 1: return ZB;
    case 2: return ZW;
    case 4: return ZD;
    case 8: return ZQ;
    }
    FAILWITH("TODO: invalid size.");
    return 0;
}

KC_PRIVATE void
cg_context_first_pass(struct ir_blk *blk, struct cg_context *ctx, CG_module *mod)
{
    blk->asm_label = asm_make_label(&mod->as, fmt_str(".L%p", blk), ASM_LBL_T_BLOCK, ASM_LBL_B_LOCAL);
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
            struct cg_address *addr = &ctx->vars[ins->dst];
            assert(addr->tag == ADDR_NONE);
            addr->tag = ADDR_TEMP_REG;
            addr->type = ctx->vars[ins->arg.rx[0]].type;
        } break;
        case ir_op_undefined: {
            struct cg_address *addr = &ctx->vars[ins->dst];
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
            struct cg_address *addr = &ctx->vars[ins->dst];
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
            struct cg_address *addr = &ctx->vars[ins->dst];
            assert(addr->tag == ADDR_NONE);
            addr->tag = ADDR_FLAGS;
            addr->i = ins->op;
            addr->type = ins->type;
        } break;
        case ir_op_cast: {
            struct cg_address *addr = &ctx->vars[ins->dst];
            assert(addr->tag == ADDR_NONE);
            addr->tag = ADDR_TEMP_REG;
            addr->type = ins->type;
        } break;
        case ir_op_getelemptr: {
            struct cg_address *dst = &ctx->vars[ins->dst];
            assert(dst->tag == ADDR_NONE);
            dst->tag = ADDR_TEMP_REG;
            dst->type = ins->type;
        } break;
        case ir_op_retval: {
            struct cg_address *addr = &ctx->vars[ins->dst];
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
                addr->stack[0] = -ctx->stack_size;
                addr->stack[1] = 0;
                size_t after = ctx->stack_size;
                addr->extra.stack_size = after - before;
                addr->type = MEM_ALLOC(KCType);
                addr->type->tag = ast_type_ptr;
                addr->type->ptr = ins->type;
            }
        } break;
        case ir_op_conslice: {
            struct cg_address *addr = &ctx->vars[ins->dst];
            assert(addr->tag == ADDR_NONE);
            addr->tag = ADDR_TEMP_WIDE;
            addr->type = ins->type;
        } break;
        case ir_op_alloca: {
            struct cg_address *addr = &ctx->vars[ins->dst];
            assert(addr->tag == ADDR_NONE);
            addr->tag = ADDR_STACK;
            int sz = type_size(ins->type);
            size_t before = ctx->stack_size;
            ctx->stack_size = align_adjust(ctx->stack_size, type_alignment(ins->type));
            ctx->stack_size += sz * ins->arg.i32;
            addr->i = -ctx->stack_size;
            size_t after = ctx->stack_size;
            addr->extra.stack_size = after - before;
            addr->type = MEM_ALLOC(KCType);
            addr->type->tag = ast_type_ptr;
            addr->type->ptr = ins->type;
        } break;
        case ir_op_load: {
            struct cg_address *dst = &ctx->vars[ins->dst];
            struct cg_address *ptr = &ctx->vars[ins->arg.rx[0]];
            assert(dst->tag == ADDR_NONE);
            if (ptr->tag == ADDR_STACK) {
                dst->tag = ADDR_STACK_LOAD;
                dst->stack[0] = ptr->i;
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
            struct cg_address *addr = &ctx->vars[ins->dst];
            assert(addr->tag == ADDR_NONE);
            addr->tag = ADDR_SYMBOL;
            addr->i = ins->arg.u32;
            addr->type = ins->type;
        } break;
        case ir_op_loadconst: FAILWITH("TODO: ir_op_loadconst"); break;
        case ir_op_loadimm: {
            struct cg_address *addr = &ctx->vars[ins->dst];
            assert(addr->tag == ADDR_NONE);
            addr->tag = ADDR_IMM_INT;
            addr->i = ins->arg.i32;
            addr->type = ins->type;
        } break;
        case ir_op_copy: /* do nothing */ break;
        case ir_op_store: {
            struct cg_address *dst = &ctx->vars[ins->dst];
            struct cg_address *src = &ctx->vars[ins->arg.rx[0]];
            if (dst->tag == ADDR_STACK && src->tag == ADDR_STACK_ARG) {
                ctx->stack_size -= dst->extra.stack_size;
                dst->i = src->i;
                dst->extra.stack_size = 0;
            }
        } break;
        case ir_op_memzero: /* do nothing */ break;
        case ir_op_pushfunarg: {
            struct cg_address *dst = &ctx->vars[ins->dst];
            assert(dst->tag != ADDR_NONE);
            if (dst->type == NULL) {
                printf("ins->dst = %%%d\n", ins->dst);
            }
            da_append(&ctx->funargs, dst);
        } break;
        case ir_op_call: {
            ctx->is_leaf = false;
            struct cg_address *proc = &ctx->vars[ins->arg.rx[0]];
            assert(proc->tag != ADDR_NONE);
            size_t ts = type_size(ins->type);
            struct cg_address *ret = &ctx->vars[ins->dst];
            ret->type = ins->type;
            /* return register */
            int arg_reg = 0;
            assert(ret->tag == ADDR_NONE);
            if (ts <= 8) {
                ret->tag = ADDR_REGISTER;
                ret->i = cg_ret_regs[0];
            } else if (ts <= 16) {
                ret->tag = ADDR_WIDE;
                ret->wide[0] = cg_ret_regs[0];
                ret->wide[1] = cg_ret_regs[1];
            } else {
                assert(i+1 < blk->code.len);
                IR_Ins *ins_sto = &blk->code.elems[i+1];
                assert(ins_sto->op == ir_op_store);
                struct cg_address *dst = &ctx->vars[ins_sto->dst];
                ret->tag = ADDR_STACK;
                ret->i = dst->i;
                ret->extra.stack_size = dst->extra.stack_size;
                ret->type = dst->type;
                /* Note: we set arg_reg to 1 so that we can pass the ptr to return value
                 * in register according to abi.
                 */
                arg_reg = 1;
            }
            int argc = ins->arg.rx[1];
            while (argc--) {
                struct cg_address *arg = da_pop(&ctx->funargs);
                assert(arg->tag != ADDR_NONE);
                size_t ts = type_size(arg->type);
                if (ts <= 8 && arg_reg < CG_ARG_REG_COUNT) {
                    arg->tag = ADDR_ARGUMENT;
                    arg->i = cg_arg_regs[arg_reg++];
                } else if (ts <= 16 && arg_reg < CG_ARG_REG_COUNT - 1) {
                    arg->tag = ADDR_WIDE_ARG;
                    arg->wide[0] = cg_arg_regs[arg_reg++];
                    arg->wide[1] = cg_arg_regs[arg_reg++];
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
            struct cg_address *addr = &ctx->vars[term->args.elems[0]];
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
                addr->i = cg_ret_regs[0];
            } else if (addr->tag == ADDR_REGISTER) {
                assert(addr->i == cg_ret_regs[0]);
            } else if (addr->tag == ADDR_TEMP_WIDE) {
                addr->tag = ADDR_WIDE;
                addr->wide[0] = cg_ret_regs[0];
                addr->wide[1] = cg_ret_regs[1];
            } else if (addr->tag == ADDR_WIDE) {
                if ((enum asm_register)addr->wide[0] == cg_ret_regs[0]
                    && (enum asm_register)addr->wide[1] == cg_ret_regs[1]) {
                    /* do nothing */
                } else {
                    FAILWITH("TODO:\n"
                             "addr->wide[0] = %s\n"
                             "addr->wide[1] = %s",
                             cg_reg_q_name[addr->wide[0]],
                             cg_reg_q_name[addr->wide[1]]);
                }
            } else if (addr->tag == ADDR_SYMBOL) {
                /* do nothing? */
            } else {
                FAILWITH("TODO: %%%d (addr->tag == %s)",
                         term->args.elems[0],
                         cg_addr_tag_to_str(addr->tag));
            }
        } else {
            assert(term->args.len == 0);
        }
    } break;
    case ir_op_goto: {
        for (size_t i = 0; i < term->args.len; ++i) {
            struct cg_address *addr = &ctx->vars[term->args.elems[i]];
            struct cg_address *formal = &ctx->vars[term->b0->args.elems[i]];
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
            addr->i = term->b0->args.elems[i];
        }
    } break;
    case ir_op_if: {
        /* assert(term->args.len == 1); */
        /* struct cg_address *addr = &ctx->vars[term->args.elems[0]]; */
        /* assert(addr->tag == ADDR_FLAGS); */
    } break;
    case ir_op_tailcall: FAILWITH("TODO: ir_op_tailcall"); break;
    }
}

KC_PRIVATE struct cg_context *
cg_create_context(IR_object *obj,
                  struct da_pointers *blocks,
                  struct cg_context *ctx,
                  CG_module *mod)
{
    assert(obj->tag == IRO_PROC || obj->tag == IRO_INIT_THUNK);
    struct ir_proc *proc = &obj->proc;
    ctx->varc = proc->regc;
    ctx->vars = MEM_ALLOC_ARRAY(struct cg_address, proc->regc);
    ctx->is_leaf = true;
    ctx->arg_stack_size = 16;
    ctx->ret_lbl = asm_make_label(&mod->as,
                                  fmt_str(".LR"SV_FMT, SV_ARGS(token_to_strview(proc->def->id))),
                                  ASM_LBL_T_BLOCK,
                                  ASM_LBL_B_LOCAL);
    ctx->proc_lbl = proc->asm_label;
    for (int i = 0; i < CG_REG_COUNT; ++i) {
        ctx->assigned[i] = REG_FREE;
    }
    struct ir_blk *entry = proc->entry;
    if (obj->tag == IRO_PROC) {
        KCType *ret_type = proc->def->type->proc.ret;
        size_t ret_type_size = type_size(ret_type);
        int arg_reg = ret_type_size <= 16 ? 0 : 1;
        for (size_t arg_num = 0; arg_num < entry->args.len; ++arg_num) {
            KCType *type = proc->node->formals.elems[arg_num].type;
            size_t ts = type_size(type);
            size_t ta = type_alignment(type);
            if (ta < 8) ta = 8;
            int x = entry->args.elems[arg_num];
            ctx->vars[x].type = type;
            if (ts <= 8 && arg_reg < CG_ARG_REG_COUNT) {
                ctx->vars[x].tag = ADDR_REGISTER;
                ctx->vars[x].i = cg_arg_regs[arg_reg++];
            } else if (ts <= 16 && arg_reg < CG_ARG_REG_COUNT - 1) {
                ctx->vars[x].tag = ADDR_WIDE;
                ctx->vars[x].wide[0] = cg_arg_regs[arg_reg++];
                ctx->vars[x].wide[1] = cg_arg_regs[arg_reg++];
            } else {
                ctx->vars[x].tag = ADDR_STACK_ARG;
                ctx->vars[x].i = ctx->arg_stack_size;
                ctx->arg_stack_size += ts;
                ctx->arg_stack_size = align_adjust(ctx->arg_stack_size, ta);
            }
        }
    }
    for (size_t i = 0; i < blocks->len; ++i) {
        cg_context_first_pass(blocks->elems[i], ctx, mod);
    }
    assert(ctx->funargs.len == 0);
    return ctx;
}

KC_PRIVATE int
cg_get_register_owner(struct cg_context *ctx, enum asm_register reg)
{
    return ctx->assigned[reg];
}

KC_PRIVATE bool
cg_register_assigned_p(struct cg_context *ctx, enum asm_register reg)
{
    return ctx->assigned[reg] != REG_FREE;
}

KC_PRIVATE int
cg_reserve_register(struct cg_context *ctx, enum asm_register reg, int local)
{
    if (ctx->assigned[reg] == local) return reg;
    if (cg_register_assigned_p(ctx, reg)) {
        FAILWITH("Register not free:\n"
                 "Trying to assign %s to %%%d\n"
                 "%s assigned to %%%d\n",
                 cg_reg_q_name[reg],
                 local,
                 cg_reg_q_name[reg],
                 ctx->assigned[reg]);
    }
    ctx->assigned[reg] = local;
    ctx->clobbered |= 1u << reg;
    return reg;
}

KC_PRIVATE enum asm_register
cg_assign_callee_save_register(struct cg_context *ctx, int local)
{
    enum asm_register reg;
    size_t i = 0;
    while (i < ARRAY_LENGTH(cg_callee_save_regs)
           && cg_register_assigned_p(ctx, reg = cg_callee_save_regs[i])) {
        i++;
    }
    assert(i < ARRAY_LENGTH(cg_callee_save_regs));
    cg_reserve_register(ctx, reg, local);
    return reg;
}

KC_PRIVATE enum asm_register
cg_emit_move_to_callee_save(struct cg_address *addr, struct cg_procedure *code)
{
    if (addr->tag != ADDR_REGISTER) FAILWITH("addr->tag = %s", cg_addr_tag_to_str(addr->tag));
    struct cg_context *ctx = &code->ctx;
    int local = ctx->assigned[addr->i];
    enum asm_register reg = cg_assign_callee_save_register(ctx, local);
    asm_mov(code->m, ZQ, REG(addr->i), REG(reg));
    cg_unassign_register(ctx, addr->i);
    ctx->vars[local].i = reg;
    return reg;
}

KC_PRIVATE enum asm_register
cg_force_reserve_register(struct cg_context *ctx, struct cg_procedure *code, enum asm_register reg, int local)
{
    if (ctx->assigned[reg] == local) return reg;
    if (cg_register_assigned_p(ctx, reg))
        cg_emit_move_to_callee_save(&ctx->vars[ctx->assigned[reg]], code);
    ctx->assigned[reg] = local;
    ctx->clobbered |= 1u << reg;
    return reg;
}

KC_PRIVATE enum asm_register
cg_assign_register(struct cg_context *ctx, int local)
{
    enum asm_register reg;
    size_t i = 0;
    while (i < ARRAY_LENGTH(cg_reg_alloc_ord)
           && cg_register_assigned_p(ctx, reg = cg_reg_alloc_ord[i])) {
        i++;
    }
    assert(i < ARRAY_LENGTH(cg_reg_alloc_ord));
    cg_reserve_register(ctx, reg, local);
    return reg;
}

KC_PRIVATE void
cg_unassign_register(struct cg_context *ctx, enum asm_register reg)
{
    ctx->assigned[reg] = REG_FREE;
}

KC_PRIVATE bool
can_optimize_red_zone(struct cg_context *ctx)
{
    return ctx->is_leaf && ctx->stack_size <= RED_ZONE_SIZE;
}

KC_PRIVATE void
cg_unassign_blk_arg_register(struct cg_context *ctx, int blk_arg)
{
    struct cg_address *b = &ctx->vars[blk_arg];
    assert(b->tag == ADDR_BLK_ARG);
    struct cg_address *a = &ctx->vars[b->i];
    if (a->tag == ADDR_REGISTER || a->tag == ADDR_ARGUMENT) {
        cg_unassign_register(ctx, a->i);
    } else if (a->tag == ADDR_BLK_ARG) {
        cg_unassign_blk_arg_register(ctx, b->i);
    } else if (a->tag == ADDR_WIDE || a->tag == ADDR_WIDE_ARG) {
        cg_unassign_register(ctx, a->wide[0]);
        cg_unassign_register(ctx, a->wide[1]);
    } else if (a->tag == ADDR_STACK) {
    } else if (a->tag == ADDR_STACK_LOAD) {
    } else {
        FAILWITH("TODO: %s", cg_addr_tag_to_str(a->tag));
    }
}

#define BEGIN_MATCH2(v0, v1)                    \
    const struct cg_address *_v0 = v0;          \
    const struct cg_address *_v1 = v1

#define MATCH_ADDR3(_a0, _a1, _a2)                  \
    (ctx->vars[ins->dst].tag == (_a0)               \
     &&    ctx->vars[ins->arg.rx[0]].tag == (_a1)   \
     &&    ctx->vars[ins->arg.rx[1]].tag == (_a2))

#define MATCH_ADDR2(_a0, _a1) (_v0->tag == (_a0) && _v1->tag == (_a1))

KC_PRIVATE void
emit_mem_cpy_code(struct cg_procedure *code,
                  enum asm_register dst, int64_t dst_offset,
                  enum asm_register src, int64_t src_offset,
                  KCType *type)
{
    struct cg_context *ctx = &code->ctx;
    enum asm_register tmp = cg_assign_register(ctx, 0);
    int64_t offset = 0;
    uint32_t segcnts[4] = {0};
    mem_copy_segment_count(type_size(type), segcnts);
    for (; segcnts[0]--; offset += 8) {
        /* movq src_offset + offset(src), tmp */
        asm_mov(code->m, ZQ, MEM_DR(src_offset + offset, src), REG(tmp));
        /* movq tmp, dst_offset + offset(dst) */
        asm_mov(code->m, ZQ, REG(tmp), MEM_DR(dst_offset + offset, dst));
    }
    for (; segcnts[1]--; offset += 4) {
        asm_mov(code->m, ZD, MEM_DR(src_offset + offset, src), REG(tmp));
        asm_mov(code->m, ZD, REG(tmp), MEM_DR(dst_offset + offset, dst));
    }
    for (; segcnts[2]--; offset += 2) {
        asm_mov(code->m, ZW, MEM_DR(src_offset + offset, src), REG(tmp));
        asm_mov(code->m, ZW, REG(tmp), MEM_DR(dst_offset + offset, dst));
    }
    for (; segcnts[3]--; offset += 1) {
        asm_mov(code->m, ZB, MEM_DR(src_offset + offset, src), REG(tmp));
        asm_mov(code->m, ZB, REG(tmp), MEM_DR(dst_offset + offset, dst));
    }
    cg_unassign_register(ctx, tmp);
}

KC_PRIVATE void
emit_op_mov_ADDR_REGISTER__ADDR_STACK(IR_Ins *ins, UNUSED void *dat, struct cg_procedure *code)
{
    assert(ins->op == ir_op_mov);
    struct cg_context *ctx = &code->ctx;
    struct cg_address *dst = &ctx->vars[ins->dst];
    struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
    assert(dst->tag == ADDR_REGISTER || dst->tag == ADDR_ARGUMENT);
    assert(x->tag == ADDR_STACK);
    cg_reserve_register(ctx, dst->i, ins->dst);
    asm_lea(code->m, ZQ, MEM_DR(x->i, RBP), REG(dst->i));
}

KC_PRIVATE void
emit_op_load_ADDR_REGISTER__ADDR_STACK(IR_Ins *ins, UNUSED void *dat, struct cg_procedure *code)
{
    assert(ins->op == ir_op_load);
    struct cg_context *ctx = &code->ctx;
    struct cg_address *dst = &ctx->vars[ins->dst];
    struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
    assert(dst->tag == ADDR_REGISTER || dst->tag == ADDR_ARGUMENT);
    if (x->tag == ADDR_BLK_ARG) x = &ctx->vars[x->i];
    assert(x->tag == ADDR_STACK);
    if (dst->tag == ADDR_ARGUMENT)
        cg_force_reserve_register(ctx, code, dst->i, ins->dst);
    else
        cg_reserve_register(ctx, dst->i, ins->dst);
    asm_mov(code->m, cg_suffix(ins->type), MEM_DR(x->i, RBP), REG(dst->i));
}

KC_PRIVATE void
emit_op_load_ADDR_REGISTER__ADDR_REGISTER(IR_Ins *ins, UNUSED void *dat, struct cg_procedure *code)
{
    assert(ins->op == ir_op_load);
    struct cg_context *ctx = &code->ctx;
    struct cg_address *dst = &ctx->vars[ins->dst];
    struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
    assert(dst->tag == ADDR_REGISTER || dst->tag == ADDR_ARGUMENT);
    assert(x->tag == ADDR_REGISTER);
    cg_unassign_register(ctx, x->i);
    if (cg_register_assigned_p(ctx, dst->i) && dst->i != x->i) {
        struct cg_address *addr = &ctx->vars[ctx->assigned[dst->i]];
        cg_emit_move_to_callee_save(addr, code);
    }
    cg_reserve_register(ctx, dst->i, ins->dst);
    asm_mov(code->m, cg_suffix(ins->type), MEM_DR(0, x->i), REG(dst->i));
}

KC_PRIVATE void
emit_op_load_ADDR_WIDE__ADDR_STACK(IR_Ins *ins, UNUSED void *dat, struct cg_procedure *code)
{
    assert(ins->op == ir_op_load);
    struct cg_context *ctx = &code->ctx;
    struct cg_address *dst = &ctx->vars[ins->dst];
    struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
    assert(dst->tag == ADDR_WIDE || dst->tag == ADDR_WIDE_ARG);
    assert(x->tag == ADDR_STACK);
    size_t ts = type_size(ins->type);
    if (ts == 16) {
        cg_reserve_register(ctx, dst->wide[0], ins->dst);
        cg_reserve_register(ctx, dst->wide[1], ins->dst);
        asm_mov(code->m, ZQ, MEM_DR(x->i, RBP), REG(dst->wide[0]));
        asm_mov(code->m, ZQ, MEM_DR(x->i + 8, RBP), REG(dst->wide[1]));
    } else {
        FAILWITH("TODO");
    }
}

KC_PRIVATE void
emit_op_load_ADDR_WIDE__ADDR_REGISTER(IR_Ins *ins, UNUSED void *dat, struct cg_procedure *code)
{
    assert(ins->op == ir_op_load);
    struct cg_context *ctx = &code->ctx;
    struct cg_address *dst = &ctx->vars[ins->dst];
    struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
    assert(dst->tag == ADDR_WIDE || dst->tag == ADDR_WIDE_ARG);
    assert(x->tag == ADDR_REGISTER);
    size_t ts = type_size(ins->type);
    if (ts == 16) {
        cg_unassign_register(ctx, x->i);
        cg_reserve_register(ctx, dst->wide[0], ins->dst);
        cg_reserve_register(ctx, dst->wide[1], ins->dst);
        if (dst->wide[0] == x->i || dst->wide[1] == x->i) {
            enum asm_register tmp = cg_assign_register(ctx, ins->dst);
            asm_mov(code->m, ZQ, REG(x->i), REG(tmp));
            asm_mov(code->m, ZQ, MEM_DR(0, tmp), REG(dst->wide[0]));
            asm_mov(code->m, ZQ, MEM_DR(8, tmp), REG(dst->wide[1]));
            cg_unassign_register(ctx, tmp);
        } else {
            asm_mov(code->m, ZQ, MEM_DR(0, x->i), REG(dst->wide[0]));
            asm_mov(code->m, ZQ, MEM_DR(8, x->i), REG(dst->wide[1]));
        }
    } else {
        FAILWITH("TODO");
    }
}

KC_PRIVATE void
emit_op_load_ADDR_WIDE__GLOBL(IR_Ins *ins, void *dat, struct cg_procedure *code)
{
    assert(ins->op == ir_op_loadglobl);
    struct ir_toplevel *tl = dat;
    struct cg_context *ctx = &code->ctx;
    struct cg_address *dst = &ctx->vars[ins->dst];
    IR_object *obj = &tl->elems[ins->arg.u32];
    KCType *type = ins->type;
    assert(dst->tag == ADDR_WIDE || dst->tag == ADDR_WIDE_ARG || dst->tag == ADDR_BLK_ARG);
    assert(obj->tag == IRO_DATA);
    assert(type->tag == ast_type_slice || type->tag == ast_type_mut_slice);
    assert(type_size(type) == 16);
    if (dst->tag == ADDR_BLK_ARG) {
        dst = &ctx->vars[dst->i];
        assert(dst->tag == ADDR_WIDE || dst->tag == ADDR_WIDE_ARG);
    }
    KCType *base_type = type->slice;
    cg_force_reserve_register(ctx, code, dst->wide[0], ins->dst);
    cg_force_reserve_register(ctx, code, dst->wide[1], ins->dst);
    switch (obj->hddr.tag) {
    case IRO_PROC: FAILWITH("TODO: IRO_PROC"); break;
    case IRO_DATA:
        asm_lea(code->m, ZQ, MEM_LR(obj->hddr.asm_label, 0, RIP), REG(dst->wide[0]));
        break;
    case IRO_EXTERN_DATA: FAILWITH("TODO: IRO_EXTERN_DATA"); break;
    case IRO_EXTERN_PROC: FAILWITH("TODO: IRO_EXTERN_PROC"); break;
    case IRO_INIT_THUNK:  FAILWITH("TODO: IRO_INIT_THUNK");  break;
    default: FAILWITH("Unreachable"); break;
    }
    asm_mov(code->m, ZQ, INT(obj->data.size / type_size(base_type)), REG(dst->wide[1]));
}

KC_PRIVATE void
emit_op_load_ADDR_REGISTER__GLOBL(IR_Ins *ins, void *dat, struct cg_procedure *code)
{
    assert(ins->op == ir_op_loadglobl);
    struct ir_toplevel *tl = dat;
    struct cg_context *ctx = &code->ctx;
    struct cg_address *dst = &ctx->vars[ins->dst];
    IR_object *obj = &tl->elems[ins->arg.u32];
    KCType *type = ins->type;
    assert(dst->tag == ADDR_REGISTER || dst->tag == ADDR_ARGUMENT);
    assert(type->tag != ast_type_array);
    cg_reserve_register(ctx, dst->i, ins->dst);
    switch (obj->hddr.tag) {
    case IRO_PROC: FAILWITH("TODO: IRO_PROC"); break;
    case IRO_DATA: {
        asm_mov(code->m, cg_suffix(type), MEM_LR(obj->hddr.asm_label, 0, RIP), REG(dst->i));
    } break;
    case IRO_EXTERN_DATA: {
        if (code->m->is_jit) {
            /* asm_mov(code->m, cg_suffix(type), MEM_LR(obj->hddr.asm_label, 0, RIP), REG(dst->i)); */
            asm_mov(code->m, cg_suffix(type), LABEL(obj->hddr.asm_label, 0), REG(dst->i));
            asm_mov(code->m, cg_suffix(type), MEM_DR(0, dst->i), REG(dst->i));
        } else {
            asm_mov(code->m, cg_suffix(type), MEM_LR(obj->hddr.asm_label, 0, RIP), REG(dst->i));
        }
    } break;
    case IRO_EXTERN_PROC: FAILWITH("TODO: IRO_EXTERN_PROC"); break;
    case IRO_INIT_THUNK:  FAILWITH("TODO: IRO_INIT_THUNK");  break;
    default: FAILWITH("Unreachable"); break;
    }
}

KC_PRIVATE void
emit_op_load_ADDR_PUSH_ARG__ADDR_STACK(IR_Ins *ins, UNUSED void *dat, struct cg_procedure *code)
{
    assert(ins->op == ir_op_load);
    struct cg_context *ctx = &code->ctx;
    struct cg_address *dst = &ctx->vars[ins->dst];
    struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
    assert(dst->tag == ADDR_PUSH_ARG);
    assert(x->tag == ADDR_STACK);
    size_t ts = type_size(ins->type);
    uint32_t counts[4] = {0};
    mem_copy_segment_count(ts, counts);
    int tmp = cg_assign_register(ctx, ins->dst);
    int64_t offset = 0;
    asm_sub(code->m, ZQ, INT(align_adjust(ts, 8)), REG(RSP));
    for (uint32_t i = 0; i < counts[0]; ++i) {
        asm_mov(code->m, ZQ, MEM_DR(x->i + offset, RBP), REG(tmp));
        asm_mov(code->m, ZQ, REG(tmp), MEM_DR(offset, RSP));
        offset += 8;
    }
    for (uint32_t i = 0; i < counts[1]; ++i) {
        asm_mov(code->m, ZD, MEM_DR(x->i + offset, RBP), REG(tmp));
        asm_mov(code->m, ZD, REG(tmp), MEM_DR(offset, RSP));
        offset += 4;
    }
    for (uint32_t i = 0; i < counts[2]; ++i) {
        asm_mov(code->m, ZW, MEM_DR(x->i + offset, RBP), REG(tmp));
        asm_mov(code->m, ZW, REG(tmp), MEM_DR(offset, RSP));
        offset += 2;
    }
    for (uint32_t i = 0; i < counts[3]; ++i) {
        asm_mov(code->m, ZB, MEM_DR(x->i + offset, RBP), REG(tmp));
        asm_mov(code->m, ZB, REG(tmp), MEM_DR(offset, RSP));
        offset += 1;
    }
    cg_unassign_register(ctx, tmp);
}

KC_PRIVATE void
emit_op_loadimm_ADDR_REGISTER(IR_Ins *ins, UNUSED void *dat, struct cg_procedure *code)
{
    assert(ins->op == ir_op_loadimm);
    struct cg_context *ctx = &code->ctx;
    struct cg_address *dst = &ctx->vars[ins->dst];
    assert(dst->tag == ADDR_REGISTER || dst->tag == ADDR_ARGUMENT);
    cg_reserve_register(ctx, dst->i, ins->dst);
    asm_mov(code->m, cg_suffix(ins->type), INT(ins->arg.i32), REG(dst->i));
}

KC_PRIVATE void
emit_op_loadimm_ADDR_PUSH_ARG(IR_Ins *ins, UNUSED void *dat, struct cg_procedure *code)
{
    assert(ins->op == ir_op_loadimm);
    struct cg_context *ctx = &code->ctx;
    struct cg_address *dst = &ctx->vars[ins->dst];
    assert(dst->tag == ADDR_PUSH_ARG);
    asm_push(code->m, ZQ, INT(ins->arg.i32));
}

KC_PRIVATE void
emit_op_neg_ADDR_REGISTER__ADDR_IMM_INT(IR_Ins *ins, UNUSED void *dat, struct cg_procedure *code)
{
    assert(ins->op == ir_op_neg);
    struct cg_context *ctx = &code->ctx;
    struct cg_address *dst = &ctx->vars[ins->dst];
    struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
    assert(dst->tag == ADDR_REGISTER || dst->tag == ADDR_ARGUMENT);
    assert(x->tag == ADDR_IMM_INT);
    cg_reserve_register(ctx, dst->i, ins->dst);
    asm_mov(code->m, cg_suffix(ins->type), INT(-x->i), REG(dst->i));
}

KC_PRIVATE void
emit_op_cast_ADDR_REGISTER__ADDR_STACK_LOAD(IR_Ins *ins, UNUSED void *dat, struct cg_procedure *code)
{
    assert(ins->op == ir_op_cast);
    struct cg_context *ctx = &code->ctx;
    struct cg_address *dst = &ctx->vars[ins->dst];
    struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
    assert(dst->tag == ADDR_REGISTER || dst->tag == ADDR_ARGUMENT);
    assert(x->tag == ADDR_STACK_LOAD);
    cg_reserve_register(ctx, dst->i, ins->dst);
    if (type_equiv(x->type, ins->type)) {
        asm_mov(code->m, cg_suffix(ins->type), MEM_DR(x->stack[0] + x->stack[1], RBP), REG(dst->i));
    } else if (type_is_integer(x->type) && ins->type->tag == ast_type_i8) {
        asm_mov(code->m, cg_suffix(ins->type), MEM_DR(x->stack[0] + x->stack[1], RBP), REG(dst->i));
    } else if (x->type->tag == ast_type_i32 && ins->type->tag == ast_type_i64) {
        asm_movsdq(code->m, MEM_DR(x->stack[0] + x->stack[1], RBP), REG(dst->i));
    } else if (x->type->tag == ast_type_i8 && ins->type->tag == ast_type_i64) {
        asm_movsbq(code->m, MEM_DR(x->stack[0] + x->stack[1], RBP), REG(dst->i));
    } else {
        FAILWITH("TODO: cast %s -> %s", ast_type_to_str(x->type), ast_type_to_str(ins->type));
    }
}

KC_PRIVATE void
emit_op_cast_ADDR_REGISTER__ADDR_IMM_INT(IR_Ins *ins, UNUSED void *dat, struct cg_procedure *code)
{
    assert(ins->op == ir_op_cast);
    struct cg_context *ctx = &code->ctx;
    struct cg_address *dst = &ctx->vars[ins->dst];
    struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
    assert(dst->tag == ADDR_REGISTER || dst->tag == ADDR_ARGUMENT);
    assert(x->tag == ADDR_IMM_INT);
    cg_reserve_register(ctx, dst->i, ins->dst);
    if (type_is_integer(ins->type) || type_is_pointer(ins->type)) {
        asm_mov(code->m, cg_suffix(ins->type), INT(x->i), REG(dst->i));
    } else {
        FAILWITH("TODO: cast %s -> %s", ast_type_to_str(x->type), ast_type_to_str(ins->type));
    }
}

KC_PRIVATE void
emit_op_cast_ADDR_REGISTER__ADDR_REGISTER(IR_Ins *ins, UNUSED void *dat, struct cg_procedure *code)
{
    assert(ins->op == ir_op_cast);
    struct cg_context *ctx = &code->ctx;
    struct cg_address *dst = &ctx->vars[ins->dst];
    struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
    assert(dst->tag == ADDR_REGISTER || dst->tag == ADDR_ARGUMENT);
    assert(x->tag == ADDR_REGISTER);
    cg_unassign_register(ctx, x->i);
    if (type_is_void(ins->type)) return;
    cg_reserve_register(ctx, dst->i, ins->dst);
    if (type_equiv(x->type, ins->type)) {
        asm_mov(code->m, cg_suffix(ins->type), REG(x->i), REG(dst->i));
    } else if (type_is_integer(x->type) && ins->type->tag == ast_type_i8) {
        asm_mov(code->m, cg_suffix(ins->type), REG(x->i), REG(dst->i));
    } else if (x->type->tag == ast_type_i32 && ins->type->tag == ast_type_i64) {
        asm_movsdq(code->m, REG(x->i), REG(dst->i));
    } else if (x->type->tag == ast_type_i8 && ins->type->tag == ast_type_i64) {
        asm_movsbq(code->m, REG(x->i), REG(dst->i));
    } else {
        FAILWITH("TODO: cast %s -> %s", ast_type_to_str(x->type), ast_type_to_str(ins->type));
    }
}

KC_PRIVATE void
emit_op_conslice_ADDR_WIDE__ADDR_REGISTER__ADDR_IMM_INT(IR_Ins *ins, UNUSED void *dat, struct cg_procedure *code)
{
    assert(ins->op == ir_op_conslice);
    struct cg_context *ctx = &code->ctx;
    struct cg_address *dst = &ctx->vars[ins->dst];
    struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
    struct cg_address *y   = &ctx->vars[ins->arg.rx[1]];
    assert(dst->tag == ADDR_WIDE || dst->tag == ADDR_WIDE_ARG);
    assert(x->tag == ADDR_REGISTER);
    assert(y->tag == ADDR_IMM_INT);
    size_t ts = type_size(ins->type);
    if (ts == 16) {
        cg_unassign_register(ctx, x->i);
        cg_reserve_register(ctx, dst->wide[0], ins->dst);
        cg_reserve_register(ctx, dst->wide[1], ins->dst);
        if (dst->wide[0] == x->i) {
            asm_mov(code->m, ZQ, INT(y->i), REG(dst->wide[1]));
        } else {
            asm_mov(code->m, ZQ, REG(x->i), REG(dst->wide[0]));
            asm_mov(code->m, ZQ, INT(y->i), REG(dst->wide[1]));
        }
    } else {
        FAILWITH("TODO");
    }
}

KC_PRIVATE void
emit_op_conslice_ADDR_WIDE__ADDR_REGISTER__ADDR_REGISTER(IR_Ins *ins, UNUSED void *dat, struct cg_procedure *code)
{
    assert(ins->op == ir_op_conslice);
    struct cg_context *ctx = &code->ctx;
    struct cg_address *dst = &ctx->vars[ins->dst];
    struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
    struct cg_address *y   = &ctx->vars[ins->arg.rx[1]];
    assert(dst->tag == ADDR_WIDE || dst->tag == ADDR_WIDE_ARG);
    assert(x->tag == ADDR_REGISTER);
    assert(y->tag == ADDR_REGISTER);
    cg_unassign_register(ctx, x->i);
    cg_unassign_register(ctx, y->i);
    cg_reserve_register(ctx, dst->wide[0], ins->dst);
    cg_reserve_register(ctx, dst->wide[1], ins->dst);
    if (dst->wide[0] != x->i || dst->wide[1] != y->i) {
        asm_push(code->m, ZQ, REG(x->i));
        asm_push(code->m, ZQ, REG(y->i));
        asm_pop(code->m,  ZQ, REG(dst->wide[1]));
        asm_pop(code->m,  ZQ, REG(dst->wide[0]));
    }
}

KC_PRIVATE void
emit_op_conslice_ADDR_WIDE__ADDR_REGISTER__ADDR_STACK_LOAD(IR_Ins *ins, UNUSED void *dat, struct cg_procedure *code)
{
    assert(ins->op == ir_op_conslice);
    struct cg_context *ctx = &code->ctx;
    struct cg_address *dst = &ctx->vars[ins->dst];
    struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
    struct cg_address *y   = &ctx->vars[ins->arg.rx[1]];
    assert(dst->tag == ADDR_WIDE || dst->tag == ADDR_WIDE_ARG);
    assert(x->tag == ADDR_REGISTER);
    assert(y->tag == ADDR_STACK_LOAD);
    cg_unassign_register(ctx, x->i);
    cg_reserve_register(ctx, dst->wide[0], ins->dst);
    cg_reserve_register(ctx, dst->wide[1], ins->dst);
    if (dst->wide[0] != x->i) {
        asm_mov(code->m, ZQ, REG(x->i), REG(dst->wide[0]));
    }
    asm_mov(code->m, ZQ, MEM_DR(y->stack[0] + y->stack[1], RBP), REG(dst->wide[1]));
}

KC_PRIVATE void
emit_op_conslice_ADDR_WIDE__ADDR_STACK__ADDR_IMM_INT(IR_Ins *ins, UNUSED void *dat, struct cg_procedure *code)
{
    assert(ins->op == ir_op_conslice);
    struct cg_context *ctx = &code->ctx;
    struct cg_address *dst = &ctx->vars[ins->dst];
    struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
    struct cg_address *y   = &ctx->vars[ins->arg.rx[1]];
    assert(dst->tag == ADDR_WIDE || dst->tag == ADDR_WIDE_ARG);
    assert(x->tag == ADDR_STACK);
    assert(y->tag == ADDR_IMM_INT);
    cg_reserve_register(ctx, dst->wide[0], ins->dst);
    cg_reserve_register(ctx, dst->wide[1], ins->dst);
    asm_lea(code->m, ZQ, MEM_DR(x->i, RBP), REG(dst->wide[0]));
    asm_mov(code->m, ZQ, INT(y->i),         REG(dst->wide[1]));
}

KC_PRIVATE void
emit_basic_op_ADDR_REGISTER__ADDR_STACK_LOAD__ADDR_IMM_INT(IR_Ins *ins, void *dat, struct cg_procedure *code)
{
    struct cg_context *ctx = &code->ctx;
    struct cg_address *dst = &ctx->vars[ins->dst];
    struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
    struct cg_address *y   = &ctx->vars[ins->arg.rx[1]];
    KCType *type = ins->type;
    enum asm_op_code op = (enum asm_op_code)(int64_t)dat;
    assert(dst->tag == ADDR_REGISTER || dst->tag == ADDR_ARGUMENT);
    assert(x->tag == ADDR_STACK_LOAD);
    assert(y->tag == ADDR_IMM_INT);
    int dst_reg = cg_reserve_register(ctx, dst->i, ins->dst);
    asm_mov(code->m, cg_suffix(type), MEM_DR(x->stack[0] + x->stack[1], RBP), REG(dst_reg));
    asm_inst2(code->m, op, cg_suffix(type), INT(y->i), REG(dst_reg));
}

KC_PRIVATE void defer_emit_div_mod(IR_Ins *ins, void *dat, struct cg_procedure *code);

KC_PRIVATE void
cg_emit_div_mod(IR_Ins *ins, struct cg_procedure *code, bool remainder_p)
{
    struct cg_context *ctx = &code->ctx;
    struct cg_address *dst = &ctx->vars[ins->dst];
    struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
    struct cg_address *y   = &ctx->vars[ins->arg.rx[1]];
    enum asm_register result = remainder_p ? RDX : RAX;
    enum asm_op_code op = DIR_IGNORE;
    enum asm_op_code conv = DIR_IGNORE;
    /* The div instruction requires the use of RAX and RDX for the dst argument.
     * If RAX or RDX are in use, push them to the stack temporarily.
     */
    bool rax_in_use = cg_register_assigned_p(ctx, RAX);
    bool rdx_in_use = cg_register_assigned_p(ctx, RDX);
    if (rax_in_use) asm_push(code->m, ZQ, REG(RAX));
    if (rdx_in_use) asm_push(code->m, ZQ, REG(RDX));
    if (type_is_signed_integer(ins->type)) {
        op = OP_IDIV;
    } else if (type_is_unsigned_integer(ins->type)) {
        op = OP_DIV;
    } else {
        FAILWITH("TODO: division is unimplemented for type.");
    }
    if (MATCH_ADDR3(ADDR_REGISTER, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
        cg_reserve_register(ctx, dst->i, ins->dst);
        goto fallthrough_ADDR_TEMP_REG_ADDR_STACK_LOAD_ADDR_IMM_INT;
    } else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
        dst->tag = ADDR_REGISTER;
        dst->i = cg_assign_register(ctx, ins->dst);
    fallthrough_ADDR_TEMP_REG_ADDR_STACK_LOAD_ADDR_IMM_INT:
        if (!type_is_integer(ins->type))
            FAILWITH("TODO: implement division for non integer types.");
        enum asm_register tmp = cg_assign_register(ctx, ins->dst);
        enum asm_op_size suffix = cg_suffix(ins->type);
        bool ext = type_is_signed_integer(ins->type);
        switch (type_size(ins->type)) {
        case 1:
            conv = OP_CDQ;
            if (ext)
                asm_movsbd(code->m, MEM_DR(x->stack[0] + x->stack[1], RBP), REG(RAX));
            else
                asm_movzbd(code->m, MEM_DR(x->stack[0] + x->stack[1], RBP), REG(RAX));
            asm_mov(code->m, ZD, INT(y->i), REG(tmp));
            asm_inst0(code->m, conv, 0);
            asm_inst1(code->m, op, ZD, REG(tmp));
            asm_mov(code->m, suffix, REG(result), REG(dst->i));
            cg_unassign_register(ctx, tmp);
            break;
        case 2:
            conv = OP_CDQ;
            if (ext)
                asm_movswd(code->m, MEM_DR(x->stack[0] + x->stack[1], RBP), REG(RAX));
            else
                asm_movzwd(code->m, MEM_DR(x->stack[0] + x->stack[1], RBP), REG(RAX));
            asm_mov(code->m, ZD, INT(y->i), REG(tmp));
            asm_inst0(code->m, conv, 0);
            asm_inst1(code->m, op, ZD, REG(tmp));
            asm_mov(code->m, suffix, REG(result), REG(dst->i));
            cg_unassign_register(ctx, tmp);
            break;
        case 4:
            conv = OP_CDQ;
            goto CASE8;
        case 8:
            conv = OP_CQO;
        CASE8:
            asm_mov(code->m, suffix, MEM_DR(x->stack[0] + x->stack[1], RBP), REG(RAX));
            asm_mov(code->m, suffix, INT(y->i), REG(tmp));
            asm_inst0(code->m, conv, 0);
            asm_inst1(code->m, op, suffix, REG(tmp));
            asm_mov(code->m, suffix, REG(result), REG(dst->i));
            cg_unassign_register(ctx, tmp);
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
                 cg_addr_tag_to_str(dst->tag),
                 cg_addr_tag_to_str(x->tag),
                 cg_addr_tag_to_str(y->tag));
    }
    if (rdx_in_use) asm_pop(code->m, ZQ, REG(RDX));
    if (rax_in_use) asm_pop(code->m, ZQ, REG(RAX));
}

KC_PRIVATE void
defer_emit_div_mod(IR_Ins *ins, void *dat, struct cg_procedure *code)
{
    struct cg_context *ctx = &code->ctx;
    struct cg_address *dst = &ctx->vars[ins->dst];
    bool remainder_p = (bool)dat;
    assert(dst->tag == ADDR_ARGUMENT);
    dst->tag = ADDR_REGISTER; // temporarily change tag so that correct path executes.
    cg_emit_div_mod(ins, code, remainder_p);
    dst->tag = ADDR_ARGUMENT;
}

KC_PRIVATE void
cg_emit_basic_op(enum asm_op_code op, IR_Ins *ins, struct cg_address *dst,
                 struct cg_address *x, struct cg_address *y,
                 struct ir_toplevel *tl,
                 struct cg_procedure *code)
{
    struct cg_context *ctx = &code->ctx;
    KCType *type = ins->type;
    if (MATCH_ADDR3(ADDR_REGISTER, ADDR_IMM_INT, ADDR_IMM_INT)) {
        cg_reserve_register(ctx, dst->i, ins->dst);
        asm_mov(code->m, cg_suffix(type), INT(x->i), REG(dst->i));
        asm_inst2(code->m, op, cg_suffix(type), INT(y->i), REG(dst->i));
    } else if (MATCH_ADDR3(ADDR_REGISTER, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
        emit_basic_op_ADDR_REGISTER__ADDR_STACK_LOAD__ADDR_IMM_INT(ins, (void*)op, code);
    } else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK_LOAD, ADDR_IMM_INT)
               || MATCH_ADDR3(ADDR_FLAGS, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
        dst->i = cg_assign_register(ctx, ins->dst);
        dst->tag = ADDR_REGISTER;
        asm_mov(code->m, cg_suffix(type), MEM_DR(x->stack[0] + x->stack[1], RBP), REG(dst->i));
        asm_inst2(code->m, op, cg_suffix(type), INT(y->i), REG(dst->i));
    } else if (MATCH_ADDR3(ADDR_BLK_ARG, ADDR_REGISTER, ADDR_REGISTER)) {
        /* cg_unassign_register(ctx, y->i); */
        /* cg_unassign_register(ctx, x->i); */
        struct cg_address *arg_addr = &ctx->vars[dst->i]; // lookup the addr for the formal block arg
        if (arg_addr->tag == ADDR_TEMP_REG) {
            arg_addr->tag = ADDR_REGISTER;
            arg_addr->i = cg_assign_callee_save_register(ctx, ins->dst);
        }
        assert(arg_addr->tag == ADDR_REGISTER);
        dst = arg_addr;
        bool move_result_p = false;
        int dst_reg = dst->i;
        if (cg_register_assigned_p(ctx, dst_reg)) {
            if (x->i == dst_reg) {
                asm_inst2(code->m, op, cg_suffix(type), REG(y->i), REG(dst_reg));
                return;
            } else if (y->i == dst_reg) {
                move_result_p = true;
                dst_reg = cg_assign_register(ctx, ins->dst);
            } else {
                /* spill to callee save */
                dst_reg = cg_emit_move_to_callee_save(dst, code);
            }
        } else {
            cg_reserve_register(ctx, dst_reg, ins->dst);
        }
        if (dst->i != x->i) {
            asm_mov(code->m, cg_suffix(type), REG(x->i), REG(dst_reg));
        }
        asm_inst2(code->m, op, cg_suffix(type), REG(y->i), REG(dst_reg));
        if (move_result_p) {
            cg_unassign_register(ctx, dst_reg);
            asm_mov(code->m, cg_suffix(type), REG(dst_reg), REG(dst->i));
        }
    } else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK_LOAD, ADDR_STACK_LOAD)) {
        dst->i = cg_assign_register(ctx, ins->dst);
        dst->tag = ADDR_REGISTER;
        asm_mov(code->m, cg_suffix(type), MEM_DR(x->stack[0] + x->stack[1], RBP), REG(dst->i));
        asm_inst2(code->m, op, cg_suffix(type), MEM_DR(y->stack[0] + y->stack[1], RBP), REG(dst->i));
    } else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_REGISTER, ADDR_STACK_LOAD)) {
        cg_unassign_register(ctx, x->i);
        dst->i = cg_reserve_register(ctx, x->i, ins->dst);
        dst->tag = ADDR_REGISTER;
        asm_inst2(code->m, op, cg_suffix(type), MEM_DR(y->stack[0] + y->stack[1], RBP), REG(dst->i));
    } else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK_LOAD, ADDR_REGISTER)) {
        cg_unassign_register(ctx, y->i);
        dst->i = cg_assign_register(ctx, ins->dst);
        dst->tag = ADDR_REGISTER;
        asm_mov(code->m, cg_suffix(type), MEM_DR(x->stack[0] + x->stack[1], RBP), REG(dst->i));
        asm_inst2(code->m, op, cg_suffix(type), REG(y->i), REG(dst->i));
    } else if (MATCH_ADDR3(ADDR_REGISTER, ADDR_REGISTER, ADDR_STACK_LOAD)) {
        cg_reserve_register(ctx, dst->i, ins->dst);
        if (dst->i != x->i) {
            asm_mov(code->m, cg_suffix(type), REG(x->i), REG(dst->i));
        }
        asm_inst2(code->m, op, cg_suffix(type), MEM_DR(y->stack[0] + y->stack[1], RBP), REG(dst->i));
    } else if (MATCH_ADDR3(ADDR_ARGUMENT, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
        dst->extra.defered.fun = emit_basic_op_ADDR_REGISTER__ADDR_STACK_LOAD__ADDR_IMM_INT;
        dst->extra.defered.ins = ins;
        dst->extra.defered.dat = (void*)op;
    } else if (MATCH_ADDR3(ADDR_REGISTER, ADDR_STACK_LOAD, ADDR_STACK_LOAD)) {
        cg_reserve_register(ctx, dst->i, ins->dst);
        asm_mov(code->m, cg_suffix(type), MEM_DR(x->stack[0] + x->stack[1], RBP), REG(dst->i));
        asm_inst2(code->m, op, cg_suffix(type), MEM_DR(y->stack[0] + y->stack[1], RBP), REG(dst->i));
    } else if (MATCH_ADDR3(ADDR_REGISTER, ADDR_REGISTER, ADDR_REGISTER)) {
        cg_unassign_register(ctx, x->i);
        cg_unassign_register(ctx, y->i);
        cg_reserve_register(ctx, dst->i, ins->dst);
        int suffix = cg_suffix(type);
        if (dst->i == x->i) {
            asm_inst2(code->m, op, suffix, REG(y->i), REG(dst->i));
        } else {
            asm_inst2(code->m, op, suffix, REG(y->i), REG(x->i));
            asm_mov(code->m, suffix, REG(x->i), REG(dst->i));
        }
    } else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_REGISTER, ADDR_IMM_INT)) {
        cg_unassign_register(ctx, x->i);
        dst->i = cg_reserve_register(ctx, x->i, ins->dst);
        dst->tag = ADDR_REGISTER;
        asm_inst2(code->m, op, cg_suffix(type), INT(y->i), REG(dst->i));
    } else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_BLK_ARG, ADDR_IMM_INT)) {
        x = &ctx->vars[x->i];
        dst->i = cg_assign_register(ctx, ins->dst);
        dst->tag = ADDR_REGISTER;
        if (x->tag == ADDR_STACK) {
            asm_mov(code->m, cg_suffix(type), MEM_DR(x->i, RBP), REG(dst->i));
            asm_inst2(code->m, op, cg_suffix(type), INT(y->i), REG(dst->i));
        } else if (x->tag == ADDR_REGISTER) {
            asm_mov(code->m, cg_suffix(type), REG(x->i), REG(dst->i));
            asm_inst2(code->m, op, cg_suffix(type), INT(y->i), REG(dst->i));
        } else {
            FAILWITH("Unhandled case: `%s`; (x->tag == %s)",
                     asm_op_code_to_str(op),
                     cg_addr_tag_to_str(x->tag));
        }
    } else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_REGISTER, ADDR_REGISTER)) {
        cg_unassign_register(ctx, x->i);
        asm_inst2(code->m, op, cg_suffix(type), REG(y->i), REG(x->i));
        cg_unassign_register(ctx, y->i);
        dst->i = cg_reserve_register(ctx, x->i, ins->dst);
        dst->tag = ADDR_REGISTER;
    } else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_SYMBOL, ADDR_IMM_INT)) {
        dst->i = cg_assign_register(ctx, ins->dst);
        dst->tag = ADDR_REGISTER;
        IR_object *obj = &tl->elems[x->i];
        asm_mov(code->m, cg_suffix(type), MEM_LR(obj->hddr.asm_label, 0, RIP), REG(dst->i));
        asm_inst2(code->m, op, cg_suffix(type), INT(y->i), REG(dst->i));
    } else {
        FAILWITH("Unhandled case: `%s`; MATCH_ADDR3(%s, %s, %s)",
                 asm_op_code_to_str(op),
                 cg_addr_tag_to_str(dst->tag),
                 cg_addr_tag_to_str(x->tag),
                 cg_addr_tag_to_str(y->tag));
    }
}

KC_PRIVATE void
cg_emit_block(struct da_pointers *blocks, size_t blk_id, struct ir_toplevel *tl, struct cg_procedure *code)
{
    struct cg_context *ctx = &code->ctx;
    struct ir_blk *blk = blocks->elems[blk_id];
    enum asm_op_code asm_op = DIR_IGNORE;
    asm_dir_label(code->m, blk->asm_label);
    for (size_t i = 0; i < blk->code.len; ++i) {
        IR_Ins *ins = &blk->code.elems[i];
        switch (ins->op) {
        case ir_op_nop: break;
        case ir_op_mov: {
            struct cg_address *dst = &ctx->vars[ins->dst];
            struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
            BEGIN_MATCH2(dst, x);
            if (MATCH_ADDR2(ADDR_REGISTER, ADDR_STACK)) {
                emit_op_mov_ADDR_REGISTER__ADDR_STACK(ins, NULL, code);
            } else if (MATCH_ADDR2(ADDR_REGISTER, ADDR_REGISTER)) {
                if (x->i == dst->i) {
                    cg_unassign_register(ctx, x->i);
                    cg_reserve_register(ctx, dst->i, ins->dst);
                } else {
                    asm_mov(code->m, ZQ, REG(x->i), REG(dst->i));
                }
            } else if (MATCH_ADDR2(ADDR_TEMP_REG, ADDR_STACK)) {
                dst->tag = ADDR_REGISTER;
                dst->i = cg_assign_register(ctx, ins->dst);
                asm_lea(code->m, ZQ, MEM_DR(x->i, RBP), REG(dst->i));
            } else if (MATCH_ADDR2(ADDR_TEMP_REG, ADDR_REGISTER)) {
                cg_unassign_register(ctx, x->i);
                dst->tag = ADDR_REGISTER;
                dst->i = cg_assign_register(ctx, ins->dst);
                if (dst->i != x->i) {
                    asm_mov(code->m, ZQ, REG(x->i), REG(dst->i));
                }
            } else if (MATCH_ADDR2(ADDR_ARGUMENT, ADDR_STACK)) {
                dst->extra.defered.fun = emit_op_mov_ADDR_REGISTER__ADDR_STACK;
                dst->extra.defered.ins = ins;
            } else {
                FAILWITH("Unhandled case: MATCH_ADDR2(%s, %s)",
                         cg_addr_tag_to_str(dst->tag),
                         cg_addr_tag_to_str(x->tag));
            }
        } break;
        case ir_op_copy: {
            struct cg_address *dst = &ctx->vars[ins->dst];
            struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
            BEGIN_MATCH2(dst, x);
            if (MATCH_ADDR2(ADDR_STACK, ADDR_REGISTER)) {
                emit_mem_cpy_code(code, RBP, dst->i, x->i, 0, ins->type);
                cg_unassign_register(ctx, x->i);
            } else {
                FAILWITH("Unhandled case (ir_op_copy): MATCH_ADDR2(%s, %s)",
                         cg_addr_tag_to_str(dst->tag),
                         cg_addr_tag_to_str(x->tag));
            }
        } break;
        case ir_op_undefined: break;
        case ir_op_add:
            if (type_is_floating_point(ins->type)) {
                FAILWITH("TODO: floating point code");
            } else {
                assert(type_is_integer(ins->type));
                asm_op = OP_ADD;
            }
            goto LBL_basic;
        case ir_op_sub:
            if (type_is_floating_point(ins->type)) {
                FAILWITH("TODO: floating point code");
            } else {
                assert(type_is_integer(ins->type));
                asm_op = OP_SUB;
            }
            goto LBL_basic;
        case ir_op_mul:
            if (type_is_floating_point(ins->type)) {
                FAILWITH("TODO: floating point code");
            } else {
                assert(type_is_integer(ins->type));
                asm_op = OP_IMUL;
            }
        LBL_basic:
            cg_emit_basic_op(asm_op, ins,
                             &ctx->vars[ins->dst],
                             &ctx->vars[ins->arg.rx[0]],
                             &ctx->vars[ins->arg.rx[1]],
                             tl,
                             code);
            break;
        case ir_op_cmpe:
        case ir_op_cmpne:
        case ir_op_cmpl:
        case ir_op_cmpg:
        case ir_op_cmple:
        case ir_op_cmpge: {
            if (type_is_floating_point(ins->type))
                FAILWITH("TODO: floating point code");
            asm_op = OP_CMP;
            assert(ctx->vars[ins->dst].tag == ADDR_FLAGS);
            struct cg_address *dst = &ctx->vars[ins->dst];
            struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
            struct cg_address *y   = &ctx->vars[ins->arg.rx[1]];
            int suffix = cg_suffix(ins->type);
            if (MATCH_ADDR3(ADDR_FLAGS, ADDR_REGISTER, ADDR_IMM_INT)) {
                asm_cmp(code->m, suffix, INT(y->i), REG(x->i));
            } else if (MATCH_ADDR3(ADDR_FLAGS, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
                enum asm_register tmp = cg_assign_register(ctx, ins->arg.rx[0]);
                asm_mov(code->m, suffix, MEM_DR(x->stack[0] + x->stack[1], RBP), REG(tmp));
                asm_cmp(code->m, suffix, INT(y->i), REG(tmp));
                cg_unassign_register(ctx, tmp);
            } else if (MATCH_ADDR3(ADDR_FLAGS, ADDR_STACK_LOAD, ADDR_REGISTER)) {
                asm_cmp(code->m, type_size(ins->type), MEM_DR(x->stack[0] + x->stack[1], RBP), REG(y->i));
                cg_unassign_register(ctx, y->i);
            } else {
                FAILWITH("Unhandled case: MATCH_ADDR3(%s, %s, %s)",
                         cg_addr_tag_to_str(dst->tag),
                         cg_addr_tag_to_str(x->tag),
                         cg_addr_tag_to_str(y->tag));
            }
        } break;
        case ir_op_div: cg_emit_div_mod(ins, code, false); break;
        case ir_op_mod: cg_emit_div_mod(ins, code, true); break;
        case ir_op_lnot:        FAILWITH("TODO: ir_op_lnot");        break;
        case ir_op_land:        FAILWITH("TODO: ir_op_land");        break;
        case ir_op_lor:            FAILWITH("TODO: ir_op_lor");        break;
        case ir_op_xor:            FAILWITH("TODO: ir_op_xor");        break;
        case ir_op_shl: asm_op = type_is_signed_integer(ins->type) ? OP_SAL : OP_SHL; goto LBL_shift;
        case ir_op_shr: asm_op = type_is_signed_integer(ins->type) ? OP_SAR : OP_SHR; goto LBL_shift;
        LBL_shift:
            {
                struct cg_address *dst = &ctx->vars[ins->dst];
                struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
                struct cg_address *y   = &ctx->vars[ins->arg.rx[1]];
                int suffix = cg_suffix(ins->type);
                if (MATCH_ADDR3(ADDR_REGISTER, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
                    asm_mov(code->m, suffix, MEM_DR(x->stack[0] + x->stack[1], RBP), REG(dst->i));
                    asm_inst2(code->m, asm_op, suffix, INT(y->i), REG(dst->i));
                } else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
                    dst->tag = ADDR_REGISTER;
                    dst->i = cg_assign_register(ctx, ins->dst);
                    asm_mov(code->m, suffix, MEM_DR(x->stack[0] + x->stack[1], RBP), REG(dst->i));
                    asm_inst2(code->m, asm_op, suffix, INT(y->i), REG(dst->i));
                } else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK_LOAD, ADDR_STACK_LOAD)) {
                    cg_reserve_register(ctx, RCX, ins->arg.rx[1]);
                    dst->tag = ADDR_REGISTER;
                    dst->i = cg_assign_register(ctx, ins->dst);
                    asm_mov(code->m, suffix, MEM_DR(x->stack[0] + x->stack[1], RBP), REG(dst->i));
                    asm_mov(code->m, suffix, MEM_DR(y->stack[0] + y->stack[1], RBP), REG(RCX));
                    asm_inst2(code->m, asm_op, suffix, REG(RCX), REG(dst->i));
                    cg_unassign_register(ctx, RCX);
                } else {
                    FAILWITH("Unhandled case: MATCH_ADDR3(%s, %s, %s)",
                             cg_addr_tag_to_str(dst->tag),
                             cg_addr_tag_to_str(x->tag),
                             cg_addr_tag_to_str(y->tag));
                }
            } break;
        case ir_op_neg: {
            struct cg_address *dst = &ctx->vars[ins->dst];
            struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
            int suffix = cg_suffix(ins->type);
            BEGIN_MATCH2(dst, x);
            if (MATCH_ADDR2(ADDR_BLK_ARG, ADDR_STACK_LOAD)) {
                struct cg_address *arg_addr = &ctx->vars[dst->i];
                if (arg_addr->tag == ADDR_TEMP_REG) {
                    arg_addr->tag = ADDR_REGISTER;
                    arg_addr->i = cg_assign_callee_save_register(ctx, ins->dst);
                }
                assert(arg_addr->tag == ADDR_REGISTER);
                dst = arg_addr;
                asm_mov(code->m, suffix, MEM_DR(x->stack[0] + x->stack[1], RBP), REG(dst->i));
                asm_neg(code->m, suffix, REG(dst->i));
            } else if (MATCH_ADDR2(ADDR_TEMP_REG, ADDR_STACK_LOAD)) {
                dst->tag = ADDR_REGISTER;
                dst->i = cg_assign_register(ctx, ins->dst);
                asm_mov(code->m, suffix, MEM_DR(x->stack[0] + x->stack[1], RBP), REG(dst->i));
                asm_neg(code->m, suffix, REG(dst->i));
            } else if (MATCH_ADDR2(ADDR_ARGUMENT, ADDR_IMM_INT)) {
                dst->extra.defered.fun = emit_op_neg_ADDR_REGISTER__ADDR_IMM_INT;
                dst->extra.defered.ins = ins;
            } else {
                FAILWITH("Unhandled case: MATCH_ADDR2(%s, %s)",
                         cg_addr_tag_to_str(dst->tag),
                         cg_addr_tag_to_str(x->tag));
            }
        } break;
        case ir_op_cast: {
            struct cg_address *dst = &ctx->vars[ins->dst];
            struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
            BEGIN_MATCH2(dst, x);
            if (MATCH_ADDR2(ADDR_ARGUMENT, ADDR_STACK_LOAD)) {
                dst->extra.defered.fun = emit_op_cast_ADDR_REGISTER__ADDR_STACK_LOAD;
                dst->extra.defered.ins = ins;
            } else if (MATCH_ADDR2(ADDR_TEMP_REG, ADDR_STACK_LOAD)) {
                dst->tag = ADDR_REGISTER;
                dst->i = cg_assign_register(ctx, ins->dst);
                emit_op_cast_ADDR_REGISTER__ADDR_STACK_LOAD(ins, NULL, code);
            } else if (MATCH_ADDR2(ADDR_TEMP_REG, ADDR_IMM_INT)) {
                dst->tag = ADDR_REGISTER;
                dst->i = cg_assign_register(ctx, ins->dst);
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
                         cg_addr_tag_to_str(dst->tag),
                         cg_addr_tag_to_str(x->tag));
            }
        } break;
        case ir_op_conslice: {
            struct cg_address *dst = &ctx->vars[ins->dst];
            struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
            struct cg_address *y   = &ctx->vars[ins->arg.rx[1]];
            if (MATCH_ADDR3(ADDR_TEMP_WIDE, ADDR_REGISTER, ADDR_IMM_INT)) {
                dst->tag = ADDR_WIDE;
                dst->wide[0] = cg_assign_register(ctx, ins->dst);
                dst->wide[1] = cg_assign_register(ctx, ins->dst);
                emit_op_conslice_ADDR_WIDE__ADDR_REGISTER__ADDR_IMM_INT(ins, NULL, code);
            } else if (MATCH_ADDR3(ADDR_WIDE_ARG, ADDR_REGISTER, ADDR_IMM_INT)) {
                dst->extra.defered.fun = emit_op_conslice_ADDR_WIDE__ADDR_REGISTER__ADDR_IMM_INT;
                dst->extra.defered.ins = ins;
            } else if (MATCH_ADDR3(ADDR_WIDE_ARG, ADDR_REGISTER, ADDR_REGISTER)) {
                dst->extra.defered.fun = emit_op_conslice_ADDR_WIDE__ADDR_REGISTER__ADDR_REGISTER;
                dst->extra.defered.ins = ins;
            } else if (MATCH_ADDR3(ADDR_TEMP_WIDE, ADDR_STACK, ADDR_IMM_INT)) {
                dst->tag = ADDR_WIDE;
                dst->wide[0] = cg_assign_register(ctx, ins->dst);
                dst->wide[1] = cg_assign_register(ctx, ins->dst);
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
                         cg_addr_tag_to_str(dst->tag),
                         cg_addr_tag_to_str(x->tag),
                         cg_addr_tag_to_str(y->tag));
            }
        } break;
        case ir_op_getelemptr: {
            struct cg_address *dst  = &ctx->vars[ins->dst];
            struct cg_address *base = &ctx->vars[ins->arg.rx[0]];
            struct cg_address *idx  = &ctx->vars[ins->arg.rx[1]];
            if (type_is_array_ptr(ins->type)) {
                KCType *base_type = ins->type->ptr->array.base;
                size_t scale = type_size(base_type);
                enum asm_scale sc;
                switch (scale) {
                case 1: sc = SB; break;
                case 2: sc = SW; break;
                case 4: sc = SD; break;
                case 8: sc = SQ; break;
                default: FAILWITH("Invalid scale: %zu", scale); break;
                }
                if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK, ADDR_IMM_INT)) {
                    dst->tag = ADDR_REGISTER;
                    dst->i = cg_assign_register(ctx, ins->dst);
                    asm_lea(code->m, ZQ, MEM_DR(base->i + (idx->i * scale), RBP), REG(dst->i));
                } else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_REGISTER, ADDR_IMM_INT)) {
                    cg_unassign_register(ctx, base->i);
                    dst->tag = ADDR_REGISTER;
                    dst->i = cg_assign_register(ctx, ins->dst);
                    asm_lea(code->m, ZQ, MEM_DR(idx->i * scale, base->i), REG(dst->i));
                } else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_REGISTER, ADDR_STACK_LOAD)) {
                    assert(idx->type);
                    dst->tag = ADDR_REGISTER;
                    dst->i = cg_assign_register(ctx, ins->dst);
                    /* load index into dst */
                    int64_t disp = idx->stack[0] + idx->stack[1];
                    asm_mov(code->m, cg_suffix(idx->type), MEM_DR(disp, RBP), REG(dst->i));
                    if (type_is_scalar(base_type)) {
                        asm_lea(code->m, ZQ, MEM_DRXS(0, base->i, dst->i, sc), REG(dst->i));
                    } else {
                        FAILWITH("TODO: index array of non-scalar type");
                    }
                    cg_unassign_register(ctx, base->i);
                } else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
                    dst->tag = ADDR_REGISTER;
                    dst->i = cg_assign_register(ctx, ins->dst);
                    int64_t disp = base->stack[0] + base->stack[1];
                    asm_mov(code->m, cg_suffix(base->type), MEM_DR(disp, RBP), REG(dst->i));
                    asm_lea(code->m, ZQ, MEM_DR(idx->i * scale, dst->i), REG(dst->i));
                } else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK_LOAD, ADDR_STACK_LOAD)) {
                    dst->tag = ADDR_REGISTER;
                    dst->i = cg_assign_register(ctx, ins->dst);
                    int tmp = cg_assign_register(ctx, ins->arg.rx[1]);
                    asm_mov(code->m, cg_suffix(base->type),
                            MEM_DR(base->stack[0] + base->stack[1], RBP), REG(dst->i));
                    asm_mov(code->m, cg_suffix(idx->type),
                            MEM_DR(idx->stack[0] + idx->stack[1], RBP), REG(tmp));
                    asm_lea(code->m, ZQ, MEM_DRXS(0, dst->i, tmp, sc), REG(dst->i));
                    cg_unassign_register(ctx, tmp);
                } else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_BLK_ARG, ADDR_IMM_INT)) {
                    dst->tag = ADDR_REGISTER;
                    dst->i = cg_assign_register(ctx, ins->dst);
                    base = &ctx->vars[base->i];
                    if (base->tag == ADDR_STACK_LOAD) {
                        asm_mov(code->m, cg_suffix(base->type),
                                MEM_DR(base->stack[0] + base->stack[1], RBP), REG(dst->i));
                        asm_lea(code->m, ZQ, MEM_DR(idx->i * scale, dst->i), REG(dst->i));
                    } else {
                        FAILWITH("Unhandled case: (base->tag == %s)", cg_addr_tag_to_str(base->tag));
                    }
                } else {
                    printf("dst = %%%d\n", ins->dst);
                    FAILWITH("Unhandled case: MATCH_ADDR3(%s, %s, %s)",
                             cg_addr_tag_to_str(dst->tag),
                             cg_addr_tag_to_str(base->tag),
                             cg_addr_tag_to_str(idx->tag));
                }
            } else if (type_is_struct_ptr(ins->type)) {
                if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK, ADDR_IMM_INT)) {
                    dst->tag = ADDR_REGISTER;
                    dst->i = cg_assign_register(ctx, ins->dst);
                    size_t offset = struct_member_offset(&ins->type->ptr->struct_t, idx->i);
                    asm_lea(code->m, ZQ, MEM_DR(base->i + offset, RBP), REG(dst->i));
                } else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK_LOAD, ADDR_IMM_INT)) {
                    dst->tag = ADDR_REGISTER;
                    dst->i = cg_assign_register(ctx, ins->dst);
                    size_t offset = struct_member_offset(&ins->type->ptr->struct_t, idx->i);
                    asm_mov(code->m, ZQ, MEM_DR(base->stack[0] + base->stack[1], RBP), REG(dst->i));
                    if (offset > 0) {
                        asm_lea(code->m, ZQ, MEM_DR(offset, dst->i), REG(dst->i));
                    }
                } else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_REGISTER, ADDR_IMM_INT)) {
                    cg_unassign_register(ctx, base->i);
                    dst->tag = ADDR_REGISTER;
                    dst->i = cg_assign_register(ctx, ins->dst);
                    size_t offset = struct_member_offset(&ins->type->ptr->struct_t, idx->i);
                    asm_lea(code->m, ZQ, MEM_DR(offset, base->i), REG(dst->i));
                } else {
                    FAILWITH("Unhandled case: MATCH_ADDR3(%s, %s, %s)",
                             cg_addr_tag_to_str(dst->tag),
                             cg_addr_tag_to_str(base->tag),
                             cg_addr_tag_to_str(idx->tag));
                }
            } else if (type_is_slice(ins->type)) {
                if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_REGISTER, ADDR_IMM_INT)) {
                    cg_unassign_register(ctx, base->i);
                    dst->tag = ADDR_REGISTER;
                    dst->i = cg_assign_register(ctx, ins->dst);
                    asm_lea(code->m, ZQ, MEM_DR(idx->i * 8, base->i), REG(dst->i));
                } else {
                    FAILWITH("Unhandled case: MATCH_ADDR3(%s, %s, %s)",
                             cg_addr_tag_to_str(dst->tag),
                             cg_addr_tag_to_str(base->tag),
                             cg_addr_tag_to_str(idx->tag));
                }
            } else if (ins->type->tag == ast_type_union) {
                if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_REGISTER, ADDR_IMM_INT)) {
                    cg_unassign_register(ctx, base->i);
                    dst->tag = ADDR_REGISTER;
                    dst->i = cg_assign_register(ctx, ins->dst);
                    asm_lea(code->m, ZQ, MEM_DR(idx->i * 8, base->i), REG(dst->i));
                } else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_BLK_ARG, ADDR_IMM_INT)) {
                    dst->tag = ADDR_REGISTER;
                    dst->i = cg_assign_register(ctx, ins->dst);
                    base = &ctx->vars[base->i];
                    if (base->tag == ADDR_STACK) {
                        asm_lea(code->m, ZQ, MEM_DR(base->i + idx->i * 8, RBP), REG(dst->i));
                    } else if (base->tag == ADDR_STACK_LOAD) {
                        asm_mov(code->m, ZQ, MEM_DR(base->stack[0] + base->stack[1], RBP), REG(dst->i));
                        asm_lea(code->m, ZQ, MEM_DR(idx->i * 8, dst->i), REG(dst->i));
                    } else {
                        FAILWITH("Unhandled case: (base->tag == %s)", cg_addr_tag_to_str(base->tag));
                    }
                } else if (MATCH_ADDR3(ADDR_TEMP_REG, ADDR_STACK, ADDR_IMM_INT)) {
                    dst->tag = ADDR_REGISTER;
                    dst->i = cg_assign_register(ctx, ins->dst);
                    asm_lea(code->m, ZQ, MEM_DR(base->i + idx->i * 8, RBP), REG(dst->i));
                } else {
                    FAILWITH("Unhandled case (ir_op_getelemptr %%%d): MATCH_ADDR3(%s, %s, %s)",
                             ins->dst,
                             cg_addr_tag_to_str(dst->tag),
                             cg_addr_tag_to_str(base->tag),
                             cg_addr_tag_to_str(idx->tag));
                }
            } else {
                printf("ins->type = %s\n", ast_type_to_str(ins->type));
                FAILWITH("TODO: ir_op_getelemptr");
            }
        } break;
        case ir_op_retval: {
            struct cg_address *dst = &ctx->vars[ins->dst];
            if (dst->tag == ADDR_BLK_ARG) dst = &ctx->vars[dst->i];
            assert(dst->tag == ADDR_STACK_LOAD);
        } break;
        case ir_op_alloca: /* nothing to do */ break;
        case ir_op_load: {
            struct cg_address *dst = &ctx->vars[ins->dst];
            struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
            BEGIN_MATCH2(dst, x);
            if (dst->tag == ADDR_STACK_LOAD) {
                /* do nothing */
            } else if (MATCH_ADDR2(ADDR_REGISTER, ADDR_REGISTER)) {
                if (x->i != dst->i) {
                    cg_unassign_register(ctx, x->i);
                    cg_reserve_register(ctx, dst->i, ins->dst);
                }
                asm_mov(code->m, cg_suffix(ins->type), MEM_DR(0, x->i), REG(dst->i));
            } else if (MATCH_ADDR2(ADDR_BLK_ARG, ADDR_REGISTER)) {
                dst = &ctx->vars[dst->i];
                if (dst->tag == ADDR_TEMP_REG) {
                    dst->tag = ADDR_REGISTER;
                    dst->i = cg_assign_callee_save_register(ctx, ins->dst);
                } else {
                    assert(dst->tag == ADDR_REGISTER);
                    if (cg_register_assigned_p(ctx, dst->i)) {
                        /* check if register was already assigned in another branch */
                        struct cg_address *owner = &ctx->vars[cg_get_register_owner(ctx, dst->i)];
                        assert(owner->tag == ADDR_BLK_ARG);
                    } else {
                        cg_reserve_register(ctx, dst->i, ins->dst);
                    }
                }
                /* ADDR_REGISTER */
                asm_mov(code->m, cg_suffix(ins->type), MEM_DR(0, x->i), REG(dst->i));
                if (x->i != dst->i) cg_unassign_register(ctx, x->i);
            } else if (MATCH_ADDR2(ADDR_BLK_ARG, ADDR_STACK)) {
                dst = &ctx->vars[dst->i];
                if (dst->tag == ADDR_STACK_LOAD) {
                    size_t ts = type_size(ins->type);
                    if (ts <= 8) {
                        dst->tag = ADDR_REGISTER;
                        dst->i = cg_assign_register(ctx, ins->dst);
                        asm_mov(code->m, cg_suffix(ins->type), MEM_DR(x->i, RBP), REG(dst->i));
                    } else if (ts <= 16) {
                        dst->tag = ADDR_WIDE;
                        dst->wide[0] = cg_assign_register(ctx, ins->dst);
                        dst->wide[1] = cg_assign_register(ctx, ins->dst);
                        asm_mov(code->m, ZQ, MEM_DR(x->i, RBP), REG(dst->wide[0]));
                        asm_mov(code->m, ZQ, MEM_DR(x->i + 8, RBP), REG(dst->wide[1]));
                    } else {
                        FAILWITH("TODO");
                    }
                } else if (dst->tag == ADDR_WIDE) {
                    if (cg_register_assigned_p(ctx, dst->wide[0])) {
                        /* check if register was already assigned in another branch */
                        int owner_var = cg_get_register_owner(ctx, dst->wide[0]);
                        struct cg_address *owner = &ctx->vars[owner_var];
                        assert(owner->tag == ADDR_BLK_ARG);
                    } else {
                        cg_reserve_register(ctx, dst->wide[0], ins->dst);
                        cg_reserve_register(ctx, dst->wide[1], ins->dst);
                    }
                    asm_mov(code->m, ZQ, MEM_DR(x->i, RBP), REG(dst->wide[0]));
                    asm_mov(code->m, ZQ, MEM_DR(x->i + 8, RBP), REG(dst->wide[1]));
                } else if (dst->tag == ADDR_TEMP_REG) {
                    dst->tag = ADDR_REGISTER;
                    dst->i = cg_assign_callee_save_register(ctx, ins->dst);
                    asm_mov(code->m, cg_suffix(ins->type), MEM_DR(x->i, RBP), REG(dst->i));
                } else if (dst->tag == ADDR_REGISTER) {
                    if (cg_register_assigned_p(ctx, dst->i)) {
                        /* check if register was already assigned in another branch */
                        struct cg_address *owner = &ctx->vars[cg_get_register_owner(ctx, dst->i)];
                        assert(owner->tag == ADDR_BLK_ARG);
                    } else {
                        cg_reserve_register(ctx, dst->i, ins->dst);
                    }
                    asm_mov(code->m, cg_suffix(ins->type), MEM_DR(x->i, RBP), REG(dst->i));
                } else {
                    printf("reg = %%%d\n", ins->dst);
                    FAILWITH("Unhandled case: (dst->tag == %s)", cg_addr_tag_to_str(dst->tag));
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
                dst->i = cg_assign_register(ctx, ins->dst);
                if (type_size(ins->type) > 8) {
                    FAILWITH("invalid load: dst = %%%d, x = %%%d",
                             ins->dst,
                             ins->arg.rx[0]);
                }
                asm_mov(code->m, cg_suffix(ins->type), MEM_DR(0, x->i), REG(dst->i));
                cg_unassign_register(ctx, x->i);
            } else if (MATCH_ADDR2(ADDR_TEMP_WIDE, ADDR_REGISTER)) {
                dst->tag = ADDR_WIDE;
                dst->wide[0] = cg_assign_register(ctx, ins->dst);
                dst->wide[1] = cg_assign_register(ctx, ins->dst);
                asm_mov(code->m, ZQ, MEM_DR(0, x->i), REG(dst->wide[0]));
                asm_mov(code->m, ZQ, MEM_DR(8, x->i), REG(dst->wide[1]));
                cg_unassign_register(ctx, x->i);
            } else if (MATCH_ADDR2(ADDR_ARGUMENT, ADDR_BLK_ARG)) {
                x = &ctx->vars[x->i];
                switch ((int)x->tag) {
                case ADDR_STACK: {
                    dst->extra.defered.fun = emit_op_load_ADDR_REGISTER__ADDR_STACK;
                    dst->extra.defered.ins = ins;
                } break;
                default:
                    FAILWITH("Unhandled case %s:", cg_addr_tag_to_str(x->tag));
                    break;
                }
            } else {
                printf("ins->dst = %%%d\n", ins->dst);
                FAILWITH("Unhandled case: MATCH_ADDR2(%s, %s)",
                         cg_addr_tag_to_str(dst->tag),
                         cg_addr_tag_to_str(x->tag));
            }
        } break;
        case ir_op_loadglobl: {
            struct cg_address *dst = &ctx->vars[ins->dst];
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
                emit_op_load_ADDR_WIDE__GLOBL(ins, tl, code);
            }
        } break;
        case ir_op_loadconst: FAILWITH("TODO: ir_op_loadconst"); break;
        case ir_op_loadimm: {
            struct cg_address *dst = &ctx->vars[ins->dst];
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
                struct cg_address *arg_addr = &ctx->vars[dst->i]; // lookup the addr for the formal block arg
                if (arg_addr->tag == ADDR_TEMP_REG) {
                    arg_addr->tag = ADDR_REGISTER;
                    arg_addr->i = cg_assign_callee_save_register(ctx, ins->dst);
                }
                ASSERT(arg_addr->tag == ADDR_REGISTER || arg_addr->tag == ADDR_ARGUMENT, "FAIL");
                dst = arg_addr;
                asm_mov(code->m, cg_suffix(ins->type), INT(ins->arg.i32), REG(dst->i));
            } else {
                FAILWITH("TODO: unhandled case dst->tag == %s", cg_addr_tag_to_str(dst->tag));
            }
        } break;
        case ir_op_store: {
            struct cg_address *dst = &ctx->vars[ins->dst];
            struct cg_address *x   = &ctx->vars[ins->arg.rx[0]];
            if (dst->tag == ADDR_BLK_ARG) dst = &ctx->vars[dst->i];
            if (x->tag   == ADDR_BLK_ARG) x   = &ctx->vars[x->i];
            BEGIN_MATCH2(dst, x);
            if (MATCH_ADDR2(ADDR_REGISTER, ADDR_IMM_INT)) {
                asm_mov(code->m, cg_suffix(ins->type), INT(x->i), MEM_DR(0, dst->i));
                cg_unassign_register(ctx, dst->i);
            } else if (MATCH_ADDR2(ADDR_REGISTER, ADDR_WIDE)) {
                asm_mov(code->m, ZQ, REG(x->wide[0]), MEM_DR(0, dst->i));
                asm_mov(code->m, ZQ, REG(x->wide[1]), MEM_DR(8, dst->i));
                cg_unassign_register(ctx, dst->i);
                cg_unassign_register(ctx, x->wide[0]);
                cg_unassign_register(ctx, x->wide[1]);
            } else if (MATCH_ADDR2(ADDR_STACK, ADDR_REGISTER)) {
                asm_mov(code->m, cg_suffix(ins->type), REG(x->i), MEM_DR(dst->i, RBP));
                cg_unassign_register(ctx, x->i);
            } else if (MATCH_ADDR2(ADDR_STACK, ADDR_STACK_ARG)) {
                /* do nothing */
            } else if (MATCH_ADDR2(ADDR_REGISTER, ADDR_STACK_LOAD)) {
                enum asm_register tmp = cg_assign_register(ctx, ins->arg.rx[0]);
                asm_mov(code->m, cg_suffix(ins->type),
                        MEM_DR(x->stack[0] + x->stack[1], RBP), REG(tmp));
                asm_mov(code->m, cg_suffix(ins->type), REG(tmp), MEM_DR(0, dst->i));
                cg_unassign_register(ctx, tmp);
                cg_unassign_register(ctx, dst->i);
            } else if (MATCH_ADDR2(ADDR_STACK, ADDR_WIDE)) {
                size_t ts = type_size(ins->type);
                if (ts == 16) {
                    asm_mov(code->m, ZQ, REG(x->wide[0]), MEM_DR(dst->i, RBP));
                    asm_mov(code->m, ZQ, REG(x->wide[1]), MEM_DR(dst->i + 8, RBP));
                    cg_unassign_register(ctx, x->wide[0]);
                    cg_unassign_register(ctx, x->wide[1]);
                } else {
                    FAILWITH("TODO");
                }
            } else if (MATCH_ADDR2(ADDR_STACK, ADDR_WIDE_ARG)) {
                size_t ts = type_size(ins->type);
                if (ts == 16) {
                    /* TODO: BUG */
                    FAILWITH("TODO: is this still a bug?");
                } else {
                    FAILWITH("TODO");
                }
            } else if (MATCH_ADDR2(ADDR_STACK, ADDR_IMM_INT)) {
                asm_mov(code->m, cg_suffix(ins->type), INT(x->i), MEM_DR(dst->i, RBP));
            } else if (MATCH_ADDR2(ADDR_REGISTER, ADDR_SYMBOL)) {
                IR_object *obj = &tl->elems[x->i];
                assert(obj->tag == IRO_DATA);
                if (type_is_slice(ins->type)) {
                    KCType *base_type = ins->type->slice;
                    enum asm_register tmp = cg_assign_register(ctx, ins->arg.rx[0]);
                    if (code->m->is_jit && !obj->hddr.is_static) {
                        asm_mov(code->m, ZQ, MEM_LR(obj->hddr.asm_label, 0, RIP), REG(tmp));
                    } else {
                        asm_lea(code->m, ZQ, MEM_LR(obj->hddr.asm_label, 0, RIP), REG(tmp));
                    }
                    asm_mov(code->m, ZQ, REG(tmp), MEM_DR(0, dst->i));
                    asm_mov(code->m, ZQ, INT(obj->data.size / type_size(base_type)), MEM_DR(8, dst->i));
                    cg_unassign_register(ctx, dst->i);
                    cg_unassign_register(ctx, tmp);
                } else {
                    FAILWITH("TODO: MATCH_ADDR2(ADDR_REGISTER, ADDR_SYMBOL)");
                }
            } else if (MATCH_ADDR2(ADDR_STACK, ADDR_SYMBOL)) {
                IR_object *obj = &tl->elems[x->i];
                assert(obj->tag == IRO_DATA);
                if (type_is_slice(ins->type)) {
                    KCType *base_type = ins->type->slice;
                    enum asm_register tmp = cg_assign_register(ctx, ins->arg.rx[0]);
                    if (code->m->is_jit && !obj->hddr.is_static) {
                        asm_mov(code->m, ZQ, MEM_LR(obj->hddr.asm_label, 0, RIP), REG(tmp));
                    } else {
                        asm_lea(code->m, ZQ, MEM_LR(obj->hddr.asm_label, 0, RIP), REG(tmp));
                    }
                    asm_mov(code->m, ZQ, REG(tmp), MEM_DR(dst->i, RBP));
                    asm_mov(code->m, ZQ, INT(obj->data.size / type_size(base_type)), MEM_DR(dst->i + 8, RBP));
                    cg_unassign_register(ctx, tmp);
                } else {
                    FAILWITH("TODO: MATCH_ADDR2(ADDR_REGISTER, ADDR_SYMBOL)");
                }
            } else if (MATCH_ADDR2(ADDR_STACK, ADDR_FLAGS)) {
                enum asm_cc cc;
                switch (x->stack[0]) {
                case ir_op_cmpe:  cc = CC_E;  break;
                case ir_op_cmpne: cc = CC_NE; break;
                case ir_op_cmpl:  cc = type_is_signed_scalar(x->type) ? CC_L  : CC_B;   break;
                case ir_op_cmpg:  cc = type_is_signed_scalar(x->type) ? CC_G  : CC_A;   break;
                case ir_op_cmple: cc = type_is_signed_scalar(x->type) ? CC_LE : CC_BE; break;
                case ir_op_cmpge: cc = type_is_signed_scalar(x->type) ? CC_GE : CC_AE; break;
                default: FAILWITH("Unreachable"); break;
                }
                asm_setcc(code->m, cc, MEM_DR(dst->i, RBP));
            } else if (MATCH_ADDR2(ADDR_STACK, ADDR_STACK)) {
                /* Nothing to do? */
            } else if (MATCH_ADDR2(ADDR_STACK_LOAD, ADDR_IMM_INT)) {
                enum asm_register tmp = cg_assign_register(ctx, ins->dst);
                asm_mov(code->m, ZQ, MEM_DR(dst->stack[0] + dst->stack[1], RBP), REG(tmp));
                asm_mov(code->m, ZQ, INT(x->i), MEM_DR(0, tmp));
                cg_unassign_register(ctx, tmp);
            } else if (MATCH_ADDR2(ADDR_STACK, ADDR_STACK_LOAD)) {
                enum asm_register tmp = cg_assign_register(ctx, ins->dst);
                asm_mov(code->m, cg_suffix(ins->type),
                        MEM_DR(x->stack[0] + x->stack[1], RBP), REG(tmp));
                asm_mov(code->m, cg_suffix(ins->type), REG(tmp), MEM_DR(dst->i, RBP));
                cg_unassign_register(ctx, tmp);
            } else if (MATCH_ADDR2(ADDR_SYMBOL, ADDR_REGISTER)) {
                IR_object *obj = &tl->elems[dst->i];
                asm_mov(code->m, cg_suffix(ins->type), REG(x->i), MEM_LR(obj->hddr.asm_label, 0, RIP));
                cg_unassign_register(ctx, x->i);
            } else {
                printf("dst = %%%d\n", ins->dst);
                printf("x   = %%%d\n", ins->arg.rx[0]);
                FAILWITH("Unhandled case: MATCH_ADDR2(%s, %s)",
                         cg_addr_tag_to_str(dst->tag),
                         cg_addr_tag_to_str(x->tag));
            }
        } break;
        case ir_op_memzero: {
            struct cg_address *dst = &ctx->vars[ins->dst];
            if (dst->tag == ADDR_STACK) {
                size_t ts = type_size(ins->type);
                uint32_t counts[4] = {0};
                mem_copy_segment_count(ts, counts);
                uint32_t offset = 0;
                for (uint32_t i = 0; i < counts[0]; ++i) {
                    asm_mov(code->m, ZQ, INT(0), MEM_DR(dst->i + offset, RBP));
                    offset += 8;
                }
                for (uint32_t i = 0; i < counts[1]; ++i) {
                    asm_mov(code->m, ZD, INT(0), MEM_DR(dst->i + offset, RBP));
                    offset += 4;
                }
                for (uint32_t i = 0; i < counts[2]; ++i) {
                    asm_mov(code->m, ZW, INT(0), MEM_DR(dst->i + offset, RBP));
                    offset += 2;
                }
                for (uint32_t i = 0; i < counts[3]; ++i) {
                    asm_mov(code->m, ZB, INT(0), MEM_DR(dst->i + offset, RBP));
                    offset += 1;
                }
            } else {
                FAILWITH("Unhandled case: dst->tag == %s",
                         cg_addr_tag_to_str(dst->tag));
            }
        } break;
        case ir_op_pushfunarg: {
            struct cg_address *arg = &ctx->vars[ins->dst];
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
                         cg_addr_tag_to_str(arg->tag), ins->dst);
                break;
            }
        } break;
        case ir_op_call: {
            size_t stack_size = 0;
            int argc = ins->arg.rx[1];
            while (argc--) {
                struct cg_address *arg = da_pop(&ctx->funargs);
                assert(arg->tag != ADDR_NONE);
                switch ((int)arg->tag) {
                case ADDR_REGISTER:
                case ADDR_ARGUMENT:
                    cg_unassign_register(ctx, arg->i);
                    break;
                case ADDR_WIDE_ARG:
                    cg_unassign_register(ctx, arg->wide[0]);
                    cg_unassign_register(ctx, arg->wide[1]);
                    break;
                case ADDR_PUSH_ARG:
                    stack_size += type_size(arg->type);
                    stack_size = align_adjust(stack_size, 8);
                    break;
                default:
                    FAILWITH("TODO: unhandled case arg->tag == %s", cg_addr_tag_to_str(arg->tag));
                    break;
                }
            }
            struct cg_address *ret = &ctx->vars[ins->dst];
            if (ret->tag == ADDR_STACK) {
                assert(cg_register_assigned_p(ctx, cg_arg_regs[0]) == false);
                asm_lea(code->m, ZQ, MEM_DR(ret->i, RBP), REG(cg_arg_regs[0]));
            }
            for (size_t i = 0; i < ARRAY_LENGTH(cg_caller_save_regs); ++i) {
                enum asm_register reg = cg_caller_save_regs[i];
                if (cg_register_assigned_p(ctx, reg)) {
                    int var = ctx->assigned[reg];
                    struct cg_address *addr = &ctx->vars[var];
                    cg_emit_move_to_callee_save(addr, code);
                }
            }
            struct cg_address *p = &ctx->vars[ins->arg.rx[0]];
            if (p->tag == ADDR_SYMBOL) {
                asm_call(code->m, LABEL(tl->elems[p->i].hddr.asm_label, 0));
            } else {
                FAILWITH("TODO: unhandled case p->tag == %s", cg_addr_tag_to_str(p->tag));
            }
            if (stack_size) {
                asm_add(code->m, ZQ, INT(stack_size), REG(RSP));
                ctx->setup_frame = true;
            }
            if (!type_is_void(ins->type)) {
                switch ((int)ret->tag) {
                case ADDR_REGISTER: {
                    cg_reserve_register(ctx, ret->i, ins->dst);
                } break;
                case ADDR_ARGUMENT: {
                    cg_unassign_register(ctx, cg_arg_regs[0]);
                    cg_reserve_register(ctx, ret->i, ins->dst);
                    asm_mov(code->m, ZQ, REG(cg_ret_regs[0]), REG(ret->i));
                    ret->extra.defered.fun = NULL;
                } break;
                case ADDR_WIDE_ARG: {
                    cg_unassign_register(ctx, cg_arg_regs[0]);
                    cg_unassign_register(ctx, cg_arg_regs[1]);
                    cg_reserve_register(ctx, ret->wide[0], ins->dst);
                    cg_reserve_register(ctx, ret->wide[1], ins->dst);
                    asm_push(code->m, ZQ, REG(cg_ret_regs[0]));
                    asm_push(code->m, ZQ, REG(cg_ret_regs[1]));
                    asm_pop(code->m,  ZQ, REG(cg_ret_regs[1]));
                    asm_pop(code->m,  ZQ, REG(cg_ret_regs[0]));
                    ret->extra.defered.fun = NULL;
                } break;
                case ADDR_WIDE: {
                    cg_reserve_register(ctx, ret->wide[0], ins->dst);
                    cg_reserve_register(ctx, ret->wide[1], ins->dst);
                } break;
                case ADDR_STACK: /* do nothing */ break;
                case ADDR_BLK_ARG: {
                    ret = &ctx->vars[ret->i];
                    if (ret->tag == ADDR_REGISTER) {
                        assert(ret->i == cg_ret_regs[0]);
                        cg_reserve_register(ctx, ret->i, ins->dst);
                    } else {
                        FAILWITH("TODO: unhandled case ret->tag == %s", cg_addr_tag_to_str(ret->tag));
                    }
                } break;
                default: {
                    FAILWITH("TODO: unhandled case ret->tag == %s", cg_addr_tag_to_str(ret->tag));
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
            struct cg_address *ret = &ctx->vars[term->args.elems[0]];
            if (ret->tag == ADDR_STACK) {
                assert(type_is_pointer(ret->type));
                size_t ts = type_size(ret->type->ptr);
                if (ts < 16) {
                    FAILWITH("TODO");
                } else if (ts == 16) {
                    asm_mov(code->m, ZQ, MEM_DR(ret->i, RBP), REG(cg_ret_regs[0]));
                    asm_mov(code->m, ZQ, MEM_DR(ret->i + 8, RBP), REG(cg_ret_regs[1]));
                } else {
                    FAILWITH("Unreachable");
                }
            } else if (ret->tag == ADDR_STACK_LOAD) {
                size_t ts = type_size(ret->type);
                if (ts <= 8) {
                    asm_mov(code->m, ZQ, MEM_DR(ret->stack[0] + ret->stack[1], RBP), REG(cg_ret_regs[0]));
                } else if (ts <= 16) {
                    int64_t disp = ret->stack[0] + ret->stack[1];
                    asm_mov(code->m, ZQ, MEM_DR(disp, RBP), REG(cg_ret_regs[0]));
                    asm_mov(code->m, ZQ, MEM_DR(disp + 8, RBP), REG(cg_ret_regs[1]));
                } else {
                    FAILWITH("Unreachable blk = %p, ts = %zu, ret = %%%d",
                             blk, ts, term->args.elems[0]);
                }
            } else if (ret->tag == ADDR_REGISTER) {
                if (ret->i != cg_ret_regs[0]) {
                    asm_mov(code->m, ZQ, REG(ret->i), REG(cg_ret_regs[0]));
                }
            } else if (ret->tag == ADDR_WIDE) {
                if (ret->wide[0] != (int)cg_ret_regs[0] || ret->wide[1] != (int)cg_ret_regs[1]) {
                    asm_push(code->m, ZQ, REG(ret->wide[0]));
                    asm_push(code->m, ZQ, REG(ret->wide[1]));
                    asm_pop(code->m,  ZQ, REG(cg_ret_regs[1]));
                    asm_pop(code->m,  ZQ, REG(cg_ret_regs[0]));
                }
            } else if (ret->tag == ADDR_IMM_INT) {
                asm_mov(code->m, cg_suffix(ret->type), INT(ret->i), REG(cg_ret_regs[0]));
            } else if (ret->tag == ADDR_NONE) {
                /* do nothing */
            } else if (ret->tag == ADDR_SYMBOL) {
                /* do nothing */
                size_t ts = type_size(ret->type);
                assert(ts <= 8);
                IR_object *obj = &tl->elems[ret->i];
                asm_mov(code->m, cg_suffix(ret->type), MEM_LR(obj->hddr.asm_label, 0, RIP), REG(cg_ret_regs[0]));
            } else {
                FAILWITH("TODO: ret->tag == %s", cg_addr_tag_to_str(ret->tag));
            }
        }
        if (blk_id < blocks->len - 1) {
            asm_jmp(code->m, LABEL(ctx->ret_lbl, 0));
        }
    } break;
    case ir_op_goto: {
        for (size_t i = 0; i < term->args.len; ++i) {
            cg_unassign_blk_arg_register(ctx, term->args.elems[i]);
        }
        if (blk_id < blocks->len - 1
            && term->b0 == blocks->elems[blk_id + 1]) {
            /* fall through to block */
        } else {
            asm_jmp(code->m, LABEL(term->b0->asm_label, 0));
        }
    } break;
    case ir_op_if: {
        assert(term->args.len == 1);
        struct cg_address *cond = &ctx->vars[term->args.elems[0]];
        if (cond->tag == ADDR_FLAGS) {
            enum asm_cc cc;
            /* We want to jump to the false branch (and fallthrough to the true branch),
             * so we logically negate the comparison.
             */
            switch (cond->stack[0]) {
            case ir_op_cmpe:  cc = CC_NE;  break;
            case ir_op_cmpne: cc = CC_E; break;
            case ir_op_cmpl:  cc = type_is_signed_scalar(cond->type) ? CC_GE : CC_AE; break;
            case ir_op_cmpg:  cc = type_is_signed_scalar(cond->type) ? CC_LE : CC_BE; break;
            case ir_op_cmple: cc = type_is_signed_scalar(cond->type) ? CC_G  : CC_A;  break;
            case ir_op_cmpge: cc = type_is_signed_scalar(cond->type) ? CC_L  : CC_B;  break;
            default: FAILWITH("Unreachable"); break;
            }
            asm_jcc(code->m, cc, LABEL(term->b1->asm_label, 0));
        } else if (cond->tag == ADDR_STACK_LOAD) {
            asm_cmp(code->m, ZB, INT(0), MEM_DR(cond->stack[0] + cond->stack[1], RBP));
            asm_je(code->m, LABEL(term->b1->asm_label, 0));
        } else if (cond->tag == ADDR_REGISTER) {
            asm_cmp(code->m, ZB, INT(0), REG(cond->i));
            asm_je(code->m, LABEL(term->b1->asm_label, 0));
        } else {
            FAILWITH("TODO: cond->tag == %s", cg_addr_tag_to_str(cond->tag));
        }
    } break;
    case ir_op_tailcall: FAILWITH("TODO: ir_op_tailcall"); break;
    default: FAILWITH("Unreachable"); break;
    }
}

KC_PRIVATE void
cg_emit_callee_save_code(size_t offset, struct cg_procedure *code)
{
    struct cg_context *ctx = &code->ctx;
    uint64_t saved = ctx->clobbered & REGC_CALLEE_SAVED;
    if (saved == 0) return;
    for (int i = 0; i < 0xf; ++i) {
        if (saved & (1lu << i)) {
            offset += 8;
            asm_mov(code->m, ZQ, REG(i), MEM_DR(-offset, RBP));
            size_t ip = asm_get_insertion_point(code->m);
            asm_set_insertion_at_end(code->m);
            asm_mov(code->m, ZQ, MEM_DR(-offset, RBP), REG(i));
            asm_set_insertion_point(code->m, ip);
        }
    }
}

KC_PRIVATE void
cg_emit_prologue_and_epilogue(struct cg_procedure *code, uint32_t proc_begin)
{
    Asm_module *m = code->m;
    struct cg_context *ctx = &code->ctx;
    asm_set_insertion_point(m, proc_begin);
    asm_dir_align(m, 16);
    asm_dir_label(m, ctx->proc_lbl);
    /* procedure entry */
    bool setup_frame = ctx->setup_frame;
    bool alloc_frame = false;
    size_t callee_save_offset = ctx->stack_size = align_adjust(ctx->stack_size, 8);
    ctx->stack_size += count_set_bits(ctx->clobbered & REGC_CALLEE_SAVED) * 8;
    ctx->stack_size = align_adjust(ctx->stack_size, 16);
    if (ctx->stack_size > 0) setup_frame = true;
    if (setup_frame) {
        asm_push(m, ZQ, REG(RBP));
        asm_mov(m, ZQ, REG(RSP), REG(RBP));
    }
    if (setup_frame && ctx->stack_size && !can_optimize_red_zone(ctx)) {
        alloc_frame = true;
        asm_sub(m, ZQ, INT(ctx->stack_size), REG(RSP));
    }
    if (ctx->has_large_retval) {
        struct cg_address *addr = &ctx->vars[ctx->rv_addr];
        if (addr->tag == ADDR_BLK_ARG) addr = &ctx->vars[addr->i];
        assert(addr->tag == ADDR_STACK_LOAD);
        asm_mov(m, ZQ, REG(cg_arg_regs[0]), MEM_DR(addr->stack[0] + addr->stack[1], RBP));
    }
    uint32_t sav = asm_get_insertion_point(m);
    asm_set_insertion_at_end(m);
    /* epilogue label */
    asm_dir_label(m, ctx->ret_lbl);
    /* callee save registers */
    asm_set_insertion_point(m, sav);
    cg_emit_callee_save_code(callee_save_offset, code);
    asm_set_insertion_at_end(m);
    /* procedure exit */
    if (alloc_frame) {
        asm_add(m, ZQ, INT(ctx->stack_size), REG(RSP));
        asm_pop(m, ZQ, REG(RBP));
    } else if (setup_frame) {
        asm_pop(m, ZQ, REG(RBP));
    }
    asm_ret(m);
}

KC_PUBLIC void
cg_emit_procedure(CG_module *mod, IR_object *obj, struct ir_toplevel *tl)
{
    assert(obj->tag == IRO_PROC || obj->tag == IRO_INIT_THUNK);
    asm_dir_section(&mod->as, ASM_SECTION_TEXT);
    struct da_pointers blks = ir_blk_reverse_post_order(obj->proc.entry);
    struct cg_procedure *code = da_allot(&mod->procs);
    if (obj->tag == IRO_INIT_THUNK)
        da_append(&mod->thunks, obj->hddr.asm_label);
    code->m = &mod->as;
    cg_create_context(obj, &blks, &code->ctx, mod);
    uint32_t proc_begin = asm_get_insertion_point(&mod->as);
    for (size_t i = 0; i < blks.len; ++i) {
        cg_emit_block(&blks, i, tl, code);
    }
    cg_emit_prologue_and_epilogue(code, proc_begin);
    da_free(&blks);
}

KC_PUBLIC void
cg_emit_data(CG_module *mod, struct ir_data *data)
{
    Asm_module *m = &mod->as;
    asm_dir_section(m, ASM_SECTION_DATA);
    asm_dir_align(m, data->alignment);
    asm_dir_label(m, data->asm_label);
    size_t size = data->size;
    size_t q = size / 8;
    size %= 8;
    size_t d = size / 4;
    size %= 4;
    size_t b = size;
    if (data->dat) {
        uint8_t *ptr = data->dat;
        while (q--) {
            asm_dir_int(m, ZQ, INT(*(uint64_t *)ptr));
            ptr += 8;
        }
        while (d--) {
            asm_dir_int(m, ZD, INT(*(uint32_t *)ptr));
            ptr += 4;
        }
        while (b--) {
            asm_dir_int(m, ZB, INT(*(uint8_t *)ptr));
            ptr += 1;
        }
    } else {
        while (q--) asm_dir_int(m, ZQ, INT(0));
        while (d--) asm_dir_int(m, ZD, INT(0));
        while (b--) asm_dir_int(m, ZB, INT(0));
    }
}

KC_PUBLIC void
cg_emit_module_code(CG_module *mod, struct ir_toplevel *ir, bool is_jit)
{
    asm_init_module(&mod->as, .is_jit = is_jit);
    void *dl_handle = NULL;
    if (is_jit) {
        dl_handle = dlopen(NULL, RTLD_NOW);
        if (dl_handle == NULL) FAILWITH("[ERROR] %s", dlerror());
    }
    /* initialize asm labels */
    for (size_t i = 0; i < ir->len; ++i) {
        IR_object *obj = &ir->elems[i];
        switch (obj->tag) {
        case IRO_PROC:
            obj->hddr.asm_label = asm_make_label(&mod->as, obj->hddr.link, ASM_LBL_T_FUNC, ASM_LBL_B_GLOBAL);
            break;
        case IRO_EXTERN_PROC: {
            uintptr_t ptr = 0;
            if (is_jit) {
                ptr = (uintptr_t)dlsym(dl_handle, obj->hddr.link);
                if (ptr == 0) FAILWITH("[ERROR] %s", dlerror());
            }
            obj->hddr.asm_label = asm_make_label(&mod->as, obj->hddr.link,
                                                 ASM_LBL_T_FUNC, ASM_LBL_B_GLOBAL,
                                                 .offset = ptr, .is_extern = true);
        } break;
        case IRO_DATA:
            obj->hddr.asm_label = asm_make_label(&mod->as, obj->hddr.link,
                                                 ASM_LBL_T_DATA, ASM_LBL_B_GLOBAL);
            cg_emit_data(mod, &obj->data);
            break;
        case IRO_EXTERN_DATA: {
            uintptr_t ptr = 0;
            if (is_jit) {
                ptr = (uintptr_t)dlsym(dl_handle, obj->hddr.link);
                if (ptr == 0) FAILWITH("[ERROR] %s", dlerror());
            }
            obj->hddr.asm_label = asm_make_label(&mod->as, obj->hddr.link,
                                                 ASM_LBL_T_DATA, ASM_LBL_B_GLOBAL,
                                                 .offset = ptr, .is_extern = true);
        } break;
        case IRO_INIT_THUNK:
            obj->hddr.asm_label = asm_make_label(&mod->as, obj->hddr.link,
                                                 ASM_LBL_T_FUNC, ASM_LBL_B_GLOBAL);
            if (!is_jit) {
                asm_dir_section(&mod->as, ASM_SECTION_INIT_ARRAY);
                asm_dir_int(&mod->as, ZQ, LABEL(obj->hddr.asm_label, 0));
            }
            break;
        default: FAILWITH("Unreachable"); break;
        }
    }
    if (is_jit) dlclose(dl_handle);
    /* generate code */
    for (size_t i = 0; i < ir->len; ++i) {
        IR_object *obj = &ir->elems[i];
        if (obj->tag == IRO_PROC || obj->tag == IRO_INIT_THUNK) {
            cg_emit_procedure(mod, obj, ir);
        }
    }
}
