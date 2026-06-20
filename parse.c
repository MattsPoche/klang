#include "common.h"

KC_PRIVATE const char *ast_binop_to_str(enum binop op);
KC_PRIVATE void ast_def_fprint(struct definition *def, FILE *file);
KC_PRIVATE KCType *parse_type(Parser *p, struct scope *scope, bool introduce_type_var_p);
KC_PRIVATE void parse_definition(Parser *p, struct definition *def, struct scope *scope);
KC_PRIVATE struct expression *parse_expression(Parser *p, struct scope *scope);
KC_PRIVATE KCType *parse_type(Parser *p, struct scope *scope, bool introduce_type_var_p);

/* --- Parser --- */
KC_PRIVATE struct token *
peek_token2(Parser *p)
{
	assert(p->tokens.idx < p->tokens.len - 1);
	return &p->tokens.elems[p->tokens.idx + 1];
}

KC_PRIVATE struct token *
peek_token(Parser *p)
{
	assert(p->tokens.idx < p->tokens.len);
	return &p->tokens.elems[p->tokens.idx];
}

KC_PRIVATE struct token *
next_token(Parser *p, struct token **rt)
{
	assert(p->tokens.len > 0);
	struct token *tok = &p->tokens.elems[p->tokens.idx];
	if (p->tokens.idx < p->tokens.len - 1) p->tokens.idx++;
	if (rt) *rt = tok;
	return tok;
}

KC_PRIVATE void
error_unexpected_token(Parser *p, struct token *token, const char *debug_filename, const int debug_line)
{
	log_compile_error_impl(p->lexer.filename, token, debug_filename, debug_line,
						   "Syntax error. Unexpected token `"SV_FMT"`.", SV_ARGS(token_to_strview(token)));
	EXIT(1);
}

KC_PUBLIC void
error_undefined_ident(struct token *id, const char *debug_filename, const int debug_line)
{
	log_compile_error_impl(id->filename, id, debug_filename, debug_line,
				   "Undefined identifier `"SV_FMT"`.", SV_ARGS(token_to_strview(id)));
	EXIT(1);
}

KC_PRIVATE struct token *
expect(Parser *p, struct token *token, enum token_type tag, const char *debug_filename, const int debug_line)
{
	if (token->tt != tag) {
		error_unexpected_token(p, token, debug_filename, debug_line);
	}
	return token;
}

#define ACCEPT(token, tag)      ((token)->tt == (tag))
#define EXPECT(token, tag)      expect(p, token, tag, __FILE__, __LINE__)
#define UNEXPECTED_TOKEN(token) error_unexpected_token(p, token, __FILE__, __LINE__)

KC_PUBLIC struct proc_type
procedure_type(struct procedure *proc)
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

/* TODO: Improve error messages in parser
 */
KC_PRIVATE struct type_ptrs
parse_type_list(Parser *p, bool allow_trailing_comma, enum token_type terminal,
				struct scope *scope, bool introduce_type_var_p)
{
	struct type_ptrs list = {0};
	struct token *tok = NULL;
	if (ACCEPT(peek_token(p), terminal)) {
		next_token(p, NULL);
		return list;
	}
	do {
		if (allow_trailing_comma && ACCEPT(peek_token(p), terminal)) {
			next_token(p, NULL);
			return list;
		}
		da_append(&list, parse_type(p, scope, introduce_type_var_p));
	} while (ACCEPT(next_token(p, &tok), tt_comma));
	EXPECT(tok, terminal);
	return list;
}

KC_PRIVATE KCType *
parse_named_struct_type(Parser *p, struct scope *scope, bool introduce_type_var_p)
{
	KCType *type = MEM_ALLOC(KCType);
	type->tag = ast_type_struct;
	struct token *tok = NULL;
	if (ACCEPT(peek_token(p), tt_rbrace)) {
		next_token(p, NULL);
		return type;
	}
	do {
		if (ACCEPT(peek_token(p), tt_rbrace)) {
			next_token(p, NULL);
			return type;
		}
		EXPECT(next_token(p, &tok), tt_ident);
		EXPECT(next_token(p, NULL), tt_colon);
		struct struct_member m = {
			.name = tok,
			.type = parse_type(p, scope, introduce_type_var_p),
		};
		assert(m.type != NULL);
		da_append(&type->struct_t, m);
	} while (ACCEPT(next_token(p, &tok), tt_comma));
	EXPECT(tok, tt_rbrace);
	return type;
}

KC_PRIVATE KCType *
parse_struct_type(Parser *p, struct scope *scope, bool introduce_type_var_p)
{
	KCType *type = MEM_ALLOC(KCType);
	type->tag = ast_type_struct;
	struct token *tok = NULL;
	if (ACCEPT(peek_token(p), tt_rbrace)) {
		next_token(p, NULL);
		return type;
	}
	do {
		if (ACCEPT(peek_token(p), tt_rbrace)) {
			next_token(p, NULL);
			return type;
		}
		da_append(&type->struct_t, (struct struct_member) {
				.type = parse_type(p, scope, introduce_type_var_p),
			});
	} while (ACCEPT(next_token(p, &tok), tt_comma));
	EXPECT(tok, tt_rbrace);
	return type;
}

