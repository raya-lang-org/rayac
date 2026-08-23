#include "lexer.h"
#include <ctype.h>

static struct { const char* word; TokenKind kind; } keywords[] = {
    {"break",     TOK_BREAK},
    {"comptime",  TOK_COMPTIME},
    {"const",     TOK_CONST},
    {"continue",  TOK_CONTINUE},
    {"defer",     TOK_DEFER},
    {"else",      TOK_ELSE},
    {"errdefer",  TOK_ERRDEFER},
    {"extend",    TOK_EXTEND},
    {"false",     TOK_FALSE},
    {"fn",        TOK_FN},
    {"for",       TOK_FOR},
    {"if",        TOK_IF},
    {"in",        TOK_IN},
    {"import",    TOK_IMPORT},
    {"match",     TOK_MATCH},
    {"module",    TOK_MODULE},
    {"noreturn",  TOK_NORETURN},
    {"null",      TOK_NULL},
    {"pub",       TOK_PUB},
    {"return",    TOK_RETURN},
    {"struct",    TOK_STRUCT},
    {"test",      TOK_TEST},
    {"traits",    TOK_TRAITS},
    {"true",      TOK_TRUE},
    {"try",       TOK_TRY},
    {"type",      TOK_TYPE},
    {"union",     TOK_UNION},
    {"unsafe",    TOK_UNSAFE},
    {"var",       TOK_VAR},
    {"while",     TOK_WHILE},
    {"enum",TOK_ENUM},
    {"as", TOK_AS},
    {"with", TOK_WITH},
    {"undefined", TOK_UNDEFINED},
    {"self", TOK_SELF},
    {"Self",  TOK_CAP_SELF},
};
#define KEYWORD_COUNT (sizeof(keywords) / sizeof(keywords[0]))

static TokenKind lookup_keyword(const char* s, size_t len) {
    for (size_t i = 0; i < KEYWORD_COUNT; i++) {
        if (strlen(keywords[i].word) == len && memcmp(keywords[i].word, s, len) == 0) {
            return keywords[i].kind;
        }
    }
    return TOK_IDENTIFIER;
}

