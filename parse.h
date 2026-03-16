#pragma once

typedef struct parser {
	struct lexer lexer;
	struct token_buffer tokens;
	struct mem_arena data;
} Parser;

#define POOL_ALLOC(p, type) ((type*)mem_arena_zero_alloc(p, sizeof(type)))

bool parser_is_at_end(Parser *p);
struct expression *parse_toplevel_expression(Parser *p, struct scope *scope);
void ast_fprint(struct expression *exp, FILE *file);
struct definition *lookup_definition(struct scope *scope, struct strview name);
char *ast_type_to_str(struct type *t);
