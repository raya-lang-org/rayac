#include "sema.h"
#include "ast.h"
#include "string_view.h"
#include <stdio.h>
#include <stdarg.h>

Sema *sema_new(Arena *arena, DiagnosticEngine *diag) {
    Sema *s = arena_alloc(arena, sizeof(Sema));
    s->arena = arena;
    s->types = type_table_new(arena);
    s->module_scope = scope_new(arena, NULL, NULL);
    s->current_scope = s->module_scope;
    s->current_self_type = NULL;
    s->current_fn = NULL;
    s->diag = diag;
    s->error_count = 0;
    return s;
}

void sema_report(Sema *s, SourceLocation loc, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    diag_error(s->diag, loc, "%s", buf);
    s->error_count++;
}

void sema_run(Sema *s, AstNode *module) {
    sema_collect_decls(s, module);
    if (s->error_count > 0) return;
    sema_resolve_bodies(s, module);
}

static SType *sema_make_fn_type(Sema *s, AstNode *fn) {
    size_t pc = fn->fn_decl.params.count;
    SType **params = arena_alloc(s->arena, pc * sizeof(SType*));
    for (size_t i = 0; i < pc; i++) {
        AstNode *p = fn->fn_decl.params.items[i];
        params[i] = sema_resolve_type(s, p->param_decl.type);
    }
    SType *ret = fn->fn_decl.ret_type ? sema_resolve_type(s, fn->fn_decl.ret_type) : st_void(s->types);
    return st_function(s->types, params, pc, ret, false);
}

void sema_collect_decls(Sema *s, AstNode *module) {
    if (module->kind != AST_COMPILATION_UNIT) return;
    for (size_t i = 0; i < module->compilation_unit.decls.count; i++) {
        AstNode *decl = module->compilation_unit.decls.items[i];
        StringView name = {0};
        SymbolKind kind = SYM_POISON;
        SType *type = NULL;
        bool is_pub = false, is_comptime = false;
        switch (decl->kind) {
            case AST_FN_DECL:
                name = decl->fn_decl.name; kind = SYM_FUNCTION;
                type = sema_make_fn_type(s, decl);
                is_pub = decl->fn_decl.is_pub; is_comptime = decl->fn_decl.is_comptime;
                break;
            case AST_STRUCT_DECL:
                name = decl->struct_decl.name; kind = SYM_STRUCT; type = st_void(s->types);
                is_pub = decl->struct_decl.is_pub;
                break;
            case AST_UNION_DECL:
                name = decl->struct_decl.name; kind = SYM_UNION; type = st_void(s->types);
                is_pub = decl->struct_decl.is_pub;
                break;
            case AST_ENUM_DECL:
                name = decl->enum_decl.name; kind = SYM_ENUM; type = st_void(s->types);
                is_pub = decl->enum_decl.is_pub;
                break;
            case AST_TRAITS_DECL:
                name = decl->traits_decl.name; kind = SYM_TRAIT; type = st_void(s->types);
                is_pub = decl->traits_decl.is_pub;
                break;
            case AST_TYPE_ALIAS:
                name = decl->type_alias.name; kind = SYM_TYPE_ALIAS; type = NULL;
                is_pub = decl->type_alias.is_pub;
                break;
            case AST_CONST_DECL:
                name = decl->var_decl.name; kind = SYM_CONST; type = NULL;
                is_pub = decl->var_decl.is_pub; is_comptime = decl->var_decl.is_comptime;
                break;
            case AST_VAR_DECL:
                name = decl->var_decl.name; kind = SYM_VAR; type = NULL;
                is_pub = decl->var_decl.is_pub; is_comptime = decl->var_decl.is_comptime;
                break;
            default: continue;
        }
        Symbol *sym = symbol_new(s->arena, kind, name, type, decl);
        sym->is_pub = is_pub; sym->is_comptime = is_comptime;
        if (scope_lookup_current(s->module_scope, name))
            sema_report(s, decl->loc, "redefinition of '%.*s'", SV_ARG(name));
        else
            scope_insert(s->arena, s->module_scope, sym);
    }
}

static void sema_check_fn_body(Sema *s, AstNode *fn);

void sema_resolve_bodies(Sema *s, AstNode *module) {
    if (module->kind != AST_COMPILATION_UNIT) return;
    for (size_t i = 0; i < module->compilation_unit.decls.count; i++) {
        AstNode *d = module->compilation_unit.decls.items[i];
        if (d->kind == AST_FN_DECL) sema_check_fn_body(s, d);
    }
}

