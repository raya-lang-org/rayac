#include "sema.h"
#include "ast.h"
#include "string_view.h"
#include <stdio.h>
#include <stdarg.h>

Sema *sema_new(Arena *arena) {
    Sema *s = arena_alloc(arena, sizeof(Sema));
    s->arena = arena;
    s->types = type_table_new(arena);
    s->module_scope = scope_new(arena, NULL, NULL);
    s->current_scope = s->module_scope;
    s->current_self_type = NULL;
    s->current_fn = NULL;
    s->error_count = 0;
    return s;
}

void sema_report(Sema *s, SourceLocation loc, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "%s:%zu:%zu: error: ", loc.file, loc.line, loc.col);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    s->error_count++;
}

void sema_run(Sema *s, AstNode *module) {
    sema_collect_decls(s, module);
    if (s->error_count > 0) return;
    sema_resolve_bodies(s, module);
}

static SType *sema_make_fn_type(Sema *s, AstNode *fn) {
    size_t pc = fn->as.fn_decl.params.count;
    if (fn->as.fn_decl.has_self) pc++;
    SType **params = arena_alloc(s->arena, pc * sizeof(SType*));
    size_t idx = 0;
    if (fn->as.fn_decl.has_self) {
        params[idx++] = s->current_self_type ? s->current_self_type : st_void(s->types);
    }
    for (size_t i = 0; i < fn->as.fn_decl.params.count; i++) {
        params[idx++] = sema_resolve_type(s, fn->as.fn_decl.params.nodes[i]->as.param.type_expr);
    }
    SType *ret = fn->as.fn_decl.ret_type ? sema_resolve_type(s, fn->as.fn_decl.ret_type) : st_void(s->types);
    return st_function(s->types, params, pc, ret, false);
}

void sema_collect_decls(Sema *s, AstNode *module) {
    if (module->kind != AST_MODULE) return;
    for (size_t i = 0; i < module->as.module.decls.count; i++) {
        AstNode *decl = module->as.module.decls.nodes[i];
        StringView name = {0};
        SymbolKind kind = SYM_POISON;
        SType *type = NULL;
        switch (decl->kind) {
            case AST_FN_DECL:
                name = decl->as.fn_decl.name;
                kind = SYM_FUNCTION;
                type = sema_make_fn_type(s, decl);
                break;
            case AST_STRUCT_DECL:
                name = decl->as.struct_decl.name;
                kind = SYM_STRUCT;
                type = st_void(s->types);
                break;
            case AST_UNION_DECL:
                name = decl->as.union_decl.name;
                kind = SYM_UNION;
                type = st_void(s->types);
                break;
            case AST_ENUM_DECL:
                name = decl->as.enum_decl.name;
                kind = SYM_ENUM;
                type = st_void(s->types);
                break;
            case AST_TRAIT_DECL:
                name = decl->as.trait_decl.name;
                kind = SYM_TRAIT;
                type = st_void(s->types);
                break;
            case AST_TYPE_ALIAS:
                name = decl->as.type_alias.name;
                kind = SYM_TYPE_ALIAS;
                type = NULL;
                break;
            case AST_CONST_DECL:
                name = decl->as.var_decl.name;
                kind = SYM_CONST;
                type = NULL;
                break;
            case AST_VAR_DECL:
                name = decl->as.var_decl.name;
                kind = SYM_VAR;
                type = NULL;
                break;
            default:
                continue;
        }
        Symbol *sym = symbol_new(s->arena, kind, name, type, decl);
        sym->is_pub = decl->has_pub;
        sym->is_comptime = decl->has_comptime;
        if (scope_lookup_current(s->module_scope, name)) {
            sema_report(s, decl->loc, "redefinition of '%.*s'", SV_ARG(name));
        } else {
            scope_insert(s->arena, s->module_scope, sym);
        }
    }
}

static void sema_check_fn_body(Sema *s, AstNode *fn);

void sema_resolve_bodies(Sema *s, AstNode *module) {
    if (module->kind != AST_MODULE) return;
    for (size_t i = 0; i < module->as.module.decls.count; i++) {
        AstNode *decl = module->as.module.decls.nodes[i];
        if (decl->kind == AST_FN_DECL) sema_check_fn_body(s, decl);
    }
}

