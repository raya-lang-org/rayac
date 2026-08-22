#include "codegen_c.h"
#include "ast.h"
#include "type.h"
#include "string_view.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define MAX_DEFER_DEPTH 64
#define MAX_DEFERS_PER_BLOCK 32

typedef struct {
    AstNode *exprs[MAX_DEFERS_PER_BLOCK];
    int count;
} DeferLevel;

typedef struct {
    Arena *arena;
    FILE *out;
    int indent;
    int temp_counter;
    bool in_unsafe;
    DeferLevel defer_stack[MAX_DEFER_DEPTH];
    int defer_depth;
    int loop_defer_base[MAX_DEFER_DEPTH];
    int loop_depth;
    AstNode *unit;
} CGen;

static void cg_indent(CGen *cg) {
    for (int i = 0; i < cg->indent; i++) fprintf(cg->out, "    ");
}

static AstNode *cg_find_method(CGen *cg, StringView type_name, StringView method_name) {
    if (!cg->unit) return NULL;
    for (size_t i = 0; i < cg->unit->compilation_unit.decls.count; i++) {
        AstNode *decl = cg->unit->compilation_unit.decls.items[i];
        if (decl->kind == AST_EXTEND_DECL) {
            if (sv_eq(decl->extend_decl.target_name, type_name)) {
                for (size_t j = 0; j < decl->extend_decl.methods.count; j++) {
                    AstNode *m = decl->extend_decl.methods.items[j];
                    if (sv_eq(m->fn_decl.name, method_name)) return m;
                }
            }
        }
    }
    return NULL;
}

static int cg_next_temp(CGen *cg) {
    return cg->temp_counter++;
}

static void cg_emit_type_expr(CGen *cg, TypeExpr *te, const char *name);
static void cg_emit_sema_type(CGen *cg, SType *type, const char *name);
static void cg_emit_expr(CGen *cg, AstNode *expr);
static void cg_emit_stmt(CGen *cg, AstNode *stmt);
static void cg_emit_block(CGen *cg, AstNode *block, bool return_trailing);
static void cg_emit_defers(CGen *cg, int down_to_level);

/* ========================================================================
 *  TypeExpr -> C type
 * ======================================================================== */
static void cg_emit_type_expr(CGen *cg, TypeExpr *te, const char *name) {
    if (!te) { fprintf(cg->out, "void %s", name); return; }
    switch (te->kind) {
        case TYPE_NAMED: {
            StringView n = te->named.name;
            if (sv_eq_cstr(n, "void"))       fprintf(cg->out, "void %s", name);
            else if (sv_eq_cstr(n, "bool"))  fprintf(cg->out, "_Bool %s", name);
            else if (sv_eq_cstr(n, "isize")) fprintf(cg->out, "intptr_t %s", name);
            else if (sv_eq_cstr(n, "usize")) fprintf(cg->out, "uintptr_t %s", name);
            else if (sv_eq_cstr(n, "noreturn")) fprintf(cg->out, "void %s", name);
            else if (n.len > 1 && n.data[0] == 'i') {
                int b = sv_to_int(sv_slice(n, 1, n.len));
                fprintf(cg->out, "int%d_t %s", b, name);
            }
            else if (n.len > 1 && n.data[0] == 'u') {
                int b = sv_to_int(sv_slice(n, 1, n.len));
                fprintf(cg->out, "uint%d_t %s", b, name);
            }
            else if (n.len > 1 && n.data[0] == 'f') {
                int b = sv_to_int(sv_slice(n, 1, n.len));
                fprintf(cg->out, "%s %s", b == 32 ? "float" : "double", name);
            }
            else {
                fprintf(cg->out, "struct raya_%.*s %s", (int)n.len, n.data, name);
            }
            break;
        }
        case TYPE_REFERENCE: {
            char buf[256];
            if (te->unary.is_const) snprintf(buf, sizeof(buf), "*const %s", name);
            else snprintf(buf, sizeof(buf), "*%s", name);
            cg_emit_type_expr(cg, te->unary.child, buf);
            break;
        }
        case TYPE_POINTER: {
            char buf[256];
            if (te->unary.is_const) snprintf(buf, sizeof(buf), "*const %s", name);
            else snprintf(buf, sizeof(buf), "*%s", name);
            cg_emit_type_expr(cg, te->unary.child, buf);
            break;
        }
        case TYPE_SLICE: {
            if (te->unary.is_const && te->unary.child->kind == TYPE_NAMED
                && sv_eq_cstr(te->unary.child->named.name, "u8")) {
                fprintf(cg->out, "raya_Str %s", name);
            } else {
                fprintf(cg->out, "raya_Slice %s", name);
            }
            break;
        }
        case TYPE_ARRAY: {
            char buf[256];
            snprintf(buf, sizeof(buf), "*%s", name);
            cg_emit_type_expr(cg, te->array.elem, buf);
            break;
        }
        case TYPE_FUNCTION: {
            fprintf(cg->out, "/* fn ptr */ void* %s", name);
            break;
        }
        case TYPE_OPTIONAL: {
            char buf[256];
            snprintf(buf, sizeof(buf), "*%s", name);
            cg_emit_type_expr(cg, te->unary.child, buf);
            break;
        }
        case TYPE_ERROR_UNION: {
            cg_emit_type_expr(cg, te->unary.child, name);
            break;
        }
    }
}

