#include "common.h"

KC_PUBLIC char *
token_type_to_str(enum token_type tt)
{
    switch (tt) {
    case tt_true:            return "tt_true";
    case tt_false:           return "tt_false";
    case tt_underscore:      return "tt_underscore";
    case tt_tilde:           return "tt_tilde";
    case tt_tick:            return "tt_tick";
    case tt_plus:            return "tt_plus";
    case tt_minus:           return "tt_minus";
    case tt_equal:           return "tt_equal";
    case tt_bang:            return "tt_bang";
    case tt_at:              return "tt_at";
    case tt_hash:            return "tt_hash";
    case tt_dollar:          return "tt_dollar";
    case tt_percent:         return "tt_percent";
    case tt_caret:           return "tt_caret";
    case tt_amper:           return "tt_amper";
    case tt_star:            return "tt_star";
    case tt_lparen:          return "tt_lparen";
    case tt_rparen:          return "tt_rparen";
    case tt_lbrace:          return "tt_lbrace";
    case tt_rbrace:          return "tt_rbrace";
    case tt_lbracket:        return "tt_lbracket";
    case tt_rbracket:        return "tt_rbracket";
    case tt_pipe:            return "tt_pipe";
    case tt_backslash:       return "tt_backslash";
    case tt_colon:           return "tt_colon";
    case tt_semicolon:       return "tt_semicolon";
    case tt_quote:           return "tt_quote";
    case tt_double_quote:    return "tt_double_quote";
    case tt_less:            return "tt_less";
    case tt_more:            return "tt_more";
    case tt_question:        return "tt_question";
    case tt_comma:           return "tt_comma";
    case tt_period:          return "tt_period";
    case tt_slash:           return "tt_slash";
    case tt_eof:             return "tt_eof";
    case tt_let:             return "tt_let";
    case tt_type:            return "tt_type";
    case tt_newtype:         return "tt_newtype";
    case tt_struct:          return "tt_struct";
    case tt_mut:             return "tt_mut";
    case tt_in:              return "tt_in";
    case tt_end:             return "tt_end";
    case tt_as:              return "tt_as";
    case tt_while:           return "tt_while";
    case tt_for:             return "tt_for";
    case tt_do:              return "tt_do";
    case tt_done:            return "tt_done";
    case tt_break:           return "tt_break";
    case tt_continue:        return "tt_continue";
    case tt_if:              return "tt_if";
    case tt_then:            return "tt_then";
    case tt_else:            return "tt_else";
    case tt_case:            return "tt_case";
    case tt_of:              return "tt_of";
    case tt_return:          return "tt_return";
    case tt_import:          return "tt_import";
    case tt_colon_equal:     return "tt_colon_equal";
    case tt_amper_equal:     return "tt_amper_equal";
    case tt_pipe_equal:      return "tt_pipe_equal";
    case tt_caret_equal:     return "tt_caret_equal";
    case tt_plus_equal:      return "tt_plus_equal";
    case tt_minus_equal:     return "tt_minus_equal";
    case tt_star_equal:      return "tt_star_equal";
    case tt_slash_equal:     return "tt_slash_equal";
    case tt_percent_equal:   return "tt_percent_equal";
    case tt_bang_equal:      return "tt_bang_equal";
    case tt_less_equal:      return "tt_less_equal";
    case tt_more_equal:      return "tt_more_equal";
    case tt_less_less:       return "tt_less_less";
    case tt_more_more:       return "tt_more_more";
    case tt_less_less_equal: return "tt_less_less_equal";
    case tt_more_more_equal: return "tt_more_more_equal";
    case tt_pipe_pipe:       return "tt_pipe_pipe";
    case tt_amper_amper:     return "tt_amper_amper";
    case tt_minus_more:      return "tt_minus_more";
    case tt_period_period:   return "tt_period_period";
    case tt_void:            return "tt_void";
    case tt_bool:            return "tt_bool";
    case tt_string:          return "tt_string";
    case tt_char:            return "tt_char";
    case tt_i8:              return "tt_i8";
    case tt_i16:             return "tt_i16";
    case tt_i32:             return "tt_i32";
    case tt_i64:             return "tt_i64";
    case tt_u8:              return "tt_u8";
    case tt_u16:             return "tt_u16";
    case tt_u32:             return "tt_u32";
    case tt_u64:             return "tt_u64";
    case tt_f32:             return "tt_f32";
    case tt_f64:             return "tt_f64";
    case tt_ident:           return "tt_ident";
    case tt_hexlit:          return "tt_hexlit";
    case tt_intlit:          return "tt_intlit";
    case tt_floatlit:        return "tt_floatlit";
    case tt_undefined:       return "tt_undefined";
    case tt_noreturn:        return "tt_noreturn";
    case tt_extern:          return "tt_extern";
    case tt_ptr:             return "tt_ptr";
    case tt_len:             return "tt_len";
    case tt_sizeof:          return "tt_sizeof";
    case tt_typevar:         return "tt_typevar";
    case TOKEN_TYPE_MAX:
    default:
        assert(0 && "TODO: Unknown token tag");
                             return NULL;
    }
}

