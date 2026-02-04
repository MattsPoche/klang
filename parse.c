#pragma once

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "common.h"
#include "mem.h"
#include "strview.h"
#include "lex.h"
#include "ast.h"
#include "parse.h"
#include "log.h"

/* --- Parser --- */
static struct token *peek_token2(Parser *p)
{
	assert(p->tokens.idx < p->tokens.len - 1);
	return &p->tokens.elems[p->tokens.idx + 1];
}

static struct token *peek_token(Parser *p)
{
	assert(p->tokens.idx < p->tokens.len);
	return &p->tokens.elems[p->tokens.idx];
}

static struct token *next_token(Parser *p, struct token **rt)
{
	assert(p->tokens.len > 0);
	struct token *tok = &p->tokens.elems[p->tokens.idx];
	if (p->tokens.idx < p->tokens.len - 1) p->tokens.idx++;
	if (rt) *rt = tok;
	return tok;
}

static struct token *expect(Parser *p, struct token *token, enum token_type tag,
							const char *debug_filename, const int debug_line)
{
	if (token->tt != tag) {
		log_error_impl(p->lexer.filename, p->lexer.contents, token->sv, token->loc,
					   debug_filename, debug_line,
					   "Syntax error. Unexpected token `"SV_FMT"`.", SV_ARGS(&token->sv));
		exit(1);
	}
	return token;
}

#define ACCEPT(token, tag) ((token)->tt == (tag))
#define EXPECT(token, tag) expect(p, token, tag, __FILE__, __LINE__)


static struct type *parse_type(Parser *p);
static void parse_definition(Parser *p, struct definition *def);
static struct expression *parse_expression(Parser *p);

struct definition *symtbl_find(struct symtbl *symtbl, struct token *name)
{
	for (size_t i = 0; i < symtbl->len; ++i) {
		if (sv_is_equal(name->sv, symtbl->elems[i]->id->sv)) {
			return symtbl->elems[i];
		}
	}
	return NULL;
}

void symtbl_add(struct symtbl *symtbl, struct definition *def)
{
	if (symtbl_find(symtbl, def->id) == NULL) {
		da_append(symtbl, def);
	} else {
		FAILWITH("TODO: Variable already defined.");
	}
}

struct definition *lookup_definition(struct token *name, struct scope *scope)
{
	while (scope) {
		struct definition *def = symtbl_find(&scope->symtbl, name);
		if (def) return def;
		scope = scope->parent;
	}
	return NULL;
}

static struct proc_type procedure_type(struct procedure *proc)
{
	struct proc_type pt = {0};
	pt.ret = proc->ret;
	da_init(&pt.args);
	for (size_t i = 0; i < proc->formals.len; ++i) {
		struct definition *arg = &proc->formals.elems[i];
		da_append(&pt.args, arg->type);
	}
	return pt;
}