/* ========================================================================
 *  SType -> C type
 * ======================================================================== */
static void cg_emit_sema_type(CGen *cg, SType *type, const char *name) {
    if (!type) { fprintf(cg->out, "void %s", name); return; }
    switch (type->kind) {
        case ST_VOID:   fprintf(cg->out, "void %s", name); break;
        case ST_BOOL:   fprintf(cg->out, "_Bool %s", name); break;
        case ST_NORETURN: fprintf(cg->out, "void %s", name); break;
        case ST_INT: {
            if (type->as.integer.is_signed) {
                switch (type->as.integer.bits) {
                    case 8:  fprintf(cg->out, "int8_t %s", name); break;
                    case 16: fprintf(cg->out, "int16_t %s", name); break;
                    case 32: fprintf(cg->out, "int32_t %s", name); break;
                    case 64: fprintf(cg->out, "int64_t %s", name); break;
                    case 128: fprintf(cg->out, "__int128 %s", name); break;
                    default: fprintf(cg->out, "int64_t %s", name); break;
                }
            } else {
                switch (type->as.integer.bits) {
                    case 8:  fprintf(cg->out, "uint8_t %s", name); break;
                    case 16: fprintf(cg->out, "uint16_t %s", name); break;
                    case 32: fprintf(cg->out, "uint32_t %s", name); break;
                    case 64: fprintf(cg->out, "uint64_t %s", name); break;
                    case 128: fprintf(cg->out, "unsigned __int128 %s", name); break;
                    default: fprintf(cg->out, "uint64_t %s", name); break;
                }
            }
            break;
        }
        case ST_FLOAT: {
            fprintf(cg->out, "%s %s",
                type->as.floating.bits == 32 ? "float" : "double", name);
            break;
        }
        case ST_POINTER: {
            char buf[256];
            if (type->as.pointer.is_const) snprintf(buf, sizeof(buf), "*const %s", name);
            else snprintf(buf, sizeof(buf), "*%s", name);
            cg_emit_sema_type(cg, type->as.pointer.base, buf);
            break;
        }
        case ST_REFERENCE: {
            char buf[256];
            if (type->as.reference.is_const) snprintf(buf, sizeof(buf), "*const %s", name);
            else snprintf(buf, sizeof(buf), "*%s", name);
            cg_emit_sema_type(cg, type->as.reference.base, buf);
            break;
        }
        case ST_SLICE: {
            if (type->as.slice.is_const && type->as.slice.base->kind == ST_INT
                && type->as.slice.base->as.integer.bits == 8
                && !type->as.slice.base->as.integer.is_signed) {
                fprintf(cg->out, "raya_Str %s", name);
            } else {
                fprintf(cg->out, "raya_Slice %s", name);
            }
            break;
        }
        case ST_ARRAY: {
            char buf[256];
            snprintf(buf, sizeof(buf), "%s[%zu]", name, (size_t)type->as.array.size);
            cg_emit_sema_type(cg, type->as.array.base, buf);
            break;
        }
        case ST_FUNCTION: {
            fprintf(cg->out, "/* fn ptr */ void* %s", name);
            break;
        }
        case ST_STRUCT: {
            fprintf(cg->out, "struct raya_%.*s %s",
                (int)type->as.struct_.name.len, type->as.struct_.name.data, name);
            break;
        }
        case ST_UNION: {
            fprintf(cg->out, "union raya_%.*s %s",
                (int)type->as.union_.name.len, type->as.union_.name.data, name);
            break;
        }
        case ST_ENUM: {
            fprintf(cg->out, "enum raya_%.*s %s",
                (int)type->as.enum_.name.len, type->as.enum_.name.data, name);
            break;
        }
        case ST_OPTIONAL: {
            char buf[256];
            snprintf(buf, sizeof(buf), "*%s", name);
            cg_emit_sema_type(cg, type->as.optional.base, buf);
            break;
        }
        case ST_ERROR_UNION: {
            cg_emit_sema_type(cg, type->as.error_union.base, name);
            break;
        }
        case ST_TRAIT:
        case ST_GENERIC_PARAM:
        case ST_OPAQUE:
            fprintf(cg->out, "/* %s */ void* %s", st_name(type), name);
            break;
    }
}

/* ========================================================================
 *  Defer helpers
 * ======================================================================== */
static void cg_emit_defers(CGen *cg, int down_to_level) {
    for (int d = cg->defer_depth; d >= down_to_level; d--) {
        DeferLevel *lvl = &cg->defer_stack[d];
        for (int i = lvl->count - 1; i >= 0; i--) {
            cg_indent(cg);
            cg_emit_expr(cg, lvl->exprs[i]);
            fprintf(cg->out, ";\n");
        }
    }
}

/* ========================================================================
 *  Operator mappings
 * ======================================================================== */