static void sema_check_fn_body(Sema *s, AstNode *fn) {
    Scope *fn_scope = scope_new(s->arena, s->module_scope, fn);
    Scope *prev_scope = s->current_scope;
    AstNode *prev_fn = s->current_fn;
    SType *prev_self = s->current_self_type;
    s->current_scope = fn_scope;
    s->current_fn = fn;

    for (size_t i = 0; i < fn->fn_decl.generic_params.count; i++) {
        AstNode *gp = fn->fn_decl.generic_params.items[i];
        Symbol *sym = symbol_new(s->arena, SYM_GENERIC_PARAM, gp->generic_param_decl.name, st_void(s->types), gp);
        scope_insert(s->arena, fn_scope, sym);
    }

    for (size_t i = 0; i < fn->fn_decl.params.count; i++) {
        AstNode *p = fn->fn_decl.params.items[i];
        if (p->param_decl.is_self) {
            SType *self_type = p->param_decl.type ? sema_resolve_type(s, p->param_decl.type) : (s->current_self_type ? s->current_self_type : st_void(s->types));
            s->current_self_type = self_type;
            Symbol *sym = symbol_new(s->arena, SYM_VAR, sv_from_cstr("self"), self_type, p);
            scope_insert(s->arena, fn_scope, sym);
        } else {
            SType *pt = sema_resolve_type(s, p->param_decl.type);
            Symbol *sym = symbol_new(s->arena, SYM_VAR, p->param_decl.name, pt, p);
            scope_insert(s->arena, fn_scope, sym);
        }
    }

    SType *body_type = sema_check_block(s, fn->fn_decl.body);
    SType *ret = fn->fn_decl.ret_type ? sema_resolve_type(s, fn->fn_decl.ret_type) : st_void(s->types);

    bool last_is_return = false;
    if (fn->fn_decl.body && fn->fn_decl.body->kind == AST_BLOCK && fn->fn_decl.body->block.stmts.count > 0) {
        AstNode *last = fn->fn_decl.body->block.stmts.items[fn->fn_decl.body->block.stmts.count - 1];
        last_is_return = (last->kind == AST_RETURN_STMT);
    }
    if (!last_is_return && !st_eq(ret, body_type) && body_type->kind != ST_NORETURN) {
        if (!st_can_coerce(body_type, ret))
            sema_report(s, fn->fn_decl.body->loc, "expected return type '%s', found '%s'", st_name(ret), st_name(body_type));
    }

    s->current_scope = prev_scope;
    s->current_fn = prev_fn;
    s->current_self_type = prev_self;
}

SType *sema_resolve_type(Sema *s, TypeExpr *type_expr) {
    if (!type_expr) return st_void(s->types);
    return st_from_ast(s->types, type_expr);
}

SType *sema_check_block(Sema *s, AstNode *block) {
    if (!block) return st_void(s->types);
    Scope *bs = scope_new(s->arena, s->current_scope, block);
    Scope *prev = s->current_scope;
    s->current_scope = bs;
    SType *last = st_void(s->types);
    for (size_t i = 0; i < block->block.stmts.count; i++)
        last = sema_check_stmt(s, block->block.stmts.items[i]);
    if (block->block.trailing_expr) last = sema_check_expr(s, block->block.trailing_expr);
    s->current_scope = prev;
    return last;
}