static struct type *parse_type(Parser *p)
{
	struct type *type = NULL;
	/* Build type signature */
	for (;;) {
		enum ast_type_tag tag;
		switch ((int)peek_token(p)->tt) {
		case tt_and: {
			struct token *tok;
			assert(type == NULL);
			next_token(p, NULL);
			if (ACCEPT(next_token(p, &tok), tt_bang)) {
				EXPECT(next_token(p, NULL), tt_lbracket);
				type = POOL_ALLOC(&p->data, struct type);
				type->tag = ast_type_mut_slice;
			} else {
				EXPECT(tok, tt_lbracket);
				type = POOL_ALLOC(&p->data, struct type);
				type->tag = ast_type_slice;
			}
			type->as.slice = parse_type(p);
			EXPECT(next_token(p, NULL), tt_rbracket);
		} break;
		case tt_lparen: {
			assert(type == NULL);
			next_token(p, NULL);
			type = POOL_ALLOC(&p->data, struct type);
			type->tag = ast_type_proc;
			da_init(&type->as.proc.args);
			/* parse formal parameter list */
			if (!ACCEPT(peek_token(p), tt_rparen)) {
				for (;;) {
					da_append(&type->as.proc.args, parse_type(p));
					if (ACCEPT(peek_token(p), tt_rparen)) break;
					EXPECT(next_token(p, NULL), tt_comma);
				}
			}
			next_token(p, NULL);
			EXPECT(next_token(p, NULL), tt_minus_more); // ->
			type->as.proc.ret = parse_type(p);
		} break;
		case tt_lbrace: {
			assert(type == NULL);
			next_token(p, NULL);
			type = POOL_ALLOC(&p->data, struct type);
			type->tag = ast_type_struct;
			struct struct_type st = {0};
			struct struct_member mem = {0};
			struct token *tok;
			if (peek_token(p)->tt == tt_ident
				&& peek_token2(p)->tt == tt_colon) {
				next_token(p, &mem.name);
				next_token(p, NULL);
				mem.type = parse_type(p);
				da_append(&st, mem);
				if (ACCEPT(next_token(p, &tok), tt_comma)) {
					if (ACCEPT(peek_token(p), tt_rbrace)) {
						FAILWITH("TODO: done parsing struct type.");
					} else {
						FAILWITH("TODO: continue parsing struct type.");
					}
				} else {
					EXPECT(tok, tt_rbrace);
					FAILWITH("TODO: done parsing struct type.");
				}
				FAILWITH("TODO: parse struct type.");
			} else {
				for (;;) {
					mem.type = parse_type(p);
					da_append(&st, mem);
					if (ACCEPT(next_token(p, &tok), tt_comma)) {
						if (ACCEPT(peek_token(p), tt_rbrace)) {
							next_token(p, NULL);
							break;
						}
					} else {
						EXPECT(tok, tt_rbrace);
						break;
					}
				}
			}
			type->as.strct = st;
		} break;
		case tt_lbracket: {
			assert(type == NULL);
			next_token(p, NULL);
			type = POOL_ALLOC(&p->data, struct type);
			type->tag = ast_type_array;
			type->as.array.base = parse_type(p);
			if (ACCEPT(peek_token(p), tt_comma)) {
				next_token(p, NULL);
				struct expression *exp = parse_expression(p);
				if (exp->tag == ast_exp_literal) {
					assert(exp->tok->tt == tt_intlit || exp->tok->tt == tt_hexlit);
					type->as.array.stag = AT_LITERAL;
					type->as.array.size.sz = exp->as.lit.as.i;
				} else {
					type->as.array.stag = AT_EXPRESSION;
					type->as.array.size.exp = exp;
				}
			} else {
				type->as.array.stag = AT_UNSIZED;
			}
			EXPECT(next_token(p, NULL), tt_rbracket);
		} break;
		case tt_star: {
			assert(type != NULL);
			next_token(p, NULL);
			struct type *tmp = type;
			type = POOL_ALLOC(&p->data, struct type);
			type->tag = ast_type_ptr;
			type->as.ptr = tmp;
		} break;
		case tt_bang: {
			assert(type != NULL);
			next_token(p, NULL);
			struct type *tmp = type;
			type = POOL_ALLOC(&p->data, struct type);
			type->tag = ast_type_mut_ptr;
			type->as.ptr = tmp;
		} break;
		case tt_void: tag = ast_type_void; goto basic_type;
		case tt_bool: tag = ast_type_bool; goto basic_type;
		case tt_i8:   tag = ast_type_i8;   goto basic_type;
		case tt_i16:  tag = ast_type_i16;  goto basic_type;
		case tt_i32:  tag = ast_type_i32;  goto basic_type;
		case tt_i64:  tag = ast_type_i64;  goto basic_type;
		case tt_u8:	  tag = ast_type_u8;   goto basic_type;
		case tt_u16:  tag = ast_type_u16;  goto basic_type;
		case tt_u32:  tag = ast_type_u32;  goto basic_type;
		case tt_u64:  tag = ast_type_u64;  goto basic_type;
		case tt_f32:  tag = ast_type_f32;  goto basic_type;
		case tt_f64:  tag = ast_type_f64;  goto basic_type;
		basic_type:
			assert(type == NULL);
			type = POOL_ALLOC(&p->data, struct type);
			type->tag = tag;
			type->as.basic = next_token(p, NULL);
			break;
		case tt_ident:
			type = POOL_ALLOC(&p->data, struct type);
			if (type == NULL) {
				type->tag = ast_type_alias;
				type->as.alias = next_token(p, NULL);
			} else {
				struct type *tmp = type;
				type->tag = ast_type_cons;
				type->as.cons.name = next_token(p, NULL);
				type->as.cons.arg = tmp;
			}
			break;
		default: goto done;
		}
	}
done:
	return type;
}

static void parse_definition(Parser *p, struct definition *def)
{
	/* We enter with `let` token already consumed */
	struct token *tok;
	if (ACCEPT(next_token(p, &tok), tt_mut)) {
		def->is_mut = true;
		next_token(p, &tok);
	} else {
		def->is_mut = false;
	}
	EXPECT(tok, tt_ident);
	def->id = tok;
	if (ACCEPT(next_token(p, &tok), tt_colon)) {
		/* variable definition */
		def->type = parse_type(p);
		EXPECT(next_token(p, NULL), tt_equal);
		def->exp = parse_expression(p);
	} else {
		/* procedure definition */
		EXPECT(tok, tt_lparen);
		struct expression *proc = POOL_ALLOC(&p->data, struct expression);
		proc->tok = def->id;
		proc->tag = ast_exp_procedure_literal;
		/* parse formal parameter list */
		if (!ACCEPT(peek_token(p), tt_rparen)) {
			for (;;) {
				struct definition arg = {0};
				if (ACCEPT(next_token(p, &tok), tt_mut)) {
					arg.is_mut = true;
					next_token(p, &tok);
				} else {
					arg.is_mut = false;
				}
				EXPECT(tok, tt_ident);
				EXPECT(next_token(p, NULL), tt_colon);
				arg.id = tok;
				arg.type = parse_type(p);
				da_append(&proc->as.proc.formals, arg);
				if (ACCEPT(peek_token(p), tt_rparen)) break;
				EXPECT(next_token(p, NULL), tt_comma);
			}
		}
		next_token(p, NULL);
		/* parse return type */
		if (ACCEPT(peek_token(p), tt_equal)) {
			proc->as.proc.ret = &AST_TYPE_VOID;
		} else {
			proc->as.proc.ret = parse_type(p);
		}
		def->type = POOL_ALLOC(&p->data, struct type);
		def->type->tag = ast_type_proc;
		def->type->as.proc = procedure_type(&proc->as.proc);
		/* parse body */
		EXPECT(next_token(p, NULL), tt_equal);
		proc->as.proc.body = parse_expression(p);
		def->exp = proc;
	}
}

