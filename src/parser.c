#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const Token EOF_TOKEN = { .kind = TOK_EOF, .text = { .data = "", .len = 0 } };

void parser_init(Parser* p, const Token* tokens, size_t count, Arena* arena) {
    p->tokens = tokens;
    p->count = count;
    p->pos = 0;
    p->arena = arena;
    p->had_error = false;
    ast_set_arena(arena);
}

const Token* parser_peek(const Parser* p, size_t offset) {
    if (p->pos + offset >= p->count) return &EOF_TOKEN;
    return &p->tokens[p->pos + offset];
}

static const Token* parser_previous(Parser* p) {
    if (p->pos == 0) return &EOF_TOKEN;
    return &p->tokens[p->pos - 1];
}

bool parser_at_end(const Parser* p) {
    return parser_peek(p, 0)->kind == TOK_EOF;
}

static const Token* parser_advance(Parser* p) {
    if (!parser_at_end(p)) p->pos++;
    return parser_previous(p);
}

bool parser_check(const Parser* p, TokenKind kind) {
    if (parser_at_end(p)) return false;
    return parser_peek(p, 0)->kind == kind;
}

bool parser_match(Parser* p, TokenKind kind) {
    if (parser_check(p, kind)) {
        parser_advance(p);
        return true;
    }
    return false;
}

Token* parser_expect(Parser* p, TokenKind kind, const char* message) {
    if (parser_check(p, kind)) return (Token*)parser_advance(p);
    
    if (!p->had_error) {
        p->had_error = true;
        const Token* tok = parser_peek(p, 0);
        fprintf(stderr, "%s:%lu:%lu: error: %s (got '%.*s')\n",
                tok->loc.filename,
                (unsigned long)tok->loc.line,
                (unsigned long)tok->loc.column,
                message,
                (int)tok->text.len, tok->text.data);
    }
    return (Token*)parser_peek(p, 0);
}

TypeExpr* parser_parse_type(Parser* p) {
    SourceLocation loc = parser_peek(p, 0)->loc;

    if (parser_match(p, TOK_QUESTION)) {
        TypeExpr* inner = parser_parse_type(p);
        return type_new_optional(inner, loc);
    }
    if (parser_match(p, TOK_BANG)) {
        TypeExpr* inner = parser_parse_type(p);
        return type_new_error_union(inner, loc);
    }
    if (parser_match(p, TOK_AMPERSAND)) {
        bool is_const = parser_match(p, TOK_CONST);
        TypeExpr* inner = parser_parse_type(p);
        return type_new_reference(inner, is_const, loc);
    }
    if (parser_match(p, TOK_STAR)) {
        bool is_const = parser_match(p, TOK_CONST);
        TypeExpr* inner = parser_parse_type(p);
        return type_new_pointer(inner, is_const, loc);
    }
    if (parser_match(p, TOK_LBRACKET)) {
        if (parser_match(p, TOK_RBRACKET)) {
            TypeExpr* elem = parser_parse_type(p);
            return type_new_slice(elem, false, loc);
        }
        AstNode* size_node = parser_parse_expr_bp(p, 0);
        parser_expect(p, TOK_RBRACKET, "Expected ']' after array size");
        TypeExpr* elem = parser_parse_type(p);
        return type_new_array(size_node, elem, loc);
    }
    if (parser_check(p, TOK_IDENTIFIER)) {
        const Token* id = parser_advance(p);
        return type_new_named(id->text, loc);
    }

    return NULL;
}

static Pattern* parser_parse_pattern(Parser* p) {
    SourceLocation loc = parser_peek(p, 0)->loc;

    if (parser_match(p, TOK_UNDERSCORE)) {
        return pat_new_wildcard(loc);
    }
    if (parser_check(p, TOK_IDENTIFIER)) {
        const Token* id = parser_advance(p);
        if (parser_match(p, TOK_LPAREN)) {
            Pattern** fields = NULL;
            size_t count = 0;
            size_t cap = 0;
            if (!parser_check(p, TOK_RPAREN)) {
                do {
                    if (count >= cap) {
                        cap = cap < 4 ? 4 : cap * 2;
                        fields = arena_alloc(p->arena, cap * sizeof(Pattern*));
                    }
                    fields[count++] = parser_parse_pattern(p);
                } while (parser_match(p, TOK_COMMA));
            }
            parser_expect(p, TOK_RPAREN, "Expected ')' after pattern fields");
            return pat_new_enum_variant(id->text, fields, count, loc);
        }
        return pat_new_identifier(id->text, loc);
    }

    return NULL;
}

