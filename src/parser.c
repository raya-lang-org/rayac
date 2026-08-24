
#include "parser.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Token navigation
 * ========================================================================== */

void parser_init(Parser *p, const Token *tokens, size_t count, Arena *arena)
{
    p->tokens = tokens;
    p->count = count;
    p->pos = 0;
    p->arena = arena;
    p->had_error = false;
}

const Token *parser_current(const Parser *p)
{
    if (p->pos >= p->count)
        return &p->tokens[p->count - 1];
    return &p->tokens[p->pos];
}

const Token *parser_peek(const Parser *p, size_t offset)
{
    size_t index = p->pos + offset;
    if (index >= p->count)
        return &p->tokens[p->count - 1];
    return &p->tokens[index];
}

bool parser_at_end(const Parser *p)
{
    return parser_current(p)->kind == TOK_EOF;
}

bool parser_check(const Parser *p, TokenKind kind)
{
    return parser_current(p)->kind == kind;
}

bool parser_match(Parser *p, TokenKind kind)
{
    if (!parser_check(p, kind))
        return false;
    p->pos++;
    return true;
}

bool parser_expect(Parser *p, TokenKind kind, const char *message)
{
    if (parser_match(p, kind))
        return true;
    parser_error(p, parser_current(p), message);
    return false;
}

/* ============================================================================
 * Diagnostics
 * ========================================================================== */

void parser_error(Parser *p, const Token *token, const char *message)
{
    p->had_error = true;
    fprintf(stderr, "parser error at %s:%lu:%lu: %s\n",
            token->loc.filename,
            (unsigned long)token->loc.line,
            (unsigned long)token->loc.column,
            message);
}

/* ============================================================================
 * Forward declarations
 * ========================================================================== */

static AstNode *parser_parse_expr_bp(Parser *p, int min_bp);
static AstNode *parser_parse_primary(Parser *p);
static AstNode *parser_parse_prefix(Parser *p);
static AstNode *parser_parse_postfix(Parser *p, AstNode *left);
static AstNode *parser_parse_top_level_decl(Parser *p);
static AstNode *parser_parse_fn_decl(Parser *p, bool is_pub, AttributeList attrs);
static AstNode *parser_parse_struct_decl(Parser *p, bool is_pub, AttributeList attrs);
static AstNode *parser_parse_union_decl(Parser *p, bool is_pub, AttributeList attrs);
static AstNode *parser_parse_enum_decl(Parser *p, bool is_pub, AttributeList attrs);
static AstNode *parser_parse_traits_decl(Parser *p, bool is_pub, AttributeList attrs);
static AstNode *parser_parse_extend_decl(Parser *p, bool is_pub, AttributeList attrs);
static AstNode *parser_parse_type_alias(Parser *p, bool is_pub, AttributeList attrs);
static AstNode *parser_parse_test_decl(Parser *p, AttributeList attrs);
static AstNode *parser_parse_param(Parser *p);
static Pattern *parser_parse_pattern(Parser *p);
static AttributeList parser_parse_attrs(Parser *p);
static void parser_parse_generic_params(Parser *p, AstNodeList *out_list);

/* ============================================================================
 * Attribute parsing
 * ========================================================================== */

static Attribute parser_parse_attribute(Parser *p)
{
    SourceLocation loc = parser_current(p)->loc;
    parser_expect(p, TOK_HASH, "expected '#'");
    parser_expect(p, TOK_LBRACKET, "expected '[' after '#'");

    const Token *name = parser_current(p);
    parser_expect(p, TOK_IDENTIFIER, "expected attribute name");

    Attribute attr = {0};
    attr.name = name->text;
    attr.loc = loc;
    ast_node_list_init(p->arena, &attr.args);

    if (parser_match(p, TOK_LPAREN)) {
        do {
            AstNode *arg = parser_parse_expr(p);
            if (arg) ast_node_list_push(p->arena, &attr.args, arg);
        } while (parser_match(p, TOK_COMMA));
        parser_expect(p, TOK_RPAREN, "expected ')' after attribute args");
    }

    parser_expect(p, TOK_RBRACKET, "expected ']' after attribute");
    return attr;
}

static AttributeList parser_parse_attrs(Parser *p)
{
    AttributeList attrs = {0};
    attribute_list_init(p->arena, &attrs);
    while (parser_check(p, TOK_HASH)) {
        Attribute attr = parser_parse_attribute(p);
        attribute_list_push(p->arena, &attrs, attr);
    }
    return attrs;
}

/* ============================================================================
 * Binding powers (Pratt parser)
 * ========================================================================== */

typedef struct {
    int left;
    int right;
} BindingPower;

static bool infix_binding_power(TokenKind kind, BindingPower *bp)
{
    switch (kind) {
        case TOK_OR_OR:
            bp->left = 10; bp->right = 11; return true;
        case TOK_AND_AND:
            bp->left = 20; bp->right = 21; return true;
        case TOK_EQ: case TOK_NE:
            bp->left = 30; bp->right = 31; return true;
        case TOK_LT: case TOK_GT: case TOK_LE: case TOK_GE:
            bp->left = 40; bp->right = 41; return true;
        case TOK_PIPE:
            bp->left = 50; bp->right = 51; return true;
        case TOK_CARET:
            bp->left = 60; bp->right = 61; return true;
        case TOK_AMPERSAND:
            bp->left = 70; bp->right = 71; return true;
        case TOK_SHL: case TOK_SHR:
            bp->left = 80; bp->right = 81; return true;
        case TOK_PLUS: case TOK_MINUS:
            bp->left = 90; bp->right = 91; return true;
        case TOK_STAR: case TOK_SLASH: case TOK_PERCENT:
            bp->left = 100; bp->right = 101; return true;
        case TOK_DOT_DOT:
            bp->left = 45; bp->right = 46; return true;
        default:
            return false;
    }
}

/* ============================================================================
 * Primary expressions
 * ========================================================================== */