KC_PRIVATE KCType *
parse_type(Parser *p, struct scope *scope, bool introduce_type_var_p)
{
	KCType *type = NULL;
	struct token *tok = NULL;
	/* Build type signature */
	enum ast_type_tag tag;
	switch ((int)peek_token(p)->tt) {
	case tt_and: {
		next_token(p, NULL);
		if (ACCEPT(next_token(p, &tok), tt_bang)) {
			EXPECT(next_token(p, NULL), tt_lbracket);
			type = MEM_ALLOC(KCType);
			type->tag = ast_type_mut_slice;
		} else {
			EXPECT(tok, tt_lbracket);
			type = MEM_ALLOC(KCType);
			type->tag = ast_type_slice;
		}
		type->slice = parse_type(p, scope, introduce_type_var_p);
		EXPECT(next_token(p, NULL), tt_rbracket);
	} break;
	case tt_lparen: {
		next_token(p, NULL);
		/* parse formal parameter list */
		struct type_ptrs args = parse_type_list(p, true, tt_rparen, scope, introduce_type_var_p);
		type = MEM_ALLOC(KCType);
		if (ACCEPT(peek_token(p), tt_minus_more)) {
			next_token(p, NULL);
			type->tag = ast_type_proc;
			type->proc.args = args;
			type->proc.ret = parse_type(p, scope, introduce_type_var_p);
		} else if (ACCEPT(peek_token(p), tt_star)) {
			next_token(p, NULL);
			assert(args.len == 1);
			type->tag = ast_type_ptr;
			type->ptr = args.elems[0];
			da_free(&args);
		} else if (ACCEPT(peek_token(p), tt_bang)) {
			next_token(p, NULL);
			assert(args.len == 1);
			type->tag = ast_type_mut_ptr;
			type->mut_ptr = args.elems[0];
			da_free(&args);
		} else {
			EXPECT(next_token(p, &tok), tt_ident);
			type->tag = ast_type_app;
			type->app.args = args;
			type->app.cons = tok;
		}
	} break;
	case tt_lbrace: {
		next_token(p, NULL);
		if (peek_token(p)->tt == tt_ident && peek_token2(p)->tt == tt_colon) {
			/* begin parsing named members */
			type = parse_named_struct_type(p, scope, introduce_type_var_p);
		} else {
			type = parse_struct_type(p, scope, introduce_type_var_p);
		}
		assert(type->tag == ast_type_struct);
	} break;
	case tt_lbracket: {
		next_token(p, NULL);
		type = MEM_ALLOC(KCType);
		type->tag = ast_type_array;
		type->array.base = parse_type(p, scope, introduce_type_var_p);
		if (ACCEPT(peek_token(p), tt_comma)) {
			next_token(p, NULL);
			struct expression *exp = parse_expression(p, scope);
			assert(exp->tag == ast_exp_literal);
			assert(exp->lit.tag == LITERAL_INT);
			type->array.is_sized = true;
			type->array.size = exp->lit.i;
		} else {
			type->array.is_sized = false;
		}
		EXPECT(next_token(p, NULL), tt_rbracket);
	} break;
	case tt_star: FAILWITH("TODO: parse error"); break;
	case tt_bang: FAILWITH("TODO: parse error"); break;
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
		type = MEM_ALLOC(KCType);
		type->tag = tag;
		type->basic = next_token(p, NULL);
		break;
	case tt_ident: {
		next_token(p, &tok);
		type = MEM_ALLOC(KCType);
		type->tag = ast_type_app;
		type->app.cons = tok;
	} break;
	case tt_typevar: {
		next_token(p, &tok);
		struct type_definition *def = lookup_type(scope, token_to_strview(tok));
		if (introduce_type_var_p) {
			if (def == NULL) {
				type = MEM_ALLOC(KCType);
				type->tag = ast_type_var;
				type->var.name = tok;
				typetbl_add(&scope->typetbl, .name = tok, .type = type, .is_var = true);
			} else {
				assert(def->is_var);
				type = def->type;
			}
		} else {
			assert(def != NULL);
			assert(def->is_var);
			type = def->type;
		}
	} break;
	}
	if (ACCEPT(peek_token(p), tt_ident)) {
		next_token(p, &tok);
		struct type_ptrs args = {0};
		da_append(&args, type);
		type = MEM_ALLOC(KCType);
		type->tag = ast_type_app;
		type->app.args = args;
		type->app.cons = tok;
	} else if (ACCEPT(peek_token(p), tt_star)) {
		next_token(p, NULL);
		KCType *tmp = type;
		type = MEM_ALLOC(KCType);
		type->tag = ast_type_ptr;
		type->ptr = tmp;
	} else if (ACCEPT(peek_token(p), tt_bang)) {
		next_token(p, NULL);
		KCType *tmp = type;
		type = MEM_ALLOC(KCType);
		type->tag = ast_type_mut_ptr;
		type->mut_ptr = tmp;
	}
	return type;
}

KC_PRIVATE void
parse_type_def(Parser *p, struct scope *scope, bool is_newtype)
{
	struct type_ptrs args = {0};
	struct scope sc = {.parent = scope};
	if (ACCEPT(peek_token(p), tt_typevar)) {
		KCType *var = MEM_ALLOC(KCType);
		var->tag = ast_type_var;
		var->var.name = next_token(p, NULL);
		da_append(&args, var);
		typetbl_add(&sc.typetbl, .name = var->var.name, .type = var, .is_var = true);
	} else if (ACCEPT(peek_token(p), tt_lparen)) {
		next_token(p, NULL);
		KCType *var = MEM_ALLOC(KCType);
		var->tag = ast_type_var;
		var->var.name = EXPECT(next_token(p, NULL), tt_typevar);
		da_append(&args, var);
		typetbl_add(&sc.typetbl, .name = var->var.name, .type = var, .is_var = true);
		while (ACCEPT(peek_token(p), tt_comma)) {
			next_token(p, NULL);
			var = MEM_ALLOC(KCType);
			var->tag = ast_type_var;
			var->var.name = EXPECT(next_token(p, NULL), tt_typevar);
			da_append(&args, var);
			typetbl_add(&sc.typetbl, .name = var->var.name, .type = var, .is_var = true);
		}
		EXPECT(next_token(p, NULL), tt_rparen);
	}
	struct token *name;
	EXPECT(next_token(p, &name), tt_ident);
	EXPECT(next_token(p, NULL), tt_equal);
	struct type_definition *type_def = da_allot(&scope->typetbl);
	type_def->name = name;
	type_def->args = args;
	type_def->is_alias = !is_newtype;
	if (!is_newtype || !ACCEPT(peek_token(p), tt_pipe)) {
		type_def->type = parse_type(p, &sc, false);
		return;
	}
	KCType *type = MEM_ALLOC(KCType);
	type->tag = ast_type_union;
	int64_t tag_value = 0;
	do {
		next_token(p, NULL);
		struct token *name = EXPECT(next_token(p, NULL), tt_ident);
		if (ACCEPT(peek_token(p), tt_lparen)) {
			next_token(p, NULL);
			struct expression *exp = parse_expression(p, &sc);
			assert(exp->tag == ast_exp_literal);
			assert(exp->lit.tag == LITERAL_INT);
			tag_value = exp->lit.i;
			EXPECT(next_token(p, NULL), tt_rparen);
		}
		KCType *mem_type;
		if (ACCEPT(peek_token(p), tt_colon)) {
			next_token(p, NULL);
			mem_type = parse_type(p, &sc, false);
		} else {
			mem_type = &AST_TYPE_VOID;
		}
		symtbl_add_valcons(&scope->symtbl, name, tag_value, mem_type, type_def);
		da_append(&type->union_t, (struct union_member) {
				.name = name,
				.type = mem_type,
				.tag_value = tag_value++,
			});
	} while (ACCEPT(peek_token(p), tt_pipe));
	type_def->type = type;
}

KC_PRIVATE void
parse_definition(Parser *p, struct definition *def, struct scope *scope)
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
		def->type = parse_type(p, scope, false);
		EXPECT(next_token(p, NULL), tt_equal);
		def->exp = parse_expression(p, scope);
	} else {
		/* procedure definition */
		EXPECT(tok, tt_lparen);
		struct expression *proc = MEM_ALLOC(struct expression);
		proc->tok = def->id;
		proc->tag = ast_exp_procedure_literal;
		/* parse formal parameter list */
		proc->proc.scope.parent = scope;
		if (!ACCEPT(peek_token(p), tt_rparen)) {
			for (;;) {
				struct definition *arg = da_allot(&proc->proc.formals);
				if (ACCEPT(next_token(p, &tok), tt_mut)) {
					arg->is_mut = true;
					next_token(p, &tok);
				} else {
					arg->is_mut = false;
				}
				EXPECT(tok, tt_ident);
				EXPECT(next_token(p, NULL), tt_colon);
				arg->id = tok;
				arg->type = parse_type(p, &proc->proc.scope, true);
				symtbl_add(&proc->proc.scope.symtbl, arg, NULL);
				if (ACCEPT(peek_token(p), tt_rparen)) break;
				EXPECT(next_token(p, NULL), tt_comma);
			}
		}
		next_token(p, NULL);
		/* parse return type */
		if (ACCEPT(peek_token(p), tt_equal)) {
			proc->proc.ret = &AST_TYPE_VOID;
		} else {
			proc->proc.ret = parse_type(p, &proc->proc.scope, true);
		}
		KCType *type = MEM_ALLOC(KCType);
		type->tag = ast_type_proc;
		type->proc = procedure_type(&proc->proc);
		//proc->type = type;
		def->type = type;
		/* parse body */
		EXPECT(next_token(p, NULL), tt_equal);
		proc->proc.body = parse_expression(p, &proc->proc.scope);
		def->exp = proc;
	}
}

