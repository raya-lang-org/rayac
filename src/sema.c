
#include "sema.h"
#include "ast.h"
#include "string_view.h"
#include <stdio.h>
#include <stdarg.h>

/* ========================================================================
 *  Method Table — linked list of (type_name, method_name) → fn_decl
 * ======================================================================== */

static uint64_t hash_method_key(StringView type, StringView name) {
    uint64_t h = 0xcbf29ce484222325;
    for (size_t i = 0; i < type.len; i++) h = (h ^ type.data[i]) * 0x100000001b3;
    for (size_t i = 0; i < name.len; i++) h = (h ^ name.data[i]) * 0x100000001b3;
    return h;
}

static void method_table_add(Sema *s, StringView type_name, StringView method_name, AstNode *fn_decl)
{
    uint64_t h = hash_method_key(type_name, method_name);
    size_t idx = h % METHOD_TABLE_BUCKETS;
    MethodEntry *e = arena_alloc(s->arena, sizeof(MethodEntry));
    e->type_name = type_name;
    e->method_name = method_name;
    e->fn_decl = fn_decl;
    e->next = s->method_table.buckets[idx];
    s->method_table.buckets[idx] = e;
}

static AstNode *method_table_lookup(Sema *s, StringView type_name, StringView method_name)
{
    uint64_t h = hash_method_key(type_name, method_name);
    size_t idx = h % METHOD_TABLE_BUCKETS;
    for (MethodEntry *e = s->method_table.buckets[idx]; e; e = e->next) {
        if (sv_eq(e->type_name, type_name) && sv_eq(e->method_name, method_name))
            return e->fn_decl;
    }
    return NULL;
}

static StringView sema_type_name_for_method(Sema *s, SType *t)
{
    (void)s;
    if (!t) return sv_from_cstr("");
    switch (t->kind) {
        case ST_POINTER:  return sema_type_name_for_method(s, t->as.pointer.base);
        case ST_REFERENCE:return sema_type_name_for_method(s, t->as.reference.base);
        case ST_STRUCT:   return t->as.struct_.name;
        case ST_UNION:    return t->as.union_.name;
        case ST_ENUM:     return t->as.enum_.name;
        default:          return sv_from_cstr("");
    }
}

/* ========================================================================
 *  Forward declarations
 * ======================================================================== */
static void sema_collect_decls(Sema *s, AstNode *module);
static void sema_resolve_bodies(Sema *s, AstNode *module);
static void sema_check_fn_body(Sema *s, AstNode *fn);
static SType *sema_make_fn_type(Sema *s, AstNode *fn);
static AstNodeList *sema_get_type_fields(Sema *s, SType *type, AstNode **out_decl);

/* ========================================================================
 *  Lifecycle
 * ======================================================================== */
Sema *sema_new(Arena *arena, DiagnosticEngine *diag)
{
    Sema *s = arena_alloc(arena, sizeof(Sema));
    s->arena = arena;
    s->types = type_table_new(arena);
    s->module_scope = scope_new(arena, NULL, NULL);
    s->current_scope = s->module_scope;
    s->current_self_type = NULL;
    s->current_fn = NULL;
    s->diag = diag;
    s->error_count = 0;
    s->in_unsafe = false;
    s->current_fn_has_return = false;
    s->in_collect_decls = false;
    memset(&s->method_table, 0, sizeof(s->method_table));
    

    return s;
}

void sema_report(Sema *s, SourceLocation loc, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    diag_error(s->diag, loc, "%s", buf);
    s->error_count++;
}

void sema_run(Sema *s, AstNode *module)
{
    sema_collect_decls(s, module);
    if (s->error_count > 0) return;
    sema_resolve_bodies(s, module);
}

/* ========================================================================
 *  Pass 1 — Symbol collection
 * ======================================================================== */
