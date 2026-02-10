#pragma once

#include "lex.h"

enum operator {
	op_sequence				= tt_semicolon, // ;
	op_add					= tt_plus,   // +
	op_sub					= tt_minus, // -
	op_mul					= tt_star, // *
	op_div					= tt_slash, // /
	op_mod					= tt_percent, // %
	op_lnot					= tt_tilde, // ~
	op_xor					= tt_caret, // ^
	op_land					= tt_and, // &
	op_lor					= tt_pipe, // |
	op_equal				= tt_equal, // =
	op_less_than			= tt_less, // <
	op_more_than			= tt_more, // >
	op_not					= tt_bang, // !
	op_member			    = tt_period, // .
	/* multi-character operators */
	op_assign				= tt_colon_equal,     // :=
	op_and_assign			= tt_and_equal,        // &=
	op_lor_assign			= tt_pipe_equal,      // |=
	op_xor_assign			= tt_caret_equal,     // ^=
	op_add_assign			= tt_plus_equal,      // +=
	op_sub_assign			= tt_minus_equal,     // -=
	op_mul_assign			= tt_star_equal,      // *=
	op_div_assign			= tt_slash_equal,     // /=
	op_mod_assign			= tt_percent_equal,   // %=
	op_not_equal			= tt_bang_equal,      // !=
	op_less_equal			= tt_less_equal,      // <=
	op_more_equal			= tt_more_equal,      // >=
	op_shift_left			= tt_less_less,       // <<
	op_shift_right			= tt_more_more,       // >>
	op_shift_left_assign	= tt_less_less_equal, // <<=
	op_shift_right_assign	= tt_more_more_equal, // >>=
	op_or					= tt_pipe_pipe,       // ||
	op_and					= tt_and_and,         // &&
	/* other operators */
	op_neg					= TOKEN_TYPE_MAX, // -
	op_pos, // +
	op_address_of, // &
	op_dereference, // *
	op_index, // [ ... ]
	op_call,
};

enum binop {
	binop_sequence			 = op_sequence,
	binop_add				 = op_add,
	binop_sub				 = op_sub,
	binop_mul				 = op_mul,
	binop_div				 = op_div,
	binop_mod				 = op_mod,
	binop_xor				 = op_xor,
	binop_land				 = op_land,
	binop_lor				 = op_lor,
	binop_equal				 = op_equal,
	binop_less_than			 = op_less_than,
	binop_more_than			 = op_more_than,
	binop_member			 = op_member,
	binop_assign			 = op_assign,
	binop_and_assign		 = op_and_assign,
	binop_lor_assign		 = op_lor_assign,
	binop_xor_assign		 = op_xor_assign,
	binop_add_assign		 = op_add_assign,
	binop_sub_assign		 = op_sub_assign,
	binop_mul_assign		 = op_mul_assign,
	binop_div_assign		 = op_div_assign,
	binop_mod_assign		 = op_mod_assign,
	binop_not_equal			 = op_not_equal,
	binop_less_equal		 = op_less_equal,
	binop_more_equal		 = op_more_equal,
	binop_shift_left		 = op_shift_left,
	binop_shift_right		 = op_shift_right,
	binop_shift_left_assign	 = op_shift_left_assign,
	binop_shift_right_assign = op_shift_right_assign,
	binop_or				 = op_or,
	binop_and				 = op_and,
};

enum unaop {
	unaop_lnot		  = op_lnot,
	unaop_not		  = op_not,
	unaop_neg		  = op_neg,
	unaop_pos		  = op_pos,
	unaop_address_of  = op_address_of,
	unaop_dereference = op_dereference,
	unaop_index		  = op_index,
	unaop_call		  = op_call,
};


enum ast_type_tag {
	ast_type_void = 0,
	ast_type_noreturn,
	ast_type_bool,
	ast_type_i8,
	ast_type_i16,
	ast_type_i32,
	ast_type_i64,
	ast_type_u8,
	ast_type_u16,
	ast_type_u32,
	ast_type_u64,
	ast_type_intlit,
	ast_type_f32,
	ast_type_f64,
	ast_type_floatlit,
	ast_type_alias,
	ast_type_cons,
	ast_type_ptr,
	ast_type_mut_ptr,
	ast_type_slice,
	ast_type_mut_slice,
	ast_type_array,
	ast_type_struct,
	ast_type_vector,
	ast_type_proc,
};

struct symtbl {
	uint32_t len, cap;
	struct definition **elems;
};

struct scope {
	struct symtbl symtbl;
	struct scope *parent;
};