static int get_binding_power(TokenKind kind) {
    switch (kind) {
        case TOK_EQ:
            return 1;
        case TOK_OR_OR:
            return 2;
        case TOK_AND_AND:
            return 3;
        case TOK_NE:
            return 4;
        case TOK_LT: case TOK_GT:
        case TOK_LE: case TOK_GE:
            return 5;
        case TOK_PLUS: case TOK_MINUS:
            return 6;
        case TOK_STAR: case TOK_SLASH: case TOK_PERCENT:
            return 7;
        default:
            return 0;
    }
}

AstNode* parser_parse_expr_bp(Parser* p, int min_bp);

AstNode* parser_parse_primary(Parser* p) {
    SourceLocation loc = parser_peek(p, 0)->loc;
    const Token* tok = parser_peek(p, 0);

    switch (tok->kind) {
        case TOK_INT_LITERAL: {
            parser_advance(p);
            int64_t val = strtoll(tok->text.data, NULL, 10);
            return ast_new_int_literal(val, loc);
        }
        case TOK_FLOAT_LITERAL: {
            parser_advance(p);
            double val = strtod(tok->text.data, NULL);
            return ast_new_float_literal(val, loc);
        }
        case TOK_STRING_LITERAL:
            parser_advance(p);
            return ast_new_string_literal(tok->text, loc);
        case TOK_CHAR_LITERAL:
            parser_advance(p);
            return ast_new_char_literal((uint8_t)tok->text.data[1], loc);
        case TOK_TRUE:
            parser_advance(p);
            return ast_new_bool_literal(true, loc);
        case TOK_FALSE:
            parser_advance(p);
            return ast_new_bool_literal(false, loc);
        case TOK_NULL:
            parser_advance(p);
            return ast_new_null_literal(loc);
        case TOK_UNDEFINED:
            parser_advance(p);
            return ast_new_undefined_literal(loc);
        case TOK_IDENTIFIER:
            parser_advance(p);
            return ast_new_identifier(tok->text, loc);
        case TOK_LPAREN: {
            parser_advance(p);
            AstNode* expr = parser_parse_expr_bp(p, 0);
            parser_expect(p, TOK_RPAREN, "Expected ')' after parenthesized expression");
            return expr;
        }
        default:
            parser_expect(p, TOK_IDENTIFIER, "Expected expression");
            return NULL;
    }
}

AstNode* parser_parse_prefix(Parser* p) {
    SourceLocation loc = parser_peek(p, 0)->loc;

    if (parser_match(p, TOK_MINUS) || parser_match(p, TOK_BANG)) {
        TokenKind op = parser_previous(p)->kind;
        AstNode* operand = parser_parse_prefix(p);
        return ast_new_unary(op, operand, loc);
    }
    if (parser_match(p, TOK_AMPERSAND)) {
        bool is_const = parser_match(p, TOK_CONST);
        (void)is_const;
        AstNode* operand = parser_parse_prefix(p);
        return ast_new_addr_of(operand, loc);
    }

    return parser_parse_primary(p);
}

AstNode* parser_parse_postfix(Parser* p, AstNode* left) {
    while (true) {
        SourceLocation loc = parser_peek(p, 0)->loc;

        if (parser_match(p, TOK_DOT)) {
            const Token* field = parser_expect(p, TOK_IDENTIFIER, "Expected field or method name");
            if (parser_match(p, TOK_LPAREN)) {
                AstNode** args = NULL;
                size_t count = 0;
                size_t cap = 0;
                if (!parser_check(p, TOK_RPAREN)) {
                    do {
                        if (count >= cap) {
                            cap = cap < 4 ? 4 : cap * 2;
                            args = arena_alloc(p->arena, cap * sizeof(AstNode*));
                        }
                        args[count++] = parser_parse_expr_bp(p, 0);
                    } while (parser_match(p, TOK_COMMA));
                }
                parser_expect(p, TOK_RPAREN, "Expected ')' after arguments");
                left = ast_new_method_call(left, field->text, args, count, loc);
            } else {
                left = ast_new_field_access(left, field->text, loc);
            }
        } else if (parser_match(p, TOK_LBRACKET)) {
            AstNode* index = parser_parse_expr_bp(p, 0);
            parser_expect(p, TOK_RBRACKET, "Expected ']' after index");
            left = ast_new_index(left, index, loc);
        } else {
            break;
        }
    }

    return left;
}