static AstNode *parser_parse_struct_literal(Parser *p, TypeExpr *type)
{
    SourceLocation loc = parser_current(p)->loc;
    parser_expect(p, TOK_LBRACE, "expected '{' in struct literal");
    AstNode *lit = ast_new_struct_literal(p->arena, type, loc);
    while (!parser_check(p, TOK_RBRACE) && !parser_at_end(p)) {
        const Token *field = parser_current(p);
        parser_expect(p, TOK_IDENTIFIER, "expected field name");
        parser_expect(p, TOK_COLON, "expected ':' after field name");
        AstNode *value = parser_parse_expr(p);
        AstNode *init = ast_new_assign(p->arena, TOK_ASSIGN,
            ast_new_identifier(p->arena, field->text, field->loc),
            value, field->loc);
        ast_struct_add_field_init(p->arena, lit, init);
        if (!parser_match(p, TOK_COMMA)) break;
    }
    parser_expect(p, TOK_RBRACE, "expected '}' in struct literal");
    return lit;
}

static AstNode *parser_parse_primary(Parser *p)
{
    const Token *tok = parser_current(p);

    switch (tok->kind) {
        case TOK_INT_LITERAL: {
            p->pos++;
            return ast_new_int_literal(p->arena, tok->int_value, tok->loc);
        }
        case TOK_FLOAT_LITERAL: {
            p->pos++;
            return ast_new_float_literal(p->arena, tok->float_value, tok->loc);
        }
        case TOK_STRING_LITERAL: {
            p->pos++;
            return ast_new_string_literal(p->arena, tok->text, tok->loc);
        }
        case TOK_CHAR_LITERAL: {
            p->pos++;
            return ast_new_char_literal(p->arena, tok->text, tok->loc);
        }
        case TOK_TRUE: {
            p->pos++;
            return ast_new_bool_literal(p->arena, true, tok->loc);
        }
        case TOK_FALSE: {
            p->pos++;
            return ast_new_bool_literal(p->arena, false, tok->loc);
        }
        case TOK_NULL: {
            p->pos++;
            return ast_new_null_literal(p->arena, tok->loc);
        }
        case TOK_UNDEFINED: {
            p->pos++;
            return ast_new_undefined_literal(p->arena, tok->loc);
        }
        case TOK_SELF: {
            p->pos++;
            return ast_new_identifier(p->arena, sv_from_cstr("self"), tok->loc);
        }
        case TOK_IDENTIFIER: {
            p->pos++;
            AstNode *node = ast_new_identifier(p->arena, tok->text, tok->loc);
            if (parser_check(p, TOK_LBRACE) && tok->text.len > 0 && tok->text.data[0] >= 'A' && tok->text.data[0] <= 'Z') {
                TypeExpr *type = type_new_named(p->arena, tok->text, tok->loc);
                return parser_parse_struct_literal(p, type);
            }
            return node;
        }
        case TOK_LPAREN: {
            p->pos++;
            AstNode *expr = parser_parse_expr(p);
            parser_expect(p, TOK_RPAREN, "expected ')' after expression");
            return expr;
        }
        case TOK_LBRACKET: {
            p->pos++;
            AstNode *lit = ast_new_array_literal(p->arena, tok->loc);
            if (parser_match(p, TOK_RBRACKET)) {
                lit->array_literal.explicit_type = parser_parse_type(p);
            } else {
                lit->array_literal.length = parser_parse_expr(p);
                parser_expect(p, TOK_RBRACKET, "expected ']' in array literal");
                lit->array_literal.explicit_type = parser_parse_type(p);
            }
            parser_expect(p, TOK_LBRACE, "expected '{' in array literal");
            while (!parser_check(p, TOK_RBRACE) && !parser_at_end(p)) {
                AstNode *elem = parser_parse_expr(p);
                if (elem) ast_array_add_elem(p->arena, lit, elem);
                if (parser_match(p, TOK_COMMA)) {
                    if (parser_check(p, TOK_RBRACE)) {
                        lit->array_literal.sentinel = true;
                        break;
                    }
                } else {
                    break;
                }
            }
            parser_expect(p, TOK_RBRACE, "expected '}' in array literal");
            return lit;
        }
        default:
            parser_error(p, tok, "expected expression");
            return NULL;
    }
}

/* ============================================================================
 * Prefix expressions
 * ========================================================================== */

static AstNode *parser_parse_prefix(Parser *p)
{
    const Token *tok = parser_current(p);

    switch (tok->kind) {
        case TOK_AMPERSAND: {
            p->pos++;
            bool is_const = parser_match(p, TOK_CONST);
            AstNode *operand = parser_parse_expr_bp(p, 110);
            (void)is_const;
            return ast_new_unary(p->arena, TOK_AMPERSAND, operand, tok->loc);
        }
        case TOK_STAR: {
            p->pos++;
            AstNode *operand = parser_parse_expr_bp(p, 110);
            return ast_new_unary(p->arena, TOK_STAR, operand, tok->loc);
        }
        case TOK_MINUS: {
            p->pos++;
            AstNode *operand = parser_parse_expr_bp(p, 110);
            return ast_new_unary(p->arena, TOK_MINUS, operand, tok->loc);
        }
        case TOK_BANG: {
            p->pos++;
            AstNode *operand = parser_parse_expr_bp(p, 110);
            return ast_new_unary(p->arena, TOK_BANG, operand, tok->loc);
        }
        case TOK_TILDE: {
            p->pos++;
            AstNode *operand = parser_parse_expr_bp(p, 110);
            return ast_new_unary(p->arena, TOK_TILDE, operand, tok->loc);
        }
        case TOK_TRY: {
            p->pos++;
            AstNode *operand = parser_parse_expr_bp(p, 1);
            AstNode *node = ast_new_try(p->arena, operand, tok->loc);
            if (parser_match(p, TOK_ELSE)) {
                parser_expect(p, TOK_PIPE, "expected '|' after 'else' in try capture");
                const Token *err_name = parser_current(p);
                parser_expect(p, TOK_IDENTIFIER, "expected error variable name");
                parser_expect(p, TOK_PIPE, "expected '|' after error variable name");
                AstNode *fallback = parser_parse_block(p);
                node = ast_new_error_capture(p->arena, node, err_name->text, fallback, tok->loc);
            }
            return node;
        }
        case TOK_UNSAFE: {
            p->pos++;
            AstNode *body = parser_parse_block(p);
            return ast_new_unsafe_block(p->arena, body, tok->loc);
        }
        default:
            return parser_parse_primary(p);
    }
}