KC_PUBLIC char *
show_token(char *str, size_t len, struct token *tok)
{
    snprintf(str, len, "%s(`%.*s`), %d, %d",
             token_type_to_str(tok->tt),
             tok->tok_len,
             &tok->text[tok->offset],
             tok->line,
             tok->column);
    return str;
}

KC_PRIVATE bool
isbrace(int c)
{
    static const char braces[] = { '(', ')', '[', ']', '{', '}' };
    for (size_t i = 0; i < sizeof(braces); ++i) {
        if (braces[i] == c) return true;
    }
    return false;
}

KC_PRIVATE int
lex_lookahead(struct lexer *lex, size_t n)
{
    size_t offset = lex->offset + n;
    if (offset < lex->length) {
        return lex->text[offset];
    } else {
        return EOF;
    }
}

KC_PRIVATE int
lex_peekc(struct lexer *lex)
{
    return lex_lookahead(lex, 0);
}

KC_PRIVATE int
lex_nextc(struct lexer *lex)
{
    if (lex->offset >= lex->length) return EOF;
    int c = lex->text[lex->offset++];
    switch (c) {
    case '\n':
        lex->line++;
        lex->column = 0;
        break;
    case '\t':
        lex->column += TAB_WIDTH;
        break;
    default:
        lex->column++;
        break;
    }
    return c;
}

KC_PRIVATE void
lex_skip_line(struct lexer *lex)
{
    for (;;) {
        int c = lex_nextc(lex);
        if (c == '\n' || c == EOF) break;
    }
}

KC_PRIVATE void
lex_skip_comment(struct lexer *lex)
{
    int c, n;
    int nest_level = 1;
    while (nest_level) {
        c = lex_nextc(lex);
        n = lex_peekc(lex);
        assert(c != EOF);
        if (c == '/' && n == '*') {
            lex_nextc(lex);
            nest_level++;
        } else if (c == '*' && n == '/') {
            lex_nextc(lex);
            nest_level--;
        }
    }
}

KC_PRIVATE void
lex_skip_ws(struct lexer *lex)
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

KC_PRIVATE struct token
make_token(struct lexer *lex)
{
    return (struct token) {
        .filename = lex->filename,
        .text      = lex->text,
        .offset      = lex->offset,
        .text_len = lex->length,
        .tok_len  = 0,
        .line      = lex->line,
        .column      = lex->column,
    };
}

KC_PUBLIC struct strview
token_to_strview(struct token *tok)
{
    return (struct strview) {
        .ptr = tok->text + tok->offset,
        .len = tok->tok_len,
    };
}

KC_PRIVATE bool
lex_str_char(struct lexer *lex, struct token *tok)
{
    switch (lex_peekc(lex)) {
    case '\\':
        lex_nextc(lex);
        lex_nextc(lex);
        tok->tok_len += 2;
        return true;
    case '\"':
        lex_nextc(lex);
        tok->tok_len++;
        return false;
    case EOF: log_compile_error_and_die(lex->filename, tok, "Syntax error"); break;
    default:
        lex_nextc(lex);
        tok->tok_len++;
        return true;
    }
}

