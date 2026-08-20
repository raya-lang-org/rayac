
#ifndef RAYA_PARSER_H
#define RAYA_PARSER_H

#include "common.h"
#include "lexer.h"
#include "ast.h"
#include "arena.h"

typedef struct Parser {
    const Token *tokens;
    size_t count;
    size_t pos;
    Arena *arena;
    bool had_error;
} Parser;

void parser_init(Parser *p, const Token *tokens, size_t count, Arena *arena);

const Token *parser_current(const Parser *p);
const Token *parser_peek(const Parser *p, size_t offset);
bool parser_at_end(const Parser *p);
bool parser_check(const Parser *p, TokenKind kind);
bool parser_match(Parser *p, TokenKind kind);
bool parser_expect(Parser *p, TokenKind kind, const char *message);

AstNode *parser_parse(Parser *p);
AstNode *parser_parse_expr(Parser *p);
AstNode *parser_parse_stmt(Parser *p);
AstNode *parser_parse_block(Parser *p);
TypeExpr *parser_parse_type(Parser *p);

void parser_error(Parser *p, const Token *token, const char *message);

#endif /* RAYA_PARSER_H */