/* ============================================================================
 * Postfix expressions
 * ========================================================================== */

static AstNode *parser_parse_postfix(Parser *p, AstNode *left)
{
    for (;;) {
        if (parser_match(p, TOK_DOT)) {
            const Token *name = parser_current(p);
            if (!parser_expect(p, TOK_IDENTIFIER, "expected identifier after '.'"))
                return left;

            if (parser_match(p, TOK_LPAREN)) {
                AstNode *call = ast_new_method_call(p->arena, left, name->text, name->loc);
                if (!parser_check(p, TOK_RPAREN)) {
                    do {
                        AstNode *arg = parser_parse_expr(p);
                        if (arg) ast_call_add_arg(p->arena, call, arg);
                    } while (parser_match(p, TOK_COMMA));
                }
                parser_expect(p, TOK_RPAREN, "expected ')' after arguments");
                left = call;
                continue;
            }

            left = ast_new_field_access(p->arena, left, name->text, name->loc);
            continue;
        }

        if (parser_match(p, TOK_LPAREN)) {
            AstNode *call = ast_new_call(p->arena, left, left->loc);
            if (!parser_check(p, TOK_RPAREN)) {
                do {
                    AstNode *arg = parser_parse_expr(p);
                    if (arg) ast_call_add_arg(p->arena, call, arg);
                } while (parser_match(p, TOK_COMMA));
            }
            parser_expect(p, TOK_RPAREN, "expected ')' after arguments");
            left = call;
            continue;
        }

        if (parser_match(p, TOK_LBRACKET)) {
            AstNode *first = parser_parse_expr(p);
            if (parser_match(p, TOK_DOT_DOT)) {
                AstNode *end = parser_parse_expr(p);
                parser_expect(p, TOK_RBRACKET, "expected ']' after slice");
                left = ast_new_slice(p->arena, left, first, end, left->loc);
                continue;
            }
            parser_expect(p, TOK_RBRACKET, "expected ']' after index");
            left = ast_new_index(p->arena, left, first, left->loc);
            continue;
        }

        break;
    }
    return left;
}

/* ============================================================================
 * Pratt expression parser
 * ========================================================================== */

static AstNode *parser_parse_expr_bp(Parser *p, int min_bp)
{
    AstNode *left = parser_parse_prefix(p);
    if (!left) return NULL;

    left = parser_parse_postfix(p, left);

    for (;;) {
        TokenKind op = parser_current(p)->kind;
        BindingPower bp;
        if (!infix_binding_power(op, &bp))
            break;
        if (bp.left < min_bp)
            break;

        p->pos++;
        AstNode *right = parser_parse_expr_bp(p, bp.right);
        if (!right) return left;
        left = ast_new_binary(p->arena, op, left, right, left->loc);
    }

    if (parser_match(p, TOK_AS)) {
        TypeExpr *type = parser_parse_type(p);
        left = ast_new_cast(p->arena, left, type, left->loc);
    }

    return left;
}

AstNode *parser_parse_expr(Parser *p)
{
    return parser_parse_expr_bp(p, 0);
}

/* ============================================================================
 * Blocks
 * ========================================================================== */

AstNode *parser_parse_block(Parser *p)
{
    SourceLocation loc = parser_current(p)->loc;
    if (!parser_expect(p, TOK_LBRACE, "expected '{'"))
        return NULL;

    AstNode *block = ast_new_block(p->arena, loc);

    while (!parser_check(p, TOK_RBRACE) && !parser_at_end(p)) {
        if (parser_check(p, TOK_RBRACE)) break;

        TokenKind k = parser_current(p)->kind;

        if (k == TOK_IF || k == TOK_WHILE || k == TOK_FOR ||
            k == TOK_MATCH || k == TOK_ERRDEFER || k == TOK_LBRACE) {
            AstNode *stmt = parser_parse_stmt(p);
            if (stmt) ast_block_add_stmt(p->arena, block, stmt);
            else break;
            continue;
        }

        if (k == TOK_RETURN || k == TOK_BREAK || k == TOK_CONTINUE ||
            k == TOK_DEFER || k == TOK_CONST || k == TOK_VAR) {
            AstNode *stmt = parser_parse_stmt(p);
            if (stmt) ast_block_add_stmt(p->arena, block, stmt);
            else break;
            continue;
        }

        AstNode *expr = parser_parse_expr(p);
        if (!expr) break;

        TokenKind op = parser_current(p)->kind;
        if (token_is_assignment_op(op)) {
            p->pos++;
            AstNode *rhs = parser_parse_expr(p);
            parser_expect(p, TOK_SEMICOLON, "expected ';' after assignment");
            ast_block_add_stmt(p->arena, block, ast_new_assign(p->arena, op, expr, rhs, expr->loc));
        } else if (parser_match(p, TOK_SEMICOLON)) {
            ast_block_add_stmt(p->arena, block, ast_new_expr_stmt(p->arena, expr, expr->loc));
        } else if (parser_check(p, TOK_RBRACE)) {
            ast_block_set_trailing(block, expr);
            break;
        } else {
            parser_error(p, parser_current(p), "expected ';' or '}' after expression");
            break;
        }
    }

    parser_expect(p, TOK_RBRACE, "expected '}'");
    return block;
}

/* ============================================================================
 * Statements
 * ========================================================================== */