static const char* cg_assign_op(TokenKind op) {
    switch (op) {
        case TOK_ASSIGN: return "=";
        case TOK_PLUS_ASSIGN: return "+=";
        case TOK_MINUS_ASSIGN: return "-=";
        case TOK_STAR_ASSIGN: return "*=";
        case TOK_SLASH_ASSIGN: return "/=";
        case TOK_PERCENT_ASSIGN: return "%=";
        case TOK_AND_ASSIGN: return "&=";
        case TOK_OR_ASSIGN: return "|=";
        case TOK_XOR_ASSIGN: return "^=";
        case TOK_SHL_ASSIGN: return "<<=";
        case TOK_SHR_ASSIGN: return ">>=";
        default: return "=";
    }
}

static const char* cg_binary_op(TokenKind op) {
    switch (op) {
        case TOK_PLUS: return "+";
        case TOK_MINUS: return "-";
        case TOK_STAR: return "*";
        case TOK_SLASH: return "/";
        case TOK_PERCENT: return "%";
        case TOK_EQ: return "==";
        case TOK_NE: return "!=";
        case TOK_LT: return "<";
        case TOK_GT: return ">";
        case TOK_LE: return "<=";
        case TOK_GE: return ">=";
        case TOK_AND_AND: return "&&";
        case TOK_OR_OR: return "||";
        case TOK_AMPERSAND: return "&";
        case TOK_PIPE: return "|";
        case TOK_CARET: return "^";
        case TOK_SHL: return "<<";
        case TOK_SHR: return ">>";
        default: return "/* unknown op */";
    }
}

static const char* cg_unary_op(TokenKind op) {
    switch (op) {
        case TOK_MINUS: return "-";
        case TOK_TILDE: return "~";
        case TOK_BANG: return "!";
        case TOK_STAR: return "*";
        case TOK_AMPERSAND: return "&";
        default: return "/* unknown unary */";
    }
}

/* ========================================================================
 *  Block emission
 * ======================================================================== */
static void cg_emit_block(CGen *cg, AstNode *block, bool return_trailing) {
    if (!block) return;
    fprintf(cg->out, "{\n");
    cg->indent++;

    cg->defer_depth++;
    cg->defer_stack[cg->defer_depth].count = 0;

    for (size_t i = 0; i < block->block.stmts.count; i++) {
        cg_emit_stmt(cg, block->block.stmts.items[i]);
    }
    if (block->block.trailing_expr) {
        cg_indent(cg);
        if (return_trailing) {
            fprintf(cg->out, "return ");
        }
        cg_emit_expr(cg, block->block.trailing_expr);
        fprintf(cg->out, ";\n");
    }

    cg_emit_defers(cg, cg->defer_depth);
    cg->defer_depth--;

    cg->indent--;
    cg_indent(cg);
    fprintf(cg->out, "}");
}

/* ========================================================================
 *  Statement emission
 * ======================================================================== */
