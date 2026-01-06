#pragma once

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "log.h"
#include "da.h"
#include "mem.h"
#include "lex.h"

char *token_type_to_str(enum token_type tt)
{
	switch (tt) {
	case tt_true: 		        return "tt_true";
	case tt_false: 		        return "tt_false";
	case tt_underscore: 		return "tt_underscore";
	case tt_tilde:				return "tt_tilde";
	case tt_tick:				return "tt_tick";
	case tt_plus:				return "tt_plus";
	case tt_minus:				return "tt_minus";
	case tt_equal:				return "tt_equal";
	case tt_bang:				return "tt_bang";
	case tt_at:					return "tt_at";
	case tt_hash:				return "tt_hash";
	case tt_dollar:				return "tt_dollar";
	case tt_percent:			return "tt_percent";
	case tt_caret:				return "tt_caret";
	case tt_and:				return "tt_and";
	case tt_star:				return "tt_star";
	case tt_lparen:				return "tt_lparen";
	case tt_rparen:				return "tt_rparen";
	case tt_lbrace:				return "tt_lbrace";
	case tt_rbrace:				return "tt_rbrace";
	case tt_lbracket:			return "tt_lbracket";
	case tt_rbracket:			return "tt_rbracket";
	case tt_pipe:				return "tt_pipe";
	case tt_backslash:			return "tt_backslash";
	case tt_colon:				return "tt_colon";
	case tt_semicolon:			return "tt_semicolon";
	case tt_quote:				return "tt_quote";
	case tt_double_quote:		return "tt_double_quote";
	case tt_less:				return "tt_less";
	case tt_more:				return "tt_more";
	case tt_question:			return "tt_question";
	case tt_comma:				return "tt_comma";
	case tt_period:				return "tt_period";
	case tt_slash:				return "tt_slash";
	case tt_eof:				return "tt_eof";
	case tt_let:                return "tt_let";
	case tt_mut:                return "tt_mut";
	case tt_in:                 return "tt_in";
	case tt_end:                return "tt_end";
	case tt_as:                 return "tt_as";
	case tt_while:				return "tt_while";
	case tt_for:				return "tt_for";
	case tt_do:				    return "tt_do";
	case tt_done:				return "tt_done";
	case tt_break:			    return "tt_break";
	case tt_continue:			return "tt_continue";
	case tt_if:					return "tt_if";
	case tt_then:				return "tt_then";
	case tt_else:				return "tt_else";
	case tt_elif:				return "tt_elif";
	case tt_case:				return "tt_case";
	case tt_of: 				return "tt_of";
	case tt_return:             return "tt_return";
	case tt_colon_equal:        return "tt_colon_equal";
	case tt_and_equal:			return "tt_and_equal";
	case tt_pipe_equal:			return "tt_pipe_equal";
	case tt_caret_equal:		return "tt_caret_equal";
	case tt_plus_equal:			return "tt_plus_equal";
	case tt_minus_equal:		return "tt_minus_equal";
	case tt_star_equal:			return "tt_star_equal";
	case tt_slash_equal:		return "tt_slash_equal";
	case tt_percent_equal:		return "tt_percent_equal";
	case tt_bang_equal:			return "tt_bang_equal";
	case tt_less_equal:			return "tt_less_equal";
	case tt_more_equal:			return "tt_more_equal";
	case tt_less_less:			return "tt_less_less";
	case tt_more_more:			return "tt_more_more";
	case tt_less_less_equal:	return "tt_less_less_equal";
	case tt_more_more_equal:	return "tt_more_more_equal";
	case tt_pipe_pipe:			return "tt_pipe_pipe";
	case tt_and_and:			return "tt_and_and";
	case tt_minus_more:			return "tt_minus_more";
	case tt_void:				return "tt_void";
	case tt_bool:				return "tt_bool";
	case tt_i8:					return "tt_i8";
	case tt_i16:				return "tt_i16";
	case tt_i32:				return "tt_i32";
	case tt_i64:				return "tt_i64";
	case tt_u8:					return "tt_u8";
	case tt_u16:				return "tt_u16";
	case tt_u32:				return "tt_u32";
	case tt_u64:				return "tt_u64";
	case tt_f32:				return "tt_f32";
	case tt_f64:				return "tt_f64";
	case tt_ident:				return "tt_ident";
	case tt_hexlit:				return "tt_hexlit";
	case tt_intlit:				return "tt_intlit";
	case tt_floatlit:			return "tt_floatlit";
	case tt_undefined:			return "tt_undefined";
	case tt_noreturn:			return "tt_noreturn";
	case TOKEN_TYPE_MAX:
	default:
		assert(0 && "TODO: Unknown token tag");
		return NULL;
	}
}