AstNode *parser_parse_stmt(Parser *p)
{
    const Token *tok = parser_current(p);

    switch (tok->kind) {
        case TOK_CONST:
        case TOK_VAR: {
            bool is_const = (tok->kind == TOK_CONST);
            p->pos++;
            const Token *name = parser_current(p);
            parser_expect(p, TOK_IDENTIFIER, "expected identifier");
            TypeExpr *type = NULL;
            if (parser_match(p, TOK_COLON))
                type = parser_parse_type(p);
            parser_expect(p, TOK_ASSIGN, "expected '=' in declaration");
            AstNode *init = parser_parse_expr(p);
            parser_expect(p, TOK_SEMICOLON, "expected ';' after declaration");
            AstNode *decl = is_const
                ? ast_new_const_decl(p->arena, name->text, name->loc)
                : ast_new_var_decl(p->arena, name->text, name->loc);
            decl->var_decl.type = type;
            decl->var_decl.init = init;
            return decl;
        }

        case TOK_RETURN: {
            p->pos++;
            if (parser_match(p, TOK_SEMICOLON))
                return ast_new_return(p->arena, NULL, tok->loc);
            AstNode *value = parser_parse_expr(p);
            parser_expect(p, TOK_SEMICOLON, "expected ';' after return");
            return ast_new_return(p->arena, value, tok->loc);
        }

        case TOK_IF: {
            p->pos++;
            AstNode *condition = parser_parse_expr(p);
            AstNode *then_block = parser_parse_block(p);
            AstNode *else_block = NULL;
            if (parser_match(p, TOK_ELSE)) {
                if (parser_check(p, TOK_IF))
                    else_block = parser_parse_stmt(p);
                else
                    else_block = parser_parse_block(p);
            }
            return ast_new_if(p->arena, condition, then_block, else_block, tok->loc);
        }

        case TOK_WHILE: {
            p->pos++;
            AstNode *condition = parser_parse_expr(p);
            AstNode *body = parser_parse_block(p);
            return ast_new_while(p->arena, condition, body, tok->loc);
        }

        case TOK_FOR: {
            p->pos++;
            const Token *var = parser_current(p);
            parser_expect(p, TOK_IDENTIFIER, "expected identifier after 'for'");
            parser_expect(p, TOK_COLON, "expected ':' after for variable");
            TypeExpr *var_type = parser_parse_type(p);
            parser_expect(p, TOK_IN, "expected 'in' after for variable type");
            AstNode *iterable = parser_parse_expr(p);
            AstNode *body = parser_parse_block(p);
            return ast_new_for(p->arena, var->text, var_type, iterable, body, tok->loc);
        }

        case TOK_BREAK: {
            p->pos++;
            parser_expect(p, TOK_SEMICOLON, "expected ';' after break");
            return ast_new_break(p->arena, tok->loc);
        }

        case TOK_CONTINUE: {
            p->pos++;
            parser_expect(p, TOK_SEMICOLON, "expected ';' after continue");
            return ast_new_continue(p->arena, tok->loc);
        }

        case TOK_DEFER: {
            p->pos++;
            AstNode *expr = parser_parse_expr(p);
            parser_expect(p, TOK_SEMICOLON, "expected ';' after defer");
            return ast_new_defer(p->arena, expr, tok->loc);
        }

        case TOK_ERRDEFER: {
            p->pos++;
            AstNode *body = parser_parse_block(p);
            return ast_new_errdefer(p->arena, body, tok->loc);
        }

        case TOK_MATCH: {
            p->pos++;
            AstNode *expr = parser_parse_expr(p);
            AstNode *match = ast_new_match(p->arena, expr, tok->loc);
            parser_expect(p, TOK_LBRACE, "expected '{' after match expression");
            while (!parser_check(p, TOK_RBRACE) && !parser_at_end(p)) {
                if (parser_check(p, TOK_RBRACE)) break;
                Pattern *pat = parser_parse_pattern(p);
                parser_expect(p, TOK_FAT_ARROW, "expected '=>' in match arm");
                AstNode *arm_expr = parser_parse_expr(p);
                ast_match_add_arm(p->arena, match, ast_new_match_arm(p->arena, pat, arm_expr, pat->loc));
                parser_match(p, TOK_COMMA);
            }
            parser_expect(p, TOK_RBRACE, "expected '}' after match arms");
            return match;
        }

        case TOK_LBRACE:
            return parser_parse_block(p);

        default: {
            AstNode *lhs = parser_parse_expr(p);
            if (!lhs) return NULL;
            TokenKind op = parser_current(p)->kind;
            if (token_is_assignment_op(op)) {
                p->pos++;
                AstNode *rhs = parser_parse_expr(p);
                parser_expect(p, TOK_SEMICOLON, "expected ';' after assignment");
                return ast_new_assign(p->arena, op, lhs, rhs, lhs->loc);
            }
            parser_expect(p, TOK_SEMICOLON, "expected ';' after expression");
            return ast_new_expr_stmt(p->arena, lhs, lhs->loc);
        }
    }
}

/* ============================================================================
 * Pattern parsing
 * ========================================================================== */

static Pattern *parser_parse_pattern(Parser *p)
{
    const Token *tok = parser_current(p);
    SourceLocation loc = tok->loc;

    if (parser_match(p, TOK_UNDERSCORE))
        return pattern_new_wildcard(p->arena, loc);

    if (tok->kind == TOK_INT_LITERAL || tok->kind == TOK_FLOAT_LITERAL ||
        tok->kind == TOK_STRING_LITERAL || tok->kind == TOK_CHAR_LITERAL ||
        tok->kind == TOK_TRUE || tok->kind == TOK_FALSE || tok->kind == TOK_NULL) {
        AstNode *lit = parser_parse_primary(p);
        return pattern_new_literal(p->arena, lit, loc);
    }

    if (parser_check(p, TOK_DOT)) {
        if (parser_peek(p, 1)->kind == TOK_LBRACE) {
            p->pos++;
            parser_expect(p, TOK_LBRACE, "expected '{'");
            Pattern *pat = pattern_new_struct_field(p->arena, loc);
            while (!parser_check(p, TOK_RBRACE) && !parser_at_end(p)) {
                const Token *field = parser_current(p);
                parser_expect(p, TOK_IDENTIFIER, "expected field name");
                parser_expect(p, TOK_COLON, "expected ':'");
                Pattern *field_pat = parser_parse_pattern(p);
                pattern_struct_add_field(p->arena, pat, field->text, field_pat);
                if (!parser_match(p, TOK_COMMA)) break;
            }
            parser_expect(p, TOK_RBRACE, "expected '}'");
            return pat;
        } else {
            p->pos++;
            const Token *name = parser_current(p);
            parser_expect(p, TOK_IDENTIFIER, "expected variant name after '.'");
            Pattern *inner = NULL;
            if (parser_match(p, TOK_LPAREN)) {
                inner = parser_parse_pattern(p);
                parser_expect(p, TOK_RPAREN, "expected ')' after variant pattern");
            }
            return pattern_new_enum_variant(p->arena, name->text, inner, loc);
        }
    }

    if (parser_check(p, TOK_IDENTIFIER)) {
        p->pos++;
        return pattern_new_identifier(p->arena, tok->text, loc);
    }

    parser_error(p, tok, "expected pattern");
    return pattern_new_wildcard(p->arena, loc);
}