static AstNodeList *sema_get_type_fields(Sema *s, SType *type, AstNode **out_decl)
{
    if (!type) return NULL;
    StringView name = {0};
    if (type->kind == ST_STRUCT || type->kind == ST_UNION) name = type->as.struct_.name;
    else return NULL;

    Symbol *sym = scope_lookup(s->module_scope, name);
    if (!sym || !sym->decl) return NULL;
    if (sym->kind != SYM_STRUCT && sym->kind != SYM_UNION) return NULL;

    AstNode *decl = sym->decl;
    *out_decl = decl;
    if (decl->kind == AST_STRUCT_DECL) return &decl->struct_decl.fields;
    if (decl->kind == AST_UNION_DECL) return &decl->union_decl.fields;
    return NULL;
}

static SType *sema_make_fn_type(Sema *s, AstNode *fn)
{
    size_t pc = fn->fn_decl.params.count;
    SType **params = arena_alloc(s->arena, pc * sizeof(SType*));
    for (size_t i = 0; i < pc; i++) {
        AstNode *p = fn->fn_decl.params.items[i];
        params[i] = sema_resolve_type(s, p->param_decl.type);
    }
    SType *ret = fn->fn_decl.ret_type ? sema_resolve_type(s, fn->fn_decl.ret_type) : st_void(s->types);
    return st_function(s->types, params, pc, ret, false);
}

static void sema_collect_decls(Sema *s, AstNode *module)
{
    s->in_collect_decls = true;
    if (module->kind != AST_COMPILATION_UNIT) return;
    for (size_t i = 0; i < module->compilation_unit.imports.count; i++) {
        AstNode *imp = module->compilation_unit.imports.items[i];
        StringView alias = imp->import_decl.alias;
        if (alias.len == 0) {
            alias = imp->import_decl.parts.items[imp->import_decl.parts.count - 1];
        }
        Symbol *sym = symbol_new(s->arena, SYM_MODULE, alias, st_void(s->types), imp);
        scope_insert(s->arena, s->module_scope, sym);
    }
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
                name = decl->union_decl.name; kind = SYM_UNION; type = st_void(s->types);
                is_pub = decl->union_decl.is_pub;
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
            /* FIX #3: Register extend methods in method table */
            case AST_EXTEND_DECL: {
                StringView target_name = decl->extend_decl.target_name;
                Symbol *target_sym = scope_lookup(s->module_scope, target_name);
                if (!target_sym || (target_sym->kind != SYM_STRUCT && target_sym->kind != SYM_UNION && target_sym->kind != SYM_ENUM)) {
                    sema_report(s, decl->loc, "extend target '%.*s' is not a struct, union, or enum", SV_ARG(target_name));
                }
                for (size_t j = 0; j < decl->extend_decl.methods.count; j++) {
                    AstNode *method = decl->extend_decl.methods.items[j];
                    if (method->kind == AST_FN_DECL) {
                        method_table_add(s, target_name, method->fn_decl.name, method);
                    }
                }
                continue;
            }
            default: continue;
        }
        Symbol *sym = symbol_new(s->arena, kind, name, type, decl);
        sym->is_pub = is_pub; sym->is_comptime = is_comptime;
        if (scope_lookup_current(s->module_scope, name))
            sema_report(s, decl->loc, "redefinition of '%.*s'", SV_ARG(name));
        else
            scope_insert(s->arena, s->module_scope, sym);
    }
    s->in_collect_decls = false;
}

/* ========================================================================
 *  Type resolution
 * ======================================================================== */