static bool is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_ident_char(char c) {
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

static bool is_digit(char c) { return c >= '0' && c <= '9'; }
static bool is_hex_digit(char c) {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static char lexer_peek_char(Lexer* l, size_t ahead) {
    if (l->pos + ahead >= l->source_len) return '\0';
    return l->source[l->pos + ahead];
}

static char lexer_advance_char(Lexer* l) {
    if (l->pos >= l->source_len) return '\0';
    char c = l->source[l->pos];
    l->pos++;
    if (c == '\n') { l->line++; l->column = 1; }
    else { l->column++; }
    return c;
}

static void lexer_skip_whitespace(Lexer* l) {
    while (l->pos < l->source_len) {
        char c = l->source[l->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
            c == '\f' || c == '\v') {
            lexer_advance_char(l);
        } else if (c == '/' && l->pos + 1 < l->source_len) {
            char next = l->source[l->pos + 1];
            if (next == '/') {
                while (l->pos < l->source_len && l->source[l->pos] != '\n') {
                    l->pos++;
                }
            } else if (next == '*') {
                l->pos += 2; l->column += 2;
                int depth = 1;
                while (l->pos < l->source_len && depth > 0) {
                    if (l->source[l->pos] == '/' && l->pos + 1 < l->source_len &&
                        l->source[l->pos + 1] == '*') {
                        depth++;
                        l->pos += 2;
                    } else if (l->source[l->pos] == '*' && l->pos + 1 < l->source_len &&
                               l->source[l->pos + 1] == '/') {
                        depth--;
                        l->pos += 2;
                    } else {
                        if (l->source[l->pos] == '\n') { l->line++; l->column = 1; }
                        else { l->column++; }
                        l->pos++;
                    }
                    if (depth > 16) {
                        diag_error(l->diag, loc_make(l->filename, l->line, l->column, l->pos),
                                   "block comment nesting exceeds maximum depth of 16");
                        break;
                    }
                }
            } else {
                break;
            }
        } else {
            break;
        }
    }
}

static Token make_token(Lexer* l, TokenKind kind, size_t start, size_t len) {
    return (Token){
        .kind = kind,
        .text = sv_from_ptr_len(l->source + start, len),
        .loc = loc_make(l->filename, l->line, l->column - len, start),
        .int_value = 0,
    };
}

void lexer_init(Lexer* l, const char* source, size_t len, const char* filename,
                DiagnosticEngine* diag) {
    l->source = source;
    l->source_len = len;
    l->pos = 0;
    l->line = 1;
    l->column = 1;
    l->filename = filename;
    l->diag = diag;
    l->has_current = false;
}

Token lexer_next(Lexer* l) {
    if (l->has_current) {
        l->has_current = false;
        return l->current;
    }

    lexer_skip_whitespace(l);

    if (l->pos >= l->source_len) {
        return make_token(l, TOK_EOF, l->source_len, 0);
    }

    size_t start = l->pos;
    size_t start_line = l->line;
    size_t start_col = l->column;
    char c = lexer_advance_char(l);

    if (is_ident_start(c)) {
        while (l->pos < l->source_len && is_ident_char(l->source[l->pos])) {
            lexer_advance_char(l);
        }
        size_t len = l->pos - start;
        TokenKind kind = lookup_keyword(l->source + start, len);
        Token t = make_token(l, kind, start, len);
        t.loc.line = start_line;
        t.loc.column = start_col;
        return t;
    }

    if (is_digit(c) || (c == '.' && is_digit(lexer_peek_char(l, 0)))) {
        int base = 10;
        if (c == '0' && l->pos < l->source_len) {
            char next = l->source[l->pos];
            if (next == 'x' || next == 'X') { base = 16; lexer_advance_char(l); }
            else if (next == 'o' || next == 'O') { base = 8; lexer_advance_char(l); }
            else if (next == 'b' || next == 'B') { base = 2; lexer_advance_char(l); }
        }

        bool is_float = false;
        while (l->pos < l->source_len) {
            char ch = l->source[l->pos];
            if (base == 16 && is_hex_digit(ch)) { lexer_advance_char(l); }
            else if (base != 16 && is_digit(ch)) { lexer_advance_char(l); }
            else if (ch == '.' && !is_float) {
              if (base != 10) break;
                    // Don't consume '.' if it's part of '..' (range operator)
                    if (l->pos + 1 < l->source_len && l->source[l->pos + 1] == '.') break;
                    is_float = true;
                    lexer_advance_char(l);
            } else if ((ch == 'e' || ch == 'E') && base == 10) {
                is_float = true;
                lexer_advance_char(l);
                if (l->pos < l->source_len && (l->source[l->pos] == '+' || l->source[l->pos] == '-')) {
                    lexer_advance_char(l);
                }
            } else {
                break;
            }
        }

        size_t len = l->pos - start;
        Token t = make_token(l, is_float ? TOK_FLOAT_LITERAL : TOK_INT_LITERAL, start, len);
        t.loc.line = start_line;
        t.loc.column = start_col;

        char* buf = (char*)malloc(len + 1);
        memcpy(buf, l->source + start, len);
        buf[len] = '\0';
        if (is_float) {
            t.float_value = strtod(buf, NULL);
        } else {
            t.int_value = strtoll(buf, NULL, base);
        }
        free(buf);
        return t;
    }

    if (c == '"') {
        while (l->pos < l->source_len && l->source[l->pos] != '"') {
            if (l->source[l->pos] == '\\') {
                lexer_advance_char(l);
                if (l->pos < l->source_len) lexer_advance_char(l);
            } else if (l->source[l->pos] == '\n') {
                diag_error(l->diag, loc_make(l->filename, l->line, l->column, l->pos),
                           "unterminated string literal");
                break;
            } else {
                lexer_advance_char(l);
            }
        }
        if (l->pos < l->source_len && l->source[l->pos] == '"') {
            lexer_advance_char(l);
        }
        Token t = make_token(l, TOK_STRING_LITERAL, start, l->pos - start);
        t.loc.line = start_line;
        t.loc.column = start_col;
        return t;
    }

    if (c == '\'') {
        if (l->pos < l->source_len && l->source[l->pos] == '\\') {
            lexer_advance_char(l);
            if (l->pos < l->source_len) lexer_advance_char(l);
        } else if (l->pos < l->source_len && l->source[l->pos] != '\'') {
            lexer_advance_char(l);
        }
        if (l->pos < l->source_len && l->source[l->pos] == '\'') {
            lexer_advance_char(l);
        } else {
            diag_error(l->diag, loc_make(l->filename, l->line, l->column, l->pos),
                       "unterminated character literal");
        }
        Token t = make_token(l, TOK_CHAR_LITERAL, start, l->pos - start);
        t.loc.line = start_line;
        t.loc.column = start_col;
        return t;
    }

    if (c == '=') {
        if (lexer_peek_char(l, 0) == '=') { lexer_advance_char(l); return make_token(l, TOK_EQ, start, 2); }
        if (lexer_peek_char(l, 0) == '>') { lexer_advance_char(l); return make_token(l, TOK_FAT_ARROW, start, 2); }
        return make_token(l, TOK_ASSIGN, start, 1);
    }
    if (c == '!') {
        if (lexer_peek_char(l, 0) == '=') { lexer_advance_char(l); return make_token(l, TOK_NE, start, 2); }
        return make_token(l, TOK_BANG, start, 1);
    }
    if (c == '<') {
        if (lexer_peek_char(l, 0) == '=') { lexer_advance_char(l); return make_token(l, TOK_LE, start, 2); }
        if (lexer_peek_char(l, 0) == '<') {
            lexer_advance_char(l);
            if (lexer_peek_char(l, 0) == '=') { lexer_advance_char(l); return make_token(l, TOK_SHL_ASSIGN, start, 3); }
            return make_token(l, TOK_SHL, start, 2);
        }
        return make_token(l, TOK_LT, start, 1);
    }
    if (c == '>') {
        if (lexer_peek_char(l, 0) == '=') { lexer_advance_char(l); return make_token(l, TOK_GE, start, 2); }
        if (lexer_peek_char(l, 0) == '>') {
            lexer_advance_char(l);
            if (lexer_peek_char(l, 0) == '=') { lexer_advance_char(l); return make_token(l, TOK_SHR_ASSIGN, start, 3); }
            return make_token(l, TOK_SHR, start, 2);
        }
        return make_token(l, TOK_GT, start, 1);
    }
    if (c == '&') {
        if (lexer_peek_char(l, 0) == '&') { lexer_advance_char(l); return make_token(l, TOK_AND_AND, start, 2); }
        if (lexer_peek_char(l, 0) == '=') { lexer_advance_char(l); return make_token(l, TOK_AND_ASSIGN, start, 2); }
        return make_token(l, TOK_AMPERSAND, start, 1);
    }
    if (c == '|') {
        if (lexer_peek_char(l, 0) == '|') { lexer_advance_char(l); return make_token(l, TOK_OR_OR, start, 2); }
        if (lexer_peek_char(l, 0) == '=') { lexer_advance_char(l); return make_token(l, TOK_OR_ASSIGN, start, 2); }
        return make_token(l, TOK_PIPE, start, 1);
    }
    if (c == '+') {
        if (lexer_peek_char(l, 0) == '=') { lexer_advance_char(l); return make_token(l, TOK_PLUS_ASSIGN, start, 2); }
        return make_token(l, TOK_PLUS, start, 1);
    }
    if (c == '-') {
        if (lexer_peek_char(l, 0) == '=') { lexer_advance_char(l); return make_token(l, TOK_MINUS_ASSIGN, start, 2); }
        if (lexer_peek_char(l, 0) == '>') { lexer_advance_char(l); return make_token(l, TOK_ARROW, start, 2); }
        return make_token(l, TOK_MINUS, start, 1);
    }
    if (c == '*') {
        if (lexer_peek_char(l, 0) == '=') { lexer_advance_char(l); return make_token(l, TOK_STAR_ASSIGN, start, 2); }
        return make_token(l, TOK_STAR, start, 1);
    }
    if (c == '/') {
        if (lexer_peek_char(l, 0) == '=') { lexer_advance_char(l); return make_token(l, TOK_SLASH_ASSIGN, start, 2); }
        return make_token(l, TOK_SLASH, start, 1);
    }
    if (c == '%') {
        if (lexer_peek_char(l, 0) == '=') { lexer_advance_char(l); return make_token(l, TOK_PERCENT_ASSIGN, start, 2); }
        return make_token(l, TOK_PERCENT, start, 1);
    }
    if (c == '^') {
        if (lexer_peek_char(l, 0) == '=') { lexer_advance_char(l); return make_token(l, TOK_XOR_ASSIGN, start, 2); }
        return make_token(l, TOK_CARET, start, 1);
    }
    if (c == '.') {
        if (lexer_peek_char(l, 0) == '.') { lexer_advance_char(l); return make_token(l, TOK_DOT_DOT, start, 2); }
        return make_token(l, TOK_DOT, start, 1);
    }
    if (c == ':') {
        if (lexer_peek_char(l, 0) == ':') { lexer_advance_char(l); return make_token(l, TOK_COLON_COLON, start, 2); }
        return make_token(l, TOK_COLON, start, 1);
    }

    switch (c) {
        case '(': return make_token(l, TOK_LPAREN, start, 1);
        case ')': return make_token(l, TOK_RPAREN, start, 1);
        case '[': return make_token(l, TOK_LBRACKET, start, 1);
        case ']': return make_token(l, TOK_RBRACKET, start, 1);
        case '{': return make_token(l, TOK_LBRACE, start, 1);
        case '}': return make_token(l, TOK_RBRACE, start, 1);
        case ',': return make_token(l, TOK_COMMA, start, 1);
        case ';': return make_token(l, TOK_SEMICOLON, start, 1);
        case '?': return make_token(l, TOK_QUESTION, start, 1);
        case '~': return make_token(l, TOK_TILDE, start, 1);
        case '#': return make_token(l, TOK_HASH, start, 1);
    }

    diag_error(l->diag, loc_make(l->filename, l->line, start_col, start),
               "unexpected character '%c' (0x%02X)", c, (unsigned char)c);
    return make_token(l, TOK_ERROR, start, 1);
}

Token lexer_peek(Lexer* l) {
    if (!l->has_current) {
        l->current = lexer_next(l);
        l->has_current = true;
    }
    return l->current;
}

void lexer_advance(Lexer* l) {
    lexer_peek(l);
    l->has_current = false;
}
