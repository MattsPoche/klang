#pragma once

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

#define ASM_ARG_REG_COUNT 6

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

enum asm_addr_tag {
	ADDR_NONE,
	ADDR_ARGUMENT,
	ADDR_WIDE_ARG,
	ADDR_PUSH_ARG,
	ADDR_REGISTER,
	ADDR_TEMP_REG,
	ADDR_WIDE,
	ADDR_TEMP_WIDE,
	ADDR_IMM_INT,
	ADDR_IMM_FLOAT,
	ADDR_STACK,
	ADDR_STACK_LOAD,
	ADDR_STACK_ARG,
	ADDR_BLK_ARG,
	ADDR_SYMBOL,
	ADDR_FLAGS,
};

struct ir_ins;
struct asm_procedure;

struct asm_address {
	enum asm_addr_tag tag;
	KCType *type;
	union {
		int64_t i;
		double d;
		float f;
		int32_t stack[2]; /* stack address */
		int32_t wide[2];  /* two registers to hold value */
	} as;
	union {
		size_t stack_size;
		int64_t offset;
		struct defer_closure {
			void (*fun)(struct ir_ins *, void *, struct asm_procedure *);
			struct ir_ins *ins;
			void *dat;
		} defered;
	} extra;
};

struct asm_context {
	size_t stack_size;
	size_t arg_stack_size;
	struct asm_addresses {
		uint32_t len, cap;
		struct asm_address **elems;
	} funargs;
	size_t varc;
	struct asm_address *vars;
	int assigned[ASM_REG_COUNT];
	int rv_addr;
	uint32_t clobbered;
	bool has_large_retval;
	bool setup_frame;
	bool is_leaf;
};

struct asm_procedure {
	struct lines prologue;
	struct lines body;
	struct lines epilogue;
	struct asm_context ctx;
};

struct asm_datum {
	struct lines body;
};

struct asm_module {
	struct asm_procedures {
		uint32_t len, cap;
		struct asm_procedure *elems;
	} procs;
	struct asm_data {
		uint32_t len, cap;
		struct asm_datum *elems;
	} data;
};

struct asm_modules {
	uint32_t len, cap;
	struct asm_module *elems;
};

#define REG_FREE -1
#define RED_ZONE_SIZE 128

KC_PUBLIC void asm_emit_procedure(struct ir_proc *proc, struct ir_toplevel *tl, struct asm_procedure *code);
KC_PUBLIC void asm_dump_procedure(struct asm_procedure *proc, FILE *file);
KC_PUBLIC void asm_emit_datum(struct ir_data *data, struct asm_datum *asm_data);
KC_PUBLIC void asm_dump_module(struct asm_module *mod, FILE *file);