SType *sema_resolve_type(Sema *s, TypeExpr *type_expr)
{
    if (!type_expr) return st_void(s->types);

    switch (type_expr->kind) {
        case TYPE_NAMED: {
            StringView n = type_expr->named.name;

            /* FIX #4: Self type resolution */
            if (sv_eq_cstr(n, "Self")) {
                if (!s->current_self_type) {
                    sema_report(s, type_expr->loc, "Self outside of method context");
                    return st_void(s->types);
                }
                return s->current_self_type;
            }

            /* Primitives (mirror st_from_ast) */
            if (sv_eq_cstr(n, "void"))   return st_void(s->types);
            if (sv_eq_cstr(n, "bool"))   return st_bool(s->types);
            if (sv_eq_cstr(n, "isize"))  return st_int(s->types, true, 64);
            if (sv_eq_cstr(n, "usize"))  return st_int(s->types, false, 64);
            if (n.len > 1 && n.data[0] == 'i') {
                int b = sv_to_int(sv_slice(n, 1, n.len));
                if (b == 8 || b == 16 || b == 32 || b == 64 || b == 128)
                    return st_int(s->types, true, b);
            }
            if (n.len > 1 && n.data[0] == 'u') {
                int b = sv_to_int(sv_slice(n, 1, n.len));
                if (b == 8 || b == 16 || b == 32 || b == 64 || b == 128)
                    return st_int(s->types, false, b);
            }
            if (n.len > 1 && n.data[0] == 'f') {
                int b = sv_to_int(sv_slice(n, 1, n.len));
                if (b == 32 || b == 64)
                    return st_float(s->types, b);
            }
            if (sv_eq_cstr(n, "noreturn")) {
                SType t = {.kind = ST_NORETURN};
                SType *p = arena_alloc(s->arena, sizeof(SType));
                memcpy(p, &t, sizeof(t));
                return p;
            }

            /* FIX #8: Strict type lookup in Pass 2 */
            if (!s->in_collect_decls) {
                Symbol *sym = scope_lookup(s->module_scope, n);
                if (!sym || (sym->kind != SYM_STRUCT && sym->kind != SYM_UNION && sym->kind != SYM_ENUM && sym->kind != SYM_TYPE_ALIAS && sym->kind != SYM_TRAIT)) {
                    sema_report(s, type_expr->loc, "unknown type '%.*s'", SV_ARG(n));
                }
            }

            return st_from_ast(s->types, type_expr);
        }
        case TYPE_REFERENCE:
            return st_reference(s->types, type_expr->unary.is_const, sema_resolve_type(s, type_expr->unary.child));
        case TYPE_POINTER:
            return st_pointer(s->types, type_expr->unary.is_const, sema_resolve_type(s, type_expr->unary.child));
        case TYPE_SLICE:
            return st_slice(s->types, type_expr->unary.is_const, sema_resolve_type(s, type_expr->unary.child));
        case TYPE_OPTIONAL:
            return st_optional(s->types, sema_resolve_type(s, type_expr->unary.child));
        case TYPE_ERROR_UNION:
            return st_error_union(s->types, sema_resolve_type(s, type_expr->unary.child));
        case TYPE_FUNCTION: {
            size_t pc = type_expr->func.params.count;
            SType **params = arena_alloc(s->arena, pc * sizeof(SType*));
            for (size_t i = 0; i < pc; i++) {
                params[i] = sema_resolve_type(s, type_expr->func.params.items[i]);
            }
            SType *ret = type_expr->func.ret ? sema_resolve_type(s, type_expr->func.ret) : st_void(s->types);
            return st_function(s->types, params, pc, ret, false);
        }
        default:
            return st_from_ast(s->types, type_expr);
    }
}

/* ========================================================================
 *  Pass 2 — Body resolution
 * ======================================================================== */
