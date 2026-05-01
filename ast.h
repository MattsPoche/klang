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
	op_cast,
	op_slice,
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
	unaop_cast		  = op_cast,
	unaop_slice       = op_slice,
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
	ast_type_f32,
	ast_type_f64,
	ast_type_app,
	ast_type_var,
	ast_type_ptr,
	ast_type_mut_ptr,
	ast_type_slice,
	ast_type_mut_slice,
	ast_type_array,
	ast_type_struct,
	ast_type_union,
	ast_type_istruct,
	ast_type_vector,
	ast_type_proc,
};

struct expression_stack {
	uint32_t len, cap;
	struct expression **elems;
};

struct symtbl_entry {
	struct token *name;
	enum symtbl_entry_tag {
		SYMTBL_VARIABL,
		SYMTBL_VALCONS,
	} tag;
	union {
		struct {
			uint32_t len, cap;
			struct valcons_entry {
				int64_t tag_val;
				struct type *type;
				struct type_definition *td;
			} *elems;
		} valcons;
		struct variable_entry {
			struct definition *def;
			struct expression *tl_exp;
		} variable;
	} as;
};

struct symtbl {
	uint32_t len, cap;
	struct symtbl_entry *elems;
};

struct typetbl {
	uint32_t len, cap;
	struct type_definition *elems;
};

struct scope {
	struct symtbl  symtbl;
	struct typetbl typetbl;
	struct scope  *parent;
};

struct type_ptrs {
	uint32_t len, cap;
	struct type **elems;
};

struct proc_type {
	struct type *ret;
	struct type_ptrs args;
};

struct array_type {
	struct type *base;
	size_t size;
	bool is_sized;
};

struct struct_type {
	uint32_t len, cap;
	struct struct_member {
		struct token *name;
		struct type *type;
	} *elems;
};

struct union_type {
	uint32_t len, cap;
	struct union_member {
		struct token *name;
		struct type *type;
		int64_t tag_value;
	} *elems;
};

struct type_app {
	struct type_ptrs args;
	struct token *cons;
};

enum type_class {
	type_class_any,
	type_class_signed_integer,
	type_class_unsigned_integer,
	type_class_integer,
	type_class_float,
	type_class_numeric,
	type_class_scalar,           // any non-aggregate type
	type_class_length,           // type that can be used with the #len operator
	type_class_indexable,
	type_class_struct,
	type_class_union,
	type_class_procedure,
};

typedef struct type_scheme {
	struct type_ptrs args;
	struct type *type;
} Forall;

struct type_env {
	struct strview name;
	Forall scheme;
	struct type_env *next;
};

struct typing_context {
	struct scope    *scope;
	struct type     *ret;
	struct type_env *env;
};

struct type_var {
	enum type_class class;
	struct token *name;
	struct type *forward;
	struct type *contains;
};

struct type {
	enum ast_type_tag tag;
	union {
		struct proc_type proc;
		struct array_type array;
		struct struct_type struct_t;
		struct union_type union_t;
		struct type_app app;
		struct type *ptr;
		struct type *mut_ptr;
		struct type *slice;
		struct type *mut_slice;
		struct token *basic;
		struct type_var var;
	} as;
};

struct procedure {
	struct def_array {
		uint32_t len, cap;
		struct definition *elems;
	} formals;
	struct type *ret;
	struct scope scope;
	struct expression *body;
};

struct value_cons {
	struct token *cons;
	struct expression *exp;
	int64_t tag_val;
};

struct definition {
	struct token *id;
	struct type *type;
	struct expression *exp;
	struct spec_array {
		uint32_t len, cap;
		struct type_spec {
			struct type *type;
			struct expression *exp;
			size_t ir_symbol;
		} *elems;
	} specs;
	size_t ir_symbol;
	bool is_mut;
	bool is_global;
	bool is_typechecked;
};

struct type_definition {
	struct token *name;
	struct type_ptrs args;
	struct type *type;
	bool is_alias;
	bool is_var;
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

struct case_branch {
	struct token *cons;
	struct definition binding;
	struct scope scope;
	struct expression *guard;
	struct expression *body;
	int64_t tag_val;
	bool binds_value;
	bool binding_is_ref;
};

struct exp_case {
	struct expression *cexp;
	struct expression *else_exp;
	struct case_branches {
		uint32_t len, cap;
		struct case_branch *elems;
	} branches;
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

struct cast {
	enum operator op;
	struct expression *exp;
	struct type *type;
};

struct call {
	enum operator op;
	struct expression *proc;
	struct expression_stack args;
};

struct index {
	enum operator op;
	struct expression *exp;
	struct expression *idx;
};

struct slice {
	enum operator op;
	struct expression *exp;
	struct expression *idx;
	struct expression *len;
};

struct initializer {
	struct expression_stack args;
};

enum ast_exp_tag {
	ast_exp_definition,
	ast_exp_let,
	ast_exp_literal,
	ast_exp_string,
	ast_exp_array_initializer,
	ast_exp_struct_initializer,
	ast_exp_named_struct_initializer,
	ast_exp_zero_struct_initializer,
	ast_exp_value_cons,
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
	bool is_lvalue;
	bool is_mutable;
	struct type *type;    // type anotation
	union _exp_internal {
		struct definition  def;
		struct let         let;
		struct literal     lit;
		struct exp_if      iff;
		struct exp_while   wloop;
		struct exp_case    ccase;
		struct call        call;
		struct cast        cast;
		struct index       idx;
		struct value_cons  valcons;
		struct procedure   proc;
		enum operator      op;
		struct unary       una;
		struct binary      bin;
		struct slice       slice;
		struct expression *get_ptr;
		struct expression *get_len;
		struct expression *ret;
		struct token      *str;
		struct expression_stack init;
		struct {
			struct expression_stack exps;
			struct token_ptrs ids;
		} named_init;
	} as;
};

struct type AST_TYPE_BOOL     = {.tag = ast_type_bool};
struct type AST_TYPE_VOID     = {.tag = ast_type_void};
struct type AST_TYPE_U8		  = {.tag = ast_type_u8};
struct type AST_TYPE_U16	  = {.tag = ast_type_u16};
struct type AST_TYPE_U32	  = {.tag = ast_type_u32};
struct type AST_TYPE_U64      = {.tag = ast_type_u64};
struct type AST_TYPE_I8       = {.tag = ast_type_i8};
struct type AST_TYPE_I16      = {.tag = ast_type_i16};
struct type AST_TYPE_I32      = {.tag = ast_type_i32};
struct type AST_TYPE_I64      = {.tag = ast_type_i64};
struct type AST_TYPE_F32      = {.tag = ast_type_f32};
struct type AST_TYPE_F64      = {.tag = ast_type_f64};
struct type AST_TYPE_STRING   = {.tag = ast_type_slice, .as.slice = &AST_TYPE_I8};

#define AST_TYPE_UNION_TAG AST_TYPE_I64