static void cg_emit_stmt(CGen *cg, AstNode *stmt) {
    if (!stmt) return;
    switch (stmt->kind) {
        case AST_VAR_DECL:
        case AST_CONST_DECL: {
            cg_indent(cg);
            if (stmt->var_decl.type) {
                char name_buf[256];
                snprintf(name_buf, sizeof(name_buf), "%.*s",
                    (int)stmt->var_decl.name.len, stmt->var_decl.name.data);
                cg_emit_type_expr(cg, stmt->var_decl.type, name_buf);
            } else if (stmt->var_decl.init && stmt->var_decl.init->sema_type) {
                char name_buf[256];
                snprintf(name_buf, sizeof(name_buf), "%.*s",
                    (int)stmt->var_decl.name.len, stmt->var_decl.name.data);
                cg_emit_sema_type(cg, stmt->var_decl.init->sema_type, name_buf);
            } else {
                fprintf(cg->out, "/* inferred */ int %.*s",
                    (int)stmt->var_decl.name.len, stmt->var_decl.name.data);
            }
            if (stmt->var_decl.init) {
                fprintf(cg->out, " = ");
                cg_emit_expr(cg, stmt->var_decl.init);
            }
            fprintf(cg->out, ";\n");
            break;
        }
        case AST_RETURN_STMT: {
            cg_indent(cg);
            cg_emit_defers(cg, 0);
            fprintf(cg->out, "return");
            if (stmt->return_stmt.value) {
                fprintf(cg->out, " ");
                cg_emit_expr(cg, stmt->return_stmt.value);
            }
            fprintf(cg->out, ";\n");
            break;
        }
        case AST_EXPR_STMT: {
            cg_indent(cg);
            cg_emit_expr(cg, stmt->expr_stmt.expr);
            fprintf(cg->out, ";\n");
            break;
        }
        case AST_BLOCK: {
            cg_indent(cg);
            cg_emit_block(cg, stmt, false);
            fprintf(cg->out, "\n");
            break;
        }
        case AST_IF_STMT: {
            cg_indent(cg);
            fprintf(cg->out, "if (");
            cg_emit_expr(cg, stmt->if_stmt.condition);
            fprintf(cg->out, ") ");
            cg_emit_block(cg, stmt->if_stmt.then_block, false);
            if (stmt->if_stmt.else_block) {
                fprintf(cg->out, " else ");
                if (stmt->if_stmt.else_block->kind == AST_IF_STMT) {
                    cg_emit_stmt(cg, stmt->if_stmt.else_block);
                } else {
                    cg_emit_block(cg, stmt->if_stmt.else_block, false);
                    fprintf(cg->out, "\n");
                }
            } else {
                fprintf(cg->out, "\n");
            }
            break;
        }
        case AST_WHILE_STMT: {
            cg_indent(cg);
            fprintf(cg->out, "while (");
            cg_emit_expr(cg, stmt->while_stmt.condition);
            fprintf(cg->out, ") ");
            cg->loop_depth++;
            cg->loop_defer_base[cg->loop_depth] = cg->defer_depth;
            cg_emit_block(cg, stmt->while_stmt.body, false);
            cg->loop_depth--;
            fprintf(cg->out, "\n");
            break;
        }
        case AST_FOR_STMT: {
            cg_indent(cg);
            AstNode *iter = stmt->for_stmt.iterable;
            bool is_range = false;
            if (iter->kind == AST_SLICE_EXPR && !iter->slice_expr.object) {
                is_range = true;
            } else if (iter->kind == AST_BINARY_EXPR) {
                is_range = true;
            }

            if (is_range) {
                AstNode *start = NULL, *end = NULL;
                if (iter->kind == AST_SLICE_EXPR) {
                    start = iter->slice_expr.start;
                    end = iter->slice_expr.end;
                } else if (iter->kind == AST_BINARY_EXPR) {
                    start = iter->binary_expr.left;
                    end = iter->binary_expr.right;
                }
                fprintf(cg->out, "for (");
                if (stmt->for_stmt.var_type) {
                    char vname[256];
                    snprintf(vname, sizeof(vname), "%.*s",
                        (int)stmt->for_stmt.var_name.len, stmt->for_stmt.var_name.data);
                    cg_emit_type_expr(cg, stmt->for_stmt.var_type, vname);
                } else {
                    fprintf(cg->out, "int64_t %.*s",
                        (int)stmt->for_stmt.var_name.len, stmt->for_stmt.var_name.data);
                }
                fprintf(cg->out, " = ");
                if (start) cg_emit_expr(cg, start); else fprintf(cg->out, "0");
                fprintf(cg->out, "; %.*s < ",
                    (int)stmt->for_stmt.var_name.len, stmt->for_stmt.var_name.data);
                if (end) cg_emit_expr(cg, end); else fprintf(cg->out, "0");
                fprintf(cg->out, "; %.*s++) ",
                    (int)stmt->for_stmt.var_name.len, stmt->for_stmt.var_name.data);
            } else {
                int tmp = cg_next_temp(cg);
                fprintf(cg->out, "{\n");
                cg->indent++;
                cg_indent(cg);
                fprintf(cg->out, "raya_Slice __iter_%d = ", tmp);
                cg_emit_expr(cg, iter);
                fprintf(cg->out, ";\n");
                cg_indent(cg);
                fprintf(cg->out, "for (size_t __i_%d = 0; __i_%d < __iter_%d.len; __i_%d++) {\n",
                    tmp, tmp, tmp, tmp);
                cg->indent++;
                cg_indent(cg);
                if (stmt->for_stmt.var_type) {
                    char vname[256];
                    snprintf(vname, sizeof(vname), "%.*s",
                        (int)stmt->for_stmt.var_name.len, stmt->for_stmt.var_name.data);
                    cg_emit_type_expr(cg, stmt->for_stmt.var_type, vname);
                } else {
                    fprintf(cg->out, "/* elem */ int %.*s",
                        (int)stmt->for_stmt.var_name.len, stmt->for_stmt.var_name.data);
                }
                fprintf(cg->out, " = ((/* elem type */ void*)__iter_%d.ptr)[__i_%d];\n", tmp, tmp);
                for (size_t i = 0; i < stmt->for_stmt.body->block.stmts.count; i++) {
                    cg_emit_stmt(cg, stmt->for_stmt.body->block.stmts.items[i]);
                }
                if (stmt->for_stmt.body->block.trailing_expr) {
                    cg_indent(cg);
                    cg_emit_expr(cg, stmt->for_stmt.body->block.trailing_expr);
                    fprintf(cg->out, ";\n");
                }
                cg->indent--;
                cg_indent(cg);
                fprintf(cg->out, "}\n");
                cg->indent--;
                cg_indent(cg);
                fprintf(cg->out, "}\n");
                break;
            }
            cg->loop_depth++;
            cg->loop_defer_base[cg->loop_depth] = cg->defer_depth;
            cg_emit_block(cg, stmt->for_stmt.body, false);
            cg->loop_depth--;
            fprintf(cg->out, "\n");
            break;
        }
        case AST_DEFER_STMT: {
            DeferLevel *lvl = &cg->defer_stack[cg->defer_depth];
            if (lvl->count < MAX_DEFERS_PER_BLOCK) {
                lvl->exprs[lvl->count++] = stmt->defer_stmt.expr;
            }
            break;
        }
        case AST_ERRDEFER_STMT: {
            cg_indent(cg);
            fprintf(cg->out, "/* TODO: errdefer */\n");
            break;
        }
        case AST_BREAK_STMT: {
            cg_indent(cg);
            if (cg->loop_depth > 0) {
                cg_emit_defers(cg, cg->loop_defer_base[cg->loop_depth] + 1);
            }
            fprintf(cg->out, "break;\n");
            break;
        }
        case AST_CONTINUE_STMT: {
            cg_indent(cg);
            if (cg->loop_depth > 0) {
                cg_emit_defers(cg, cg->loop_defer_base[cg->loop_depth] + 1);
            }
            fprintf(cg->out, "continue;\n");
            break;
        }
        case AST_MATCH_STMT: {
            cg_indent(cg);
            fprintf(cg->out, "/* TODO: match */\n");
            break;
        }
        case AST_ASSIGN_STMT: {
            cg_indent(cg);
            cg_emit_expr(cg, stmt->assign_stmt.lhs);
            fprintf(cg->out, " %s ", cg_assign_op(stmt->assign_stmt.op));
            cg_emit_expr(cg, stmt->assign_stmt.rhs);
            fprintf(cg->out, ";\n");
            break;
        }
        default:
            cg_indent(cg);
            cg_emit_expr(cg, stmt);
            fprintf(cg->out, ";\n");
            break;
    }
}

