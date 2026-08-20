
#include "ast.h"
#include <stdio.h>
#include <string.h>

static void* grow_array(Arena* arena, void* old, size_t old_cap, size_t elem_size, size_t new_cap) {
    void* new_ptr = arena_alloc(arena, new_cap * elem_size);
    if (old && old_cap > 0) {
        memcpy(new_ptr, old, old_cap * elem_size);
    }
    return new_ptr;
}

void ast_node_list_init(Arena* arena, AstNodeList* list) {
    (void)arena;
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void ast_node_list_push(Arena* arena, AstNodeList* list, AstNode* node) {
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity ? list->capacity * 2 : 4;
        list->items = (AstNode**)grow_array(arena, list->items, list->capacity, sizeof(AstNode*), new_cap);
        list->capacity = new_cap;
    }
    list->items[list->count++] = node;
}

void type_expr_list_init(Arena* arena, TypeExprList* list) {
    (void)arena;
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void type_expr_list_push(Arena* arena, TypeExprList* list, TypeExpr* type) {
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity ? list->capacity * 2 : 4;
        list->items = (TypeExpr**)grow_array(arena, list->items, list->capacity, sizeof(TypeExpr*), new_cap);
        list->capacity = new_cap;
    }
    list->items[list->count++] = type;
}

void string_view_list_init(Arena* arena, StringViewList* list) {
    (void)arena;
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void string_view_list_push(Arena* arena, StringViewList* list, StringView sv) {
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity ? list->capacity * 2 : 4;
        list->items = (StringView*)grow_array(arena, list->items, list->capacity, sizeof(StringView), new_cap);
        list->capacity = new_cap;
    }
    list->items[list->count++] = sv;
}

void pattern_list_init(Arena* arena, PatternList* list) {
    (void)arena;
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void pattern_list_push(Arena* arena, PatternList* list, Pattern* pat) {
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity ? list->capacity * 2 : 4;
        list->items = (Pattern**)grow_array(arena, list->items, list->capacity, sizeof(Pattern*), new_cap);
        list->capacity = new_cap;
    }
    list->items[list->count++] = pat;
}

void attribute_list_init(Arena* arena, AttributeList* list) {
    (void)arena;
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void attribute_list_push(Arena* arena, AttributeList* list, Attribute attr) {
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity ? list->capacity * 2 : 4;
        list->items = (Attribute*)grow_array(arena, list->items, list->capacity, sizeof(Attribute), new_cap);
        list->capacity = new_cap;
    }
    list->items[list->count++] = attr;
}

#define NEW_NODE(arena, kind_, loc_) \
    ({ AstNode* _n = (AstNode*)arena_alloc((arena), sizeof(AstNode)); \
       memset(_n, 0, sizeof(AstNode)); \
       _n->kind = (kind_); _n->loc = (loc_); _n; })

#define NEW_TYPE(arena, kind_, loc_) \
    ({ TypeExpr* _t = (TypeExpr*)arena_alloc((arena), sizeof(TypeExpr)); \
       memset(_t, 0, sizeof(TypeExpr)); \
       _t->kind = (kind_); _t->loc = (loc_); _t; })

#define NEW_PATTERN(arena, kind_, loc_) \
    ({ Pattern* _p = (Pattern*)arena_alloc((arena), sizeof(Pattern)); \
       memset(_p, 0, sizeof(Pattern)); \
       _p->kind = (kind_); _p->loc = (loc_); _p; })

AstNode* ast_new_compilation_unit(Arena* arena, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_COMPILATION_UNIT, loc);
    ast_node_list_init(arena, &n->compilation_unit.imports);
    ast_node_list_init(arena, &n->compilation_unit.decls);
    return n;
}

AstNode* ast_new_module_decl(Arena* arena, StringView name, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_MODULE_DECL, loc);
    n->module_decl.name = name;
    return n;
}

AstNode* ast_new_import_decl(Arena* arena, StringView first, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_IMPORT_DECL, loc);
    string_view_list_init(arena, &n->import_decl.parts);
    string_view_list_push(arena, &n->import_decl.parts, first);
    return n;
}

AstNode* ast_new_fn_decl(Arena* arena, StringView name, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_FN_DECL, loc);
    n->fn_decl.name = name;
    ast_node_list_init(arena, &n->fn_decl.generic_params);
    ast_node_list_init(arena, &n->fn_decl.params);
    attribute_list_init(arena, &n->fn_decl.attrs);
    return n;
}

AstNode* ast_new_struct_decl(Arena* arena, StringView name, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_STRUCT_DECL, loc);
    n->struct_decl.name = name;
    ast_node_list_init(arena, &n->struct_decl.generic_params);
    ast_node_list_init(arena, &n->struct_decl.fields);
    attribute_list_init(arena, &n->struct_decl.attrs);
    return n;
}

AstNode* ast_new_union_decl(Arena* arena, StringView name, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_UNION_DECL, loc);
    n->struct_decl.name = name;
    ast_node_list_init(arena, &n->struct_decl.generic_params);
    ast_node_list_init(arena, &n->struct_decl.fields);
    attribute_list_init(arena, &n->struct_decl.attrs);
    return n;
}