struct expression *parse_toplevel_expression(Parser *p, struct symtbl *symtbl)
{
	struct token *tok;
	struct expression *exp = NULL;
	switch ((int)next_token(p, &tok)->tt) {
	case tt_let:
		exp = POOL_ALLOC(&p->data, struct expression);
		exp->tok = tok;
		exp->tag = ast_exp_definition;
		parse_definition(p, &exp->as.def);
		symtbl_add(symtbl, &exp->as.def);
		break;
	case tt_eof: break;
	default: {
		char str[0x1000] = {0};
		printf("last = %s\n", show_token(str, sizeof(str), tok));
		FAILWITH("TODO: parse_expression.");
	} break;
	}
	return exp;
}

#define ASSOC_LEFT(x)   (-(x))
#define ASSOC_RIGHT(x)  (x)
#define ASSOC_LEFT_P(x) ((x) < 0)

static int precedence(enum operator op)
{
	switch (op) {
	case op_sequence:
		return ASSOC_LEFT(1);
	case op_assign:
	case op_and_assign:
	case op_lor_assign:
	case op_xor_assign:
	case op_add_assign:
	case op_sub_assign:
	case op_mul_assign:
	case op_div_assign:
	case op_mod_assign:
	case op_shift_left:
	case op_shift_right:
	case op_shift_left_assign:
	case op_shift_right_assign:
		return ASSOC_RIGHT(2);
	case op_or:
		return ASSOC_LEFT(3);
	case op_and:
		return ASSOC_LEFT(4);
	case op_lor:
		return ASSOC_LEFT(5);
	case op_xor:
		return ASSOC_LEFT(6);
	case op_land:
		return ASSOC_LEFT(7);
	case op_equal:
	case op_not_equal:
		return ASSOC_LEFT(8);
	case op_less_equal:
	case op_more_equal:
	case op_less_than:
	case op_more_than:
		return ASSOC_LEFT(9);
	case op_add:
	case op_sub:
		return ASSOC_LEFT(10);
	case op_mul:
	case op_div:
	case op_mod:
		return ASSOC_LEFT(11);
	case op_lnot:
	case op_not:
	case op_neg:
	case op_pos:
	case op_address_of:
	case op_dereference:
		return ASSOC_RIGHT(12);
	case op_index:
	case op_member:
	case op_call:
		return ASSOC_LEFT(13);
	default:
		FAILWITH("Unknown operator.");
		return 0;
	}
}

static bool check_precedence(enum operator op1, enum operator op2)
{
	int p1 = precedence(op1);
	int p2 = precedence(op2);
	return (ABS(p1) < ABS(p2) || (ABS(p1) == ABS(p2) && ASSOC_LEFT_P(p1)));
}

static bool shunt(struct expression *op1, struct expression_stack *out, struct expression_stack *ops)
{
	struct expression *op2;
	/* we treat NULL as parentheses */
	if (op1 == NULL) {
		if (ops->len == 0) return 0;
		/* while ((op2 = da_pop(ops)) != NULL) { */
		for (;;) {
			if (ops->len == 0) return 0;
			if ((op2 = da_pop(ops)) == NULL) return 1;
			da_append(out, op2);
		}
		FAILWITH("Unreachable");
		return 1;
	}
	while (ops->len > 0
		   && (op2 = da_peek(ops)) != NULL
		   && check_precedence(op1->as.op, op2->as.op)) {
		da_append(out, da_pop(ops));
	}
	da_append(ops, op1);
	return 1;
}

static struct expression *build_expression_tree(struct expression_stack *out)
{
	struct expression_stack stack = {0};
	struct expression *exp;
	for (size_t i = 0; i < out->len; ++i) {
		exp = out->elems[i];
		if (exp->tag == ast_exp_binary) {
			assert(stack.len >= 2);
			exp->as.bin.right = da_pop(&stack);
			exp->as.bin.left = da_pop(&stack);
		} else if (exp->tag == ast_exp_unary) {
			assert(stack.len >= 1);
			exp->as.una.exp = da_pop(&stack);
		}
		da_append(&stack, exp);
	}
	assert(stack.len == 1);
	exp = da_pop(&stack);
	da_free(&stack);
	return exp;
}

static struct expression *parse_if(Parser *p, struct expression *exp)
{
	struct token *tok = NULL;
	exp->tag = ast_exp_if;
	exp->as.iff.cond = parse_expression(p);
	EXPECT(next_token(p, NULL), tt_then);
	exp->as.iff.tb = parse_expression(p);
	if (ACCEPT(next_token(p, &tok), tt_elif)) {
		exp->as.iff.fb = parse_if(p, POOL_ALLOC(&p->data, struct expression));
	} else if (ACCEPT(tok, tt_else)) {
		exp->as.iff.fb = parse_expression(p);
		EXPECT(next_token(p, NULL), tt_end);
	} else {
		exp->as.iff.fb = NULL;
		EXPECT(tok, tt_end);
	}
	return exp;
}