/* ============================================================================
 * Type parsing
 * ========================================================================== */

TypeExpr *parser_parse_type(Parser *p)
{
    const Token *tok = parser_current(p);
    SourceLocation loc = tok->loc;
    TypeExpr *primary = NULL;

    if (parser_match(p, TOK_AMPERSAND)) {
        bool is_const = parser_match(p, TOK_CONST);
        TypeExpr *child = parser_parse_type(p);
        primary = type_new_reference(p->arena, child, is_const, loc);
    } else if (parser_match(p, TOK_STAR)) {
        bool is_const = parser_match(p, TOK_CONST);
        TypeExpr *child = parser_parse_type(p);
        primary = type_new_pointer(p->arena, child, is_const, loc);
    } else if (parser_match(p, TOK_LBRACKET)) {
        if (parser_match(p, TOK_RBRACKET)) {
            bool is_const = parser_match(p, TOK_CONST);
            TypeExpr *child = parser_parse_type(p);
            primary = type_new_slice(p->arena, child, is_const, loc);
        } else {
            AstNode *length = parser_parse_expr(p);
            parser_expect(p, TOK_RBRACKET, "expected ']' in array type");
            TypeExpr *child = parser_parse_type(p);
            primary = type_new_array(p->arena, length, child, loc);
        }
    } else if (parser_match(p, TOK_FN)) {
        primary = type_new_function(p->arena, loc);
        parser_expect(p, TOK_LPAREN, "expected '(' in function type");
        if (!parser_check(p, TOK_RPAREN)) {
            do {
                TypeExpr *param = parser_parse_type(p);
                type_func_add_param(p->arena, primary, param);
            } while (parser_match(p, TOK_COMMA));
        }
        parser_expect(p, TOK_RPAREN, "expected ')'");
        if (parser_match(p, TOK_ARROW)) {
            TypeExpr *ret = parser_parse_type(p);
            type_func_set_ret(primary, ret);
        }
    } else if (parser_match(p, TOK_TYPE)) {
        primary = type_new_named(p->arena, sv_from_cstr("type"), loc);
    } else if (parser_match(p, TOK_SELF)) {
        primary = type_new_named(p->arena, sv_from_cstr("Self"), loc);
    } else if (parser_check(p, TOK_IDENTIFIER) && sv_eq_cstr(tok->text, "Self")) {
        p->pos++;
        primary = type_new_named(p->arena, sv_from_cstr("Self"), loc);
    } else if (parser_check(p, TOK_IDENTIFIER)) {
        p->pos++;
        primary = type_new_named(p->arena, tok->text, loc);
        if (parser_match(p, TOK_LPAREN)) {
            while (!parser_check(p, TOK_RPAREN) && !parser_at_end(p)) {
                TypeExpr *arg = parser_parse_type(p);
                type_add_generic_arg(p->arena, primary, arg);
                if (!parser_match(p, TOK_COMMA)) break;
            }
            parser_expect(p, TOK_RPAREN, "expected ')' after generic arguments");
        }
    } else {
        parser_error(p, tok, "expected type");
        return NULL;
    }

    if (parser_match(p, TOK_QUESTION))
        return type_new_optional(p->arena, primary, parser_current(p)->loc);
    if (parser_match(p, TOK_BANG)) {
        TypeExpr *success = parser_parse_type(p);
        return type_new_error_union(p->arena, primary, success, loc);
    }
    return primary;
}

/* ============================================================================
 * Parameter parsing (including self shorthand)
 * ========================================================================== */

static AstNode *parser_parse_param(Parser *p)
{
    const Token *tok = parser_current(p);

    if (tok->kind == TOK_AMPERSAND || tok->kind == TOK_SELF) {
        bool is_ref = false;
        bool is_const = false;

        if (parser_match(p, TOK_AMPERSAND)) {
            is_ref = true;
            if (parser_match(p, TOK_CONST)) is_const = true;
        }

        const Token *self_tok = parser_current(p);
        parser_expect(p, TOK_SELF, "expected 'self'");

        TypeExpr *type = NULL;
        if (parser_match(p, TOK_COLON)) {
            type = parser_parse_type(p);
        } else if (is_ref) {
            TypeExpr *self_type = type_new_named(p->arena, sv_from_cstr("Self"), self_tok->loc);
            type = type_new_reference(p->arena, self_type, is_const, self_tok->loc);
        } else {
            type = type_new_named(p->arena, sv_from_cstr("Self"), self_tok->loc);
        }

        AstNode *param = ast_new_param_decl(p->arena, sv_from_cstr("self"), self_tok->loc);
        param->param_decl.type = type;
        param->param_decl.is_self = true;

        if (parser_match(p, TOK_ASSIGN)) {
            param->param_decl.default_value = parser_parse_expr(p);
        }

        return param;
    }

    const Token *name = parser_current(p);
    parser_expect(p, TOK_IDENTIFIER, "expected parameter name");
    parser_expect(p, TOK_COLON, "expected ':' after parameter name");
    TypeExpr *type = parser_parse_type(p);

    AstNode *param = ast_new_param_decl(p->arena, name->text, name->loc);
    param->param_decl.type = type;

    if (parser_match(p, TOK_ASSIGN)) {
        param->param_decl.default_value = parser_parse_expr(p);
    }

    return param;
}