AstNode* ast_new_enum_decl(Arena* arena, StringView name, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_ENUM_DECL, loc);
    n->enum_decl.name = name;
    ast_node_list_init(arena, &n->enum_decl.variants);
    attribute_list_init(arena, &n->enum_decl.attrs);
    return n;
}

AstNode* ast_new_traits_decl(Arena* arena, StringView name, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_TRAITS_DECL, loc);
    n->traits_decl.name = name;
    ast_node_list_init(arena, &n->traits_decl.methods);
    attribute_list_init(arena, &n->traits_decl.attrs);
    return n;
}

AstNode* ast_new_extend_decl(Arena* arena, StringView target, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_EXTEND_DECL, loc);
    n->extend_decl.target_name = target;
    ast_node_list_init(arena, &n->extend_decl.generic_params);
    string_view_list_init(arena, &n->extend_decl.trait_names);
    ast_node_list_init(arena, &n->extend_decl.methods);
    attribute_list_init(arena, &n->extend_decl.attrs);
    return n;
}

AstNode* ast_new_type_alias(Arena* arena, StringView name, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_TYPE_ALIAS, loc);
    n->type_alias.name = name;
    attribute_list_init(arena, &n->type_alias.attrs);
    return n;
}

AstNode* ast_new_const_decl(Arena* arena, StringView name, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_CONST_DECL, loc);
    n->var_decl.name = name;
    attribute_list_init(arena, &n->var_decl.attrs);
    return n;
}

AstNode* ast_new_var_decl(Arena* arena, StringView name, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_VAR_DECL, loc);
    n->var_decl.name = name;
    attribute_list_init(arena, &n->var_decl.attrs);
    return n;
}

AstNode* ast_new_test_decl(Arena* arena, StringView name, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_TEST_DECL, loc);
    n->test_decl.name = name;
    attribute_list_init(arena, &n->test_decl.attrs);
    return n;
}

AstNode* ast_new_field_decl(Arena* arena, StringView name, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_FIELD_DECL, loc);
    n->field_decl.name = name;
    attribute_list_init(arena, &n->field_decl.attrs);
    return n;
}

AstNode* ast_new_variant_decl(Arena* arena, StringView name, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_VARIANT_DECL, loc);
    n->variant_decl.name = name;
    return n;
}

AstNode* ast_new_trait_method_decl(Arena* arena, StringView name, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_TRAIT_METHOD_DECL, loc);
    n->trait_method_decl.name = name;
    ast_node_list_init(arena, &n->trait_method_decl.params);
    return n;
}

AstNode* ast_new_param_decl(Arena* arena, StringView name, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_PARAM_DECL, loc);
    n->param_decl.name = name;
    return n;
}

AstNode* ast_new_generic_param_decl(Arena* arena, StringView name, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_GENERIC_PARAM_DECL, loc);
    n->generic_param_decl.name = name;
    string_view_list_init(arena, &n->generic_param_decl.trait_constraints);
    return n;
}

AstNode* ast_new_block(Arena* arena, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_BLOCK, loc);
    ast_node_list_init(arena, &n->block.stmts);
    return n;
}

AstNode* ast_new_expr_stmt(Arena* arena, AstNode* expr, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_EXPR_STMT, loc);
    n->expr_stmt.expr = expr;
    return n;
}

AstNode* ast_new_return(Arena* arena, AstNode* value, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_RETURN_STMT, loc);
    n->return_stmt.value = value;
    return n;
}

AstNode* ast_new_if(Arena* arena, AstNode* cond, AstNode* then_block, AstNode* else_block, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_IF_STMT, loc);
    n->if_stmt.condition = cond;
    n->if_stmt.then_block = then_block;
    n->if_stmt.else_block = else_block;
    return n;
}

AstNode* ast_new_while(Arena* arena, AstNode* cond, AstNode* body, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_WHILE_STMT, loc);
    n->while_stmt.condition = cond;
    n->while_stmt.body = body;
    return n;
}

AstNode* ast_new_for(Arena* arena, StringView var, TypeExpr* type, AstNode* iterable, AstNode* body, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_FOR_STMT, loc);
    n->for_stmt.var_name = var;
    n->for_stmt.var_type = type;
    n->for_stmt.iterable = iterable;
    n->for_stmt.body = body;
    return n;
}

AstNode* ast_new_defer(Arena* arena, AstNode* expr, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_DEFER_STMT, loc);
    n->defer_stmt.expr = expr;
    return n;
}

AstNode* ast_new_errdefer(Arena* arena, AstNode* body, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_ERRDEFER_STMT, loc);
    n->errdefer_stmt.body = body;
    return n;
}

AstNode* ast_new_break(Arena* arena, SourceLocation loc) {
    return NEW_NODE(arena, AST_BREAK_STMT, loc);
}

AstNode* ast_new_continue(Arena* arena, SourceLocation loc) {
    return NEW_NODE(arena, AST_CONTINUE_STMT, loc);
}

AstNode* ast_new_match(Arena* arena, AstNode* expr, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_MATCH_STMT, loc);
    n->match_stmt.expr = expr;
    ast_node_list_init(arena, &n->match_stmt.arms);
    return n;
}

AstNode* ast_new_match_arm(Arena* arena, Pattern* pat, AstNode* expr, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_MATCH_ARM, loc);
    n->match_arm.pattern = pat;
    n->match_arm.expr = expr;
    return n;
}

