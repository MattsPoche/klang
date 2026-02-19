#pragma once

typedef struct parser {
	struct lexer lexer;
	struct token_buffer tokens;
	struct mem_arena data;
} Parser;

#define POOL_ALLOC(p, type) mem_arena_alloc(p, sizeof(type))

struct expression *parse_toplevel_expression(Parser *p, struct symtbl *symtbl);
void ast_fprint(struct expression *exp, FILE *file);
struct definition *lookup_definition(struct scope *scope, struct strview name);
char *ast_type_to_str(struct type *t);