/* ========================================================================
 *  Expression emission
 * ======================================================================== */
static StringView cg_typeexpr_name(TypeExpr *te) {
    while (te && (te->kind == TYPE_REFERENCE || te->kind == TYPE_POINTER || te->kind == TYPE_OPTIONAL)) {
        te = te->unary.child;
    }
    if (te && te->kind == TYPE_NAMED) return te->named.name;
    return sv_from_cstr("");
}

static StringView cg_type_name(SType *type) {
    if (!type) return sv_from_cstr("");
    switch (type->kind) {
        case ST_STRUCT: return type->as.struct_.name;
        case ST_UNION: return type->as.union_.name;
        case ST_ENUM: return type->as.enum_.name;
        case ST_POINTER: return cg_type_name(type->as.pointer.base);
        case ST_REFERENCE: return cg_type_name(type->as.reference.base);
        default: return sv_from_cstr("");
    }
}


static bool cg_is_pointer_type(AstNode *obj) {
    if (!obj || !obj->sema_type) return false;
    SType *t = obj->sema_type;
    return t->kind == ST_POINTER || t->kind == ST_REFERENCE;
}

static void cg_emit_expr(CGen *cg, AstNode *expr) {
    if (!expr) return;
    switch (expr->kind) {
        case AST_INT_LITERAL:
            fprintf(cg->out, "%ld", (long)expr->int_literal.value);
            break;
        case AST_FLOAT_LITERAL:
            fprintf(cg->out, "%g", expr->float_literal.value);
            break;
        case AST_STRING_LITERAL: {
            StringView s = expr->string_literal.value;
            fprintf(cg->out, "(raya_Str){ .ptr = (uint8_t const*)\"");
            for (size_t i = 0; i < s.len; i++) {
                char c = s.data[i];
                if (c == '\\' || c == '"') fprintf(cg->out, "\\%c", c);
                else if (c == '\n') fprintf(cg->out, "\\n");
                else if (c == '\t') fprintf(cg->out, "\\t");
                else if (c == '\0') fprintf(cg->out, "\\0");
                else fprintf(cg->out, "%c", c);
            }
            fprintf(cg->out, "\", .len = %zu }", s.len);
            break;
        }
        case AST_CHAR_LITERAL: {
            StringView s = expr->char_literal.value;
            unsigned char c = s.len > 0 ? (unsigned char)s.data[0] : 0;
            fprintf(cg->out, "%u", (unsigned)c);
            break;
        }
        case AST_BOOL_LITERAL:
            fprintf(cg->out, "%s", expr->bool_literal.value ? "true" : "false");
            break;
        case AST_NULL_LITERAL: {
            if (expr->sema_type && (expr->sema_type->kind == ST_POINTER ||
                                    expr->sema_type->kind == ST_REFERENCE ||
                                    expr->sema_type->kind == ST_SLICE)) {
                fprintf(cg->out, "((void*)0)");
            } else {
                fprintf(cg->out, "0");
            }
            break;
        }
        case AST_UNDEFINED_LITERAL:
            fprintf(cg->out, "0");
            break;
        case AST_IDENTIFIER:
            fprintf(cg->out, "%.*s", (int)expr->identifier.name.len, expr->identifier.name.data);
            break;
        case AST_BINARY_EXPR: {
            fprintf(cg->out, "(");
            cg_emit_expr(cg, expr->binary_expr.left);
            fprintf(cg->out, " %s ", cg_binary_op(expr->binary_expr.op));
            cg_emit_expr(cg, expr->binary_expr.right);
            fprintf(cg->out, ")");
            break;
        }
        case AST_UNARY_EXPR: {
            fprintf(cg->out, "(%s", cg_unary_op(expr->unary_expr.op));
            cg_emit_expr(cg, expr->unary_expr.operand);
            fprintf(cg->out, ")");
            break;
        }
        case AST_CALL_EXPR: {
            cg_emit_expr(cg, expr->call_expr.callee);
            fprintf(cg->out, "(");
            for (size_t i = 0; i < expr->call_expr.args.count; i++) {
                if (i > 0) fprintf(cg->out, ", ");
                cg_emit_expr(cg, expr->call_expr.args.items[i]);
            }
            fprintf(cg->out, ")");
            break;
        }
        case AST_METHOD_CALL_EXPR: {
          SType *recv_type = expr->method_call_expr.receiver->sema_type;
            StringView type_name = cg_type_name(recv_type);
            fprintf(cg->out, "raya_%.*s_%.*s(",
                (int)type_name.len, type_name.data,
                (int)expr->method_call_expr.method_name.len,
                expr->method_call_expr.method_name.data);

            AstNode *method = cg_find_method(cg, type_name, expr->method_call_expr.method_name);
            bool needs_ref = false;
            if (method && method->fn_decl.params.count > 0) {
                AstNode *first = method->fn_decl.params.items[0];
                if (first->kind == AST_PARAM_DECL && first->param_decl.type) {
                    TypeExpr *te = first->param_decl.type;
                    needs_ref = (te->kind == TYPE_REFERENCE || te->kind == TYPE_POINTER);
                }
            }

            bool receiver_is_ptr = cg_is_pointer_type(expr->method_call_expr.receiver);
            if (needs_ref && !receiver_is_ptr) {
                fprintf(cg->out, "&");
            }
            cg_emit_expr(cg, expr->method_call_expr.receiver);
            for (size_t i = 0; i < expr->method_call_expr.args.count; i++) {
                fprintf(cg->out, ", ");
                cg_emit_expr(cg, expr->method_call_expr.args.items[i]);
            }
            fprintf(cg->out, ")");
            break;
        }
        case AST_FIELD_ACCESS_EXPR: {
            cg_emit_expr(cg, expr->field_access_expr.object);
            if (cg_is_pointer_type(expr->field_access_expr.object)) {
                fprintf(cg->out, "->%.*s", (int)expr->field_access_expr.field_name.len,
                    expr->field_access_expr.field_name.data);
            } else {
                fprintf(cg->out, ".%.*s", (int)expr->field_access_expr.field_name.len,
                    expr->field_access_expr.field_name.data);
            }
            break;
        }
        case AST_INDEX_EXPR: {
            SType *obj_type = expr->index_expr.object->sema_type;
            if (obj_type && obj_type->kind == ST_SLICE) {
                fprintf(cg->out, "((");
                cg_emit_sema_type(cg, obj_type->as.slice.base, "");
                fprintf(cg->out, "*)");
                cg_emit_expr(cg, expr->index_expr.object);
                fprintf(cg->out, ".ptr)[");
                cg_emit_expr(cg, expr->index_expr.index);
                fprintf(cg->out, "]");
            } else {
                cg_emit_expr(cg, expr->index_expr.object);
                fprintf(cg->out, "[");
                cg_emit_expr(cg, expr->index_expr.index);
                fprintf(cg->out, "]");
            }
            break;
        }
        case AST_SLICE_EXPR: {
            if (expr->slice_expr.object) {
                fprintf(cg->out, "(raya_Slice){ .ptr = ");
                if (expr->slice_expr.start) {
                    fprintf(cg->out, "&(");
                    cg_emit_expr(cg, expr->slice_expr.object);
                    fprintf(cg->out, ")[");
                    cg_emit_expr(cg, expr->slice_expr.start);
                    fprintf(cg->out, "]");
                } else {
                    cg_emit_expr(cg, expr->slice_expr.object);
                }
                fprintf(cg->out, ", .len = ");
                if (expr->slice_expr.end && expr->slice_expr.start) {
                    fprintf(cg->out, "(");
                    cg_emit_expr(cg, expr->slice_expr.end);
                    fprintf(cg->out, " - ");
                    cg_emit_expr(cg, expr->slice_expr.start);
                    fprintf(cg->out, ")");
                } else if (expr->slice_expr.end) {
                    cg_emit_expr(cg, expr->slice_expr.end);
                } else {
                    fprintf(cg->out, "0");
                }
                fprintf(cg->out, " }");
            } else {
                fprintf(cg->out, "/* range */ 0");
            }
            break;
        }
        case AST_CAST_EXPR: {
            fprintf(cg->out, "(");
            cg_emit_type_expr(cg, expr->cast_expr.type, "");
            fprintf(cg->out, ")(");
            cg_emit_expr(cg, expr->cast_expr.expr);
            fprintf(cg->out, ")");
            break;
        }
        case AST_TRY_EXPR: {
            cg_emit_expr(cg, expr->try_expr.expr);
            break;
        }
        case AST_ERROR_CAPTURE_EXPR: {
            cg_emit_expr(cg, expr->error_capture_expr.expr);
            break;
        }
        case AST_UNSAFE_BLOCK_EXPR: {
            cg_emit_block(cg, expr->unsafe_block_expr.body, false);
            break;
        }
        case AST_ARRAY_LITERAL: {
            fprintf(cg->out, "{");
            for (size_t i = 0; i < expr->array_literal.elements.count; i++) {
                if (i > 0) fprintf(cg->out, ", ");
                cg_emit_expr(cg, expr->array_literal.elements.items[i]);
            }
            fprintf(cg->out, "}");
            break;
        }
        case AST_STRUCT_LITERAL: {
            StringView type_name = cg_typeexpr_name(expr->struct_literal.type);
            fprintf(cg->out, "(struct raya_%.*s){",
                (int)type_name.len, type_name.data);
            for (size_t i = 0; i < expr->struct_literal.fields.count; i++) {
                if (i > 0) fprintf(cg->out, ", ");
                AstNode *field = expr->struct_literal.fields.items[i];
                if (field->kind == AST_ASSIGN_STMT) {
                    fprintf(cg->out, ".%.*s = ",
                        (int)field->assign_stmt.lhs->identifier.name.len,
                        field->assign_stmt.lhs->identifier.name.data);
                    cg_emit_expr(cg, field->assign_stmt.rhs);
                } else {
                    fprintf(cg->out, "/* field */ 0");
                }
            }
            fprintf(cg->out, "}");
            break;
        }
        case AST_MATCH_ARM:
            fprintf(cg->out, "/* match arm */ 0");
            break;
        default:
            fprintf(cg->out, "/* unhandled expr kind %d */ 0", expr->kind);
            break;
    }
}