static struct expression *parse_expression(Parser *p)
{
	struct expression_stack out = {0};
	struct expression_stack ops = {0};
	struct token *tok = NULL;
	struct expression *exp = NULL;
	bool op_prev = true; // decide if operator should be treated as unary
	/* shunting yard */
	for (;;) {
		exp = POOL_ALLOC(&p->data, struct expression);
	repeat:
		tok = NULL;
		enum token_type tt = peek_token(p)->tt;
		switch (tt) {
		case tt_let:
			if (op_prev == false) goto flush; // in this case let acts as a terminator
			op_prev = false;
			next_token(p, &exp->tok);
			exp->tag = ast_exp_let;
			parse_definition(p, &exp->as.def);
			EXPECT(next_token(p, NULL), tt_in);
			exp->as.let.body = parse_expression(p);
			da_append(&out, exp);
			break;
		case tt_if:
			assert(op_prev == true);
			op_prev = false;
			next_token(p, &exp->tok);
			da_append(&out, parse_if(p, exp));
			break;
		case tt_case: {
			assert(op_prev == true);
			op_prev = false;
			next_token(p, &exp->tok);
			exp->tag = ast_exp_case;
			struct exp_case *c = &exp->as.ccase;
			c->cexp = parse_expression(p);
			/* parse branches */
			for (;;) {
				if (ACCEPT(next_token(p, &tok), tt_of)) {
					struct case_branch branch = {0};
					for (;;) { // parse matches
						da_append(&branch.matches, parse_expression(p));
						if (ACCEPT(next_token(p, &tok), tt_do)) break;
						EXPECT(tok, tt_comma);
					}
					branch.exp = parse_expression(p);
					da_append(&c->branches, branch);
					if (ACCEPT(peek_token(p), tt_end)) {
						next_token(p, NULL);
						break;
					}
				} else {
					EXPECT(tok, tt_else);
					c->else_exp = parse_expression(p);
					EXPECT(next_token(p, NULL), tt_end);
					break;
				}
			}
			assert(c->branches.len > 0);
			da_append(&out, exp);
		} break;
		case tt_underscore: FAILWITH("TODO: tt_underscore"); break;
		case tt_mut: FAILWITH("TODO: tt_mut"); break;
		case tt_tick: FAILWITH("TODO: tt_tick"); break;
		case tt_at: FAILWITH("TODO: tt_at"); break;
		case tt_hash: FAILWITH("TODO: tt_hash"); break;
		case tt_dollar: FAILWITH("TODO: tt_dollar"); break;
		case tt_backslash: FAILWITH("TODO: tt_backslash"); break;
		case tt_colon: FAILWITH("TODO: tt_colon"); break;
		case tt_quote: FAILWITH("TODO: tt_quote"); break;
		case tt_double_quote: FAILWITH("TODO: tt_double_quote"); break;
		case tt_question: FAILWITH("TODO: tt_question"); break;
		case tt_as: FAILWITH("TODO: tt_as"); break;
		case tt_while: FAILWITH("TODO: tt_while"); break;
		case tt_for: FAILWITH("TODO: tt_for"); break;
		case tt_break: FAILWITH("TODO: tt_break"); break;
		case tt_continue: FAILWITH("TODO: tt_continue"); break;
		case tt_void: FAILWITH("TODO: tt_void"); break;
		case tt_bool: FAILWITH("TODO: tt_bool"); break;
		case tt_i8: FAILWITH("TODO: tt_i8"); break;
		case tt_i16: FAILWITH("TODO: tt_i16"); break;
		case tt_i32: FAILWITH("TODO: tt_i32"); break;
		case tt_i64: FAILWITH("TODO: tt_i64"); break;
		case tt_u8: FAILWITH("TODO: tt_u8"); break;
		case tt_u16: FAILWITH("TODO: tt_u16"); break;
		case tt_u32: FAILWITH("TODO: tt_u32"); break;
		case tt_u64: FAILWITH("TODO: tt_u64"); break;
		case tt_f32: FAILWITH("TODO: tt_f32"); break;
		case tt_f64: FAILWITH("TODO: tt_f64"); break;
		case tt_return:
			assert(op_prev == true);
			op_prev = false;
			next_token(p, &exp->tok);
			exp->tag = ast_exp_return;
			if (ACCEPT(next_token(p, &tok), tt_lbrace)) { // return struct literal
				FAILWITH("TODO: parse struct literal");
			} else if (ACCEPT(tok, tt_rbracket)) { // return array literal
				FAILWITH("TODO: parse array literal");
			} else { // return expression
				EXPECT(tok, tt_lparen);
				exp->as.ret = parse_expression(p);
				EXPECT(next_token(p, NULL), tt_rparen);
			}
			da_append(&out, exp);
			break;
		case tt_extern:
			/* TODO: check that string is a valid symbol.
			 */
			assert(op_prev == true);
			op_prev = false;
			exp->tok = next_token(p, NULL);
			EXPECT(next_token(p, NULL), tt_lparen);
			EXPECT(next_token(p, &exp->tok), tt_string);
			EXPECT(next_token(p, NULL), tt_rparen);
			exp->tag = ast_exp_extern_symbol;
			da_append(&out, exp);
			break;
		case tt_ptr:
			assert(op_prev == true);
			op_prev = false;
			exp->tok = next_token(p, NULL);
			exp->tag = ast_exp_get_ptr;
			EXPECT(next_token(p, NULL), tt_lparen);
			exp->as.get_ptr = parse_expression(p);
			EXPECT(next_token(p, NULL), tt_rparen);
			da_append(&out, exp);
			break;
		case tt_len:
			assert(op_prev == true);
			op_prev = false;
			exp->tok = next_token(p, NULL);
			exp->tag = ast_exp_get_len;
			EXPECT(next_token(p, NULL), tt_lparen);
			exp->as.get_ptr = parse_expression(p);
			EXPECT(next_token(p, NULL), tt_rparen);
			da_append(&out, exp);
			break;
		case tt_ident:
			assert(op_prev == true);
			op_prev = false;
			exp->tag = ast_exp_ident;
			exp->tok = next_token(p, &exp->as.id);
			da_append(&out, exp);
			break;
		case tt_string:
			assert(op_prev == true);
			op_prev = false;
			exp->tag = ast_exp_string;
			exp->tok = next_token(p, &exp->as.id);
			da_append(&out, exp);
			break;
		case tt_true:
			assert(op_prev == true);
			op_prev = false;
			exp->tok = next_token(p, &tok);
			exp->tag = ast_exp_literal;
			exp->as.lit.token = tok;
			exp->as.lit.as.i = 1;
			da_append(&out, exp);
			break;
		case tt_false:
			assert(op_prev == true);
			op_prev = false;
			exp->tok = next_token(p, &tok);
			exp->tag = ast_exp_literal;
			exp->as.lit.token = tok;
			exp->as.lit.as.i = 0;
			da_append(&out, exp);
			break;
		case tt_intlit:
			assert(op_prev == true);
			op_prev = false;
			exp->tok = next_token(p, &tok);
			exp->tag = ast_exp_literal;
			exp->as.lit.token = tok;
			assert(sv_to_int(tok->sv, &exp->as.lit.as.i));
			da_append(&out, exp);
			break;
		case tt_hexlit:
			assert(op_prev == true);
			op_prev = false;
			exp->tag = ast_exp_literal;
			exp->tok = next_token(p, &tok);
			exp->as.lit.token = tok;
			assert(sv_to_int(tok->sv, &exp->as.lit.as.i));
			da_append(&out, exp);
			break;
		case tt_floatlit: FAILWITH("TODO: tt_floatlit"); break;
		case tt_undefined:
			assert(op_prev == true);
			op_prev = false;
			exp->tok = next_token(p, &tok);
			exp->tag = ast_exp_undefined;
			exp->as.lit.token = tok;
			exp->as.lit.as.i = 0;
			da_append(&out, exp);
			break;
		case tt_noreturn: FAILWITH("TODO: tt_noreturn"); break;
		case tt_lbrace:
			next_token(p, &exp->tok);
			assert(op_prev == true);
			op_prev = false;
			if (ACCEPT(peek_token(p), tt_ident) && ACCEPT(peek_token2(p), tt_equal)) {
				exp->tag = ast_exp_named_initializer;
				struct expression_stack exps = {0};
				struct token_ptrs ids = {0};
				for (;;) {
					EXPECT(next_token(p, &tok), tt_ident);
					da_append(&ids, tok);
					EXPECT(next_token(p, NULL), tt_equal);
					da_append(&exps, parse_expression(p));
					if (ACCEPT(next_token(p, &tok), tt_rbrace)) break;
					EXPECT(tok, tt_comma);
					/* allow trailing comma */
					if (ACCEPT(peek_token(p), tt_rbrace)) {
						next_token(p, NULL);
						break;
					}
				}
				exp->as.named_init.ids = ids;
				exp->as.named_init.exps = exps;
			} else if (ACCEPT(peek_token(p), tt_rbrace)) {
				next_token(p, &exp->tok);
				exp->tag = ast_exp_zero_initializer;
			} else {
				exp->tag = ast_exp_initializer;
				struct expression_stack exps = {0};
				if (peek_token(p)->tt != tt_rbrace) {
					for (;;) {
						da_append(&exps, parse_expression(p));
						if (ACCEPT(next_token(p, &tok), tt_rbrace)) break;
						EXPECT(tok, tt_comma);
						/* allow trailing comma */
						if (ACCEPT(peek_token(p), tt_rbrace)) {
							next_token(p, NULL);
							break;
						}
					}
				}
				exp->as.init = exps;
			}
			da_append(&out, exp);
			break;
		case tt_lbracket:
			// TODO: slice constructor
			next_token(p, &exp->tok);
			assert(op_prev == false);
			exp->tag = ast_exp_unary;
			exp->as.idx.op = op_index;
			exp->as.idx.idx = parse_expression(p);
			EXPECT(next_token(p, NULL), tt_rbracket);
			shunt(exp, &out, &ops);
			break;
			/* operators */
		case tt_lparen: {
			next_token(p, &exp->tok);
			if (op_prev) {
				da_append(&ops, NULL); // Null is interperated as lparen
				goto repeat;
			}
			op_prev = false;
			exp->tag = ast_exp_unary;
			exp->as.call.op = op_call;
			exp->as.call.proc = NULL;
			struct expression_stack *args = &exp->as.call.args;
			da_init(args);
			if (!ACCEPT(peek_token(p), tt_rparen)) {
				for (;;) {
					da_append(args, parse_expression(p));
					if (ACCEPT(peek_token(p), tt_rparen)) break;
					EXPECT(next_token(p, NULL), tt_comma);
				}
			}
			next_token(p, NULL);
			shunt(exp, &out, &ops);
		} break;
		case tt_rparen:
			assert(op_prev == false);
			if (shunt(NULL, &out, &ops)) {
				next_token(p, NULL);
				goto repeat;
			} else {
				goto flush;
			}
			break;
			/* binary ops */
		case tt_semicolon:
		case tt_slash:
		case tt_percent:
		case tt_caret:
		case tt_pipe:
		case tt_colon_equal:
		case tt_and_equal:
		case tt_pipe_equal:
		case tt_caret_equal:
		case tt_plus_equal:
		case tt_minus_equal:
		case tt_star_equal:
		case tt_slash_equal:
		case tt_percent_equal:
		case tt_equal:
		case tt_less:
		case tt_more:
		case tt_bang_equal:
		case tt_less_equal:
		case tt_more_equal:
		case tt_less_less:
		case tt_more_more:
		case tt_less_less_equal:
		case tt_more_more_equal:
		case tt_pipe_pipe:
		case tt_and_and:
		case tt_period:
			assert(op_prev == false);
			op_prev = true;
			next_token(p, &exp->tok);
			exp->tag = ast_exp_binary;
			exp->as.bin.op = (enum operator)tt;
			shunt(exp, &out, &ops);
			break;
			/* unary ops */
		case tt_tilde:
		case tt_bang:
			assert(op_prev == true);
			next_token(p, &exp->tok);
			exp->tag = ast_exp_unary;
			exp->as.una.op = (enum operator)tt;
			shunt(exp, &out, &ops);
			break;
			/* binary or unary ops */
		case tt_plus:
			next_token(p, &exp->tok);
			if (op_prev) {
				exp->tag = ast_exp_unary;
				exp->as.una.op = op_pos;
				shunt(exp, &out, &ops);
			} else {
				op_prev = true;
				exp->tag = ast_exp_binary;
				exp->as.bin.op = (enum operator)tt;
				shunt(exp, &out, &ops);
			}
			break;
		case tt_minus:
			next_token(p, &exp->tok);
			if (op_prev) {
				exp->tag = ast_exp_unary;
				exp->as.una.op = op_neg;
				shunt(exp, &out, &ops);
			} else {
				op_prev = true;
				exp->tag = ast_exp_binary;
				exp->as.bin.op = (enum operator)tt;
				shunt(exp, &out, &ops);
			}
			break;
		case tt_star:
			next_token(p, &exp->tok);
			if (op_prev) {
				exp->tag = ast_exp_unary;
				exp->as.una.op = op_dereference;
				shunt(exp, &out, &ops);
			} else {
				op_prev = true;
				exp->tag = ast_exp_binary;
				exp->as.bin.op = (enum operator)tt;
				shunt(exp, &out, &ops);
			}
			break;
		case tt_and:
			next_token(p, &exp->tok);
			if (op_prev) {
				exp->tag = ast_exp_unary;
				exp->as.una.op = op_address_of;
				shunt(exp, &out, &ops);
			} else {
				op_prev = true;
				exp->tag = ast_exp_binary;
				exp->as.bin.op = (enum operator)tt;
				shunt(exp, &out, &ops);
			}
			break;
			/* terminators */
		case tt_rbrace:
		case tt_rbracket:
		case tt_comma:
		case tt_in:
		case tt_end:
		case tt_done:
		case tt_then:
		case tt_else:
		case tt_elif:
		case tt_of:
		case tt_do:
		case tt_minus_more:
		case tt_eof:
			assert(op_prev == false);
			goto flush;
		case TOKEN_TYPE_MAX:
			FAILWITH("Invalid token (parse_expression)");
			break;
		default: FAILWITH("Unreachable"); break;
		}
	}
flush:
	while (ops.len) da_append(&out, da_pop(&ops));
	exp = build_expression_tree(&out);
	da_free(&out);
	da_free(&ops);
	return exp;
}