/* ============================================================================
 * Generic parameter parsing
 * ========================================================================== */

static void parser_parse_generic_params(Parser *p, AstNodeList *out_list)
{
    if (!parser_check(p, TOK_LPAREN)) return;
    if (parser_peek(p, 1)->kind != TOK_IDENTIFIER) return;
    if (parser_peek(p, 2)->kind != TOK_COLON) return;
    if (parser_peek(p, 3)->kind != TOK_TYPE) return;

    parser_expect(p, TOK_LPAREN, "expected '('");
    do {
        const Token *gp_name = parser_current(p);
        parser_expect(p, TOK_IDENTIFIER, "expected generic parameter name");
        parser_expect(p, TOK_COLON, "expected ':'");
        parser_expect(p, TOK_TYPE, "expected 'type'");

        AstNode *gp = ast_new_generic_param_decl(p->arena, gp_name->text, gp_name->loc);
        if (parser_match(p, TOK_WITH)) {
            do {
                const Token *trait = parser_current(p);
                parser_expect(p, TOK_IDENTIFIER, "expected trait name");
                string_view_list_push(p->arena, &gp->generic_param_decl.trait_constraints, trait->text);
            } while (parser_match(p, TOK_COMMA));
        }
        ast_node_list_push(p->arena, out_list, gp);
    } while (parser_match(p, TOK_COMMA));
    parser_expect(p, TOK_RPAREN, "expected ')'");
}

/* ============================================================================
 * Declaration parsers
 * ========================================================================== */

static AstNode *parser_parse_fn_decl(Parser *p, bool is_pub, AttributeList attrs)
{
    bool is_comptime = parser_match(p, TOK_COMPTIME);
    bool is_unsafe = parser_match(p, TOK_UNSAFE);
    parser_expect(p, TOK_FN, "expected 'fn'");

    const Token *name = parser_current(p);
    parser_expect(p, TOK_IDENTIFIER, "expected function name");

    AstNode *fn = ast_new_fn_decl(p->arena, name->text, name->loc);
    fn->fn_decl.is_pub = is_pub;
    fn->fn_decl.is_comptime = is_comptime;
    fn->fn_decl.is_unsafe = is_unsafe;
    fn->fn_decl.attrs = attrs;

    parser_parse_generic_params(p, &fn->fn_decl.generic_params);

    parser_expect(p, TOK_LPAREN, "expected '('");
    if (!parser_check(p, TOK_RPAREN)) {
        do {
            AstNode *param = parser_parse_param(p);
            if (param) ast_fn_add_param(p->arena, fn, param);
        } while (parser_match(p, TOK_COMMA));
    }
    parser_expect(p, TOK_RPAREN, "expected ')'");

    if (parser_match(p, TOK_ARROW)) {
        fn->fn_decl.ret_type = parser_parse_type(p);
    }

    if (parser_match(p, TOK_SEMICOLON)) {
        fn->fn_decl.body = NULL;
    } else {
        fn->fn_decl.body = parser_parse_block(p);
    }

    return fn;
}

static AstNode *parser_parse_struct_decl(Parser *p, bool is_pub, AttributeList attrs)
{
    parser_expect(p, TOK_STRUCT, "expected 'struct'");

    const Token *name = parser_current(p);
    parser_expect(p, TOK_IDENTIFIER, "expected struct name");

    AstNode *s = ast_new_struct_decl(p->arena, name->text, name->loc);
    s->struct_decl.is_pub = is_pub;
    s->struct_decl.attrs = attrs;

    parser_parse_generic_params(p, &s->struct_decl.generic_params);

    parser_expect(p, TOK_LBRACE, "expected '{'");
    while (!parser_check(p, TOK_RBRACE) && !parser_at_end(p)) {
        if (parser_check(p, TOK_RBRACE)) break;

        AttributeList field_attrs = parser_parse_attrs(p);
        bool field_pub = parser_match(p, TOK_PUB);

        const Token *field_name = parser_current(p);
        parser_expect(p, TOK_IDENTIFIER, "expected field name");
        parser_expect(p, TOK_COLON, "expected ':' after field name");
        TypeExpr *field_type = parser_parse_type(p);

        AstNode *field = ast_new_field_decl(p->arena, field_name->text, field_name->loc);
        field->field_decl.is_pub = field_pub;
        field->field_decl.type = field_type;
        field->field_decl.attrs = field_attrs;

        if (parser_match(p, TOK_ASSIGN)) {
            field->field_decl.default_value = parser_parse_expr(p);
        }

        ast_struct_add_field(p->arena, s, field);
        if (!parser_match(p, TOK_COMMA))
            break;
    }
    parser_expect(p, TOK_RBRACE, "expected '}'");
    return s;
}

static AstNode *parser_parse_union_decl(Parser *p, bool is_pub, AttributeList attrs)
{
    parser_expect(p, TOK_UNION, "expected 'union'");

    const Token *name = parser_current(p);
    parser_expect(p, TOK_IDENTIFIER, "expected union name");

    AstNode *u = ast_new_union_decl(p->arena, name->text, name->loc);
    u->struct_decl.is_pub = is_pub;
    u->struct_decl.attrs = attrs;

    parser_parse_generic_params(p, &u->struct_decl.generic_params);

    parser_expect(p, TOK_LBRACE, "expected '{'");
    while (!parser_check(p, TOK_RBRACE) && !parser_at_end(p)) {
        if (parser_check(p, TOK_RBRACE)) break;

        AttributeList field_attrs = parser_parse_attrs(p);
        bool field_pub = parser_match(p, TOK_PUB);

        const Token *field_name = parser_current(p);
        parser_expect(p, TOK_IDENTIFIER, "expected field name");
        parser_expect(p, TOK_COLON, "expected ':' after field name");
        TypeExpr *field_type = parser_parse_type(p);

        AstNode *field = ast_new_field_decl(p->arena, field_name->text, field_name->loc);
        field->field_decl.is_pub = field_pub;
        field->field_decl.type = field_type;
        field->field_decl.attrs = field_attrs;

        if (parser_match(p, TOK_ASSIGN)) {
            field->field_decl.default_value = parser_parse_expr(p);
        }

        ast_struct_add_field(p->arena, u, field);
        if (!parser_match(p, TOK_COMMA))
            break;
    }
    parser_expect(p, TOK_RBRACE, "expected '}'");
    return u;
}