char *show_token(char *str, size_t len, struct token *tok)
{
	snprintf(str, len, "%s(`"SV_FMT"`), %d, %d",
			 token_type_to_str(tok->tt),
			 SV_ARGS(&tok->sv),
			 tok->loc.line,
			 tok->loc.column);
	return str;
}

static bool isbrace(int c)
{
	static const int braces[] = { '(', ')', '[', ']', '{', '}' };
	for (size_t i = 0; i < sizeof(braces)-1; ++i) {
		if (braces[i] == c) return true;
	}
	return false;
}

static int lex_lookahead(struct lexer *lex, size_t n)
{
	if (n < lex->sv.len) {
		return lex->sv.ptr[n];
	} else {
		return EOF;
	}
}

static int lex_peekc(struct lexer *lex)
{
	return lex_lookahead(lex, 0);
}

static int lex_nextc(struct lexer *lex)
{
	if (lex->sv.len == 0) return EOF;
	int c = lex->sv.ptr[0];
	lex->sv.len--;
	lex->sv.ptr++;
	switch (c) {
	case '\n':
		lex->loc.line++;
		lex->loc.column = 0;
		break;
	case '\t':
		lex->loc.column += TAB_WIDTH;
		break;
	default:
		lex->loc.column++;
		break;
	}
	return c;
}

static void lex_skip_line(struct lexer *lex)
{
	for (;;) {
		int c = lex_nextc(lex);
		if (c == '\n' || c == EOF) break;
	}
}

static void lex_skip_comment(struct lexer *lex)
{
	for (;;) {
		int c = lex_nextc(lex);
		assert(c != EOF);
		if (c == '*' && lex_peekc(lex) == '/') {
			lex_nextc(lex);
			break;
		}
	}
}

static void lex_skip_ws(struct lexer *lex)
{
	for (;;) {
		int c = lex_peekc(lex);
		if (isspace(c)) {
			lex_nextc(lex);
		} else if (c == '/' && lex_lookahead(lex, 1) == '/') {
			/* single line comment */
			lex_nextc(lex);
			lex_nextc(lex);
			lex_skip_line(lex);
		} else if (c == '/' && lex_lookahead(lex, 1) == '*') {
			/* multi line comment */
			lex_nextc(lex);
			lex_nextc(lex);
			lex_skip_comment(lex);
		} else {
			break;
		}
	}
}