static void sema_resolve_bodies(Sema *s, AstNode *module)
{
    if (module->kind != AST_COMPILATION_UNIT) return;
    for (size_t i = 0; i < module->compilation_unit.decls.count; i++) {
        AstNode *d = module->compilation_unit.decls.items[i];
        if (d->kind == AST_FN_DECL) {
            s->current_self_type = NULL;
            sema_check_fn_body(s, d);
        }
        /* FIX #7: Type-check top-level const/var initializers */
        else if (d->kind == AST_CONST_DECL || d->kind == AST_VAR_DECL) {
            if (d->var_decl.init) {
                SType *init_type = sema_check_expr(s, d->var_decl.init);
                if (d->var_decl.type) {
                    SType *decl_type = sema_resolve_type(s, d->var_decl.type);
                    if (!st_can_coerce(init_type, decl_type)) {
                        sema_report(s, d->loc, "cannot initialize %s '%.*s' of type '%s' with '%s'",
                            d->kind == AST_CONST_DECL ? "constant" : "variable",
                            SV_ARG(d->var_decl.name), st_name(decl_type), st_name(init_type));
                    }
                }
            }
        }
        /* FIX #3 continued: Type-check extend method bodies */
        else if (d->kind == AST_EXTEND_DECL) {
            StringView target_name = d->extend_decl.target_name;
            Symbol *target_sym = scope_lookup(s->module_scope, target_name);
            if (target_sym && (target_sym->kind == SYM_STRUCT || target_sym->kind == SYM_UNION || target_sym->kind == SYM_ENUM)) {
                SType *prev_self = s->current_self_type;
                s->current_self_type = target_sym->type && target_sym->type->kind != ST_VOID
                    ? target_sym->type
                    : st_void(s->types);
                for (size_t j = 0; j < d->extend_decl.methods.count; j++) {
                    AstNode *method = d->extend_decl.methods.items[j];
                    if (method->kind == AST_FN_DECL) {
                        sema_check_fn_body(s, method);
                    }
                }
                s->current_self_type = prev_self;
            }
        }
    }
}

static void sema_check_fn_body(Sema *s, AstNode *fn)
{
    Scope *fn_scope = scope_new(s->arena, s->module_scope, fn);
    Scope *prev_scope = s->current_scope;
    AstNode *prev_fn = s->current_fn;
    SType *prev_self = s->current_self_type;
    bool prev_unsafe = s->in_unsafe;
    s->current_scope = fn_scope;
    s->current_fn = fn;
    s->current_fn_has_return = false;
    /* FIX #5: unsafe fn sets in_unsafe */
    s->in_unsafe = fn->fn_decl.is_unsafe;

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

    /* FIX #2: Only check trailing expression if no explicit returns */
    if (!s->current_fn_has_return && !st_eq(ret, body_type) && body_type->kind != ST_NORETURN) {
        if (!st_can_coerce(body_type, ret))
            sema_report(s, fn->fn_decl.body->loc, "expected return type '%s', found '%s'", st_name(ret), st_name(body_type));
    }

    s->current_scope = prev_scope;
    s->current_fn = prev_fn;
    s->current_self_type = prev_self;
    s->in_unsafe = prev_unsafe;
}

/* ========================================================================
 *  Statement checking
 * ======================================================================== */