/* --- AST Printer --- */
static void ast_def_fprint(struct definition *def, FILE *file);
static void ast_type_fprint(struct type *t, FILE *file);

void ast_type_fprint(struct type *t, FILE *file)
{
	switch (t->tag) {
	case ast_type_void:		fputs("void", file); break;
	case ast_type_noreturn: fputs("noreturn", file); break;
	case ast_type_bool:		fputs("bool", file); break;
	case ast_type_i8:		fputs("i8", file); break;
	case ast_type_i16:		fputs("i16", file); break;
	case ast_type_i32:		fputs("i32", file); break;
	case ast_type_i64:		fputs("i64", file); break;
	case ast_type_u8:		fputs("u8", file); break;
	case ast_type_u16:		fputs("u16", file); break;
	case ast_type_u32:		fputs("u32", file); break;
	case ast_type_u64:		fputs("u64", file); break;
	case ast_type_f32:		fputs("f32", file); break;
	case ast_type_f64:		fputs("f64", file); break;
	case ast_type_intlit:
		fputs("_builtin_intlit_t", file);
		break;
	case ast_type_floatlit:
		fputs("_builtin_floatlit_t", file);
		break;
	case ast_type_alias:
		FAILWITH("TODO: print type alias.");
		break;
	case ast_type_cons:
		FAILWITH("TODO: print type cons.");
		break;
	case ast_type_proc:
		fputc('(', file);
		if (t->as.proc.args.len > 0) {
			ast_type_fprint(t->as.proc.args.elems[0], file);
			for (size_t i = 1; i < t->as.proc.args.len; ++i) {
				fputs(", ", file);
				ast_type_fprint(t->as.proc.args.elems[i], file);
			}
		}
		fputs(") -> ", file);
		ast_type_fprint(t->as.proc.ret, file);
		break;
	case ast_type_struct:
		fputc('{', file);
		if (t->as.strct.len > 0) {
			struct struct_member *mem = &t->as.strct.elems[0];
			if (mem->name) fprintf(file, SV_FMT": ", SV_ARGS(&mem->name->sv));
			ast_type_fprint(mem->type, file);
			for (size_t i = 1; i < t->as.strct.len; ++i) {
				mem = &t->as.strct.elems[i];
				fputs(", ", file);
				if (mem->name) fprintf(file, SV_FMT": ", SV_ARGS(&mem->name->sv));
				ast_type_fprint(mem->type, file);
			}
		}
		fputc('}', file);
		break;
	case ast_type_array:
		fputc('[', file);
		ast_type_fprint(t->as.array.base, file);
		if (t->as.array.stag == AT_EXPRESSION) {
			fputs(", ", file);
			ast_fprint(t->as.array.size.exp, file);
		} else if (t->as.array.stag == AT_LITERAL) {
			fprintf(file, ", %zu", t->as.array.size.sz);
		}
		fputc(']', file);
		break;
	case ast_type_vector:
		FAILWITH("TODO: print vector type.");
		break;
	case ast_type_mut_ptr:
		ast_type_fprint(t->as.ptr, file);
		fputc('!', file);
		break;
	case ast_type_ptr:
		ast_type_fprint(t->as.ptr, file);
		fputc('*', file);
		break;
	case ast_type_slice:
		fputs("&[", file);
		ast_type_fprint(t->as.slice, file);
		fputc(']', file);
		break;
	case ast_type_mut_slice:
		FAILWITH("TODO: ast_type_mut_slice.");
#if 0
		fputs("&![", file);
		ast_type_fprint(t->as.slice, file);
		fputs(", ", file);
		ast_fprint(t->as.array.size, file);
		fputc(']', file);
#endif
		break;
	default:
		FAILWITH("Unreachable condition.");
		break;
	}
}