static void sema_check_fn_body(Sema *s, AstNode *fn) {
    Scope *fn_scope = scope_new(s->arena, s->module_scope, fn);
    Scope *prev_scope = s->current_scope;
    AstNode *prev_fn = s->current_fn;
    SType *prev_self = s->current_self_type;
    s->current_scope = fn_scope;
    s->current_fn = fn;

    for (size_t i = 0; i < fn->as.fn_decl.generic_params.count; i++) {
        AstNode *gp = fn->as.fn_decl.generic_params.nodes[i];
        SType *t = st_void(s->types);
        Symbol *sym = symbol_new(s->arena, SYM_GENERIC_PARAM, gp->as.generic_param.name, t, gp);
        scope_insert(s->arena, fn_scope, sym);
    }

    for (size_t i = 0; i < fn->as.fn_decl.params.count; i++) {
        AstNode *param = fn->as.fn_decl.params.nodes[i];
        SType *pt = sema_resolve_type(s, param->as.param.type_expr);
        Symbol *sym = symbol_new(s->arena, SYM_VAR, param->as.param.name, pt, param);
        scope_insert(s->arena, fn_scope, sym);
    }

    SType *body_type = sema_check_block(s, fn->as.fn_decl.body);

    SType *ret = fn->as.fn_decl.ret_type ? sema_resolve_type(s, fn->as.fn_decl.ret_type) : st_void(s->types);

    bool last_is_return = false;
    if (fn->as.fn_decl.body && fn->as.fn_decl.body->kind == AST_BLOCK) {
        AstNode *block = fn->as.fn_decl.body;
        if (block->as.block.stmts.count > 0) {
            AstNode *last = block->as.block.stmts.nodes[block->as.block.stmts.count - 1];
            last_is_return = (last->kind == AST_RETURN);
        }
    }
    if (!last_is_return && !st_eq(ret, body_type) && body_type->kind != ST_NORETURN) {
        if (!st_can_coerce(body_type, ret)) {
            sema_report(s, fn->as.fn_decl.body->loc,
                "expected return type '%s', found '%s'",
                st_name(ret), st_name(body_type));
        }
    }

    s->current_scope = prev_scope;
    s->current_fn = prev_fn;
    s->current_self_type = prev_self;
}

SType *sema_resolve_type(Sema *s, AstNode *type_expr) {
    if (!type_expr) return st_void(s->types);
    return st_from_ast(s->types, type_expr);
}

SType *sema_check_block(Sema *s, AstNode *block) {
    if (!block) return st_void(s->types);
    Scope *block_scope = scope_new(s->arena, s->current_scope, block);
    Scope *prev = s->current_scope;
    s->current_scope = block_scope;
    SType *last = st_void(s->types);
    for (size_t i = 0; i < block->as.block.stmts.count; i++) {
        AstNode *stmt = block->as.block.stmts.nodes[i];
        last = sema_check_stmt(s, stmt);
    }
    s->current_scope = prev;
    return last;
}