KC_PRIVATE void
parse_toplevel_expression(Parser *p, struct expression_stack *tl_exps, struct scope *scope)
{
	struct token *tok;
	struct expression *exp = NULL;
	switch ((int)next_token(p, &tok)->tt) {
	case tt_import: {
		EXPECT(next_token(p, &tok), tt_string);
		struct strview sv = sv_unescape_string(token_to_strview(tok));
		const char *file = fmt_str(SV_FMT".k", SV_ARGS(sv));
		struct scope *p = scope->parent;
		scope->parent = parse_file(file, tl_exps);
		scope->parent->parent = p;
		break;
	} break;
	case tt_let:
		exp = MEM_ALLOC(struct expression);
		exp->tok = tok;
		exp->tag = ast_exp_definition;
		parse_definition(p, &exp->def, scope);
		symtbl_add(&scope->symtbl, &exp->def, exp);
		da_append(tl_exps, exp);
		break;
	case tt_type: {
		parse_type_def(p, scope, false);
	} break;
	case tt_newtype: {
		parse_type_def(p, scope, true);
	} break;
	case tt_eof: break;
	default: UNEXPECTED_TOKEN(tok); break;
	}
}

KC_PUBLIC bool
parser_is_at_end(Parser *p)
{
	return peek_token(p)->tt == tt_eof;
}

#define ASSOC_LEFT(x)   (-(x))
#define ASSOC_RIGHT(x)  (x)
#define ASSOC_LEFT_P(x) ((x) < 0)

KC_PRIVATE int
precedence(enum operator op)
{
	switch (op) {
	case op_sequence:
		return ASSOC_LEFT(10);
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
		return ASSOC_RIGHT(20);
	case op_or:
		return ASSOC_LEFT(30);
	case op_and:
		return ASSOC_LEFT(40);
	case op_lor:
		return ASSOC_LEFT(50);
	case op_xor:
		return ASSOC_LEFT(60);
	case op_land:
		return ASSOC_LEFT(70);
	case op_equal:
	case op_not_equal:
		return ASSOC_LEFT(80);
	case op_less_equal:
	case op_more_equal:
	case op_less_than:
	case op_more_than:
		return ASSOC_LEFT(90);
	case op_add:
	case op_sub:
		return ASSOC_LEFT(100);
	case op_mul:
	case op_div:
	case op_mod:
		return ASSOC_LEFT(110);
	case op_cast:
		return ASSOC_LEFT(120);
	case op_lnot:
	case op_not:
	case op_neg:
	case op_pos:
	case op_address_of:
	case op_dereference:
		return ASSOC_RIGHT(130);
	case op_index:
	case op_member:
	case op_slice:
	case op_call:
		return ASSOC_LEFT(140);
	default:
		FAILWITH("Unknown operator.");
		return 0;
	}
}

KC_PRIVATE bool
check_precedence(enum operator op1, enum operator op2)
{
	int p1 = precedence(op1);
	int p2 = precedence(op2);
	return (ABS(p1) < ABS(p2) || (ABS(p1) == ABS(p2) && ASSOC_LEFT_P(p1)));
}

KC_PRIVATE bool
shunt(struct expression *op1, struct expression_stack *out, struct expression_stack *ops)
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
		   && check_precedence(op1->op, op2->op)) {
		da_append(out, da_pop(ops));
	}
	da_append(ops, op1);
	return 1;
}

KC_PRIVATE bool
exp_is_intlit(struct expression *exp)
{
	return exp->tag == ast_exp_literal
		&& exp->lit.tag == LITERAL_INT;
}

KC_PRIVATE struct expression *
copy_exp(struct expression *exp)
{
	struct expression *new_exp = MEM_ALLOC(struct expression);
	*new_exp = *exp;
	return new_exp;
}

KC_PRIVATE struct expression *
eval_binop_exp(UNUSED Parser *p, struct expression *exp, struct expression *right, struct expression *left)
{
	if (exp_is_intlit(right) && exp_is_intlit(left)) {
		int64_t x = left->lit.i;
		int64_t y = right->lit.i;
		switch ((enum binop)exp->bin.op) {
		case binop_sequence:
			exp->bin.right = right;
			exp->bin.left = left;
			break;
		case binop_add:
			exp->tag = ast_exp_literal;
			exp->lit.tag = LITERAL_INT;
			exp->lit.i = x + y;
			break;
		case binop_sub:
			exp->tag = ast_exp_literal;
			exp->lit.tag = LITERAL_INT;
			exp->lit.i = x - y;
			break;
		case binop_mul:
			exp->tag = ast_exp_literal;
			exp->lit.tag = LITERAL_INT;
			exp->lit.i = x * y;
			break;
		case binop_div:
			exp->tag = ast_exp_literal;
			exp->lit.tag = LITERAL_INT;
			exp->lit.i = x / y;
			break;
		case binop_mod:
			exp->tag = ast_exp_literal;
			exp->lit.tag = LITERAL_INT;
			exp->lit.i = x % y;
			break;
		case binop_xor:
			exp->tag = ast_exp_literal;
			exp->lit.tag = LITERAL_INT;
			exp->lit.i = x ^ y;
			break;
		case binop_land:
			exp->tag = ast_exp_literal;
			exp->lit.tag = LITERAL_INT;
			exp->lit.i = x & y;
			break;
		case binop_lor:
			exp->tag = ast_exp_literal;
			exp->lit.tag = LITERAL_INT;
			exp->lit.i = x | y;
			break;
		case binop_equal:
			exp->tag = ast_exp_literal;
			exp->lit.tag = LITERAL_BOOL;
			exp->lit.i = x == y;
			break;
		case binop_less_than:
			exp->tag = ast_exp_literal;
			exp->lit.tag = LITERAL_BOOL;
			exp->lit.i = x < y;
			break;
		case binop_more_than:
			exp->tag = ast_exp_literal;
			exp->lit.tag = LITERAL_BOOL;
			exp->lit.i = x > y;
			break;
		case binop_less_equal:
			exp->tag = ast_exp_literal;
			exp->lit.tag = LITERAL_BOOL;
			exp->lit.i = x <= y;
			break;
		case binop_more_equal:
			exp->tag = ast_exp_literal;
			exp->lit.tag = LITERAL_BOOL;
			exp->lit.i = x >= y;
			break;
		case binop_or:
		case binop_and:
		case binop_assign:
		case binop_and_assign:
		case binop_lor_assign:
		case binop_xor_assign:
		case binop_add_assign:
		case binop_sub_assign:
		case binop_mul_assign:
		case binop_div_assign:
		case binop_mod_assign:
		case binop_not_equal:
		case binop_shift_left:
		case binop_shift_right:
		case binop_shift_left_assign:
		case binop_shift_right_assign:
		case binop_member: FAILWITH("TODO: binop_member"); break;
		default: FAILWITH("Unreachable"); break;
		}
	} else {
		exp->bin.right = right;
		exp->bin.left = left;
	}
	return exp;
}