static AstNode *parser_parse_enum_decl(Parser *p, bool is_pub, AttributeList attrs)
{
    parser_expect(p, TOK_ENUM, "expected 'enum'");

    const Token *name = parser_current(p);
    parser_expect(p, TOK_IDENTIFIER, "expected enum name");

    AstNode *e = ast_new_enum_decl(p->arena, name->text, name->loc);
    e->enum_decl.is_pub = is_pub;
    e->enum_decl.attrs = attrs;

    parser_expect(p, TOK_LBRACE, "expected '{'");
    while (!parser_check(p, TOK_RBRACE) && !parser_at_end(p)) {
        if (parser_check(p, TOK_RBRACE)) break;

        const Token *var_name = parser_current(p);
        parser_expect(p, TOK_IDENTIFIER, "expected variant name");

        AstNode *variant = ast_new_variant_decl(p->arena, var_name->text, var_name->loc);

        if (parser_match(p, TOK_LPAREN)) {
            variant->variant_decl.payload_type = parser_parse_type(p);
            parser_expect(p, TOK_RPAREN, "expected ')' after variant payload");
        }

        if (parser_match(p, TOK_ASSIGN)) {
            variant->variant_decl.discriminant = parser_parse_expr(p);
        }

        ast_enum_add_variant(p->arena, e, variant);
        if (!parser_match(p, TOK_COMMA))
            break;
    }
    parser_expect(p, TOK_RBRACE, "expected '}'");
    return e;
}

static AstNode *parser_parse_traits_decl(Parser *p, bool is_pub, AttributeList attrs)
{
    parser_expect(p, TOK_TRAITS, "expected 'traits'");

    const Token *name = parser_current(p);
    parser_expect(p, TOK_IDENTIFIER, "expected trait name");

    AstNode *traits = ast_new_traits_decl(p->arena, name->text, name->loc);
    traits->traits_decl.is_pub = is_pub;
    traits->traits_decl.attrs = attrs;

    parser_expect(p, TOK_LBRACE, "expected '{'");
    while (!parser_check(p, TOK_RBRACE) && !parser_at_end(p)) {
        if (parser_check(p, TOK_RBRACE)) break;

        bool method_pub = parser_match(p, TOK_PUB);
        if (!parser_expect(p, TOK_FN, "expected 'fn' in trait method")) {
            if (!parser_at_end(p)) p->pos++;
            continue;
        }

        const Token *method_name = parser_current(p);
        parser_expect(p, TOK_IDENTIFIER, "expected method name");

        AstNode *method = ast_new_trait_method_decl(p->arena, method_name->text, method_name->loc);
        method->trait_method_decl.is_pub = method_pub;

        parser_expect(p, TOK_LPAREN, "expected '('");
        if (!parser_check(p, TOK_RPAREN)) {
            do {
                AstNode *param = parser_parse_param(p);
                if (param) ast_node_list_push(p->arena, &method->trait_method_decl.params, param);
            } while (parser_match(p, TOK_COMMA));
        }
        parser_expect(p, TOK_RPAREN, "expected ')'");

        if (parser_match(p, TOK_ARROW)) {
            method->trait_method_decl.ret_type = parser_parse_type(p);
        }

        parser_expect(p, TOK_SEMICOLON, "expected ';' after trait method");
        ast_traits_add_method(p->arena, traits, method);
    }
    parser_expect(p, TOK_RBRACE, "expected '}'");
    return traits;
}

static AstNode *parser_parse_extend_decl(Parser *p, bool is_pub, AttributeList attrs)
{
    (void)is_pub;
    parser_expect(p, TOK_EXTEND, "expected 'extend'");

    const Token *target = parser_current(p);
    parser_expect(p, TOK_IDENTIFIER, "expected type name after 'extend'");

    AstNode *extend = ast_new_extend_decl(p->arena, target->text, target->loc);
    extend->extend_decl.attrs = attrs;

    parser_parse_generic_params(p, &extend->extend_decl.generic_params);

    if (parser_match(p, TOK_WITH)) {
        do {
            const Token *trait = parser_current(p);
            parser_expect(p, TOK_IDENTIFIER, "expected trait name");
            ast_extend_add_trait(p->arena, extend, trait->text);
        } while (parser_match(p, TOK_COMMA));
    }

    parser_expect(p, TOK_LBRACE, "expected '{'");
    while (!parser_check(p, TOK_RBRACE) && !parser_at_end(p)) {
        if (parser_check(p, TOK_RBRACE)) break;

        AttributeList fn_attrs = parser_parse_attrs(p);
        bool fn_pub = parser_match(p, TOK_PUB);
        bool fn_comptime = parser_match(p, TOK_COMPTIME);
        bool fn_unsafe = parser_match(p, TOK_UNSAFE);
        if (!parser_expect(p, TOK_FN, "expected 'fn'")) {
            if (!parser_at_end(p)) p->pos++;
            continue;
        }

        const Token *fn_name = parser_current(p);
        parser_expect(p, TOK_IDENTIFIER, "expected function name");

        AstNode *fn = ast_new_fn_decl(p->arena, fn_name->text, fn_name->loc);
        fn->fn_decl.is_pub = fn_pub;
        fn->fn_decl.is_comptime = fn_comptime;
        fn->fn_decl.is_unsafe = fn_unsafe;
        fn->fn_decl.attrs = fn_attrs;

        parser_parse_generic_params(p, &fn->fn_decl.generic_params);

        parser_expect(p, TOK_LPAREN, "expected '('");
        if (!parser_check(p, TOK_RPAREN)) {
            do {
                AstNode *param = parser_parse_param(p);
                if (param) ast_fn_add_param(p->arena, fn, param);
            } while (parser_match(p, TOK_COMMA));
        }
        parser_expect(p, TOK_RPAREN, "expected ')'");

        if (parser_match(p, TOK_ARROW)) {
            fn->fn_decl.ret_type = parser_parse_type(p);
        }

        if (parser_match(p, TOK_SEMICOLON)) {
            fn->fn_decl.body = NULL;
        } else {
            fn->fn_decl.body = parser_parse_block(p);
        }

        ast_extend_add_method(p->arena, extend, fn);
    }
    parser_expect(p, TOK_RBRACE, "expected '}'");
    return extend;
}