SType *sema_check_stmt(Sema *s, AstNode *stmt) {
    if (!stmt) return st_void(s->types);
    switch (stmt->kind) {
        case AST_VAR_DECL:
        case AST_CONST_DECL: {
            SType *init_type = st_void(s->types);
            if (stmt->as.var_decl.init) {
                init_type = sema_check_expr(s, stmt->as.var_decl.init);
            }
            SType *decl_type = NULL;
            if (stmt->as.var_decl.type_expr) {
                decl_type = sema_resolve_type(s, stmt->as.var_decl.type_expr);
                if (stmt->as.var_decl.init && !st_can_coerce(init_type, decl_type)) {
                    sema_report(s, stmt->loc, "cannot initialize variable of type '%s' with '%s'",
                        st_name(decl_type), st_name(init_type));
                }
            } else {
                decl_type = init_type;
            }
            Symbol *sym = symbol_new(s->arena,
                stmt->kind == AST_CONST_DECL ? SYM_CONST : SYM_VAR,
                stmt->as.var_decl.name, decl_type, stmt);
            scope_insert(s->arena, s->current_scope, sym);
            return st_void(s->types);
        }
        case AST_RETURN: {
            SType *ret = st_void(s->types);
            if (stmt->as.return_stmt.expr) {
                ret = sema_check_expr(s, stmt->as.return_stmt.expr);
            }
            SType *expected = s->current_fn && s->current_fn->as.fn_decl.ret_type
                ? sema_resolve_type(s, s->current_fn->as.fn_decl.ret_type)
                : st_void(s->types);
            if (!st_can_coerce(ret, expected)) {
                sema_report(s, stmt->loc, "return type mismatch: expected '%s', found '%s'",
                    st_name(expected), st_name(ret));
            }
            return st_void(s->types);
        }
        case AST_EXPR_STMT:
            sema_check_expr(s, stmt->as.expr_stmt.expr);
            return st_void(s->types);
        case AST_BLOCK:
            return sema_check_block(s, stmt);
        case AST_IF: {
            SType *cond = sema_check_expr(s, stmt->as.if_stmt.cond);
            if (cond->kind != ST_BOOL) {
                sema_report(s, stmt->as.if_stmt.cond->loc, "if condition must be boolean");
            }
            sema_check_block(s, stmt->as.if_stmt.then_branch);
            if (stmt->as.if_stmt.else_branch) {
                sema_check_block(s, stmt->as.if_stmt.else_branch);
            }
            return st_void(s->types);
        }
        case AST_WHILE: {
            SType *cond = sema_check_expr(s, stmt->as.while_stmt.cond);
            if (cond->kind != ST_BOOL) {
                sema_report(s, stmt->as.while_stmt.cond->loc, "while condition must be boolean");
            }
            sema_check_block(s, stmt->as.while_stmt.body);
            return st_void(s->types);
        }
        case AST_FOR: {
            Scope *for_scope = scope_new(s->arena, s->current_scope, stmt);
            Scope *prev = s->current_scope;
            s->current_scope = for_scope;
            SType *iter = sema_check_expr(s, stmt->as.for_stmt.iter);
            (void)iter;
            SType *var_type = stmt->as.for_stmt.type_expr
                ? sema_resolve_type(s, stmt->as.for_stmt.type_expr)
                : st_int(s->types, true, 64);
            Symbol *sym = symbol_new(s->arena, SYM_VAR, stmt->as.for_stmt.var_name, var_type, stmt);
            scope_insert(s->arena, for_scope, sym);
            sema_check_block(s, stmt->as.for_stmt.body);
            s->current_scope = prev;
            return st_void(s->types);
        }
        case AST_DEFER:
            sema_check_expr(s, stmt->as.defer_stmt.expr);
            return st_void(s->types);
        case AST_ERRDEFER:
            sema_check_block(s, stmt->as.errdefer_stmt.block);
            return st_void(s->types);
        case AST_BREAK:
        case AST_CONTINUE:
            return st_void(s->types);
        case AST_MATCH:
            sema_check_expr(s, stmt->as.match_stmt.expr);
            return st_void(s->types);
        case AST_ASSIGN: {
            SType *lhs = sema_check_expr(s, stmt->as.assign.lhs);
            SType *rhs = sema_check_expr(s, stmt->as.assign.rhs);
            if (!st_can_coerce(rhs, lhs)) {
                sema_report(s, stmt->loc, "cannot assign '%s' to '%s'",
                    st_name(rhs), st_name(lhs));
            }
            return st_void(s->types);
        }
        default:
            return sema_check_expr(s, stmt);
    }
}