KC_PRIVATE struct expression *
eval_unaop_exp(struct expression *exp, struct expression *operand)
{
	if (exp_is_intlit(operand)) {
		switch ((enum unaop)exp->bin.op) {
		case unaop_not: FAILWITH("TODO: unaop_not"); break;
		case unaop_lnot:
			exp = operand;
			exp->lit.i = ~exp->lit.i;
			break;
		case unaop_neg:
			*exp = *operand;
			exp->lit.i = -exp->lit.i;
			break;
		case unaop_pos:
			*exp = *operand;
			break;
		case unaop_address_of:
		case unaop_dereference:
		case unaop_index:
		case unaop_call:
		case unaop_slice:
		case unaop_cast: exp->una.exp = operand; break;
		default: FAILWITH("Unreachable"); break;
		}
	} else if (exp->una.op == op_slice) {
		struct expression *idx = exp->slice.idx;
		struct expression *len = exp->slice.len;
		if (idx == NULL) {
			idx = MEM_ALLOC(struct expression);
			idx->tag = ast_exp_literal;
			idx->lit.tag = LITERAL_INT;
			idx->lit.i = 0;
			exp->slice.idx = idx;
		}
		if (len == NULL) {
			len = MEM_ALLOC(struct expression);
			len->tag = ast_exp_get_len;
			len->get_len = copy_exp(operand);
			if (exp_is_intlit(idx) && idx->lit.i == 0) {
				exp->slice.len = len;
			} else {
				exp->slice.len = MEM_ALLOC(struct expression);
				exp->slice.len->tag = ast_exp_binary;
				exp->slice.len->bin.op = op_sub;
				exp->slice.len->bin.left = len;
				exp->slice.len->bin.right = copy_exp(idx);
			}
		}
		exp->una.exp = operand;
	} else {
		exp->una.exp = operand;
	}
	return exp;
}

KC_PRIVATE struct expression *
build_expression_tree(Parser *p, struct expression_stack *out)
{
	struct expression_stack stack = {0};
	struct expression *exp;
	for (size_t i = 0; i < out->len; ++i) {
		exp = out->elems[i];
		if (exp->tag == ast_exp_binary) {
			if (stack.len < 2) {
				printf("exp->bin.op = %s\n", ast_binop_to_str((enum binop)exp->bin.op));
				FAILWITH("TODO: stack.len < 2");
			}
			struct expression *right = da_pop(&stack);
			struct expression *left  = da_pop(&stack);
			eval_binop_exp(p, exp, right, left);
		} else if (exp->tag == ast_exp_unary) {
			assert(stack.len >= 1);
			eval_unaop_exp(exp, da_pop(&stack));
		}
		da_append(&stack, exp);
	}
	assert(stack.len == 1);
	exp = da_pop(&stack);
	da_free(&stack);
	return exp;
}

KC_PRIVATE struct expression *
parse_if(Parser *p, struct expression *exp, struct scope *scope)
{
	struct token *tok = NULL;
	exp->tag = ast_exp_if;
	exp->iff.cond = parse_expression(p, scope);
	EXPECT(next_token(p, NULL), tt_then);
	exp->iff.tb = parse_expression(p, scope);
	if (ACCEPT(next_token(p, &tok), tt_elif)) {
		exp->iff.fb = parse_if(p, MEM_ALLOC(struct expression), scope);
	} else if (ACCEPT(tok, tt_else)) {
		exp->iff.fb = parse_expression(p, scope);
		EXPECT(next_token(p, NULL), tt_end);
	} else {
		exp->iff.fb = NULL;
		EXPECT(tok, tt_end);
	}
	return exp;
}

KC_PRIVATE struct expression *
parse_square_bracket_expression(Parser *p, struct expression *exp, struct scope *scope)
{
#define PARSE_INIT       0
#define PARSE_INDEX      1
#define PARSE_LENGTH     2
#define BUILD_SLICE_EXP  3
#define BUILD_INDEX_EXP  4
#define PARSE_DONE      -1
	struct expression *idx = NULL;
	struct expression *len = NULL;
	exp->tag = ast_exp_unary;
	for (int state = PARSE_INIT; state != PARSE_DONE;) {
		switch (state) {
		case PARSE_INIT:
			if (ACCEPT(peek_token(p), tt_period_period)) {
				next_token(p, NULL);
				state = PARSE_LENGTH;
			} else {
				state = PARSE_INDEX;
			}
			break;
		case PARSE_LENGTH:
			if (!ACCEPT(peek_token(p), tt_rbracket)) {
				len = parse_expression(p, scope);
			}
			state = BUILD_SLICE_EXP;
			break;
		case PARSE_INDEX:
			idx = parse_expression(p, scope);
			if (ACCEPT(peek_token(p), tt_period_period)) {
				next_token(p, NULL);
				state = PARSE_LENGTH;
			} else {
				state = BUILD_INDEX_EXP;
			}
			break;
		case BUILD_SLICE_EXP:
			exp->slice.op = op_slice;
			exp->slice.idx = idx;
			exp->slice.len = len;
			state = PARSE_DONE;
			break;
		case BUILD_INDEX_EXP:
			exp->idx.op = op_index;
			exp->idx.idx = idx;
			state = PARSE_DONE;
			break;
		default: FAILWITH("Unreachable");
		}
	}
	EXPECT(next_token(p, NULL), tt_rbracket);
	return exp;
#undef PARSE_INIT
#undef PARSE_INDEX
#undef PARSE_LENGTH
#undef BUILD_SLICE
#undef BUILD_INDEX
#undef PARSE_DONE
}

KC_PRIVATE struct expression *
parse_array_initializer_list(Parser *p, struct scope *scope, struct expression *exp)
{
	exp->tag = ast_exp_array_initializer;
	struct expression_stack exps = {0};
	struct token *tok = NULL;
	if (peek_token(p)->tt != tt_rbracket) {
		for (;;) {
			da_append(&exps, parse_expression(p, scope));
			if (ACCEPT(next_token(p, &tok), tt_rbracket)) break;
			EXPECT(tok, tt_comma);
			/* allow trailing comma */
			if (ACCEPT(peek_token(p), tt_rbracket)) {
				next_token(p, NULL);
				break;
			}
		}
	}
	exp->init = exps;
	return exp;
}

KC_PRIVATE struct expression *
parse_initializer_list(Parser *p, struct scope *scope, struct expression *exp)
{
	struct token *tok = NULL;
	if (ACCEPT(peek_token(p), tt_ident) && ACCEPT(peek_token2(p), tt_equal)) {
		exp->tag = ast_exp_named_struct_initializer;
		struct expression_stack exps = {0};
		struct token_ptrs ids = {0};
		for (;;) {
			EXPECT(next_token(p, &tok), tt_ident);
			da_append(&ids, tok);
			EXPECT(next_token(p, NULL), tt_equal);
			da_append(&exps, parse_expression(p, scope));
			if (ACCEPT(next_token(p, &tok), tt_rbrace)) break;
			EXPECT(tok, tt_comma);
			/* allow trailing comma */
			if (ACCEPT(peek_token(p), tt_rbrace)) {
				next_token(p, NULL);
				break;
			}
		}
		exp->named_init.ids = ids;
		exp->named_init.exps = exps;
	} else if (ACCEPT(peek_token(p), tt_rbrace)) {
		next_token(p, &exp->tok);
		exp->tag = ast_exp_zero_struct_initializer;
	} else {
		exp->tag = ast_exp_struct_initializer;
		struct expression_stack exps = {0};
		if (peek_token(p)->tt != tt_rbrace) {
			for (;;) {
				da_append(&exps, parse_expression(p, scope));
				if (ACCEPT(next_token(p, &tok), tt_rbrace)) break;
				EXPECT(tok, tt_comma);
				/* allow trailing comma */
				if (ACCEPT(peek_token(p), tt_rbrace)) {
					next_token(p, NULL);
					break;
				}
			}
		}
		exp->init = exps;
	}
	return exp;
}

#define CHK_OP_PREV(cond) if (op_prev != (cond)) UNEXPECTED_TOKEN(next_token(p, NULL))

#pragma push_macro("ASSERT")
#undef ASSERT
#define ASSERT(...) FAILWITH("TODO: replace this assert with an actual error message.")