AstNode* ast_new_assign(Arena* arena, TokenKind op, AstNode* lhs, AstNode* rhs, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_ASSIGN_STMT, loc);
    n->assign_stmt.op = op;
    n->assign_stmt.lhs = lhs;
    n->assign_stmt.rhs = rhs;
    return n;
}

AstNode* ast_new_int_literal(Arena* arena, int64_t value, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_INT_LITERAL, loc);
    n->int_literal.value = value;
    return n;
}

AstNode* ast_new_float_literal(Arena* arena, double value, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_FLOAT_LITERAL, loc);
    n->float_literal.value = value;
    return n;
}

AstNode* ast_new_string_literal(Arena* arena, StringView value, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_STRING_LITERAL, loc);
    n->string_literal.value = value;
    return n;
}

AstNode* ast_new_char_literal(Arena* arena, StringView value, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_CHAR_LITERAL, loc);
    n->char_literal.value = value;
    return n;
}

AstNode* ast_new_bool_literal(Arena* arena, bool value, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_BOOL_LITERAL, loc);
    n->bool_literal.value = value;
    return n;
}

AstNode* ast_new_null_literal(Arena* arena, SourceLocation loc) {
    return NEW_NODE(arena, AST_NULL_LITERAL, loc);
}

AstNode* ast_new_undefined_literal(Arena* arena, SourceLocation loc) {
    return NEW_NODE(arena, AST_UNDEFINED_LITERAL, loc);
}

AstNode* ast_new_identifier(Arena* arena, StringView name, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_IDENTIFIER, loc);
    n->identifier.name = name;
    return n;
}

AstNode* ast_new_binary(Arena* arena, TokenKind op, AstNode* left, AstNode* right, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_BINARY_EXPR, loc);
    n->binary_expr.op = op;
    n->binary_expr.left = left;
    n->binary_expr.right = right;
    return n;
}

AstNode* ast_new_unary(Arena* arena, TokenKind op, AstNode* operand, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_UNARY_EXPR, loc);
    n->unary_expr.op = op;
    n->unary_expr.operand = operand;
    return n;
}

AstNode* ast_new_call(Arena* arena, AstNode* callee, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_CALL_EXPR, loc);
    n->call_expr.callee = callee;
    ast_node_list_init(arena, &n->call_expr.args);
    return n;
}

AstNode* ast_new_method_call(Arena* arena, AstNode* receiver, StringView method, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_METHOD_CALL_EXPR, loc);
    n->method_call_expr.receiver = receiver;
    n->method_call_expr.method_name = method;
    ast_node_list_init(arena, &n->method_call_expr.args);
    return n;
}

AstNode* ast_new_field_access(Arena* arena, AstNode* object, StringView field, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_FIELD_ACCESS_EXPR, loc);
    n->field_access_expr.object = object;
    n->field_access_expr.field_name = field;
    return n;
}

AstNode* ast_new_index(Arena* arena, AstNode* object, AstNode* index, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_INDEX_EXPR, loc);
    n->index_expr.object = object;
    n->index_expr.index = index;
    return n;
}

AstNode* ast_new_slice(Arena* arena, AstNode* object, AstNode* start, AstNode* end, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_SLICE_EXPR, loc);
    n->slice_expr.object = object;
    n->slice_expr.start = start;
    n->slice_expr.end = end;
    return n;
}

AstNode* ast_new_cast(Arena* arena, AstNode* expr, TypeExpr* type, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_CAST_EXPR, loc);
    n->cast_expr.expr = expr;
    n->cast_expr.type = type;
    return n;
}

AstNode* ast_new_try(Arena* arena, AstNode* expr, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_TRY_EXPR, loc);
    n->try_expr.expr = expr;
    return n;
}

AstNode* ast_new_error_capture(Arena* arena, AstNode* expr, StringView err_name, AstNode* fallback, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_ERROR_CAPTURE_EXPR, loc);
    n->error_capture_expr.expr = expr;
    n->error_capture_expr.err_name = err_name;
    n->error_capture_expr.fallback = fallback;
    return n;
}

AstNode* ast_new_unsafe_block(Arena* arena, AstNode* body, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_UNSAFE_BLOCK_EXPR, loc);
    n->unsafe_block_expr.body = body;
    return n;
}

AstNode* ast_new_array_literal(Arena* arena, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_ARRAY_LITERAL, loc);
    ast_node_list_init(arena, &n->array_literal.elements);
    return n;
}

AstNode* ast_new_struct_literal(Arena* arena, TypeExpr* type, SourceLocation loc) {
    AstNode* n = NEW_NODE(arena, AST_STRUCT_LITERAL, loc);
    n->struct_literal.type = type;
    ast_node_list_init(arena, &n->struct_literal.fields);
    return n;
}

TypeExpr* type_new_named(Arena* arena, StringView name, SourceLocation loc) {
    TypeExpr* t = NEW_TYPE(arena, TYPE_NAMED, loc);
    t->named.name = name;
    type_expr_list_init(arena, &t->named.generic_args);
    return t;
}

