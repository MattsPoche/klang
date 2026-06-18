#pragma once

typedef struct parser {
	struct lexer lexer;
	struct token_buffer tokens;
} Parser;

#define ERROR_UNDEFINED_IDENT(token)  error_undefined_ident(token, __FILE__, __LINE__)

KC_PUBLIC struct scope *parse_file(const char *filename, struct expression_stack *tl_exps);
KC_PUBLIC bool parser_is_at_end(Parser *p);
KC_PUBLIC const char *ast_type_to_str(KCType *t);
KC_PUBLIC void error_undefined_ident(struct token *id, const char *debug_filename, const int debug_line);
KC_PUBLIC void ast_fprint(struct expression *exp, FILE *file);
KC_PUBLIC void ast_type_fprint(KCType *t, FILE *file);
KC_PUBLIC struct proc_type procedure_type(struct procedure *proc);