/* ========================================================================
 *  Top-level helpers
 * ======================================================================== */
static StringView cg_fn_receiver_type(AstNode *fn) {
    if (fn->fn_decl.params.count == 0) return sv_from_cstr("");
    AstNode *first = fn->fn_decl.params.items[0];
    if (first->kind == AST_PARAM_DECL && first->param_decl.is_self) {
        return cg_typeexpr_name(first->param_decl.type);
    }
    return sv_from_cstr("");
}

static void cg_emit_struct(CGen *cg, AstNode *s) {
    fprintf(cg->out, "typedef struct raya_%.*s {\n",
        (int)s->struct_decl.name.len, s->struct_decl.name.data);
    cg->indent++;
    for (size_t i = 0; i < s->struct_decl.fields.count; i++) {
        AstNode *field = s->struct_decl.fields.items[i];
        cg_indent(cg);
        char name_buf[256];
        snprintf(name_buf, sizeof(name_buf), "%.*s",
            (int)field->field_decl.name.len, field->field_decl.name.data);
        cg_emit_type_expr(cg, field->field_decl.type, name_buf);
        fprintf(cg->out, ";\n");
    }
    cg->indent--;
    fprintf(cg->out, "} raya_%.*s;\n\n",
        (int)s->struct_decl.name.len, s->struct_decl.name.data);
}