TypeExpr* type_new_reference(Arena* arena, TypeExpr* child, bool is_const, SourceLocation loc) {
    TypeExpr* t = NEW_TYPE(arena, TYPE_REFERENCE, loc);
    t->unary.child = child;
    t->unary.is_const = is_const;
    return t;
}

TypeExpr* type_new_pointer(Arena* arena, TypeExpr* child, bool is_const, SourceLocation loc) {
    TypeExpr* t = NEW_TYPE(arena, TYPE_POINTER, loc);
    t->unary.child = child;
    t->unary.is_const = is_const;
    return t;
}

TypeExpr* type_new_slice(Arena* arena, TypeExpr* child, bool is_const, SourceLocation loc) {
    TypeExpr* t = NEW_TYPE(arena, TYPE_SLICE, loc);
    t->unary.child = child;
    t->unary.is_const = is_const;
    return t;
}

TypeExpr* type_new_array(Arena* arena, AstNode* length, TypeExpr* elem, SourceLocation loc) {
    TypeExpr* t = NEW_TYPE(arena, TYPE_ARRAY, loc);
    t->array.length = length;
    t->array.elem = elem;
    return t;
}

TypeExpr* type_new_optional(Arena* arena, TypeExpr* child, SourceLocation loc) {
    TypeExpr* t = NEW_TYPE(arena, TYPE_OPTIONAL, loc);
    t->unary.child = child;
    return t;
}

TypeExpr* type_new_error_union(Arena* arena, TypeExpr* child, SourceLocation loc) {
    TypeExpr* t = NEW_TYPE(arena, TYPE_ERROR_UNION, loc);
    t->unary.child = child;
    return t;
}

TypeExpr* type_new_function(Arena* arena, SourceLocation loc) {
    TypeExpr* t = NEW_TYPE(arena, TYPE_FUNCTION, loc);
    type_expr_list_init(arena, &t->func.params);
    return t;
}

void type_add_generic_arg(Arena* arena, TypeExpr* type, TypeExpr* arg) {
    type_expr_list_push(arena, &type->named.generic_args, arg);
}

void type_func_add_param(Arena* arena, TypeExpr* func, TypeExpr* param) {
    type_expr_list_push(arena, &func->func.params, param);
}

void type_func_set_ret(TypeExpr* func, TypeExpr* ret) {
    func->func.ret = ret;
}

Pattern* pattern_new_wildcard(Arena* arena, SourceLocation loc) {
    return NEW_PATTERN(arena, PATTERN_WILDCARD, loc);
}

Pattern* pattern_new_literal(Arena* arena, AstNode* lit, SourceLocation loc) {
    Pattern* p = NEW_PATTERN(arena, PATTERN_LITERAL, loc);
    p->literal = lit;
    return p;
}

Pattern* pattern_new_identifier(Arena* arena, StringView name, SourceLocation loc) {
    Pattern* p = NEW_PATTERN(arena, PATTERN_IDENTIFIER, loc);
    p->ident = name;
    return p;
}

Pattern* pattern_new_enum_variant(Arena* arena, StringView name, Pattern* inner, SourceLocation loc) {
    Pattern* p = NEW_PATTERN(arena, PATTERN_ENUM_VARIANT, loc);
    p->enum_variant.name = name;
    p->enum_variant.inner = inner;
    return p;
}

Pattern* pattern_new_struct_field(Arena* arena, SourceLocation loc) {
    Pattern* p = NEW_PATTERN(arena, PATTERN_STRUCT_FIELD, loc);
    string_view_list_init(arena, &p->struct_field.fields);
    pattern_list_init(arena, &p->struct_field.patterns);
    return p;
}

void pattern_struct_add_field(Arena* arena, Pattern* pat, StringView name, Pattern* field_pat) {
    string_view_list_push(arena, &pat->struct_field.fields, name);
    pattern_list_push(arena, &pat->struct_field.patterns, field_pat);
}

void ast_import_add_part(Arena* arena, AstNode* import_decl, StringView part) {
    string_view_list_push(arena, &import_decl->import_decl.parts, part);
}

void ast_import_set_alias(AstNode* import_decl, StringView alias) {
    import_decl->import_decl.alias = alias;
}

void ast_set_module(AstNode* unit, StringView name) {
    unit->compilation_unit.module_name = name;
}

void ast_add_import(Arena* arena, AstNode* unit, AstNode* import_decl) {
    ast_node_list_push(arena, &unit->compilation_unit.imports, import_decl);
}

void ast_add_decl(Arena* arena, AstNode* unit, AstNode* decl) {
    ast_node_list_push(arena, &unit->compilation_unit.decls, decl);
}

void ast_block_add_stmt(Arena* arena, AstNode* block, AstNode* stmt) {
    ast_node_list_push(arena, &block->block.stmts, stmt);
}

void ast_block_set_trailing(AstNode* block, AstNode* expr) {
    block->block.trailing_expr = expr;
}

void ast_fn_add_param(Arena* arena, AstNode* fn, AstNode* param) {
    ast_node_list_push(arena, &fn->fn_decl.params, param);
}

void ast_fn_add_generic(Arena* arena, AstNode* fn, AstNode* param) {
    ast_node_list_push(arena, &fn->fn_decl.generic_params, param);
}

void ast_struct_add_field(Arena* arena, AstNode* s, AstNode* field) {
    ast_node_list_push(arena, &s->struct_decl.fields, field);
}

