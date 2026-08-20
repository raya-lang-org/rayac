#include "symbol.h"
#include "type.h"
#include "sema.h"

void sema_collect_decls(Sema *s, AstNode *module) {
    assert(module->kind == AST_MODULE);
    
    for (size_t i = 0; i < module->as.module.decls.count; i++) {
        AstNode *decl = module->as.module.decls.nodes[i];
        StringView name = {0};
        SymbolKind kind = SYM_POISON;
        Type *type = NULL;
        
        switch (decl->kind) {
            case AST_FN_DECL: {
                name = decl->as.fn_decl.name;
                kind = SYM_FUNCTION;
                // Build function type from AST signature (without body)
                type = sema_make_fn_type(s, decl);
                break;
            }
            case AST_STRUCT_DECL:
                name = decl->as.struct_decl.name;
                kind = SYM_STRUCT;
                type = type_opaque(s->types); // resolved later
                break;
            case AST_UNION_DECL:
                name = decl->as.union_decl.name;
                kind = SYM_UNION;
                type = type_opaque(s->types);
                break;
            case AST_ENUM_DECL:
                name = decl->as.enum_decl.name;
                kind = SYM_ENUM;
                type = type_opaque(s->types);
                break;
            case AST_TRAIT_DECL:
                name = decl->as.trait_decl.name;
                kind = SYM_TRAIT;
                type = type_opaque(s->types);
                break;
            case AST_TYPE_ALIAS:
                name = decl->as.type_alias.name;
                kind = SYM_TYPE_ALIAS;
                type = NULL; // resolved on use
                break;
            case AST_CONST_DECL:
            case AST_VAR_DECL:
                name = decl->as.var_decl.name;
                kind = (decl->kind == AST_CONST_DECL) ? SYM_CONST : SYM_VAR;
                type = NULL; // inferred in Pass 2
                break;
            default:
                continue;
        }
        
        Symbol *sym = symbol_new(s->arena, kind, name, type, decl);
        sym->is_pub = decl->has_pub;
        sym->is_comptime = decl->has_comptime;
        
        if (scope_lookup_current(s->module_scope, name)) {
            sema_report(s, decl->loc, "redefinition of '%.*s'", SV_ARG(name));
            s->error_count++;
        } else {
            scope_insert(s->arena, s->module_scope, sym);
        }
    }
}


void sema_resolve_bodies(Sema *s, AstNode *module) {
    for (size_t i = 0; i < module->as.module.decls.count; i++) {
        AstNode *decl = module->as.module.decls.nodes[i];
        if (decl->kind == AST_FN_DECL) {
            sema_check_fn_body(s, decl);
        } else if (decl->kind == AST_STRUCT_DECL) {
            sema_check_struct(s, decl);
        }
        // ... union, enum, trait, extend ...
    }
}

static void sema_check_fn_body(Sema *s, AstNode *fn) {
    Scope *fn_scope = scope_new(s->arena, s->module_scope, fn);
    Scope *prev_scope = s->current_scope;
    AstNode *prev_fn = s->current_fn;
    s->current_scope = fn_scope;
    s->current_fn = fn;
    
    // Bind generic params
    for (size_t i = 0; i < fn->as.fn_decl.generic_params.count; i++) {
        AstNode *gp = fn->as.fn_decl.generic_params.nodes[i];
        Type *t = type_generic_param(s->types, gp->as.generic_param.name, i);
        Symbol *sym = symbol_new(s->arena, SYM_GENERIC_PARAM, gp->as.generic_param.name, t, gp);
        scope_insert(s->arena, fn_scope, sym);
    }
    
    // Bind regular params
    for (size_t i = 0; i < fn->as.fn_decl.params.count; i++) {
        AstNode *param = fn->as.fn_decl.params.nodes[i];
        Type *pt = sema_resolve_type(s, param->as.param.type_expr);
        Symbol *sym = symbol_new(s->arena, SYM_VAR, param->as.param.name, pt, param);
        scope_insert(s->arena, fn_scope, sym);
    }
    
    // Handle self parameter (C.2 in your spec)
    if (fn->as.fn_decl.has_self) {
        Type *self_type = sema_resolve_self_type(s, fn);
        Symbol *self_sym = symbol_new(s->arena, SYM_VAR, SV("self"), self_type, NULL);
        scope_insert(s->arena, fn_scope, self_sym);
    }
    
    // Check body
    Type *body_type = sema_check_block(s, fn->as.fn_decl.body);
    
    // Check return type compatibility
    Type *ret = sema_resolve_type(s, fn->as.fn_decl.ret_type);
    if (ret && !type_eq(ret, body_type) && body_type->kind != TYPE_NORETURN) {
        if (!type_can_coerce(body_type, ret)) {
            sema_report(s, fn->as.fn_decl.body->loc, 
                "expected return type '%s', found '%s'", 
                type_name(ret), type_name(body_type));
            s->error_count++;
        }
    }
    
    s->current_scope = prev_scope;
    s->current_fn = prev_fn;
}