AstNode* parser_parse_expr_bp(Parser* p, int min_bp) {
    AstNode* left = parser_parse_prefix(p);
    left = parser_parse_postfix(p, left);

    while (true) {
        TokenKind op = parser_peek(p, 0)->kind;
        int bp = get_binding_power(op);
        if (bp == 0 || bp < min_bp) break;

        SourceLocation loc = parser_advance(p)->loc;

        if (op == TOK_EQ) {
            AstNode* right = parser_parse_expr_bp(p, bp);
            left = ast_new_assign(op, left, right, loc);
        } else {
            AstNode* right = parser_parse_expr_bp(p, bp + 1);
            left = ast_new_binary(op, left, right, loc);
        }
    }

    return left;
}

static void* arena_grow_array(Arena* arena, void* old_ptr, size_t old_count, size_t* cap, size_t elem_size) {
    size_t new_cap = (*cap == 0) ? 16 : (*cap * 2);
    void* new_ptr = arena_alloc(arena, new_cap * elem_size);
    if (old_ptr && old_count > 0) {
        memcpy(new_ptr, old_ptr, old_count * elem_size);
    }
    *cap = new_cap;
    return new_ptr;
}


AstNode* parser_parse_block(Parser* p) {
    SourceLocation loc = parser_peek(p, 0)->loc;
    parser_expect(p, TOK_LBRACE, "Expected '{' to start block");

    size_t count = 0;
    size_t cap = 16; // Pre-allocate larger default size
    AstNode** stmts = arena_alloc(p->arena, cap * sizeof(AstNode*));

    while (p->pos < p->count) {
        TokenKind k = p->tokens[p->pos].kind;
        if (k == TOK_RBRACE || k == TOK_EOF) break;

        AstNode* stmt = parser_parse_stmt(p);
        if (stmt) {
            if (count >= cap) {
                stmts = arena_grow_array(p->arena, stmts, count, &cap, sizeof(AstNode*));
            }
            stmts[count++] = stmt;
        }
    }

    parser_expect(p, TOK_RBRACE, "Expected '}' to end block");

    AstNode* block = ast_new_block(loc);
    block->as.block.stmts = stmts;
    block->as.block.stmt_count = count;
    block->as.block.trailing_expr = NULL;
    return block;

}

static AstNode* parser_parse_match_stmt(Parser* p) {
    SourceLocation loc = parser_peek(p, 0)->loc;
    parser_advance(p); // consume 'match'

    AstNode* expr = parser_parse_expr_bp(p, 0);
    parser_expect(p, TOK_LBRACE, "Expected '{' after match expr");

    AstNode* match_node = ast_new_match(expr, loc);

    typedef struct { Pattern* pat; AstNode* expr; SourceLocation arrow_loc; } MatchArm;
    size_t count = 0;
    size_t cap = 4;
    MatchArm* arms = arena_alloc(p->arena, cap * sizeof(MatchArm));

    while (!parser_check(p, TOK_RBRACE) && !parser_at_end(p)) {
        Pattern* pat = parser_parse_pattern(p);
        SourceLocation arrow_loc = parser_peek(p, 0)->loc;
        parser_expect(p, TOK_FAT_ARROW, "Expected '=>' after pattern");
        AstNode* body = parser_parse_stmt(p);

        if (count >= cap) {
            cap *= 2;
            MatchArm* new_arms = arena_alloc(p->arena, cap * sizeof(MatchArm));
            memcpy(new_arms, arms, count * sizeof(MatchArm));
            arms = new_arms;
        }

        arms[count].pat = pat;
        arms[count].expr = body;
        arms[count].arrow_loc = arrow_loc;
        count++;

        if (!parser_check(p, TOK_RBRACE)) {
            parser_match(p, TOK_COMMA);
        }
    }

    parser_expect(p, TOK_RBRACE, "Expected '}' after match cases");

    match_node->as.match_stmt.arms = (void*)arms;
    match_node->as.match_stmt.arm_count = count;
    return match_node;
}