static void ast_def_fprint(struct definition *def, FILE *file)
{
	fputs("let ", file);
	if (def->is_mut) fputs("mut ", file);
	fprintf(file, SV_FMT, SV_ARGS(&def->id->sv));
	if (def->type->tag == ast_type_proc
		&& def->exp->tag == ast_exp_procedure_literal) {
		struct procedure *proc = &def->exp->as.proc;
		fputc('(', file);
		if (proc->formals.len > 0) {
			struct definition *arg = &proc->formals.elems[0];
			fprintf(file, SV_FMT": ", SV_ARGS(&arg->id->sv));
			ast_type_fprint(arg->type, file);
			for (size_t i = 1; i < proc->formals.len; ++i) {
				arg = &proc->formals.elems[i];
				fprintf(file, ", "SV_FMT": ", SV_ARGS(&arg->id->sv));
				ast_type_fprint(arg->type, file);
			}
		}
		fputs(") ", file);
		ast_type_fprint(proc->ret, file);
		fputs(" =\n", file);
		ast_fprint(proc->body, file);
	} else {
		fputs(": ", file);
		ast_type_fprint(def->type, file);
		fputs(" = ", file);
		ast_fprint(def->exp, file);
	}
}

void ast_fprint(struct expression *exp, FILE *file)
{
	switch (exp->tag) {
	case ast_exp_definition:
		ast_def_fprint(&exp->as.def, file);
		break;
	case ast_exp_let:
		ast_def_fprint(&exp->as.let.def, file);
		fputs(" in\n", file);
		ast_fprint(exp->as.let.body, file);
		break;
	case ast_exp_literal:
		fprintf(file, SV_FMT, SV_ARGS(&exp->as.lit.token->sv));
		break;
	case ast_exp_procedure_literal:
		FAILWITH("TODO: ast_fprint ast_exp_procedure_literal");
		break;
	case ast_exp_undefined:
		fputs("undefined", file);
		break;
	case ast_exp_zero_initializer:
		fputs("{}", file);
		break;
	case ast_exp_initializer:
		fputc('{', file);
		if (exp->as.init.len > 0) {
			ast_fprint(exp->as.init.elems[0], file);
			for (size_t i = 1; i < exp->as.init.len; ++i) {
				fputs(", ", file);
				ast_fprint(exp->as.init.elems[i], file);
			}
		}
		fputc('}', file);
		break;
	case ast_exp_named_initializer: {
		struct token_ptrs *ids = &exp->as.named_init.ids;
		struct expression_stack *exps = &exp->as.named_init.exps;
		fputc('{', file);
		if (ids->len > 0) {
			fprintf(file, SV_FMT" = ", SV_ARGS(&ids->elems[0]->sv));
			ast_fprint(exps->elems[0], file);
			for (size_t i = 1; i < ids->len; ++i) {
				fputs(", ", file);
				fprintf(file, SV_FMT" = ", SV_ARGS(&ids->elems[i]->sv));
				ast_fprint(exps->elems[i], file);
			}
		}
		fputc('}', file);
	} break;
	case ast_exp_string:
	case ast_exp_ident:
		fprintf(file, SV_FMT, SV_ARGS(&exp->as.id->sv));
		break;
	case ast_exp_get_ptr:
		fputs("#ptr(", file);
		ast_fprint(exp->as.get_ptr, file);
		fputc(')', file);
		break;
	case ast_exp_get_len:
		fputs("#len(", file);
		ast_fprint(exp->as.get_ptr, file);
		fputc(')', file);
		break;
	case ast_exp_extern_symbol:
		fprintf(file, "#extern("SV_FMT")", SV_ARGS(&exp->tok->sv));
		break;
	case ast_exp_binary:
		fputc('(', file);
		ast_fprint(exp->as.bin.left, file);
		fputc(')', file);
		switch ((enum binop)exp->as.bin.op) {
		case binop_sequence:			fputs("; ", file);    break;
		case binop_add:					fputs(" + ", file);   break;
		case binop_sub:					fputs(" - ", file);   break;
		case binop_mul:					fputs(" * ", file);   break;
		case binop_div:					fputs(" / ", file);   break;
		case binop_mod:					fputs(" %% ", file);  break;
		case binop_xor:					fputs(" ^ ", file);   break;
		case binop_land:				fputs(" & ", file);   break;
		case binop_lor:					fputs(" | ", file);   break;
		case binop_equal:				fputs(" = ", file);   break;
		case binop_less_than:			fputs(" < ", file);   break;
		case binop_more_than:			fputs(" > ", file);   break;
		case binop_member:				fputs(".", file);     break;
		case binop_assign:				fputs(" := ", file);  break;
		case binop_and_assign:			fputs(" &= ", file);  break;
		case binop_lor_assign:			fputs(" |= ", file);  break;
		case binop_xor_assign:			fputs(" ^= ", file);  break;
		case binop_add_assign:			fputs(" += ", file);  break;
		case binop_sub_assign:			fputs(" -= ", file);  break;
		case binop_mul_assign:			fputs(" *= ", file);  break;
		case binop_div_assign:			fputs(" /= ", file);  break;
		case binop_mod_assign:			fputs(" %%= ", file); break;
		case binop_not_equal:			fputs(" != ", file);  break;
		case binop_less_equal:			fputs(" <= ", file);  break;
		case binop_more_equal:			fputs(" >= ", file);  break;
		case binop_shift_left:			fputs(" << ", file);  break;
		case binop_shift_right:			fputs(" >> ", file);  break;
		case binop_shift_left_assign:	fputs(" <<= ", file); break;
		case binop_shift_right_assign:	fputs(" >>= ", file); break;
		case binop_or:					fputs(" || ", file);  break;
		case binop_and:					fputs(" && ", file);  break;
		default:
			FAILWITH("Unreachable condition");
			break;
		}
		fputc('(', file);
		ast_fprint(exp->as.bin.right, file);
		fputc(')', file);
		break;
	case ast_exp_unary:
		if (exp->as.op == op_call) {
			fputc('(', file);
			ast_fprint(exp->as.call.proc, file);
			fputc(')', file);
			fputc('(', file);
			if (exp->as.call.args.len > 0) {
				ast_fprint(exp->as.call.args.elems[0], file);
				for (size_t i = 1; i < exp->as.call.args.len; ++i) {
					fputs(", ", file);
					ast_fprint(exp->as.call.args.elems[i], file);
				}
			}
			fputc(')', file);
		} else if (exp->as.op == op_index) {
			fputc('(', file);
			ast_fprint(exp->as.idx.exp, file);
			fputc(')', file);
			fputc('[', file);
			if (exp->as.idx.type) {
				ast_type_fprint(exp->as.idx.type, file);
				fputs(", ", file);
			}
			ast_fprint(exp->as.idx.idx, file);
			fputc(']', file);
		} else {
			switch ((int)exp->as.op) {
			case op_pos:		 fputc('+', file); break;
			case op_neg:		 fputc('-', file); break;
			case op_dereference: fputc('*', file); break;
			case op_address_of:	 fputc('&', file); break;
			case op_lnot:		 fputc('~', file); break;
			case op_not:		 fputc('!', file); break;
			default:
				FAILWITH("Unreachable condition");
				break;
			}
			fputc('(', file);
			ast_fprint(exp->as.una.exp, file);
			fputc(')', file);
		}
		break;
	case ast_exp_if:
		fputs("if ", file);
		ast_fprint(exp->as.iff.cond, file);
		fputs(" then ", file);
		ast_fprint(exp->as.iff.tb, file);
		if (exp->as.iff.fb) {
			fputs(" else ", file);
			ast_fprint(exp->as.iff.fb, file);
		}
		fputs(" end", file);
		break;
	case ast_exp_case: {
		fputs("case ", file);
		struct exp_case *c = &exp->as.ccase;
		ast_fprint(c->cexp, file);
		fputc('\n', file);
		for (size_t i = 0; i < c->branches.len; ++i) {
			struct case_branch *cb = &c->branches.elems[i];
			fputs("of ", file);
			for (size_t i = 0; i < cb->matches.len - 1; ++i) {
				ast_fprint(cb->matches.elems[i], file);
				fputs(", ", file);
			}
			ast_fprint(cb->matches.elems[cb->matches.len - 1], file);
			fputs(" do ", file);
			ast_fprint(cb->exp, file);
			fputc('\n', file);
		}
		if (c->else_exp) {
			fputs("else ", file);
			ast_fprint(c->else_exp, file);
			fputc('\n', file);
		}
		fputs("end", file);
	} break;
	case ast_exp_return:
		fputs("return(", file);
		ast_fprint(exp->as.ret, file);
		fputc(')', file);
		break;
	case ast_exp_break:    FAILWITH("TODO: ast_exp_break"); break;
	case ast_exp_continue: FAILWITH("TODO: ast_exp_continue"); break;
	default:
		FAILWITH("Unreachable condition.");
		break;
	}
}