void ast_struct_add_generic(Arena* arena, AstNode* s, AstNode* param) {
    ast_node_list_push(arena, &s->struct_decl.generic_params, param);
}

void ast_enum_add_variant(Arena* arena, AstNode* e, AstNode* variant) {
    ast_node_list_push(arena, &e->enum_decl.variants, variant);
}

void ast_traits_add_method(Arena* arena, AstNode* traits, AstNode* method) {
    ast_node_list_push(arena, &traits->traits_decl.methods, method);
}

void ast_extend_add_method(Arena* arena, AstNode* extend, AstNode* method) {
    ast_node_list_push(arena, &extend->extend_decl.methods, method);
}

void ast_extend_add_generic(Arena* arena, AstNode* extend, AstNode* param) {
    ast_node_list_push(arena, &extend->extend_decl.generic_params, param);
}

void ast_call_add_arg(Arena* arena, AstNode* call, AstNode* arg) {
    ast_node_list_push(arena, &call->call_expr.args, arg);
}

void ast_array_add_elem(Arena* arena, AstNode* arr, AstNode* elem) {
    ast_node_list_push(arena, &arr->array_literal.elements, elem);
}

void ast_struct_add_field_init(Arena* arena, AstNode* lit, AstNode* field_init) {
    ast_node_list_push(arena, &lit->struct_literal.fields, field_init);
}

void ast_match_add_arm(Arena* arena, AstNode* match, AstNode* arm) {
    ast_node_list_push(arena, &match->match_stmt.arms, arm);
}

void ast_extend_add_trait(Arena* arena, AstNode* extend, StringView trait) {
    string_view_list_push(arena, &extend->extend_decl.trait_names, trait);
}

bool sv_is_primitive_type(StringView sv) {
    static const char* primitives[] = {
        "void", "bool", "noreturn",
        "i8", "i16", "i32", "i64", "i128", "isize",
        "u8", "u16", "u32", "u64", "u128", "usize",
        "f32", "f64",
        NULL
    };
    for (size_t i = 0; primitives[i]; i++) {
        if (sv_eq_cstr(sv, primitives[i])) return true;
    }
    return false;
}

bool token_is_assignment_op(TokenKind kind) {
    return kind == TOK_ASSIGN || kind == TOK_PLUS_ASSIGN || kind == TOK_MINUS_ASSIGN ||
           kind == TOK_STAR_ASSIGN || kind == TOK_SLASH_ASSIGN || kind == TOK_PERCENT_ASSIGN ||
           kind == TOK_AND_ASSIGN || kind == TOK_OR_ASSIGN || kind == TOK_XOR_ASSIGN ||
           kind == TOK_SHL_ASSIGN || kind == TOK_SHR_ASSIGN;
}

/* ============================================================================
 * AST dump
 * ========================================================================== */

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) printf("  ");
}

static void print_sv(StringView sv) {
    printf("%.*s", (int)sv.len, sv.data);
}

static void print_type(TypeExpr* t);
static void print_node(AstNode* node, int indent);
static void print_pattern(Pattern* p);

static void print_type(TypeExpr* t) {
    if (!t) { printf("(TYPE null)"); return; }
    switch (t->kind) {
        case TYPE_NAMED:
            printf("(TYPE ");
            print_sv(t->named.name);
            if (t->named.generic_args.count > 0) {
                printf(" ");
                for (size_t i = 0; i < t->named.generic_args.count; i++) {
                    print_type(t->named.generic_args.items[i]);
                    if (i + 1 < t->named.generic_args.count) printf(" ");
                }
            }
            printf(")");
            break;
        case TYPE_REFERENCE:
            printf("(TYPE &%s", t->unary.is_const ? "const " : "");
            print_type(t->unary.child);
            printf(")");
            break;
        case TYPE_POINTER:
            printf("(TYPE *%s", t->unary.is_const ? "const " : "");
            print_type(t->unary.child);
            printf(")");
            break;
        case TYPE_SLICE:
            printf("(TYPE []%s", t->unary.is_const ? "const " : "");
            print_type(t->unary.child);
            printf(")");
            break;
        case TYPE_ARRAY:
            printf("(TYPE [");
            if (t->array.length) print_node(t->array.length, 0);
            printf("] ");
            print_type(t->array.elem);
            printf(")");
            break;
        case TYPE_OPTIONAL:
            printf("(TYPE ? ");
            print_type(t->unary.child);
            printf(")");
            break;
        case TYPE_ERROR_UNION:
            printf("(TYPE ! ");
            print_type(t->unary.child);
            printf(")");
            break;
        case TYPE_FUNCTION:
            printf("(TYPE fn(");
            for (size_t i = 0; i < t->func.params.count; i++) {
                print_type(t->func.params.items[i]);
                if (i + 1 < t->func.params.count) printf(" ");
            }
            printf(")");
            if (t->func.ret) {
                printf(" -> ");
                print_type(t->func.ret);
            }
            printf(")");
            break;
    }
}

