#pragma once

typedef struct parser {
	struct lexer lexer;
	struct token_buffer tokens;
	struct mem_arena data;
} Parser;

#define POOL_ALLOC(p, type) mem_arena_alloc(p, sizeof(type))

struct expression *parse_toplevel_expression(Parser *p, struct symtbl *symtbl);
void print_error_message(const char *filename,
						 int line,
						 int column,
						 const char *debug_filename,
						 const int debug_line,
						 const char *msg,
						 ...);
void ast_fprint(struct expression *exp, FILE *file);