SType *sema_check_expr(Sema *s, AstNode *expr) {
    if (!expr) return st_void(s->types);
    switch (expr->kind) {
        case AST_INT_LITERAL:
            expr->sema_type = st_int(s->types, true, 64);
            return expr->sema_type;
        case AST_FLOAT_LITERAL:
            expr->sema_type = st_float(s->types, 64);
            return expr->sema_type;
        case AST_STRING_LITERAL:
            expr->sema_type = st_slice(s->types, true, st_int(s->types, false, 8));
            return expr->sema_type;
        case AST_CHAR_LITERAL:
            expr->sema_type = st_int(s->types, false, 8);
            return expr->sema_type;
        case AST_BOOL_LITERAL:
            expr->sema_type = st_bool(s->types);
            return expr->sema_type;
        case AST_IDENT: {
            Symbol *sym = scope_lookup(s->current_scope, expr->as.ident.name);
            if (!sym) {
                sema_report(s, expr->loc, "use of undeclared identifier '%.*s'",
                    SV_ARG(expr->as.ident.name));
                expr->sema_type = st_void(s->types);
                expr->as.ident.sym = NULL;
                return expr->sema_type;
            }
            expr->as.ident.sym = sym;
            expr->sema_type = sym->type ? sym->type : st_void(s->types);
            return expr->sema_type;
        }
        case AST_BINARY: {
            SType *left = sema_check_expr(s, expr->as.binary.left);
            SType *right = sema_check_expr(s, expr->as.binary.right);
            switch (expr->as.binary.op) {
                case BINOP_ADD: case BINOP_SUB:
                case BINOP_MUL: case BINOP_DIV: case BINOP_MOD:
                    if (!st_is_numeric(left) || !st_is_numeric(right)) {
                        sema_report(s, expr->loc, "invalid operands to arithmetic expression");
                        expr->sema_type = st_void(s->types);
                    } else {
                        expr->sema_type = left->kind == ST_FLOAT || right->kind == ST_FLOAT
                            ? st_float(s->types, 64) : left;
                    }
                    break;
                case BINOP_EQ: case BINOP_NE:
                case BINOP_LT: case BINOP_GT:
                case BINOP_LE: case BINOP_GE:
                    expr->sema_type = st_bool(s->types);
                    break;
                case BINOP_AND: case BINOP_OR:
                    if (left->kind != ST_BOOL || right->kind != ST_BOOL) {
                        sema_report(s, expr->loc, "logical operators require boolean operands");
                    }
                    expr->sema_type = st_bool(s->types);
                    break;
                case BINOP_BITAND: case BINOP_BITOR: case BINOP_BITXOR:
                case BINOP_SHL: case BINOP_SHR:
                    if (!st_is_integer(left) || !st_is_integer(right)) {
                        sema_report(s, expr->loc, "bitwise operators require integer operands");
                    }
                    expr->sema_type = left;
                    break;
                default:
                    expr->sema_type = left;
                    break;
            }
            return expr->sema_type;
        }
        case AST_CALL: {
            SType *callee = sema_check_expr(s, expr->as.call.callee);
            if (callee->kind != ST_FUNCTION) {
                sema_report(s, expr->loc, "called object is not a function");
                expr->sema_type = st_void(s->types);
                return expr->sema_type;
            }
            expr->sema_type = callee->as.function.ret;
            return expr->sema_type;
        }
        case AST_AS_CAST: {
            SType *target = sema_resolve_type(s, expr->as.cast.type_expr);
            SType *src = sema_check_expr(s, expr->as.cast.expr);
            (void)src;
            expr->sema_type = target;
            return target;
        }
        case AST_TRY: {
            SType *inner = sema_check_expr(s, expr->as.try_expr.expr);
            if (inner->kind != ST_ERROR_UNION) {
                sema_report(s, expr->loc, "try requires error union type, got '%s'",
                    st_name(inner));
                expr->sema_type = inner;
                return inner;
            }
            expr->sema_type = inner->as.error_union.base;
            return expr->sema_type;
        }
        case AST_UNSAFE: {
            return sema_check_block(s, expr->as.unsafe_expr.body);
        }
        case AST_PREFIX: {
            SType *operand = sema_check_expr(s, expr->as.prefix.operand);
            switch (expr->as.prefix.op) {
                case PREFIX_NEG:
                case PREFIX_BITNOT:
                    if (!st_is_numeric(operand)) {
                        sema_report(s, expr->loc, "invalid operand to unary operator");
                    }
                    expr->sema_type = operand;
                    break;
                case PREFIX_NOT:
                    if (operand->kind != ST_BOOL) {
                        sema_report(s, expr->loc, "logical not requires boolean operand");
                    }
                    expr->sema_type = st_bool(s->types);
                    break;
                case PREFIX_DEREF:
                    if (operand->kind == ST_POINTER) {
                        expr->sema_type = operand->as.pointer.base;
                    } else if (operand->kind == ST_REFERENCE) {
                        expr->sema_type = operand->as.reference.base;
                    } else {
                        sema_report(s, expr->loc, "cannot dereference non-pointer type");
                        expr->sema_type = st_void(s->types);
                    }
                    break;
                case PREFIX_REF:
                case PREFIX_REFCONST:
                    expr->sema_type = st_reference(s->types,
                        expr->as.prefix.op == PREFIX_REFCONST, operand);
                    break;
                default:
                    expr->sema_type = operand;
                    break;
            }
            return expr->sema_type;
        }
        case AST_POSTFIX: {
            SType *base = sema_check_expr(s, expr->as.postfix.base);
            (void)base;
            expr->sema_type = st_void(s->types);
            return expr->sema_type;
        }
        case AST_ARRAY_LITERAL:
            expr->sema_type = st_void(s->types);
            return expr->sema_type;
        case AST_STRUCT_LITERAL:
            expr->sema_type = st_void(s->types);
            return expr->sema_type;
        default:
            expr->sema_type = st_void(s->types);
            return expr->sema_type;
    }
}