SType *sema_check_block(Sema *s, AstNode *block)
{
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

SType *sema_check_stmt(Sema *s, AstNode *stmt)
{
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
            /* FIX #2: Track explicit returns */
            s->current_fn_has_return = true;
            size_t errs_before = s->error_count;
            SType *ret = st_void(s->types);
            if (stmt->return_stmt.value) {
                ret = sema_check_expr(s, stmt->return_stmt.value);
            }
            SType *expected = s->current_fn && s->current_fn->fn_decl.ret_type
                ? sema_resolve_type(s, s->current_fn->fn_decl.ret_type)
                : st_void(s->types);
            if (s->error_count == errs_before && !st_can_coerce(ret, expected)) {
                sema_report(s, stmt->loc, "return type mismatch: expected '%s', found '%s'",
                    st_name(expected), st_name(ret));
            }
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

/* ========================================================================
 *  Expression checking
 * ======================================================================== */
SType *sema_check_expr(Sema *s, AstNode *expr)
{
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
        case AST_IDENTIFIER: {
            Symbol *sym = scope_lookup(s->current_scope, expr->identifier.name);
            if (!sym) {
                sema_report(s, expr->loc, "use of undeclared identifier '%.*s'", SV_ARG(expr->identifier.name));
                expr->sema_type = st_void(s->types);
                expr->identifier.sym = NULL;
                return expr->sema_type;
            }
            expr->identifier.sym = sym;
            expr->sema_type = sym->type ? sym->type : st_void(s->types);
            return expr->sema_type;
        }
        case AST_BINARY_EXPR: {
            SType *l = sema_check_expr(s, expr->binary_expr.left);
            SType *r = sema_check_expr(s, expr->binary_expr.right);
            TokenKind op = expr->binary_expr.op;
            switch (op) {
                case TOK_PLUS: case TOK_MINUS: case TOK_STAR: case TOK_SLASH: case TOK_PERCENT:
                    if (!st_is_numeric(l) || !st_is_numeric(r)) {
                        sema_report(s, expr->loc, "invalid operands to arithmetic expression");
                        expr->sema_type = st_void(s->types);
                    } else {
                        expr->sema_type = (l->kind == ST_FLOAT || r->kind == ST_FLOAT) ? st_float(s->types, 64) : l;
                    }
                    break;
                case TOK_EQ: case TOK_NE: case TOK_LT: case TOK_GT: case TOK_LE: case TOK_GE:
                    expr->sema_type = st_bool(s->types);
                    break;
                case TOK_AND_AND: case TOK_OR_OR:
                    if (l->kind != ST_BOOL || r->kind != ST_BOOL) sema_report(s, expr->loc, "logical operators require boolean operands");
                    expr->sema_type = st_bool(s->types);
                    break;
                case TOK_AMPERSAND: case TOK_PIPE: case TOK_CARET: case TOK_SHL: case TOK_SHR:
                    if (!st_is_integer(l) || !st_is_integer(r)) sema_report(s, expr->loc, "bitwise operators require integer operands");
                    expr->sema_type = l;
                    break;
                default:
                    expr->sema_type = l;
                    break;
            }
            return expr->sema_type;
        }
        case AST_UNARY_EXPR: {
            SType *o = sema_check_expr(s, expr->unary_expr.operand);
            TokenKind op = expr->unary_expr.op;
            switch (op) {
                case TOK_MINUS: case TOK_TILDE:
                    if (!st_is_numeric(o)) sema_report(s, expr->loc, "invalid operand to unary operator");
                    expr->sema_type = o;
                    break;
                case TOK_BANG:
                    if (o->kind != ST_BOOL) sema_report(s, expr->loc, "logical not requires boolean operand");
                    expr->sema_type = st_bool(s->types);
                    break;
                case TOK_STAR:
                    if (o->kind == ST_POINTER) {
                        /* FIX #5: Raw pointer dereference requires unsafe */
                        if (!s->in_unsafe) {
                            sema_report(s, expr->loc, "dereferencing raw pointer requires 'unsafe' context");
                        }
                        expr->sema_type = o->as.pointer.base;
                    } else if (o->kind == ST_REFERENCE) {
                        expr->sema_type = o->as.reference.base;
                    } else { 
                        sema_report(s, expr->loc, "cannot dereference non-pointer type"); 
                        expr->sema_type = st_void(s->types); 
                    }
                    break;
                case TOK_AMPERSAND:
                    expr->sema_type = st_reference(s->types, false, o);
                    break;
                default:
                    expr->sema_type = o;
                    break;
            }
            return expr->sema_type;
        }
        case AST_CALL_EXPR: {
            SType *c = sema_check_expr(s, expr->call_expr.callee);
            if (c->kind != ST_FUNCTION) { 
                sema_report(s, expr->loc, "called object is not a function"); 
                expr->sema_type = st_void(s->types); 
                return expr->sema_type; 
            }
            /* FIX #6: Check argument count and types */
            size_t expected = c->as.function.param_count;
            size_t got = expr->call_expr.args.count;
            if (got != expected) {
                sema_report(s, expr->loc, "function expects %zu argument(s), got %zu", expected, got);
            } else {
                for (size_t i = 0; i < expected; i++) {
                    SType *arg_type = sema_check_expr(s, expr->call_expr.args.items[i]);
                    SType *param_type = c->as.function.params[i];
                    if (!st_can_coerce(arg_type, param_type)) {
                        sema_report(s, expr->call_expr.args.items[i]->loc,
                            "argument %zu: expected '%s', found '%s'",
                            i + 1, st_name(param_type), st_name(arg_type));
                    }
                }
            }
            expr->sema_type = c->as.function.ret;
            return expr->sema_type;
        }
        case AST_CAST_EXPR: {
            SType *t = sema_resolve_type(s, expr->cast_expr.type);
            sema_check_expr(s, expr->cast_expr.expr);
            expr->sema_type = t;
            return t;
        }
        case AST_TRY_EXPR: {
            SType *inner = sema_check_expr(s, expr->try_expr.expr);
            if (inner->kind != ST_ERROR_UNION) { 
                sema_report(s, expr->loc, "try requires error union type, got '%s'", st_name(inner)); 
                expr->sema_type = inner; 
                return inner; 
            }
            expr->sema_type = inner->as.error_union.base;
            return expr->sema_type;
        }
        case AST_UNSAFE_BLOCK_EXPR: {
            bool prev = s->in_unsafe;
            s->in_unsafe = true;
            SType *t = sema_check_block(s, expr->unsafe_block_expr.body);
            s->in_unsafe = prev;
            expr->sema_type = t;
            return expr->sema_type;
        }
        case AST_FIELD_ACCESS_EXPR: {
            SType *base = sema_check_expr(s, expr->field_access_expr.object);
            SType *struct_type = base;
            if (base->kind == ST_POINTER) struct_type = base->as.pointer.base;
            if (base->kind == ST_REFERENCE) struct_type = base->as.reference.base;

            AstNode *decl = NULL;
            AstNodeList *fields = sema_get_type_fields(s, struct_type, &decl);
            if (!fields) {
                sema_report(s, expr->loc, "field access on non-struct type '%s'", st_name(base));
                expr->sema_type = st_void(s->types);
                return expr->sema_type;
            }

            StringView fname = expr->field_access_expr.field_name;
            for (size_t i = 0; i < fields->count; i++) {
                AstNode *f = fields->items[i];
                if (sv_eq(f->field_decl.name, fname)) {
                    expr->sema_type = sema_resolve_type(s, f->field_decl.type);
                    return expr->sema_type;
                }
            }
            sema_report(s, expr->loc, "struct '%s' has no field '%.*s'", st_name(struct_type), SV_ARG(fname));
            expr->sema_type = st_void(s->types);
            return expr->sema_type;
        }
        case AST_INDEX_EXPR: {
            SType *obj = sema_check_expr(s, expr->index_expr.object);
            sema_check_expr(s, expr->index_expr.index);
            if (obj->kind == ST_ARRAY) expr->sema_type = obj->as.array.base;
            else if (obj->kind == ST_SLICE) expr->sema_type = obj->as.slice.base;
            else expr->sema_type = st_void(s->types);
            return expr->sema_type;
        }
        case AST_SLICE_EXPR: {
            sema_check_expr(s, expr->slice_expr.object);
            if (expr->slice_expr.start) sema_check_expr(s, expr->slice_expr.start);
            if (expr->slice_expr.end) sema_check_expr(s, expr->slice_expr.end);
            expr->sema_type = st_void(s->types);
            return expr->sema_type;
        }
        case AST_ARRAY_LITERAL: {
            for (size_t i = 0; i < expr->array_literal.elements.count; i++) sema_check_expr(s, expr->array_literal.elements.items[i]);
            expr->sema_type = st_void(s->types);
            return expr->sema_type;
        }
        case AST_STRUCT_LITERAL: {
            SType *st = expr->struct_literal.type ? sema_resolve_type(s, expr->struct_literal.type) : st_void(s->types);
            expr->sema_type = st;
            AstNode *decl = NULL;
            AstNodeList *decl_fields = sema_get_type_fields(s, st, &decl);
            if (!decl_fields && expr->struct_literal.type) {
                sema_report(s, expr->loc, "cannot resolve fields for type '%s'", st_name(st));
                return st;
            }
            for (size_t i = 0; i < expr->struct_literal.fields.count; i++) {
                AstNode *field = expr->struct_literal.fields.items[i];
                if (field->kind != AST_ASSIGN_STMT || field->assign_stmt.lhs->kind != AST_IDENTIFIER) {
                    sema_report(s, field->loc, "invalid struct field initializer");
                    continue;
                }
                StringView fname = field->assign_stmt.lhs->identifier.name;
                SType *fval = sema_check_expr(s, field->assign_stmt.rhs);
                bool found = false;
                for (size_t j = 0; j < decl_fields->count; j++) {
                    AstNode *df = decl_fields->items[j];
                    if (sv_eq(df->field_decl.name, fname)) {
                        found = true;
                        SType *ftype = sema_resolve_type(s, df->field_decl.type);
                        if (!st_can_coerce(fval, ftype)) {
                            sema_report(s, field->loc, "field '%.*s' expects '%s', got '%s'",
                                SV_ARG(fname), st_name(ftype), st_name(fval));
                        }
                        break;
                    }
                }
                if (!found) {
                    sema_report(s, field->loc, "struct '%s' has no field '%.*s'",
                        st_name(st), SV_ARG(fname));
                }
            }
            return st;
        }
        /* FIX #1: Method call resolution */
        case AST_METHOD_CALL_EXPR: {
            SType *recv = sema_check_expr(s, expr->method_call_expr.receiver);
            StringView mname = expr->method_call_expr.method_name;

            SType *concrete = recv;
            if (recv->kind == ST_POINTER) concrete = recv->as.pointer.base;
            if (recv->kind == ST_REFERENCE) concrete = recv->as.reference.base;

            StringView type_name = sema_type_name_for_method(s, concrete);
            if (type_name.len == 0) {
                sema_report(s, expr->loc, "cannot call method on type '%s'", st_name(recv));
                expr->sema_type = st_void(s->types);
                return expr->sema_type;
            }

            AstNode *method = method_table_lookup(s, type_name, mname);
            if (!method) {
                sema_report(s, expr->loc, "no method '%.*s' on type '%.*s'",
                    SV_ARG(mname), SV_ARG(type_name));
                expr->sema_type = st_void(s->types);
                return expr->sema_type;
            }

            /* Temporarily set Self so method signature resolves correctly */
            SType *prev_self = s->current_self_type;
            s->current_self_type = concrete;
            SType *fn_type = sema_make_fn_type(s, method);
            s->current_self_type = prev_self;

            if (fn_type->kind != ST_FUNCTION) {
                expr->sema_type = st_void(s->types);
                return expr->sema_type;
            }

            size_t expected = fn_type->as.function.param_count;
            size_t got = expr->method_call_expr.args.count;

            if (expected == 0) {
                sema_report(s, expr->loc, "method '%.*s' has no 'self' parameter", SV_ARG(mname));
                expr->sema_type = st_void(s->types);
                return expr->sema_type;
            }

            /* Check receiver against first param (self) */
            SType *self_param = fn_type->as.function.params[0];
            bool receiver_ok = st_eq(recv, self_param) || st_can_coerce(recv, self_param);
            if (!receiver_ok) {
                sema_report(s, expr->loc, "method '%.*s' expects receiver of type '%s', found '%s'",
                    SV_ARG(mname), st_name(self_param), st_name(recv));
            }

            /* Check remaining args */
            if (got + 1 != expected) {
                sema_report(s, expr->loc, "method '%.*s' expects %zu argument(s) (including self), got %zu",
                    SV_ARG(mname), expected, got + 1);
            } else {
                for (size_t i = 0; i < got; i++) {
                    SType *arg_type = sema_check_expr(s, expr->method_call_expr.args.items[i]);
                    SType *param_type = fn_type->as.function.params[i + 1];
                    if (!st_can_coerce(arg_type, param_type)) {
                        sema_report(s, expr->method_call_expr.args.items[i]->loc,
                            "argument %zu: expected '%s', found '%s'",
                            i + 1, st_name(param_type), st_name(arg_type));
                    }
                }
            }

            expr->sema_type = fn_type->as.function.ret;
            return expr->sema_type;
        }
        case AST_MATCH_ARM:
            expr->sema_type = sema_check_expr(s, expr->match_arm.expr);
            return expr->sema_type;
        default:
            expr->sema_type = st_void(s->types);
            return expr->sema_type;
    }
}