SType *sema_check_stmt(Sema *s, AstNode *stmt) {
    if (!stmt) return st_void(s->types);
    switch (stmt->kind) {
        case AST_VAR_DECL: case AST_CONST_DECL: {
            SType *init_type = st_void(s->types);
            if (stmt->var_decl.init) init_type = sema_check_expr(s, stmt->var_decl.init);
            SType *decl_type = NULL;
            if (stmt->var_decl.type) {
                decl_type = sema_resolve_type(s, stmt->var_decl.type);
                if (stmt->var_decl.init && !st_can_coerce(init_type, decl_type))
                    sema_report(s, stmt->loc, "cannot initialize variable of type '%s' with '%s'", st_name(decl_type), st_name(init_type));
            } else {
                decl_type = init_type;
            }
            Symbol *sym = symbol_new(s->arena, stmt->kind == AST_CONST_DECL ? SYM_CONST : SYM_VAR, stmt->var_decl.name, decl_type, stmt);
            scope_insert(s->arena, s->current_scope, sym);
            return st_void(s->types);
        }
        case AST_RETURN_STMT: {
            SType *ret = st_void(s->types);
            if (stmt->return_stmt.value) ret = sema_check_expr(s, stmt->return_stmt.value);
            SType *expected = s->current_fn && s->current_fn->fn_decl.ret_type ? sema_resolve_type(s, s->current_fn->fn_decl.ret_type) : st_void(s->types);
            if (!st_can_coerce(ret, expected))
                sema_report(s, stmt->loc, "return type mismatch: expected '%s', found '%s'", st_name(expected), st_name(ret));
            return st_void(s->types);
        }
        case AST_EXPR_STMT:
            sema_check_expr(s, stmt->expr_stmt.expr);
            return st_void(s->types);
        case AST_BLOCK: return sema_check_block(s, stmt);
        case AST_IF_STMT: {
            SType *cond = sema_check_expr(s, stmt->if_stmt.condition);
            if (cond->kind != ST_BOOL) sema_report(s, stmt->if_stmt.condition->loc, "if condition must be boolean");
            sema_check_block(s, stmt->if_stmt.then_block);
            if (stmt->if_stmt.else_block) sema_check_block(s, stmt->if_stmt.else_block);
            return st_void(s->types);
        }
        case AST_WHILE_STMT: {
            SType *cond = sema_check_expr(s, stmt->while_stmt.condition);
            if (cond->kind != ST_BOOL) sema_report(s, stmt->while_stmt.condition->loc, "while condition must be boolean");
            sema_check_block(s, stmt->while_stmt.body);
            return st_void(s->types);
        }
        case AST_FOR_STMT: {
            Scope *fs = scope_new(s->arena, s->current_scope, stmt);
            Scope *prev = s->current_scope;
            s->current_scope = fs;
            sema_check_expr(s, stmt->for_stmt.iterable);
            SType *vt = stmt->for_stmt.var_type ? sema_resolve_type(s, stmt->for_stmt.var_type) : st_int(s->types, true, 64);
            Symbol *sym = symbol_new(s->arena, SYM_VAR, stmt->for_stmt.var_name, vt, stmt);
            scope_insert(s->arena, fs, sym);
            sema_check_block(s, stmt->for_stmt.body);
            s->current_scope = prev;
            return st_void(s->types);
        }
        case AST_DEFER_STMT:
            sema_check_expr(s, stmt->defer_stmt.expr);
            return st_void(s->types);
        case AST_ERRDEFER_STMT:
            sema_check_block(s, stmt->errdefer_stmt.body);
            return st_void(s->types);
        case AST_BREAK_STMT: case AST_CONTINUE_STMT: return st_void(s->types);
        case AST_MATCH_STMT:
            sema_check_expr(s, stmt->match_stmt.expr);
            return st_void(s->types);
        case AST_ASSIGN_STMT: {
            SType *lhs = sema_check_expr(s, stmt->assign_stmt.lhs);
            SType *rhs = sema_check_expr(s, stmt->assign_stmt.rhs);
            if (!st_can_coerce(rhs, lhs))
                sema_report(s, stmt->loc, "cannot assign '%s' to '%s'", st_name(rhs), st_name(lhs));
            return st_void(s->types);
        }
        default: return sema_check_expr(s, stmt);
    }
}