struct proc_type {
	struct type *ret;
	struct type_ptrs {
		uint32_t len, cap;
		struct type **elems;
	} args;
};

enum array_type_size_tag {
	AT_UNSIZED,
	AT_LITERAL,
	AT_EXPRESSION,
};

struct array_type {
	struct type *base;
	enum array_type_size_tag stag;
	union {
		struct expression *exp;
		size_t sz;
	} size;
};

struct struct_type {
	uint32_t len, cap;
	struct struct_member {
		struct token *name;
		struct type *type;
	} *elems;
};

struct type_cons {
	struct token *name;
	struct type *arg;
};

struct type {
	enum ast_type_tag tag;
	union {
		struct proc_type proc;
		struct array_type array;
		struct struct_type strct;
		struct type_cons cons;
		struct type *ptr;
		struct type *mut_ptr;
		struct type *slice;
		struct type *mut_slice;
		struct token *basic;
		struct token *alias;
	} as;
};

struct formals {
	uint32_t len, cap;
	struct definition *elems;
};

struct procedure {
	struct formals formals;
	struct type *ret;
	struct scope scope;
	struct expression *body;
};

struct definition {
	struct token *id;
	struct type *type;
	struct expression *exp;
	size_t ir_symbol;
	bool is_mut;
	bool is_global;
};

struct let {
	struct definition def;
	struct scope scope;
	struct expression *body;
};

struct literal {
	struct token *token;
	union {
		int64_t i;
		double  d;
	} as;
};

struct exp_if {
	struct expression *cond;
	struct expression *tb;
	struct expression *fb;
};

struct exp_while {
	struct expression *cond;
	struct expression *body;
};

struct expression_stack {
	uint32_t len, cap;
	struct expression **elems;
};

struct case_branch {
	struct expression_stack matches;
	struct expression *exp;
};

struct case_branches {
	uint32_t len, cap;
	struct case_branch *elems;
};

struct exp_case {
	struct expression *cexp;
	struct expression *else_exp;
	struct case_branches branches;
};

struct binary {
	enum operator op;
	struct expression *left;
	struct expression *right;
};

struct unary {
	enum operator op;
	struct expression *exp;
};

struct call {
	enum operator op;
	struct expression *proc;
	struct expression_stack args;
};

struct index {
	enum operator op;
	struct expression *exp;
	struct type *type;      // optional type
	struct expression *idx;
};

struct initializer {
	struct expression_stack args;
};

struct named_initializer {
};

enum ast_exp_tag {
	ast_exp_definition,
	ast_exp_let,
	ast_exp_literal,
	ast_exp_string,
	ast_exp_initializer,
	ast_exp_named_initializer,
	ast_exp_zero_initializer,
	ast_exp_procedure_literal,
	ast_exp_undefined,
	ast_exp_ident,
	ast_exp_binary,
	ast_exp_unary,
	ast_exp_while,
	ast_exp_if,
	ast_exp_case,
	ast_exp_return,
	ast_exp_break,
	ast_exp_continue,
	ast_exp_extern_symbol,
	ast_exp_get_ptr,
	ast_exp_get_len,
};

struct expression {
	enum ast_exp_tag tag;
	struct token *tok;
	bool is_mutable;
	bool is_addressable;
	struct type *type;    // type anotation
	union _exp_internal {
		struct definition  def;
		struct let         let;
		struct literal     lit;
		struct exp_if      iff;
		struct exp_while   wloop;
		struct exp_case    ccase;
		struct call        call;
		struct index       idx;
		struct procedure   proc;
		enum operator      op;
		struct unary       una;
		struct binary      bin;
		struct expression *get_ptr;
		struct expression *get_len;
		struct expression *ret;
		struct token      *id;
		struct expression_stack init;
		struct {
			struct expression_stack exps;
			struct token_ptrs ids;
		} named_init;
	} as;
};

struct type AST_TYPE_BOOL   = {.tag = ast_type_bool};
struct type AST_TYPE_VOID   = {.tag = ast_type_void};
struct type AST_TYPE_U64    = {.tag = ast_type_u64};
struct type AST_TYPE_U32	= {.tag = ast_type_u32};
struct type AST_TYPE_U16	= {.tag = ast_type_u16};
struct type AST_TYPE_U8		= {.tag = ast_type_u8};
struct type AST_TYPE_I8     = {.tag = ast_type_i8};
struct type AST_TYPE_STRING = {.tag = ast_type_slice, .as.slice = &AST_TYPE_I8};