static void cg_emit_union(CGen *cg, AstNode *u) {
    fprintf(cg->out, "typedef union raya_%.*s {\n",
        (int)u->union_decl.name.len, u->union_decl.name.data);
    cg->indent++;
    for (size_t i = 0; i < u->union_decl.fields.count; i++) {
        AstNode *field = u->union_decl.fields.items[i];
        cg_indent(cg);
        char name_buf[256];
        snprintf(name_buf, sizeof(name_buf), "%.*s",
            (int)field->field_decl.name.len, field->field_decl.name.data);
        cg_emit_type_expr(cg, field->field_decl.type, name_buf);
        fprintf(cg->out, ";\n");
    }
    cg->indent--;
    fprintf(cg->out, "} raya_%.*s;\n\n",
        (int)u->union_decl.name.len, u->union_decl.name.data);
}

static void cg_emit_enum(CGen *cg, AstNode *e) {
    fprintf(cg->out, "typedef enum raya_%.*s {\n",
        (int)e->enum_decl.name.len, e->enum_decl.name.data);
    cg->indent++;
    for (size_t i = 0; i < e->enum_decl.variants.count; i++) {
        AstNode *variant = e->enum_decl.variants.items[i];
        cg_indent(cg);
        fprintf(cg->out, "raya_%.*s_%.*s",
            (int)e->enum_decl.name.len, e->enum_decl.name.data,
            (int)variant->variant_decl.name.len, variant->variant_decl.name.data);
        if (i + 1 < e->enum_decl.variants.count) fprintf(cg->out, ",");
        fprintf(cg->out, "\n");
    }
    cg->indent--;
    fprintf(cg->out, "} raya_%.*s;\n\n",
        (int)e->enum_decl.name.len, e->enum_decl.name.data);
}

static void cg_emit_fn_forward(CGen *cg, AstNode *fn, StringView method_of) {
    if (fn->fn_decl.is_extern) {
        fprintf(cg->out, "extern ");
    }
    if (fn->fn_decl.ret_type) {
        cg_emit_type_expr(cg, fn->fn_decl.ret_type, "");
    } else {
        fprintf(cg->out, "void");
    }
    StringView receiver_type = method_of.len > 0 ? method_of : cg_fn_receiver_type(fn);
    if (receiver_type.len > 0) {
        fprintf(cg->out, " raya_%.*s_%.*s",
            (int)receiver_type.len, receiver_type.data,
            (int)fn->fn_decl.name.len, fn->fn_decl.name.data);
    } else {
        fprintf(cg->out, " %.*s", (int)fn->fn_decl.name.len, fn->fn_decl.name.data);
    }
    fprintf(cg->out, "(");
    for (size_t i = 0; i < fn->fn_decl.params.count; i++) {
        if (i > 0) fprintf(cg->out, ", ");
        AstNode *param = fn->fn_decl.params.items[i];
        char name_buf[256];
        snprintf(name_buf, sizeof(name_buf), "%.*s",
            (int)param->param_decl.name.len, param->param_decl.name.data);
        cg_emit_type_expr(cg, param->param_decl.type, name_buf);
    }
    fprintf(cg->out, ");\n");
}