KC_PRIVATE struct expression *
parse_expression(Parser *p, struct scope *scope)
{
	struct expression_stack out = {0};
	struct expression_stack ops = {0};
	struct token *tok = NULL;
	struct expression *exp = NULL;
	bool op_prev = true; // decide if operator should be treated as unary
	/* shunting yard */
	for (;;) {
		exp = MEM_ALLOC(struct expression);
	repeat:
		tok = NULL;
		tok = peek_token(p);
		switch (tok->tt) {
		case tt_let:
			if (op_prev == false) goto flush; // in this case let acts as a terminator
			op_prev = false;
			next_token(p, &exp->tok);
			exp->tag = ast_exp_let;
			parse_definition(p, &exp->let.def, scope);
			EXPECT(next_token(p, NULL), tt_in);
			exp->let.scope.parent = scope;
			symtbl_add(&exp->let.scope.symtbl, &exp->let.def, NULL);
			exp->let.body = parse_expression(p, &exp->let.scope);
			da_append(&out, exp);
			break;
		case tt_if:
			CHK_OP_PREV(true);
			op_prev = false;
			next_token(p, &exp->tok);
			da_append(&out, parse_if(p, exp, scope));
			break;
		case tt_while:
			CHK_OP_PREV(true);
			op_prev = false;
			next_token(p, &exp->tok);
			exp->tag = ast_exp_while;
			exp->wloop.cond = parse_expression(p, scope);
			EXPECT(next_token(p, NULL), tt_do);
			exp->wloop.body = parse_expression(p, scope);
			EXPECT(next_token(p, NULL), tt_done);
			da_append(&out, exp);
			break;
		case tt_case: {
			CHK_OP_PREV(true);
			op_prev = false;
			next_token(p, &exp->tok);
			exp->tag = ast_exp_case;
			struct exp_case *c = &exp->ccase;
			c->cexp = parse_expression(p, scope);
			EXPECT(next_token(p, NULL), tt_of);
			/* parse branches */
			do {
				struct case_branch *branch = da_allot(&c->branches);
				EXPECT(next_token(p, &branch->cons), tt_ident);
				branch->scope.parent = scope;
				/* parse binding */
				if (ACCEPT(peek_token(p), tt_lparen)) {
					next_token(p, NULL);
					if (ACCEPT(peek_token(p), tt_underscore)) {
						next_token(p, NULL);
					} else {
						branch->binds_value = true;
						if (ACCEPT(peek_token(p), tt_mut)) {
							next_token(p, NULL);
							branch->binding.is_mut = true;
						}
						if (ACCEPT(peek_token(p), tt_and)) {
							next_token(p, NULL);
							branch->binding_is_ref = true;
						}
						EXPECT(next_token(p, &branch->binding.id), tt_ident);
						symtbl_add(&branch->scope.symtbl, &branch->binding, NULL);
					}
					EXPECT(next_token(p, NULL), tt_rparen);
				}
				/* parse guard */
				if (ACCEPT(peek_token(p), tt_if)) {
					next_token(p, NULL);
					branch->guard = parse_expression(p, &branch->scope);
				}
				/* parse body exp */
				EXPECT(next_token(p, NULL), tt_minus_more);
				branch->body = parse_expression(p, &branch->scope);
				if (ACCEPT(peek_token(p), tt_comma)) {
					next_token(p, NULL);
					if (ACCEPT(peek_token(p), tt_else)) {
						next_token(p, NULL);
						c->else_exp = parse_expression(p, scope);
						EXPECT(peek_token(p), tt_end);
						break;
					}
				} else {
					EXPECT(peek_token(p), tt_end);
					break;
				}
			} while (!ACCEPT(peek_token(p), tt_end));
			next_token(p, NULL);
			if (c->branches.len == 0) {
				log_compile_error_and_die(p->lexer.filename, tok, "Case expression has no branches.");
			}
			da_append(&out, exp);
		} break;
		case tt_import: FAILWITH("TODO: tt_import"); break;
		case tt_underscore: FAILWITH("TODO: tt_underscore"); break;
		case tt_mut: FAILWITH("TODO: tt_mut"); break;
		case tt_tick: FAILWITH("TODO: tt_tick"); break;
		case tt_at: FAILWITH("TODO: tt_at"); break;
		case tt_hash: FAILWITH("TODO: tt_hash"); break;
		case tt_backslash: FAILWITH("TODO: tt_backslash"); break;
		case tt_colon: FAILWITH("TODO: tt_colon"); break;
		case tt_quote: FAILWITH("TODO: tt_quote"); break;
		case tt_double_quote: FAILWITH("TODO: tt_double_quote"); break;
		case tt_question: FAILWITH("TODO: tt_question"); break;
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
		case tt_type: FAILWITH("TODO: tt_type"); break;
		case tt_newtype: FAILWITH("TODO: tt_newtype"); break;
		case tt_typevar: FAILWITH("TODO: tt_typevar"); break;
		case tt_struct: FAILWITH("TODO: tt_struct"); break;
		case tt_sizeof:
			CHK_OP_PREV(true);
			op_prev = false;
			exp->tok = next_token(p, NULL);
			exp->tag = ast_exp_size_of;
			EXPECT(next_token(p, NULL), tt_lparen);
			if (ACCEPT(peek_token(p), tt_colon)) {
				next_token(p, NULL);
				exp->size_of.type = parse_type(p, scope, false);
			} else {
				exp->size_of.exp = parse_expression(p, scope);
			}
			EXPECT(next_token(p, NULL), tt_rparen);
			da_append(&out, exp);
			break;
		case tt_return:
			CHK_OP_PREV(true);
			op_prev = false;
			next_token(p, &exp->tok);
			exp->tag = ast_exp_return;
			if (ACCEPT(next_token(p, &tok), tt_lbrace)) { // return struct literal
				FAILWITH("TODO: parse struct literal");
			} else if (ACCEPT(tok, tt_rbracket)) { // return array literal
				FAILWITH("TODO: parse array literal");
			} else { // return expression
				EXPECT(tok, tt_lparen);
				exp->ret = parse_expression(p, scope);
				EXPECT(next_token(p, NULL), tt_rparen);
			}
			da_append(&out, exp);
			break;
		case tt_extern:
			CHK_OP_PREV(true);
			op_prev = false;
			next_token(p, NULL);
			EXPECT(next_token(p, NULL), tt_lparen);
			EXPECT(next_token(p, &exp->tok), tt_ident);
			EXPECT(next_token(p, NULL), tt_rparen);
			exp->tag = ast_exp_extern_symbol;
			da_append(&out, exp);
			break;
		case tt_ptr:
			CHK_OP_PREV(true);
			op_prev = false;
			exp->tok = next_token(p, NULL);
			exp->tag = ast_exp_get_ptr;
			EXPECT(next_token(p, NULL), tt_lparen);
			exp->get_ptr = parse_expression(p, scope);
			EXPECT(next_token(p, NULL), tt_rparen);
			da_append(&out, exp);
			break;
		case tt_len:
			CHK_OP_PREV(true);
			op_prev = false;
			exp->tok = next_token(p, NULL);
			exp->tag = ast_exp_get_len;
			EXPECT(next_token(p, NULL), tt_lparen);
			exp->get_ptr = parse_expression(p, scope);
			EXPECT(next_token(p, NULL), tt_rparen);
			da_append(&out, exp);
			break;
		case tt_ident:
			CHK_OP_PREV(true);
			op_prev = false;
			next_token(p, &exp->tok);
			struct symtbl_entry *entry = lookup_entry(scope, token_to_strview(exp->tok));
			if (entry == NULL) {
				exp->tag = ast_exp_ident;
			} else {
				switch (entry->tag) {
				case SYMTBL_VARIABL:
					exp->tag = ast_exp_ident;
					break;
				case SYMTBL_VALCONS:
					exp->tag = ast_exp_value_cons;
					exp->valcons.cons = exp->tok;
					if (ACCEPT(peek_token(p), tt_lbrace)) {
						struct expression *arg = MEM_ALLOC(struct expression);
						next_token(p, &arg->tok);
						arg = parse_initializer_list(p, scope, arg);
						exp->valcons.exp = arg;
					} else if (ACCEPT(peek_token(p), tt_lparen)) {
						next_token(p, NULL);
						exp->valcons.exp = parse_expression(p, scope);
						EXPECT(next_token(p, NULL), tt_rparen);
					}
					break;
				default: FAILWITH("Unreachable");
				}
			}
			da_append(&out, exp);
			break;
		case tt_dollar: FAILWITH("TODO: tt_dollar"); break;
		case tt_string:
			CHK_OP_PREV(true);
			op_prev = false;
			exp->tag = ast_exp_literal;
			exp->tok = next_token(p, NULL);
			exp->lit.tag = LITERAL_STRING;
			exp->lit.s = sv_unescape_string(token_to_strview(exp->tok));
			da_append(&out, exp);
			break;
		case tt_char: {
			CHK_OP_PREV(true);
			op_prev = false;
			exp->tag = ast_exp_literal;
			exp->tok = next_token(p, NULL);
			exp->lit.tag = LITERAL_CHAR;
			struct strview sv = sv_unescape_string(sv_drop_char(token_to_strview(exp->tok)));
			if (sv.len != 1)
				FAILWITH("Unreachable. Possibly something wrong with lexer?");
			exp->lit.i = sv.ptr[0];
			FREE(sv.ptr);
			da_append(&out, exp);
		} break;
		case tt_true:
			CHK_OP_PREV(true);
			op_prev = false;
			exp->tok = next_token(p, &tok);
			exp->tag = ast_exp_literal;
			exp->lit.tag = LITERAL_BOOL;
			exp->lit.i = true;
			da_append(&out, exp);
			break;
		case tt_false:
			CHK_OP_PREV(true);
			op_prev = false;
			exp->tok = next_token(p, &tok);
			exp->tag = ast_exp_literal;
			exp->lit.tag = LITERAL_BOOL;
			exp->lit.i = false;
			da_append(&out, exp);
			break;
		case tt_intlit:
			CHK_OP_PREV(true);
			op_prev = false;
			exp->tok = next_token(p, &tok);
			exp->tag = ast_exp_literal;
			exp->lit.tag = LITERAL_INT;
			if (!sv_to_int(token_to_strview(tok), &exp->lit.i))
				FAILWITH("Unreachable. Something is wrong with the lexer?");
			da_append(&out, exp);
			break;
		case tt_hexlit:
			CHK_OP_PREV(true);
			op_prev = false;
			exp->tag = ast_exp_literal;
			exp->tok = next_token(p, &tok);
			exp->lit.tag = LITERAL_INT;
			if (!sv_to_int(token_to_strview(tok), &exp->lit.i))
				FAILWITH("Unreachable. Something is wrong with the lexer?");
			da_append(&out, exp);
			break;
		case tt_floatlit: FAILWITH("TODO: tt_floatlit"); break;
		case tt_undefined:
			CHK_OP_PREV(true);
			op_prev = false;
			exp->tok = next_token(p, &tok);
			exp->tag = ast_exp_undefined;
			da_append(&out, exp);
			break;
		case tt_noreturn: FAILWITH("TODO: tt_noreturn"); break;
		case tt_lbrace:
			CHK_OP_PREV(true);
			op_prev = false;
			next_token(p, &exp->tok);
			parse_initializer_list(p, scope, exp);
			da_append(&out, exp);
			break;
		case tt_lbracket: {
			next_token(p, &exp->tok);
			if (op_prev == false) {
				shunt(parse_square_bracket_expression(p, exp, scope), &out, &ops);
			} else {
				op_prev = false;
				parse_array_initializer_list(p, scope, exp);
				da_append(&out, exp);
			}
		} break;
			/* operators */
		case tt_lparen: {
			next_token(p, &exp->tok);
			if (op_prev) {
				da_append(&ops, NULL); // Null is interperated as lparen
				goto repeat;
			}
			op_prev = false;
			exp->tag = ast_exp_unary;
			exp->call.op = op_call;
			exp->call.proc = NULL;
			struct expression_stack *args = &exp->call.args;
			da_init(args);
			if (!ACCEPT(peek_token(p), tt_rparen)) {
				for (;;) {
					da_append(args, parse_expression(p, scope));
					if (ACCEPT(peek_token(p), tt_rparen)) break;
					EXPECT(next_token(p, NULL), tt_comma);
				}
			}
			next_token(p, NULL);
			shunt(exp, &out, &ops);
		} break;
		case tt_rparen:
			CHK_OP_PREV(false);
			if (shunt(NULL, &out, &ops)) {
				next_token(p, NULL);
				goto repeat;
			} else {
				goto flush;
			}
			break;
		case tt_as:
			CHK_OP_PREV(false);
			next_token(p, &exp->tok);
			exp->tag = ast_exp_unary;
			exp->cast.op = op_cast;
			exp->cast.type = parse_type(p, scope, false);
			shunt(exp, &out, &ops);
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
			CHK_OP_PREV(false);
			op_prev = true;
			next_token(p, &exp->tok);
			exp->tag = ast_exp_binary;
			exp->bin.op = (enum operator)tok->tt;
			shunt(exp, &out, &ops);
			break;
			/* unary ops */
		case tt_tilde:
		case tt_bang:
			CHK_OP_PREV(true);
			next_token(p, &exp->tok);
			exp->tag = ast_exp_unary;
			exp->una.op = (enum operator)tok->tt;
			shunt(exp, &out, &ops);
			break;
			/* binary or unary ops */
		case tt_plus:
			next_token(p, &exp->tok);
			if (op_prev) {
				exp->tag = ast_exp_unary;
				exp->una.op = op_pos;
				shunt(exp, &out, &ops);
			} else {
				op_prev = true;
				exp->tag = ast_exp_binary;
				exp->bin.op = (enum operator)tok->tt;
				shunt(exp, &out, &ops);
			}
			break;
		case tt_minus:
			next_token(p, &exp->tok);
			if (op_prev) {
				exp->tag = ast_exp_unary;
				exp->una.op = op_neg;
				shunt(exp, &out, &ops);
			} else {
				op_prev = true;
				exp->tag = ast_exp_binary;
				exp->bin.op = (enum operator)tok->tt;
				shunt(exp, &out, &ops);
			}
			break;
		case tt_star:
			next_token(p, &exp->tok);
			if (op_prev) {
				exp->tag = ast_exp_unary;
				exp->una.op = op_dereference;
				shunt(exp, &out, &ops);
			} else {
				op_prev = true;
				exp->tag = ast_exp_binary;
				exp->bin.op = (enum operator)tok->tt;
				shunt(exp, &out, &ops);
			}
			break;
		case tt_and:
			next_token(p, &exp->tok);
			if (op_prev) {
				exp->tag = ast_exp_unary;
				exp->una.op = op_address_of;
				shunt(exp, &out, &ops);
			} else {
				op_prev = true;
				exp->tag = ast_exp_binary;
				exp->bin.op = (enum operator)tok->tt;
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
		case tt_period_period:
		case tt_eof:
			CHK_OP_PREV(false);
			goto flush;
		case TOKEN_TYPE_MAX:
			FAILWITH("Invalid token (parse_expression)");
			break;
		default: FAILWITH("Unreachable"); break;
		}
	}
flush:
	while (ops.len) da_append(&out, da_pop(&ops));
	exp = build_expression_tree(p, &out);
	da_free(&out);
	da_free(&ops);
	return exp;
}
#undef ASSERT
#pragma pop_macro("ASSERT")

KC_PRIVATE const char *
ast_binop_to_str(enum binop op)
{
	switch (op) {
	case binop_sequence:		   return "binop_sequence";
	case binop_add:				   return "binop_add";
	case binop_sub:				   return "binop_sub";
	case binop_mul:				   return "binop_mul";
	case binop_div:				   return "binop_div";
	case binop_mod:				   return "binop_mod";
	case binop_xor:				   return "binop_xor";
	case binop_land:			   return "binop_land";
	case binop_lor:				   return "binop_lor";
	case binop_equal:			   return "binop_equal";
	case binop_less_than:		   return "binop_less_than";
	case binop_more_than:		   return "binop_more_than";
	case binop_member:			   return "binop_member";
	case binop_assign:			   return "binop_assign";
	case binop_and_assign:		   return "binop_and_assign";
	case binop_lor_assign:		   return "binop_lor_assign";
	case binop_xor_assign:		   return "binop_xor_assign";
	case binop_add_assign:		   return "binop_add_assign";
	case binop_sub_assign:		   return "binop_sub_assign";
	case binop_mul_assign:		   return "binop_mul_assign";
	case binop_div_assign:		   return "binop_div_assign";
	case binop_mod_assign:		   return "binop_mod_assign";
	case binop_not_equal:		   return "binop_not_equal";
	case binop_less_equal:		   return "binop_less_equal";
	case binop_more_equal:		   return "binop_more_equal";
	case binop_shift_left:		   return "binop_shift_left";
	case binop_shift_right:		   return "binop_shift_right";
	case binop_shift_left_assign:  return "binop_shift_left_assign";
	case binop_shift_right_assign: return "binop_shift_right_assign";
	case binop_or:				   return "binop_or";
	case binop_and:				   return "binop_and";
	default: FAILWITH("Unreachable");
	}
}

/* --- AST Printer --- */

KC_PUBLIC const char *
ast_type_to_str(KCType *t)
{
	static char buf[1024];
	FILE *f = fmemopen(buf, sizeof(buf), "w");;
	assert(f != NULL);
	ast_type_fprint(t, f);
	fclose(f);
	return str_dup(buf, strlen(buf));
}

KC_PUBLIC void
ast_type_fprint(KCType *t, FILE *file)
{
	t = type_find(t);
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
	case ast_type_var:
		if (t->var.name) {
#if 1
			fprintf(file, SV_FMT, SV_ARGS(token_to_strview(t->var.name)));
#else
			fprintf(file, SV_FMT"@%p", SV_ARGS(token_to_strview(t->var.name)), t);
#endif
		} else {
			fprintf(file, "'%p", t);
		}
		break;
	case ast_type_app:
		if (t->app.args.len) {
			fputc('(', file);
			ast_type_fprint(t->app.args.elems[0], file);
			for (size_t i = 1; i < t->app.args.len; ++i) {
				fputs(", ", file);
				ast_type_fprint(t->app.args.elems[i], file);
			}
			fputs(") ", file);
		}
		fprintf(file, SV_FMT, SV_ARGS(token_to_strview(t->app.cons)));
		break;
	case ast_type_proc:
		fputc('(', file);
		if (t->proc.args.len > 0) {
			ast_type_fprint(t->proc.args.elems[0], file);
			for (size_t i = 1; i < t->proc.args.len; ++i) {
				fputs(", ", file);
				ast_type_fprint(t->proc.args.elems[i], file);
			}
		}
		fputs(") -> ", file);
		ast_type_fprint(t->proc.ret, file);
		break;
	case ast_type_istruct:
	case ast_type_struct:
		fputc('{', file);
		if (t->struct_t.len > 0) {
			struct struct_member *mem = &t->struct_t.elems[0];
			if (mem->name) fprintf(file, SV_FMT": ", SV_ARGS(token_to_strview(mem->name)));
			ast_type_fprint(mem->type, file);
			for (size_t i = 1; i < t->struct_t.len; ++i) {
				mem = &t->struct_t.elems[i];
				fputs(", ", file);
				if (mem->name) fprintf(file, SV_FMT": ", SV_ARGS(token_to_strview(mem->name)));
				ast_type_fprint(mem->type, file);
			}
		}
		fputc('}', file);
		break;
	case ast_type_array:
		fputc('[', file);
		ast_type_fprint(t->array.base, file);
		if (t->array.is_sized) {
			fprintf(file, ", %zu", t->array.size);
		}
		fputc(']', file);
		break;
	case ast_type_vector:
		FAILWITH("TODO: print vector type.");
		break;
	case ast_type_mut_ptr:
		ast_type_fprint(t->ptr, file);
		fputc('!', file);
		break;
	case ast_type_ptr:
		ast_type_fprint(t->ptr, file);
		fputc('*', file);
		break;
	case ast_type_slice:
		fputs("&[", file);
		ast_type_fprint(t->slice, file);
		fputc(']', file);
		break;
	case ast_type_mut_slice:
		fputs("&![", file);
		ast_type_fprint(t->slice, file);
		fputc(']', file);
		break;
	case ast_type_union:
		fputs("#U{", file);
		if (t->union_t.len > 0) {
			struct union_member *mem = &t->union_t.elems[0];
			if (mem->name) fprintf(file, SV_FMT": ", SV_ARGS(token_to_strview(mem->name)));
			ast_type_fprint(mem->type, file);
			for (size_t i = 1; i < t->union_t.len; ++i) {
				mem = &t->union_t.elems[i];
				fputs(", ", file);
				if (mem->name) fprintf(file, SV_FMT": ", SV_ARGS(token_to_strview(mem->name)));
				ast_type_fprint(mem->type, file);
			}
		}
		fputc('}', file);
		break;
	default:
		FAILWITH("Unreachable condition.");
		break;
	}
}