KC_PUBLIC void
tokenize(struct lexer *lex, struct token_buffer *tokens)
{
    for (;;) {
        lex_skip_ws(lex);
        struct token tok = make_token(lex);
        int c = lex_nextc(lex);
        tok.tok_len++;
        if (c == EOF) {
            tok.tt = tt_eof;
            da_append(tokens, tok);
            return;
        }
        if (c == '#' && lex_peekc(lex) == '\"') {
            tok.tt = tt_char;
            lex_nextc(lex);
            tok.tok_len++;
            while (lex_str_char(lex, &tok));
        } else if (c == '#' || c == '_' || isalpha(c)) {
            /* Identifier */
            while ((c = lex_peekc(lex)) == '_' || isalnum(c)) {
                lex_nextc(lex);
                tok.tok_len++;
            }
            /* Match Keywords */
            struct strview sv = token_to_strview(&tok);
            CHECK_EXAUSTIVE_KEYWORDS(69);
            if (sv_is_equal(sv, sv_of_cstr("_")))               tok.tt = tt_underscore;
            else if (sv_is_equal(sv, sv_of_cstr("let")))        tok.tt = tt_let;
            else if (sv_is_equal(sv, sv_of_cstr("type")))       tok.tt = tt_type;
            else if (sv_is_equal(sv, sv_of_cstr("newtype")))    tok.tt = tt_newtype;
            else if (sv_is_equal(sv, sv_of_cstr("struct")))     tok.tt = tt_struct;
            else if (sv_is_equal(sv, sv_of_cstr("mut")))        tok.tt = tt_mut;
            else if (sv_is_equal(sv, sv_of_cstr("in")))         tok.tt = tt_in;
            else if (sv_is_equal(sv, sv_of_cstr("end")))        tok.tt = tt_end;
            else if (sv_is_equal(sv, sv_of_cstr("as")))         tok.tt = tt_as;
            else if (sv_is_equal(sv, sv_of_cstr("while")))      tok.tt = tt_while;
            else if (sv_is_equal(sv, sv_of_cstr("for")))        tok.tt = tt_for;
            else if (sv_is_equal(sv, sv_of_cstr("do")))         tok.tt = tt_do;
            else if (sv_is_equal(sv, sv_of_cstr("done")))       tok.tt = tt_done;
            else if (sv_is_equal(sv, sv_of_cstr("break")))      tok.tt = tt_break;
            else if (sv_is_equal(sv, sv_of_cstr("continue")))   tok.tt = tt_continue;
            else if (sv_is_equal(sv, sv_of_cstr("if")))         tok.tt = tt_if;
            else if (sv_is_equal(sv, sv_of_cstr("then")))       tok.tt = tt_then;
            else if (sv_is_equal(sv, sv_of_cstr("else")))       tok.tt = tt_else;
            else if (sv_is_equal(sv, sv_of_cstr("case")))       tok.tt = tt_case;
            else if (sv_is_equal(sv, sv_of_cstr("of")))         tok.tt = tt_of;
            else if (sv_is_equal(sv, sv_of_cstr("return")))     tok.tt = tt_return;
            else if (sv_is_equal(sv, sv_of_cstr("true")))       tok.tt = tt_true;
            else if (sv_is_equal(sv, sv_of_cstr("false")))      tok.tt = tt_false;
            else if (sv_is_equal(sv, sv_of_cstr("void")))       tok.tt = tt_void;
            else if (sv_is_equal(sv, sv_of_cstr("bool")))       tok.tt = tt_bool;
            else if (sv_is_equal(sv, sv_of_cstr("i8")))         tok.tt = tt_i8;
            else if (sv_is_equal(sv, sv_of_cstr("i16")))        tok.tt = tt_i16;
            else if (sv_is_equal(sv, sv_of_cstr("i32")))        tok.tt = tt_i32;
            else if (sv_is_equal(sv, sv_of_cstr("i64")))        tok.tt = tt_i64;
            else if (sv_is_equal(sv, sv_of_cstr("u8")))         tok.tt = tt_u8;
            else if (sv_is_equal(sv, sv_of_cstr("u16")))        tok.tt = tt_u16;
            else if (sv_is_equal(sv, sv_of_cstr("u32")))        tok.tt = tt_u32;
            else if (sv_is_equal(sv, sv_of_cstr("u64")))        tok.tt = tt_u64;
            else if (sv_is_equal(sv, sv_of_cstr("f32")))        tok.tt = tt_f32;
            else if (sv_is_equal(sv, sv_of_cstr("f64")))        tok.tt = tt_f64;
            else if (sv_is_equal(sv, sv_of_cstr("import")))     tok.tt = tt_import;
            else if (sv_is_equal(sv, sv_of_cstr("#undefined"))) tok.tt = tt_undefined;
            else if (sv_is_equal(sv, sv_of_cstr("#noreturn")))  tok.tt = tt_noreturn;
            else if (sv_is_equal(sv, sv_of_cstr("#extern")))    tok.tt = tt_extern;
            else if (sv_is_equal(sv, sv_of_cstr("#ptr")))       tok.tt = tt_ptr;
            else if (sv_is_equal(sv, sv_of_cstr("#len")))       tok.tt = tt_len;
            else if (sv_is_equal(sv, sv_of_cstr("#sizeof")))    tok.tt = tt_sizeof;
            else tok.tt = tt_ident;
        } else if (isdigit(c)) {
            c = lex_peekc(lex);
            if (c == 'x' || c == 'X') {
                lex_nextc(lex);
                tok.tok_len++;
                while (isxdigit(lex_peekc(lex))) {
                    lex_nextc(lex);
                    tok.tok_len++;
                }
                tok.tt = tt_hexlit;
            } else {
                while (isdigit(lex_peekc(lex))) {
                    lex_nextc(lex);
                    tok.tok_len++;
                }
                if (lex_peekc(lex) == '.') {
                    if (lex_lookahead(lex, 1) == '.') {
                        tok.tt = tt_intlit;
                    } else {
                        lex_nextc(lex);
                        tok.tok_len++;
                        if (!isdigit(lex_peekc(lex))) {
                            log_compile_error_and_die(lex->filename, &tok, "Syntax error");
                        }
                        while (isdigit(lex_peekc(lex))) {
                            lex_nextc(lex);
                            tok.tok_len++;
                        }
                        tok.tt = tt_floatlit;
                    }
                } else {
                    tok.tt = tt_intlit;
                }
            }
        } else {
            switch (c) {
            case '\"': {
                tok.tt = tt_string;
                while (lex_str_char(lex, &tok));
            } break;
            case '\'':
                tok.tt = tt_typevar;
                lex_nextc(lex);
                tok.tok_len++;
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
                        || c == ','
                        || !ispunct(c)
                        || isbrace(c)) break;
                    lex_nextc(lex);
                    tok.tok_len++;
                }
                struct strview sv = token_to_strview(&tok);
                if (tok.tok_len == 1) tok.tt = c;
                else if (sv_is_equal(sv, sv_of_cstr(":=")))  tok.tt = tt_colon_equal;
                else if (sv_is_equal(sv, sv_of_cstr("&=")))  tok.tt = tt_amper_equal;
                else if (sv_is_equal(sv, sv_of_cstr("|=")))  tok.tt = tt_pipe_equal;
                else if (sv_is_equal(sv, sv_of_cstr("^=")))  tok.tt = tt_caret_equal;
                else if (sv_is_equal(sv, sv_of_cstr("+=")))  tok.tt = tt_plus_equal;
                else if (sv_is_equal(sv, sv_of_cstr("-=")))  tok.tt = tt_minus_equal;
                else if (sv_is_equal(sv, sv_of_cstr("*=")))  tok.tt = tt_star_equal;
                else if (sv_is_equal(sv, sv_of_cstr("/=")))  tok.tt = tt_slash_equal;
                else if (sv_is_equal(sv, sv_of_cstr("%=")))  tok.tt = tt_percent_equal;
                else if (sv_is_equal(sv, sv_of_cstr("!=")))  tok.tt = tt_bang_equal;
                else if (sv_is_equal(sv, sv_of_cstr("<=")))  tok.tt = tt_less_equal;
                else if (sv_is_equal(sv, sv_of_cstr(">=")))  tok.tt = tt_more_equal;
                else if (sv_is_equal(sv, sv_of_cstr("<<")))  tok.tt = tt_less_less;
                else if (sv_is_equal(sv, sv_of_cstr(">>")))  tok.tt = tt_more_more;
                else if (sv_is_equal(sv, sv_of_cstr("<<="))) tok.tt = tt_less_less_equal;
                else if (sv_is_equal(sv, sv_of_cstr(">>="))) tok.tt = tt_more_more_equal;
                else if (sv_is_equal(sv, sv_of_cstr("&&")))  tok.tt = tt_amper_amper;
                else if (sv_is_equal(sv, sv_of_cstr("||")))  tok.tt = tt_pipe_pipe;
                else if (sv_is_equal(sv, sv_of_cstr("->")))  tok.tt = tt_minus_more;
                else if (sv_is_equal(sv, sv_of_cstr("..")))  tok.tt = tt_period_period;
                else {
                    log_compile_error_and_die(lex->filename, &tok, "invalid operator `"SV_FMT"`.", SV_ARGS(sv));
                }
                break;
            }
        }
        da_append(tokens, tok);
    }
}
