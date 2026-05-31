#pragma once

struct da_pointers {
	uint32_t len, cap;
	void **elems;
};

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
	KCType *type;
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
	int64_t asm_label;
};

enum ir_obj_tag {
	IRO_PROC,
	IRO_DATA,
	IRO_EXTERN_DATA,
	IRO_EXTERN_PROC,
};

#define IR_OBJ_HDDR_MEMBERS						\
	enum ir_obj_tag tag;						\
	struct ir_proc *init_proc;					\
	char *link;									\
	int64_t asm_label;							\
	bool is_static

struct ir_obj_hddr {
	IR_OBJ_HDDR_MEMBERS;
};

struct ir_proc {
	IR_OBJ_HDDR_MEMBERS;
	struct definition *def;
	struct procedure *node;
	struct ir_blk *entry;
	KCType *type;
	uint16_t regc;
	uint16_t argc;
	uint16_t retc;
};

struct ir_data {
	IR_OBJ_HDDR_MEMBERS;
	size_t size;
	void *dat;
	KCType *type;
};

typedef union ir_object {
	struct ir_obj_hddr hddr;
	enum ir_obj_tag tag;
	struct ir_proc proc;
	struct ir_data data;
} IR_object;


typedef struct ir_toplevel {
	uint32_t len, cap;
	IR_object *elems;
} IR_toplevel;

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

KC_PUBLIC struct ir_toplevel ast_compile(struct scope *scope);
KC_PUBLIC struct ir_blk *ast_compile_expression(struct expression *exp, struct ast_comp_dest dst, size_t proc_id,
											 struct ir_blk *blk, struct scope *scope, struct ir_toplevel *tl);
KC_PUBLIC void ast_compile_procedure(size_t proc_id, struct ir_toplevel *tl);
KC_PUBLIC union ir_object *get_toplevel_obj(struct ir_toplevel *tl, size_t id);
KC_PUBLIC struct ir_proc *get_toplevel_proc(struct ir_toplevel *tl, size_t id);
KC_PUBLIC struct da_pointers ir_blk_reverse_post_order(struct ir_blk *root);
KC_PUBLIC void ir_proc_fprint(struct ir_proc *proc, FILE *file);
KC_PUBLIC struct expression *ast_desugar(Parser *p, struct expression *exp, struct scope *scope);