KC_PRIVATE void
ast_def_fprint(struct definition *def, FILE *file)
{
	fputs("let ", file);
	if (def->is_mut) fputs("mut ", file);
	fprintf(file, SV_FMT, SV_ARGS(token_to_strview(def->id)));
	if (def->type->tag == ast_type_proc
		&& def->exp->tag == ast_exp_procedure_literal) {
		struct procedure *proc = &def->exp->proc;
		fputc('(', file);
		if (proc->formals.len > 0) {
			struct definition *arg = &proc->formals.elems[0];
			fprintf(file, SV_FMT": ", SV_ARGS(token_to_strview(arg->id)));
			ast_type_fprint(arg->type, file);
			for (size_t i = 1; i < proc->formals.len; ++i) {
				arg = &proc->formals.elems[i];
				fprintf(file, ", "SV_FMT": ", SV_ARGS(token_to_strview(arg->id)));
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

KC_PUBLIC void
ast_fprint(struct expression *exp, FILE *file)
{
	switch (exp->tag) {
	case ast_exp_definition:
		ast_def_fprint(&exp->def, file);
		break;
	case ast_exp_let:
		ast_def_fprint(&exp->let.def, file);
		fputs(" in\n", file);
		ast_fprint(exp->let.body, file);
		break;
	case ast_exp_literal:
		fprintf(file, SV_FMT, SV_ARGS(token_to_strview(exp->tok)));
		break;
	case ast_exp_procedure_literal: {
		struct procedure *proc = &exp->proc;
		struct def_array *formals = &proc->formals;
		fputs("#proc(", file);
		if (formals->len) {
			fprintf(file, SV_FMT, SV_ARGS(token_to_strview(formals->elems[0].id)));
			fputs(": ", file);
			ast_type_fprint(formals->elems[0].type, file);
			for (size_t i = 1; i < formals->len; ++i) {
				fputs(", ", file);
				fprintf(file, SV_FMT, SV_ARGS(token_to_strview(formals->elems[0].id)));
				fputs(": ", file);
				ast_type_fprint(formals->elems[0].type, file);
			}
		}
		if (proc->ret->tag == ast_type_void) {
			fputc(')', file);
		} else {
			fputs(") ", file);
			ast_type_fprint(proc->ret, file);
		}
		fputs(" =>\n", file);
		ast_fprint(proc->body, file);
	} break;
	case ast_exp_undefined:
		fputs("undefined", file);
		break;
	case ast_exp_zero_struct_initializer:
		fputs("{}", file);
		break;
	case ast_exp_struct_initializer:
		fputc('{', file);
		if (exp->init.len > 0) {
			ast_fprint(exp->init.elems[0], file);
			for (size_t i = 1; i < exp->init.len; ++i) {
				fputs(", ", file);
				ast_fprint(exp->init.elems[i], file);
			}
		}
		fputc('}', file);
		break;
	case ast_exp_named_struct_initializer: {
		struct token_ptrs *ids = &exp->named_init.ids;
		struct expression_stack *exps = &exp->named_init.exps;
		fputc('{', file);
		if (ids->len > 0) {
			fprintf(file, SV_FMT" = ", SV_ARGS(token_to_strview(ids->elems[0])));
			ast_fprint(exps->elems[0], file);
			for (size_t i = 1; i < ids->len; ++i) {
				fputs(", ", file);
				fprintf(file, SV_FMT" = ", SV_ARGS(token_to_strview(ids->elems[i])));
				ast_fprint(exps->elems[i], file);
			}
		}
		fputc('}', file);
	} break;
	case ast_exp_array_initializer: FAILWITH("TODO: ast_exp_array_initializer"); break;
	case ast_exp_ident:
		fprintf(file, SV_FMT, SV_ARGS(token_to_strview(exp->tok)));
		break;
	case ast_exp_get_ptr:
		fputs("#ptr(", file);
		ast_fprint(exp->get_ptr, file);
		fputc(')', file);
		break;
	case ast_exp_get_len:
		fputs("#len(", file);
		ast_fprint(exp->get_ptr, file);
		fputc(')', file);
		break;
	case ast_exp_size_of:
		fputs("#sizeof(", file);
		if (exp->size_of.exp) {
			ast_fprint(exp->size_of.exp, file);
		} else {
			assert(exp->size_of.type);
			fputs(": ", file);
			ast_type_fprint(exp->size_of.type, file);
		}
		fputc(')', file);
		break;
	case ast_exp_extern_symbol:
		fprintf(file, "#extern("SV_FMT")", SV_ARGS(token_to_strview(exp->tok)));
		break;
	case ast_exp_binary:
		fputc('(', file);
		ast_fprint(exp->bin.left, file);
		fputc(')', file);
		switch ((enum binop)exp->bin.op) {
		case binop_sequence:			fputs("; ", file);    break;
		case binop_add:					fputs(" + ", file);   break;
		case binop_sub:					fputs(" - ", file);   break;
		case binop_mul:					fputs(" * ", file);   break;
		case binop_div:					fputs(" / ", file);   break;
		case binop_mod:					fputs(" % ", file);  break;
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
		ast_fprint(exp->bin.right, file);
		fputc(')', file);
		break;
	case ast_exp_value_cons: FAILWITH("TODO: ast_exp_value_cons"); break;
	case ast_exp_unary:
		if (exp->op == op_call) {
			fputc('(', file);
			ast_fprint(exp->call.proc, file);
			fputc(')', file);
			fputc('(', file);
			if (exp->call.args.len > 0) {
				ast_fprint(exp->call.args.elems[0], file);
				for (size_t i = 1; i < exp->call.args.len; ++i) {
					fputs(", ", file);
					ast_fprint(exp->call.args.elems[i], file);
				}
			}
			fputc(')', file);
		} else if (exp->op == op_index) {
			fputc('(', file);
			ast_fprint(exp->idx.exp, file);
			fputc(')', file);
			fputc('[', file);
			ast_fprint(exp->idx.idx, file);
			fputc(']', file);
		} else if (exp->op == op_cast) {
			ast_fprint(exp->cast.exp, file);
			fputs(" as ", file);
			ast_type_fprint(exp->cast.type, file);
		} else {
			switch ((int)exp->op) {
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
			ast_fprint(exp->una.exp, file);
			fputc(')', file);
		}
		break;
	case ast_exp_if:
		fputs("if ", file);
		ast_fprint(exp->iff.cond, file);
		fputs(" then ", file);
		ast_fprint(exp->iff.tb, file);
		if (exp->iff.fb) {
			fputs(" else ", file);
			ast_fprint(exp->iff.fb, file);
		}
		fputs(" end", file);
		break;
	case ast_exp_while:
		fputs("while ", file);
		ast_fprint(exp->wloop.cond, file);
		fputs(" do ", file);
		ast_fprint(exp->wloop.body, file);
		fputs(" done", file);
		break;
	case ast_exp_case: {
		fputs("case ", file);
		struct exp_case *c = &exp->ccase;
		ast_fprint(c->cexp, file);
		fputs("of ", file);
		fputc('\n', file);
		for (size_t i = 0; i < c->branches.len; ++i) {
			struct case_branch *cb = &c->branches.elems[i];
			fputc('$', file);
			fprintf(file, SV_FMT, SV_ARGS(token_to_strview(cb->cons)));
			if (cb->binds_value) {
				fputc('(', file);
				if (cb->binding.is_mut) fputs("mut ", file);
				if (cb->binding_is_ref) fputc('&', file);
				fprintf(file, SV_FMT")", SV_ARGS(token_to_strview(cb->binding.id)));
			}
			if (cb->guard) {
				fputs(" if ", file);
				ast_fprint(cb->guard, file);
			}
			fputs(" -> ", file);
			ast_fprint(cb->body, file);
			fputs(",\n", file);
		}
		if (c->else_exp) {
			fputs("else ", file);
			ast_fprint(c->else_exp, file);
			fputs(",\n", file);
		}
		fputs("end", file);
	} break;
	case ast_exp_return:
		fputs("return(", file);
		ast_fprint(exp->ret, file);
		fputc(')', file);
		break;
	case ast_exp_break:    FAILWITH("TODO: ast_exp_break"); break;
	case ast_exp_continue: FAILWITH("TODO: ast_exp_continue"); break;
	default:
		FAILWITH("Unreachable condition.");
		break;
	}
}

KC_PUBLIC struct scope *
parse_file(const char *filename, struct expression_stack *tl_exps)
{
	struct strview sv = {0};
	if (sv_open_file(filename, &sv) == false) {
		fprintf(stderr, "[Error] %s: %s\n", filename, strerror(errno));
		EXIT(1);
	}
	if (sv.len == 0) {
		fprintf(stderr, "[Error] %s: %s\n", filename, "Empty file");
		EXIT(1);
	}
	Parser parser = {
		.lexer = {
			.filename = filename,
			.text     = sv.ptr,
			.length   = sv.len,
			.line     = 1,
		}
	};
	tokenize(&parser.lexer, &parser.tokens);
	struct scope *sc = MEM_ALLOC(struct scope);
	while (!parser_is_at_end(&parser))
		parse_toplevel_expression(&parser, tl_exps, sc);
	return sc;
}
