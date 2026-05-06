#pragma once

typedef struct parser {
	struct lexer lexer;
	struct token_buffer tokens;
} Parser;

#define ERROR_UNDEFINED_IDENT(token)  error_undefined_ident(p, token, __FILE__, __LINE__)

static bool parser_is_at_end(Parser *p);
static struct expression *parse_toplevel_expression(Parser *p, struct scope *scope);
static char *ast_type_to_str(KCType *t);
static void error_undefined_ident(Parser *p, struct token *id, const char *debug_filename, const int debug_line);
static bool exp_is_integer_literal(struct expression *exp);
static void ast_fprint(struct expression *exp, FILE *file);
static void ast_type_fprint(KCType *t, FILE *file);
static struct proc_type procedure_type(struct procedure *proc);