static void cg_emit_fn_def(CGen *cg, AstNode *fn, StringView method_of) {
    if (fn->fn_decl.is_extern) return;
    if (fn->fn_decl.ret_type) {
        cg_emit_type_expr(cg, fn->fn_decl.ret_type, "");
    } else {
        fprintf(cg->out, "void");
    }
    StringView receiver_type = method_of.len > 0 ? method_of : cg_fn_receiver_type(fn);
    if (receiver_type.len > 0) {
        fprintf(cg->out, " raya_%.*s_%.*s",
            (int)receiver_type.len, receiver_type.data,
            (int)fn->fn_decl.name.len, fn->fn_decl.name.data);
    } else {
        fprintf(cg->out, " %.*s", (int)fn->fn_decl.name.len, fn->fn_decl.name.data);
    }
    fprintf(cg->out, "(");
    for (size_t i = 0; i < fn->fn_decl.params.count; i++) {
        if (i > 0) fprintf(cg->out, ", ");
        AstNode *param = fn->fn_decl.params.items[i];
        char name_buf[256];
        snprintf(name_buf, sizeof(name_buf), "%.*s",
            (int)param->param_decl.name.len, param->param_decl.name.data);
        cg_emit_type_expr(cg, param->param_decl.type, name_buf);
    }
    fprintf(cg->out, ") ");
    if (fn->fn_decl.body) {
        cg_emit_block(cg, fn->fn_decl.body, true);
        fprintf(cg->out, "\n\n");
    } else {
        fprintf(cg->out, ";\n\n");
    }
}

static void cg_emit_compilation_unit(CGen *cg, AstNode *unit) {
    fprintf(cg->out, "/* Generated by rayac C transpile backend v0.1 */\n");
    fprintf(cg->out, "#include <stdint.h>\n");
    fprintf(cg->out, "#include <stddef.h>\n");
    fprintf(cg->out, "#include <stdbool.h>\n");
    fprintf(cg->out, "#include <string.h>\n");
    fprintf(cg->out, "#include \"raya_rt.h\"\n\n");
    fprintf(cg->out, "typedef struct { uint8_t const* ptr; size_t len; } raya_Str;\n");
    fprintf(cg->out, "typedef struct { void* ptr; size_t len; } raya_Slice;\n\n");

    /* Forward declarations of types */
    for (size_t i = 0; i < unit->compilation_unit.decls.count; i++) {
        AstNode *decl = unit->compilation_unit.decls.items[i];
        if (decl->kind == AST_STRUCT_DECL) {
            fprintf(cg->out, "struct raya_%.*s;\n",
                (int)decl->struct_decl.name.len, decl->struct_decl.name.data);
        } else if (decl->kind == AST_UNION_DECL) {
            fprintf(cg->out, "union raya_%.*s;\n",
                (int)decl->union_decl.name.len, decl->union_decl.name.data);
        } else if (decl->kind == AST_ENUM_DECL) {
            fprintf(cg->out, "enum raya_%.*s;\n",
                (int)decl->enum_decl.name.len, decl->enum_decl.name.data);
        }
    }
    fprintf(cg->out, "\n");

    /* Type definitions */
    for (size_t i = 0; i < unit->compilation_unit.decls.count; i++) {
        AstNode *decl = unit->compilation_unit.decls.items[i];
        if (decl->kind == AST_STRUCT_DECL) cg_emit_struct(cg, decl);
        else if (decl->kind == AST_UNION_DECL) cg_emit_union(cg, decl);
        else if (decl->kind == AST_ENUM_DECL) cg_emit_enum(cg, decl);
    }

    /* Forward declarations of functions */
    for (size_t i = 0; i < unit->compilation_unit.decls.count; i++) {
        AstNode *decl = unit->compilation_unit.decls.items[i];
        if (decl->kind == AST_FN_DECL) {
            cg_emit_fn_forward(cg, decl, sv_from_cstr(""));
        } else if (decl->kind == AST_EXTEND_DECL) {
            for (size_t j = 0; j < decl->extend_decl.methods.count; j++) {
                cg_emit_fn_forward(cg, decl->extend_decl.methods.items[j], decl->extend_decl.target_name);
            }
        }
    }
    fprintf(cg->out, "\n");

    /* Function definitions */
    for (size_t i = 0; i < unit->compilation_unit.decls.count; i++) {
        AstNode *decl = unit->compilation_unit.decls.items[i];
        if (decl->kind == AST_FN_DECL) {
            cg_emit_fn_def(cg, decl, sv_from_cstr(""));
        } else if (decl->kind == AST_EXTEND_DECL) {
            for (size_t j = 0; j < decl->extend_decl.methods.count; j++) {
                cg_emit_fn_def(cg, decl->extend_decl.methods.items[j], decl->extend_decl.target_name);
            }
        }
    }
}

/* ========================================================================
 *  Public API
 * ======================================================================== */
void codegen_c_emit(AstNode *module, FILE *out) {
    CGen cg = {0};
    cg.out = out;
    cg.indent = 0;
    cg.temp_counter = 0;
    cg.in_unsafe = false;
    cg.defer_depth = -1;
    cg.loop_depth = 0;
    cg.unit = module;
    cg_emit_compilation_unit(&cg, module);
}