void tokenize(struct lexer *lex, struct token_buffer *tokens)
{
	for (;;) {
		lex_skip_ws(lex);
		struct token tok = {
			.loc = lex->loc,
			.sv.len = 0,
			.sv.ptr = lex->sv.ptr,
		};
		int c = lex_nextc(lex);
		tok.sv.len++;
		if (c == EOF) {
			tok.tt = tt_eof;
			da_append(tokens, tok);
			return;
		}
		if (c == '_' || isalpha(c)) {
			/* Identifier */
			while ((c = lex_peekc(lex)) == '_' || isalnum(c)) {
				lex_nextc(lex);
				tok.sv.len++;
			}
			/* Match Keywords */
			CHECK_EXAUSTIVE_KEYWORDS(58);
			if (sv_is_equal(tok.sv, sv_of_cstr("_")))              tok.tt = tt_underscore;
			else if (sv_is_equal(tok.sv, sv_of_cstr("let")))       tok.tt = tt_let;
			else if (sv_is_equal(tok.sv, sv_of_cstr("mut")))       tok.tt = tt_mut;
			else if (sv_is_equal(tok.sv, sv_of_cstr("in")))        tok.tt = tt_in;
			else if (sv_is_equal(tok.sv, sv_of_cstr("end")))       tok.tt = tt_end;
			else if (sv_is_equal(tok.sv, sv_of_cstr("as")))        tok.tt = tt_as;
			else if (sv_is_equal(tok.sv, sv_of_cstr("while")))     tok.tt = tt_while;
			else if (sv_is_equal(tok.sv, sv_of_cstr("for")))       tok.tt = tt_for;
			else if (sv_is_equal(tok.sv, sv_of_cstr("do")))        tok.tt = tt_do;
			else if (sv_is_equal(tok.sv, sv_of_cstr("done")))      tok.tt = tt_done;
			else if (sv_is_equal(tok.sv, sv_of_cstr("break")))     tok.tt = tt_break;
			else if (sv_is_equal(tok.sv, sv_of_cstr("continue")))  tok.tt = tt_continue;
			else if (sv_is_equal(tok.sv, sv_of_cstr("if")))        tok.tt = tt_if;
			else if (sv_is_equal(tok.sv, sv_of_cstr("then")))      tok.tt = tt_then;
			else if (sv_is_equal(tok.sv, sv_of_cstr("else")))      tok.tt = tt_else;
			else if (sv_is_equal(tok.sv, sv_of_cstr("elif")))      tok.tt = tt_elif;
			else if (sv_is_equal(tok.sv, sv_of_cstr("case")))      tok.tt = tt_case;
			else if (sv_is_equal(tok.sv, sv_of_cstr("of")))        tok.tt = tt_of;
			else if (sv_is_equal(tok.sv, sv_of_cstr("return")))    tok.tt = tt_return;
			else if (sv_is_equal(tok.sv, sv_of_cstr("true")))      tok.tt = tt_true;
			else if (sv_is_equal(tok.sv, sv_of_cstr("false")))     tok.tt = tt_false;
			else if (sv_is_equal(tok.sv, sv_of_cstr("void")))      tok.tt = tt_void;
			else if (sv_is_equal(tok.sv, sv_of_cstr("bool")))      tok.tt = tt_bool;
			else if (sv_is_equal(tok.sv, sv_of_cstr("i8")))        tok.tt = tt_i8;
			else if (sv_is_equal(tok.sv, sv_of_cstr("i16")))       tok.tt = tt_i16;
			else if (sv_is_equal(tok.sv, sv_of_cstr("i32")))       tok.tt = tt_i32;
			else if (sv_is_equal(tok.sv, sv_of_cstr("i64")))       tok.tt = tt_i64;
			else if (sv_is_equal(tok.sv, sv_of_cstr("u8")))        tok.tt = tt_u8;
			else if (sv_is_equal(tok.sv, sv_of_cstr("u16")))       tok.tt = tt_u16;
			else if (sv_is_equal(tok.sv, sv_of_cstr("u32")))       tok.tt = tt_u32;
			else if (sv_is_equal(tok.sv, sv_of_cstr("u64")))       tok.tt = tt_u64;
			else if (sv_is_equal(tok.sv, sv_of_cstr("f32")))       tok.tt = tt_f32;
			else if (sv_is_equal(tok.sv, sv_of_cstr("f64")))       tok.tt = tt_f64;
			else if (sv_is_equal(tok.sv, sv_of_cstr("undefined"))) tok.tt = tt_undefined;
			else if (sv_is_equal(tok.sv, sv_of_cstr("noreturn")))  tok.tt = tt_noreturn;
			else tok.tt = tt_ident;
		} else if (isdigit(c)) {
			c = lex_peekc(lex);
			if (c == 'x' || c == 'X') {
				lex_nextc(lex);
				tok.sv.len++;
				while (isxdigit(lex_peekc(lex))) {
					lex_nextc(lex);
					tok.sv.len++;
				}
				tok.tt = tt_hexlit;
			} else {
				while (isdigit(lex_peekc(lex))) {
					lex_nextc(lex);
					tok.sv.len++;
				}
				if (lex_peekc(lex) == '.') {
					lex_nextc(lex);
					tok.sv.len++;
					if (!isdigit(lex_peekc(lex))) {
						log_error_and_die(lex->filename, lex->contents, tok.sv, tok.loc, "Syntax error");
					}
					while (isdigit(lex_peekc(lex))) {
						lex_nextc(lex);
						tok.sv.len++;
					}
					tok.tt = tt_floatlit;
				} else {
					tok.tt = tt_intlit;
				}
			}
		} else {
			switch (c) {
			case '\"':
				assert(0 && "TODO: lex string literal.");
				break;
			case '\'':
				assert(0 && "TODO: lex char literal.");
				break;
			case '(':
			case ')':
			case '{':
			case '}':
			case '[':
			case ']':
			case '\\':
			case ',':
				tok.tt = c;
				break;
			default:
				for (;;) {
					int c = lex_peekc(lex);
					if (c == '*'
						|| !ispunct(c)
						|| isbrace(c)) break;
					lex_nextc(lex);
					tok.sv.len++;
				}
				if (tok.sv.len == 1) tok.tt = c;
				else if (sv_is_equal(tok.sv, sv_of_cstr(":=")))  tok.tt = tt_colon_equal;
				else if (sv_is_equal(tok.sv, sv_of_cstr("&=")))  tok.tt = tt_and_equal;
				else if (sv_is_equal(tok.sv, sv_of_cstr("|=")))  tok.tt = tt_pipe_equal;
				else if (sv_is_equal(tok.sv, sv_of_cstr("^=")))  tok.tt = tt_caret_equal;
				else if (sv_is_equal(tok.sv, sv_of_cstr("+=")))  tok.tt = tt_plus_equal;
				else if (sv_is_equal(tok.sv, sv_of_cstr("-=")))  tok.tt = tt_minus_equal;
				else if (sv_is_equal(tok.sv, sv_of_cstr("*=")))  tok.tt = tt_star_equal;
				else if (sv_is_equal(tok.sv, sv_of_cstr("/=")))  tok.tt = tt_slash_equal;
				else if (sv_is_equal(tok.sv, sv_of_cstr("%=")))  tok.tt = tt_percent_equal;
				else if (sv_is_equal(tok.sv, sv_of_cstr("!=")))  tok.tt = tt_bang_equal;
				else if (sv_is_equal(tok.sv, sv_of_cstr("<=")))  tok.tt = tt_less_equal;
				else if (sv_is_equal(tok.sv, sv_of_cstr(">=")))  tok.tt = tt_more_equal;
				else if (sv_is_equal(tok.sv, sv_of_cstr("<<")))  tok.tt = tt_less_less;
				else if (sv_is_equal(tok.sv, sv_of_cstr(">>")))  tok.tt = tt_more_more;
				else if (sv_is_equal(tok.sv, sv_of_cstr("<<="))) tok.tt = tt_less_less_equal;
				else if (sv_is_equal(tok.sv, sv_of_cstr(">>="))) tok.tt = tt_more_more_equal;
				else if (sv_is_equal(tok.sv, sv_of_cstr("&&")))  tok.tt = tt_and_and;
				else if (sv_is_equal(tok.sv, sv_of_cstr("||")))  tok.tt = tt_pipe_pipe;
				else if (sv_is_equal(tok.sv, sv_of_cstr("->")))  tok.tt = tt_minus_more;
				else {
					printf("invalid operator = `"SV_FMT"`.\n", SV_ARGS(&tok.sv));
					assert(0 && "TODO: invalid operator.");
				}
				break;
			}
		}
		da_append(tokens, tok);
	}
}
