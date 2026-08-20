#ifndef RAYA_LEXER_H
#define RAYA_LEXER_H

#include "common.h"
#include "string_view.h"
#include "source_loc.h"
#include "diag.h"

typedef enum {
    TOK_MODULE, TOK_IMPORT, TOK_FN, TOK_PUB, TOK_CONST, TOK_VAR,
    TOK_COMPTIME, TOK_DEFER, TOK_ERRDEFER, TOK_TEST, TOK_RETURN,
    TOK_IF, TOK_ELSE, TOK_WHILE, TOK_FOR, TOK_TRY, TOK_BREAK,
    TOK_CONTINUE, TOK_UNION, TOK_STRUCT, TOK_UNSAFE, TOK_NORETURN, TOK_IN, 
    TOK_TYPE, TOK_TRAITS, TOK_EXTEND, TOK_MATCH, TOK_ENUM,TOK_AS,TOK_WITH, TOK_UNDEFINED, TOK_SELF, TOK_CAP_SELF,
    TOK_INT_LITERAL, TOK_FLOAT_LITERAL, TOK_STRING_LITERAL, TOK_CHAR_LITERAL,
    TOK_TRUE, TOK_FALSE, TOK_NULL,
    TOK_IDENTIFIER,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_EQ, TOK_NE, TOK_LT, TOK_GT, TOK_LE, TOK_GE, TOK_UNDERSCORE,
    TOK_ASSIGN, TOK_PLUS_ASSIGN, TOK_MINUS_ASSIGN, TOK_STAR_ASSIGN,
    TOK_SLASH_ASSIGN, TOK_PERCENT_ASSIGN, TOK_AND_ASSIGN, TOK_OR_ASSIGN,
    TOK_XOR_ASSIGN, TOK_SHL_ASSIGN, TOK_SHR_ASSIGN,
    TOK_AMPERSAND, TOK_PIPE, TOK_CARET, TOK_SHL, TOK_SHR,
    TOK_AND_AND, TOK_OR_OR, TOK_BANG, TOK_TILDE,
    TOK_DOT, TOK_DOT_DOT, TOK_ARROW, TOK_FAT_ARROW, TOK_COLON_COLON, TOK_HASH,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACKET, TOK_RBRACKET,
    TOK_LBRACE, TOK_RBRACE, TOK_COMMA, TOK_SEMICOLON, TOK_COLON, TOK_QUESTION,
    TOK_EOF, TOK_ERROR,
    TOK_COUNT
} TokenKind;

static inline const char* token_kind_name(TokenKind k) {
    static const char* names[] = {
        "module", "import", "fn", "pub", "const", "var",
        "comptime", "defer", "errdefer", "test", "return",
        "if", "else", "while", "for", "try", "break",
        "continue", "union", "struct", "unsafe", "noreturn", "in"
        "type", "traits", "extend", "match", "enum","as", "with", "undefined","self", "Self",
        "int_literal", "float_literal", "string_literal", "char_literal",
        "true", "false", "null",
        "identifier",
        "+", "-", "*", "/", "%",
        "==", "!=", "<", ">", "<=", ">=", "_",
        "=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=",
        "&", "|", "^", "<<", ">>",
        "&&", "||", "!", "~",
        ".", "..", "->", "=>", "::", "#",
        "(", ")", "[", "]", "{", "}", ",", ";", ":", "?",
        "EOF", "ERROR"
    };
    return (k >= 0 && k < TOK_COUNT) ? names[k] : "UNKNOWN";
}

typedef struct {
    TokenKind kind;
    StringView text;
    SourceLocation loc;
    union {
        int64_t int_value;
        double float_value;
    };
} Token;

typedef struct {
    const char* source;
    size_t source_len;
    size_t pos;
    size_t line;
    size_t column;
    const char* filename;
    DiagnosticEngine* diag;
    Token current;
    bool has_current;
} Lexer;

void lexer_init(Lexer* l, const char* source, size_t len, const char* filename,
                DiagnosticEngine* diag);
Token lexer_next(Lexer* l);
Token lexer_peek(Lexer* l);
void lexer_advance(Lexer* l);

#endif
