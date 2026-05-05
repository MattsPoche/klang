#pragma once

#include "strview.h"
#define TAB_WIDTH 4

enum token_type {
	tt_underscore   = '_',
	tt_tilde        = '~',
	tt_tick         = '`',
	tt_plus         = '+',
	tt_minus        = '-',
	tt_equal        = '=',
	tt_bang         = '!',
	tt_at           = '@',
	tt_hash         = '#',
	tt_dollar       = '$',
	tt_percent      = '%',
	tt_caret        = '^',
	tt_and          = '&',
	tt_star         = '*',
	tt_lparen       = '(',
	tt_rparen       = ')',
	tt_lbrace       = '{',
	tt_rbrace       = '}',
	tt_lbracket     = '[',
	tt_rbracket     = ']',
	tt_pipe         = '|',
	tt_backslash    = '\\',
	tt_colon        = ':',
	tt_semicolon    = ';',
	tt_quote        = '\'',
	tt_double_quote = '\"',
	tt_less         = '<',
	tt_more         = '>',
	tt_question     = '?',
	tt_comma        = ',',
	tt_period       = '.',
	tt_slash        = '/',
	tt_eof = 0x100,
	/* Keywords */
	tt_struct,
	tt_type,
	tt_newtype,
	tt_let,
	tt_mut,
	tt_in,
	tt_end,
	tt_as,
	tt_while,
	tt_for,
	tt_do,
	tt_done,
	tt_break,
	tt_continue,
	tt_if,
	tt_then,
	tt_else,
	tt_elif,
	tt_case,
	tt_of,
	tt_return,
	tt_true,
	tt_false,
	tt_colon_equal,     // :=
	tt_and_equal,       // &=
	tt_pipe_equal,      // |=
	tt_caret_equal,     // ^=
	tt_plus_equal,      // +=
	tt_minus_equal,     // -=
	tt_star_equal,      // *=
	tt_slash_equal,     // /=
	tt_percent_equal,   // %=
	tt_bang_equal,      // !=
	tt_less_equal,      // <=
	tt_more_equal,      // >=
	tt_less_less,       // <<
	tt_more_more,       // >>
	tt_less_less_equal, // <<=
	tt_more_more_equal, // >>=
	tt_pipe_pipe,       // ||
	tt_and_and,         // &&
	tt_minus_more,      // ->
	tt_period_period,   // ..
	/* Basic Data Types */
	tt_void,
	tt_bool,
	tt_string,
	tt_typevar,
	/* Integer */
	tt_i8,
	tt_i16,
	tt_i32,
	tt_i64,
	tt_u8,
	tt_u16,
	tt_u32,
	tt_u64,
	/* Floating Point */
	tt_f32,
	tt_f64,
	tt_ident,
	tt_hexlit,
	tt_intlit,
	tt_floatlit,
	/* Hash symbols */
	tt_undefined,
	tt_noreturn,
	tt_extern,
	tt_ptr,
	tt_len,
	TOKEN_TYPE_MAX,
};

#define CHECK_EXAUSTIVE_KEYWORDS(n) static_assert(TOKEN_TYPE_MAX - tt_eof == (n))
CHECK_EXAUSTIVE_KEYWORDS(67);

struct token {
	enum token_type tt;
	char *text;
	uint32_t text_len;
	uint32_t tok_len;
	uint32_t offset;
	uint32_t line;
	uint32_t column;
};

struct token_buffer {
	uint32_t len, cap;
	size_t idx;
	struct token *elems;
};

struct token_ptrs {
	uint32_t len, cap;
	struct token **elems;
};

struct lexer {
	const char *filename;
	char *text;
	uint32_t length;
	uint32_t offset;
	uint32_t line;
	uint32_t column;
};

static char *token_type_to_str(enum token_type tt);
static void tokenize(struct lexer *lex, struct token_buffer *tokens);
static struct strview token_to_strview(struct token *tok);
static char *show_token(char *str, size_t len, struct token *tok);