AstNode* parser_parse_stmt(Parser* p) {
    SourceLocation loc = parser_peek(p, 0)->loc;

    if (parser_match(p, TOK_IF)) {
        AstNode* cond = parser_parse_expr_bp(p, 0);
        AstNode* then_block = parser_parse_block(p);
        AstNode* else_stmt = NULL;
        if (parser_match(p, TOK_ELSE)) {
            if (parser_check(p, TOK_IF)) {
                else_stmt = parser_parse_stmt(p);
            } else {
                else_stmt = parser_parse_block(p);
            }
        }
        return ast_new_if(cond, then_block, else_stmt, loc);
    }

    if (parser_match(p, TOK_VAR)) {
        const Token* id = parser_expect(p, TOK_IDENTIFIER, "Expected identifier after 'var'");
        TypeExpr* type_ = NULL;
        if (parser_match(p, TOK_COLON)) {
            type_ = parser_parse_type(p);
        }
        AstNode* init = NULL;
        if (parser_match(p, TOK_EQ)) {
            init = parser_parse_expr_bp(p, 0);
        }
        parser_expect(p, TOK_SEMICOLON, "Expected ';' after var decl");
        return ast_new_var_decl(id->text, type_, init, loc);
    }

    if (parser_match(p, TOK_CONST)) {
        const Token* id = parser_expect(p, TOK_IDENTIFIER, "Expected identifier after 'const'");
        TypeExpr* type_ = NULL;
        if (parser_match(p, TOK_COLON)) {
            type_ = parser_parse_type(p);
        }
        AstNode* init = NULL;
        if (parser_match(p, TOK_EQ)) {
            init = parser_parse_expr_bp(p, 0);
        }
        parser_expect(p, TOK_SEMICOLON, "Expected ';' after const decl");
        return ast_new_const_decl(id->text, type_, init, loc);
    }

    if (parser_match(p, TOK_RETURN)) {
        AstNode* expr = NULL;
        if (!parser_check(p, TOK_SEMICOLON)) {
            expr = parser_parse_expr_bp(p, 0);
        }
        parser_expect(p, TOK_SEMICOLON, "Expected ';' after return statement");
        return ast_new_return(expr, loc);
    }

    if (parser_check(p, TOK_MATCH)) {
        return parser_parse_match_stmt(p);
    }

    if (parser_match(p, TOK_IF)) {
        AstNode* cond = parser_parse_expr_bp(p, 0);
        AstNode* then_block = parser_parse_block(p);
        AstNode* else_stmt = NULL;
        if (parser_match(p, TOK_ELSE)) {
            if (parser_check(p, TOK_IF)) {
                else_stmt = parser_parse_stmt(p);
            } else {
                else_stmt = parser_parse_block(p);
            }
        }
        return ast_new_if(cond, then_block, else_stmt, loc);
    }

    if (parser_match(p, TOK_WHILE)) {
        AstNode* cond = parser_parse_expr_bp(p, 0);
        AstNode* body = parser_parse_block(p);
        return ast_new_while(cond, body, loc);
    }

    AstNode* expr = parser_parse_expr_bp(p, 0);
    parser_expect(p, TOK_SEMICOLON, "Expected ';' after expression statement");
    return ast_new_expr_stmt(expr, loc);
}

AstNode* parser_parse(Parser* p) {
   SourceLocation loc = parser_peek(p, 0)->loc;

    size_t count = 0;
    size_t cap = 64; // High capacity for top-level module decls
    AstNode** decls = arena_alloc(p->arena, cap * sizeof(AstNode*));

    while (p->pos < p->count && p->tokens[p->pos].kind != TOK_EOF) {
        AstNode* stmt = parser_parse_stmt(p);
        if (stmt) {
            if (count >= cap) {
                decls = arena_grow_array(p->arena, decls, count, &cap, sizeof(AstNode*));
            }
            decls[count++] = stmt;
        } else {
            parser_advance(p);
        }
    }

    AstNode* module = ast_new_module_decl((StringView){.data = "main", .len = 4}, loc);
    module->as.module_decl.decl = decls;
    module->as.module_decl.decl_count = count;

    return module;
} 
