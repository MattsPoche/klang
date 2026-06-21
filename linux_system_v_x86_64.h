#pragma once
#include "memas.h"

#define CG_REG_COUNT     16
#define CG_ARG_REG_COUNT 6

enum cg_register_bits {
	BIT_RAX = 1u << RAX,
	BIT_RDI = 1u << RDI,
	BIT_RSI = 1u << RSI,
	BIT_RDX = 1u << RDX,
	BIT_RCX = 1u << RCX,
	BIT_R8  = 1u << R8,
	BIT_R9  = 1u << R9,
	BIT_R10 = 1u << R10,
	BIT_R11 = 1u << R11,
	BIT_RBX = 1u << RBX,
	BIT_R12 = 1u << R12,
	BIT_R13 = 1u << R13,
	BIT_R14 = 1u << R14,
	BIT_R15 = 1u << R15,
};

enum cg_register_class {
	REGC_ARG          = BIT_RDI|BIT_RSI|BIT_RDX|BIT_RCX|BIT_R8|BIT_R9,
	REGC_RET          = BIT_RAX|BIT_RDX,
	REGC_CALLEE_SAVED = BIT_RBX|BIT_R12|BIT_R13|BIT_R14|BIT_R15,
	REGC_CALLER_SAVED = BIT_RAX|BIT_RDI|BIT_RSI|BIT_RDX|BIT_RCX|BIT_R8|BIT_R9|BIT_R10|BIT_R11,
	REGC_STATIC_CHAIN = BIT_R10,
};

enum cg_addr_tag {
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
struct cg_procedure;

struct cg_address {
	enum cg_addr_tag tag;
	KCType *type;
	union {
		int64_t i;
		float64_t d;
		float32_t f;
		int32_t stack[2]; /* stack address */
		int32_t wide[2];  /* two registers to hold value */
	};
	union {
		size_t stack_size;
		int64_t offset;
		struct defer_closure {
			void (*fun)(struct ir_ins *, void *, struct cg_procedure *);
			struct ir_ins *ins;
			void *dat;
		} defered;
	} extra;
};

struct cg_context {
	size_t stack_size;
	size_t arg_stack_size;
	struct cg_addresses {
		uint32_t len, cap;
		struct cg_address **elems;
	} funargs;
	size_t varc;
	struct cg_address *vars;
	int assigned[CG_REG_COUNT];
	int rv_addr;
	uint32_t clobbered;
	asm_label_id proc_lbl;
	asm_label_id ret_lbl;
	bool has_large_retval;
	bool setup_frame;
	bool is_leaf;
};

struct cg_procedure {
	Asm_module *m;
	struct cg_context ctx;
};

typedef struct cg_module {
	struct {
		uint32_t len, cap;
		struct cg_procedure *elems;
	} procs;
	struct {
		uint32_t len, cap;
		uint32_t *elems;
	} thunks;
	Asm_module as;
} CG_module;

typedef struct cg_modules {
	uint32_t len, cap;
	CG_module *elems;
} CG_modules;

#define REG_FREE -1
#define RED_ZONE_SIZE 128

KC_PUBLIC void cg_emit_module_code(CG_module *mod, struct ir_toplevel *ir, bool is_jit);
KC_PUBLIC void cg_emit_procedure(CG_module *mod, IR_object *obj, struct ir_toplevel *tl);
KC_PUBLIC void cg_emit_data(CG_module *mod, struct ir_data *data);