SType *sema_check_expr(Sema *s, AstNode *expr) {
    if (!expr) return st_void(s->types);
    switch (expr->kind) {
        case AST_INT_LITERAL: return st_int(s->types, true, 64);
        case AST_FLOAT_LITERAL: return st_float(s->types, 64);
        case AST_STRING_LITERAL: return st_slice(s->types, true, st_int(s->types, false, 8));
        case AST_CHAR_LITERAL: return st_int(s->types, false, 8);
        case AST_BOOL_LITERAL: return st_bool(s->types);
        case AST_IDENTIFIER: {
            Symbol *sym = scope_lookup(s->current_scope, expr->identifier.name);
            if (!sym) {
                sema_report(s, expr->loc, "use of undeclared identifier '%.*s'", SV_ARG(expr->identifier.name));
                return st_void(s->types);
            }
            return sym->type ? sym->type : st_void(s->types);
        }
        case AST_BINARY_EXPR: {
            SType *l = sema_check_expr(s, expr->binary_expr.left);
            SType *r = sema_check_expr(s, expr->binary_expr.right);
            TokenKind op = expr->binary_expr.op;
            switch (op) {
                case TOK_PLUS: case TOK_MINUS: case TOK_STAR: case TOK_SLASH: case TOK_PERCENT:
                    if (!st_is_numeric(l) || !st_is_numeric(r)) {
                        sema_report(s, expr->loc, "invalid operands to arithmetic expression");
                        return st_void(s->types);
                    }
                    return (l->kind == ST_FLOAT || r->kind == ST_FLOAT) ? st_float(s->types, 64) : l;
                case TOK_EQ: case TOK_NE: case TOK_LT: case TOK_GT: case TOK_LE: case TOK_GE: return st_bool(s->types);
                case TOK_AND_AND: case TOK_OR_OR:
                    if (l->kind != ST_BOOL || r->kind != ST_BOOL) sema_report(s, expr->loc, "logical operators require boolean operands");
                    return st_bool(s->types);
                case TOK_AMPERSAND: case TOK_PIPE: case TOK_CARET: case TOK_SHL: case TOK_SHR:
                    if (!st_is_integer(l) || !st_is_integer(r)) sema_report(s, expr->loc, "bitwise operators require integer operands");
                    return l;
                default: return l;
            }
        }
        case AST_UNARY_EXPR: {
            SType *o = sema_check_expr(s, expr->unary_expr.operand);
            TokenKind op = expr->unary_expr.op;
            switch (op) {
                case TOK_MINUS: case TOK_TILDE:
                    if (!st_is_numeric(o)) sema_report(s, expr->loc, "invalid operand to unary operator");
                    return o;
                case TOK_BANG:
                    if (o->kind != ST_BOOL) sema_report(s, expr->loc, "logical not requires boolean operand");
                    return st_bool(s->types);
                case TOK_STAR:
                    if (o->kind == ST_POINTER) return o->as.pointer.base;
                    if (o->kind == ST_REFERENCE) return o->as.reference.base;
                    sema_report(s, expr->loc, "cannot dereference non-pointer type");
                    return st_void(s->types);
                case TOK_AMPERSAND: return st_reference(s->types, false, o);
                default: return o;
            }
        }
        case AST_CALL_EXPR: {
            SType *c = sema_check_expr(s, expr->call_expr.callee);
            if (c->kind != ST_FUNCTION) { sema_report(s, expr->loc, "called object is not a function"); return st_void(s->types); }
            return c->as.function.ret;
        }
        case AST_CAST_EXPR: {
            SType *t = sema_resolve_type(s, expr->cast_expr.type);
            sema_check_expr(s, expr->cast_expr.expr);
            return t;
        }
        case AST_TRY_EXPR: {
            SType *inner = sema_check_expr(s, expr->try_expr.expr);
            if (inner->kind != ST_ERROR_UNION) { sema_report(s, expr->loc, "try requires error union type, got '%s'", st_name(inner)); return inner; }
            return inner->as.error_union.base;
        }
        case AST_UNSAFE_BLOCK_EXPR: return sema_check_block(s, expr->unsafe_block_expr.body);
        case AST_FIELD_ACCESS_EXPR: {
            sema_check_expr(s, expr->field_access_expr.object);
            return st_void(s->types);
        }
        case AST_INDEX_EXPR: {
            SType *obj = sema_check_expr(s, expr->index_expr.object);
            sema_check_expr(s, expr->index_expr.index);
            if (obj->kind == ST_ARRAY) return obj->as.array.base;
            if (obj->kind == ST_SLICE) return obj->as.slice.base;
            return st_void(s->types);
        }
        case AST_SLICE_EXPR: {
            sema_check_expr(s, expr->slice_expr.object);
            if (expr->slice_expr.start) sema_check_expr(s, expr->slice_expr.start);
            if (expr->slice_expr.end) sema_check_expr(s, expr->slice_expr.end);
            return st_void(s->types);
        }
        case AST_ARRAY_LITERAL: {
            for (size_t i = 0; i < expr->array_literal.elements.count; i++) sema_check_expr(s, expr->array_literal.elements.items[i]);
            return st_void(s->types);
        }
        case AST_STRUCT_LITERAL: {
            for (size_t i = 0; i < expr->struct_literal.fields.count; i++) sema_check_expr(s, expr->struct_literal.fields.items[i]);
            return st_void(s->types);
        }
        case AST_METHOD_CALL_EXPR: {
            sema_check_expr(s, expr->method_call_expr.receiver);
            return st_void(s->types);
        }
        case AST_MATCH_ARM: return sema_check_expr(s, expr->match_arm.expr);
        default: return st_void(s->types);
    }
}