static AstNode *parser_parse_type_alias(Parser *p, bool is_pub, AttributeList attrs)
{
    parser_expect(p, TOK_TYPE, "expected 'type'");

    const Token *name = parser_current(p);
    parser_expect(p, TOK_IDENTIFIER, "expected type alias name");

    parser_expect(p, TOK_ASSIGN, "expected '='");
    TypeExpr *type = parser_parse_type(p);
    parser_expect(p, TOK_SEMICOLON, "expected ';' after type alias");

    AstNode *alias = ast_new_type_alias(p->arena, name->text, name->loc);
    alias->type_alias.is_pub = is_pub;
    alias->type_alias.type = type;
    alias->type_alias.attrs = attrs;
    return alias;
}

static AstNode *parser_parse_test_decl(Parser *p, AttributeList attrs)
{
    parser_expect(p, TOK_TEST, "expected 'test'");

    const Token *name = parser_current(p);
    parser_expect(p, TOK_STRING_LITERAL, "expected test name string");

    AstNode *body = parser_parse_block(p);

    AstNode *test = ast_new_test_decl(p->arena, name->text, name->loc);
    test->test_decl.body = body;
    test->test_decl.attrs = attrs;
    return test;
}

/* ============================================================================
 * Top-level declaration dispatcher
 * ========================================================================== */

static AstNode *parser_parse_top_level_decl(Parser *p)
{
    AttributeList attrs = parser_parse_attrs(p);
    bool is_pub = parser_match(p, TOK_PUB);

    TokenKind k = parser_current(p)->kind;

    if (k == TOK_UNSAFE && parser_peek(p, 1)->kind == TOK_FN) {
        return parser_parse_fn_decl(p, is_pub, attrs);
    }

    switch (k) {
        case TOK_FN:
            return parser_parse_fn_decl(p, is_pub, attrs);
        case TOK_STRUCT:
            return parser_parse_struct_decl(p, is_pub, attrs);
        case TOK_UNION:
            return parser_parse_union_decl(p, is_pub, attrs);
        case TOK_ENUM:
            return parser_parse_enum_decl(p, is_pub, attrs);
        case TOK_TRAITS:
            return parser_parse_traits_decl(p, is_pub, attrs);
        case TOK_EXTEND:
            return parser_parse_extend_decl(p, is_pub, attrs);
        case TOK_TYPE:
            return parser_parse_type_alias(p, is_pub, attrs);
        case TOK_TEST:
            return parser_parse_test_decl(p, attrs);
        case TOK_CONST:
        case TOK_VAR: {
            bool is_const = (k == TOK_CONST);
            p->pos++;
            const Token *name = parser_current(p);
            parser_expect(p, TOK_IDENTIFIER, "expected identifier");
            TypeExpr *type = NULL;
            if (parser_match(p, TOK_COLON))
                type = parser_parse_type(p);
            parser_expect(p, TOK_ASSIGN, "expected '='");
            AstNode *init = parser_parse_expr(p);
            parser_expect(p, TOK_SEMICOLON, "expected ';'");
            AstNode *decl = is_const
                ? ast_new_const_decl(p->arena, name->text, name->loc)
                : ast_new_var_decl(p->arena, name->text, name->loc);
            decl->var_decl.is_pub = is_pub;
            decl->var_decl.type = type;
            decl->var_decl.init = init;
            decl->var_decl.attrs = attrs;
            return decl;
        }
        default:
            parser_error(p, parser_current(p), "expected top-level declaration");
            return NULL;
    }
}

/* ============================================================================
 * Entry point
 * ========================================================================== */

AstNode *parser_parse(Parser *p)
{
    AstNode *root = ast_new_compilation_unit(p->arena, parser_current(p)->loc);

    if (parser_match(p, TOK_MODULE)) {
        const Token *name = parser_current(p);
        parser_expect(p, TOK_IDENTIFIER, "expected module name");
        parser_expect(p, TOK_SEMICOLON, "expected ';' after module declaration");
        ast_set_module(root, name->text);
    }

    while (parser_match(p, TOK_IMPORT)) {
        const Token *first = parser_current(p);
        parser_expect(p, TOK_IDENTIFIER, "expected module name after import");

        AstNode *import = ast_new_import_decl(p->arena, first->text, first->loc);

        while (parser_match(p, TOK_DOT)) {
            const Token *part = parser_current(p);
            parser_expect(p, TOK_IDENTIFIER, "expected identifier after '.'");
            ast_import_add_part(p->arena, import, part->text);
        }

        if (parser_match(p, TOK_AS)) {
            const Token *alias = parser_current(p);
            parser_expect(p, TOK_IDENTIFIER, "expected identifier after 'as'");
            ast_import_set_alias(import, alias->text);
        }

        parser_expect(p, TOK_SEMICOLON, "expected ';' after import");
        ast_add_import(p->arena, root, import);
    }

    while (!parser_at_end(p)) {
        AstNode *decl = parser_parse_top_level_decl(p);
        if (!decl) {
            if (!parser_at_end(p)) p->pos++;
            continue;
        }
        ast_add_decl(p->arena, root, decl);
    }

    return root;
}