Type *sema_check_expr(Sema *s, AstNode *expr) {
    switch (expr->kind) {
        case AST_INT_LITERAL:
            // Default to isize, allow inference context to narrow later
            expr->sema_type = type_int(s->types, true, 64);
            return expr->sema_type;
            
        case AST_FLOAT_LITERAL:
            expr->sema_type = type_float(s->types, 64);
            return expr->sema_type;
            
        case AST_STRING_LITERAL:
            expr->sema_type = type_slice(s->types, true, type_int(s->types, false, 8));
            return expr->sema_type;
            
        case AST_IDENT: {
            Symbol *sym = scope_lookup(s->current_scope, expr->as.ident.name);
            if (!sym) {
                sema_report(s, expr->loc, "use of undeclared identifier '%.*s'", 
                    SV_ARG(expr->as.ident.name));
                s->error_count++;
                expr->sema_type = type_opaque(s->types);
                expr->as.ident.sym = NULL;
                return expr->sema_type;
            }
            expr->as.ident.sym = sym;
            expr->sema_type = sym->type ? sym->type : type_opaque(s->types);
            return expr->sema_type;
        }
        
        case AST_BINARY: {
            Type *left = sema_check_expr(s, expr->as.binary.left);
            Type *right = sema_check_expr(s, expr->as.binary.right);
            
            switch (expr->as.binary.op) {
                case BINOP_ADD: case BINOP_SUB:
                case BINOP_MUL: case BINOP_DIV: case BINOP_MOD:
                    if (!type_is_numeric(left) || !type_is_numeric(right)) {
                        sema_report(s, expr->loc, "invalid operands to arithmetic expression");
                        s->error_count++;
                    }
                    expr->sema_type = type_common(left, right);
                    break;
                    
                case BINOP_EQ: case BINOP_NE:
                case BINOP_LT: case BINOP_GT:
                case BINOP_LE: case BINOP_GE:
                    expr->sema_type = type_bool(s->types);
                    break;
                    
                case BINOP_AND: case BINOP_OR:
                    if (left->kind != TYPE_BOOL || right->kind != TYPE_BOOL) {
                        sema_report(s, expr->loc, "logical operators require boolean operands");
                        s->error_count++;
                    }
                    expr->sema_type = type_bool(s->types);
                    break;
                    
                default:
                    expr->sema_type = left;
                    break;
            }
            return expr->sema_type;
        }
        
        case AST_CALL: {
            Type *callee = sema_check_expr(s, expr->as.call.callee);
            if (callee->kind != TYPE_FUNCTION) {
                sema_report(s, expr->loc, "called object is not a function");
                s->error_count++;
                expr->sema_type = type_opaque(s->types);
                return expr->sema_type;
            }
            // Check arg count and types...
            expr->sema_type = callee->as.function.ret;
            return expr->sema_type;
        }
        
        case AST_AS_CAST: {
            Type *target = sema_resolve_type(s, expr->as.cast.type_expr);
            Type *src = sema_check_expr(s, expr->as.cast.expr);
            // Validate cast legality here
            expr->sema_type = target;
            return target;
        }
        
        case AST_TRY: {
            Type *inner = sema_check_expr(s, expr->as.try_expr.expr);
            if (inner->kind != TYPE_ERROR_UNION) {
                sema_report(s, expr->loc, "try requires error union type, got '%s'", 
                    type_name(inner));
                s->error_count++;
                expr->sema_type = inner;
                return inner;
            }
            expr->sema_type = inner->as.error_union.base;
            return expr->sema_type;
        }
        
        // ... prefix, postfix, array literals, struct literals, etc.
        
        default:
            expr->sema_type = type_opaque(s->types);
            return expr->sema_type;
    }
}