static void print_pattern(Pattern* p) {
    if (!p) { printf("(PATTERN null)"); return; }
    switch (p->kind) {
        case PATTERN_WILDCARD:
            printf("(PATTERN _)");
            break;
        case PATTERN_LITERAL:
            printf("(PATTERN ");
            print_node(p->literal, 0);
            printf(")");
            break;
        case PATTERN_IDENTIFIER:
            printf("(PATTERN ");
            print_sv(p->ident);
            printf(")");
            break;
        case PATTERN_ENUM_VARIANT:
            printf("(PATTERN .");
            print_sv(p->enum_variant.name);
            if (p->enum_variant.inner) {
                printf(" ");
                print_pattern(p->enum_variant.inner);
            }
            printf(")");
            break;
        case PATTERN_STRUCT_FIELD:
            printf("(PATTERN .{");
            for (size_t i = 0; i < p->struct_field.fields.count; i++) {
                if (i > 0) printf(", ");
                print_sv(p->struct_field.fields.items[i]);
                printf(": ");
                print_pattern(p->struct_field.patterns.items[i]);
            }
            printf("})");
            break;
    }
}

static void print_node(AstNode* node, int indent) {
    if (!node) {
        print_indent(indent);
        printf("(null)\n");
        return;
    }
    print_indent(indent);
    switch (node->kind) {
        case AST_COMPILATION_UNIT:
            printf("(COMPILATION_UNIT");
            if (node->compilation_unit.module_name.len > 0) {
                printf(" module=");
                print_sv(node->compilation_unit.module_name);
            }
            printf("\n");
            for (size_t i = 0; i < node->compilation_unit.imports.count; i++)
                print_node(node->compilation_unit.imports.items[i], indent + 1);
            for (size_t i = 0; i < node->compilation_unit.decls.count; i++)
                print_node(node->compilation_unit.decls.items[i], indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_MODULE_DECL:
            printf("(MODULE_DECL "); print_sv(node->module_decl.name); printf(")\n");
            break;
        case AST_IMPORT_DECL:
            printf("(IMPORT_DECL ");
            for (size_t i = 0; i < node->import_decl.parts.count; i++) {
                if (i > 0) printf(".");
                print_sv(node->import_decl.parts.items[i]);
            }
            if (node->import_decl.alias.len > 0) {
                printf(" as "); print_sv(node->import_decl.alias);
            }
            printf(")\n");
            break;
        case AST_FN_DECL:
            printf("(FN_DECL%s%s ",
                   node->fn_decl.is_pub ? " pub" : "",
                   node->fn_decl.is_comptime ? " comptime" : "");
            print_sv(node->fn_decl.name);
            if (node->fn_decl.generic_params.count > 0) {
                printf(" (GENERIC");
                for (size_t i = 0; i < node->fn_decl.generic_params.count; i++) {
                    printf(" ");
                    print_sv(node->fn_decl.generic_params.items[i]->generic_param_decl.name);
                }
                printf(")");
            }
            printf("\n");
            for (size_t i = 0; i < node->fn_decl.params.count; i++)
                print_node(node->fn_decl.params.items[i], indent + 1);
            if (node->fn_decl.ret_type) {
                print_indent(indent + 1); printf("-> "); print_type(node->fn_decl.ret_type); printf("\n");
            }
            if (node->fn_decl.body) {
                print_node(node->fn_decl.body, indent + 1);
            } else {
                print_indent(indent + 1); printf("(EXTERN)\n");
            }
            print_indent(indent); printf(")\n");
            break;
        case AST_STRUCT_DECL:
            printf("(STRUCT_DECL%s ", node->struct_decl.is_pub ? " pub" : "");
            print_sv(node->struct_decl.name);
            printf("\n");
            for (size_t i = 0; i < node->struct_decl.fields.count; i++)
                print_node(node->struct_decl.fields.items[i], indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_UNION_DECL:
            printf("(UNION_DECL%s ", node->struct_decl.is_pub ? " pub" : "");
            print_sv(node->struct_decl.name);
            printf("\n");
            for (size_t i = 0; i < node->struct_decl.fields.count; i++)
                print_node(node->struct_decl.fields.items[i], indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_ENUM_DECL:
            printf("(ENUM_DECL%s ", node->enum_decl.is_pub ? " pub" : "");
            print_sv(node->enum_decl.name);
            printf("\n");
            for (size_t i = 0; i < node->enum_decl.variants.count; i++)
                print_node(node->enum_decl.variants.items[i], indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_TRAITS_DECL:
            printf("(TRAITS_DECL%s ", node->traits_decl.is_pub ? " pub" : "");
            print_sv(node->traits_decl.name);
            printf("\n");
            for (size_t i = 0; i < node->traits_decl.methods.count; i++)
                print_node(node->traits_decl.methods.items[i], indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_EXTEND_DECL:
            printf("(EXTEND_DECL ");
            print_sv(node->extend_decl.target_name);
            if (node->extend_decl.trait_names.count > 0) {
                printf(" with ");
                for (size_t i = 0; i < node->extend_decl.trait_names.count; i++) {
                    if (i > 0) printf(", ");
                    print_sv(node->extend_decl.trait_names.items[i]);
                }
            }
            printf("\n");
            for (size_t i = 0; i < node->extend_decl.methods.count; i++)
                print_node(node->extend_decl.methods.items[i], indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_TYPE_ALIAS:
            printf("(TYPE_ALIAS%s ", node->type_alias.is_pub ? " pub" : "");
            print_sv(node->type_alias.name);
            printf(" = "); print_type(node->type_alias.type); printf(")\n");
            break;
        case AST_CONST_DECL:
            printf("(CONST_DECL%s ", node->var_decl.is_pub ? " pub" : "");
            print_sv(node->var_decl.name);
            if (node->var_decl.type) { printf(" : "); print_type(node->var_decl.type); }
            printf(" =\n");
            print_node(node->var_decl.init, indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_VAR_DECL:
            printf("(VAR_DECL%s ", node->var_decl.is_pub ? " pub" : "");
            print_sv(node->var_decl.name);
            if (node->var_decl.type) { printf(" : "); print_type(node->var_decl.type); }
            printf(" =\n");
            print_node(node->var_decl.init, indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_TEST_DECL:
            printf("(TEST_DECL "); print_sv(node->test_decl.name); printf("\n");
            print_node(node->test_decl.body, indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_FIELD_DECL:
            printf("(FIELD_DECL%s ", node->field_decl.is_pub ? " pub" : "");
            print_sv(node->field_decl.name);
            printf(" : "); print_type(node->field_decl.type);
            if (node->field_decl.default_value) {
                printf(" = "); print_node(node->field_decl.default_value, 0);
            }
            printf(")\n");
            break;
        case AST_VARIANT_DECL:
            printf("(VARIANT_DECL "); print_sv(node->variant_decl.name);
            if (node->variant_decl.payload_type) {
                printf(" "); print_type(node->variant_decl.payload_type);
            }
            if (node->variant_decl.discriminant) {
                printf(" = "); print_node(node->variant_decl.discriminant, 0);
            }
            printf(")\n");
            break;
        case AST_TRAIT_METHOD_DECL:
            printf("(TRAIT_METHOD_DECL%s ", node->trait_method_decl.is_pub ? " pub" : "");
            print_sv(node->trait_method_decl.name);
            printf("\n");
            for (size_t i = 0; i < node->trait_method_decl.params.count; i++)
                print_node(node->trait_method_decl.params.items[i], indent + 1);
            if (node->trait_method_decl.ret_type) {
                print_indent(indent + 1); printf("-> "); print_type(node->trait_method_decl.ret_type); printf("\n");
            }
            print_indent(indent); printf(")\n");
            break;
        case AST_PARAM_DECL:
            printf("(PARAM_DECL%s ", node->param_decl.is_self ? " self" : "");
            print_sv(node->param_decl.name);
            if (node->param_decl.type) { printf(" : "); print_type(node->param_decl.type); }
            if (node->param_decl.default_value) { printf(" = "); print_node(node->param_decl.default_value, 0); }
            printf(")\n");
            break;
        case AST_GENERIC_PARAM_DECL:
            printf("(GENERIC_PARAM_DECL "); print_sv(node->generic_param_decl.name);
            if (node->generic_param_decl.trait_constraints.count > 0) {
                printf(" with ");
                for (size_t i = 0; i < node->generic_param_decl.trait_constraints.count; i++) {
                    if (i > 0) printf(", ");
                    print_sv(node->generic_param_decl.trait_constraints.items[i]);
                }
            }
            printf(")\n");
            break;
        case AST_BLOCK:
            printf("(BLOCK\n");
            for (size_t i = 0; i < node->block.stmts.count; i++)
                print_node(node->block.stmts.items[i], indent + 1);
            if (node->block.trailing_expr) {
                print_indent(indent + 1); printf("(TRAILING\n");
                print_node(node->block.trailing_expr, indent + 2);
                print_indent(indent + 1); printf(")\n");
            }
            print_indent(indent); printf(")\n");
            break;
        case AST_EXPR_STMT:
            printf("(EXPR_STMT\n");
            print_node(node->expr_stmt.expr, indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_RETURN_STMT:
            printf("(RETURN_STMT\n");
            if (node->return_stmt.value) print_node(node->return_stmt.value, indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_IF_STMT:
            printf("(IF_STMT\n");
            print_node(node->if_stmt.condition, indent + 1);
            print_node(node->if_stmt.then_block, indent + 1);
            if (node->if_stmt.else_block) print_node(node->if_stmt.else_block, indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_WHILE_STMT:
            printf("(WHILE_STMT\n");
            print_node(node->while_stmt.condition, indent + 1);
            print_node(node->while_stmt.body, indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_FOR_STMT:
            printf("(FOR_STMT "); print_sv(node->for_stmt.var_name);
            printf(" : "); print_type(node->for_stmt.var_type);
            printf(" in\n");
            print_node(node->for_stmt.iterable, indent + 1);
            print_node(node->for_stmt.body, indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_DEFER_STMT:
            printf("(DEFER_STMT\n");
            print_node(node->defer_stmt.expr, indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_ERRDEFER_STMT:
            printf("(ERRDEFER_STMT\n");
            print_node(node->errdefer_stmt.body, indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_BREAK_STMT:
            printf("(BREAK_STMT)\n"); break;
        case AST_CONTINUE_STMT:
            printf("(CONTINUE_STMT)\n"); break;
        case AST_MATCH_STMT:
            printf("(MATCH_STMT\n");
            print_node(node->match_stmt.expr, indent + 1);
            for (size_t i = 0; i < node->match_stmt.arms.count; i++)
                print_node(node->match_stmt.arms.items[i], indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_MATCH_ARM:
            printf("(MATCH_ARM "); print_pattern(node->match_arm.pattern);
            printf(" =>\n");
            print_node(node->match_arm.expr, indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_ASSIGN_STMT:
            printf("(ASSIGN_STMT %s\n", token_kind_name(node->assign_stmt.op));
            print_node(node->assign_stmt.lhs, indent + 1);
            print_node(node->assign_stmt.rhs, indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_INT_LITERAL:
            printf("(INT_LITERAL %lld)\n", (long long)node->int_literal.value); break;
        case AST_FLOAT_LITERAL:
            printf("(FLOAT_LITERAL %g)\n", node->float_literal.value); break;
        case AST_STRING_LITERAL:
            printf("(STRING_LITERAL "); print_sv(node->string_literal.value); printf(")\n"); break;
        case AST_CHAR_LITERAL:
            printf("(CHAR_LITERAL "); print_sv(node->char_literal.value); printf(")\n"); break;
        case AST_BOOL_LITERAL:
            printf("(BOOL_LITERAL %s)\n", node->bool_literal.value ? "true" : "false"); break;
        case AST_NULL_LITERAL:
            printf("(NULL_LITERAL)\n"); break;
        case AST_UNDEFINED_LITERAL:
            printf("(UNDEFINED_LITERAL)\n"); break;
        case AST_IDENTIFIER:
            printf("(IDENTIFIER "); print_sv(node->identifier.name); printf(")\n"); break;
        case AST_BINARY_EXPR:
            printf("(BINARY %s\n", token_kind_name(node->binary_expr.op));
            print_node(node->binary_expr.left, indent + 1);
            print_node(node->binary_expr.right, indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_UNARY_EXPR:
            printf("(UNARY %s\n", token_kind_name(node->unary_expr.op));
            print_node(node->unary_expr.operand, indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_CALL_EXPR:
            printf("(CALL_EXPR\n");
            print_node(node->call_expr.callee, indent + 1);
            for (size_t i = 0; i < node->call_expr.args.count; i++)
                print_node(node->call_expr.args.items[i], indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_METHOD_CALL_EXPR:
            printf("(METHOD_CALL_EXPR "); print_sv(node->method_call_expr.method_name); printf("\n");
            print_node(node->method_call_expr.receiver, indent + 1);
            for (size_t i = 0; i < node->method_call_expr.args.count; i++)
                print_node(node->method_call_expr.args.items[i], indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_FIELD_ACCESS_EXPR:
            printf("(FIELD_ACCESS_EXPR "); print_sv(node->field_access_expr.field_name); printf("\n");
            print_node(node->field_access_expr.object, indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_INDEX_EXPR:
            printf("(INDEX_EXPR\n");
            print_node(node->index_expr.object, indent + 1);
            print_node(node->index_expr.index, indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_SLICE_EXPR:
            printf("(SLICE_EXPR\n");
            print_node(node->slice_expr.object, indent + 1);
            print_node(node->slice_expr.start, indent + 1);
            print_node(node->slice_expr.end, indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_CAST_EXPR:
            printf("(CAST_EXPR\n");
            print_node(node->cast_expr.expr, indent + 1);
            print_indent(indent + 1); print_type(node->cast_expr.type); printf("\n");
            print_indent(indent); printf(")\n");
            break;
        case AST_TRY_EXPR:
            printf("(TRY_EXPR\n");
            print_node(node->try_expr.expr, indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_ERROR_CAPTURE_EXPR:
            printf("(ERROR_CAPTURE_EXPR "); print_sv(node->error_capture_expr.err_name); printf("\n");
            print_node(node->error_capture_expr.expr, indent + 1);
            print_node(node->error_capture_expr.fallback, indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_UNSAFE_BLOCK_EXPR:
            printf("(UNSAFE_BLOCK_EXPR\n");
            print_node(node->unsafe_block_expr.body, indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_ARRAY_LITERAL:
            printf("(ARRAY_LITERAL%s ", node->array_literal.sentinel ? " sentinel" : "");
            if (node->array_literal.length) { print_node(node->array_literal.length, 0); printf(" "); }
            print_type(node->array_literal.explicit_type); printf("\n");
            for (size_t i = 0; i < node->array_literal.elements.count; i++)
                print_node(node->array_literal.elements.items[i], indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_STRUCT_LITERAL:
            printf("(STRUCT_LITERAL "); print_type(node->struct_literal.type); printf("\n");
            for (size_t i = 0; i < node->struct_literal.fields.count; i++)
                print_node(node->struct_literal.fields.items[i], indent + 1);
            print_indent(indent); printf(")\n");
            break;
        case AST_ARG_LIST:
            printf("(ARG_LIST\n");
            for (size_t i = 0; i < node->arg_list.args.count; i++)
                print_node(node->arg_list.args.items[i], indent + 1);
            print_indent(indent); printf(")\n");
            break;
        default:
            printf("(UNKNOWN_KIND_%d)\n", node->kind);
            break;
    }
}

void ast_print(AstNode* node, int indent) {
    print_node(node, indent);
}
