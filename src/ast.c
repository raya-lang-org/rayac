#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arena.h" 
/* ============================================================================
 * Arena allocation state & helpers
 * ========================================================================== */

static Arena* g_ast_arena = NULL;

void ast_set_arena(Arena* arena) {
    g_ast_arena = arena;
}

void* ast_alloc(size_t size) {
    if (g_ast_arena) {
        return arena_alloc(g_ast_arena, size);
    }
    void* ptr = calloc(1, size);
    if (!ptr) {
        fprintf(stderr, "Out of memory in ast_alloc\n");
        exit(1);
    }
    return ptr;
}

void* ast_alloc_n(size_t count, size_t size) {
    return ast_alloc(count * size);
}

char* ast_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* dup = (char*)ast_alloc(len + 1);
    memcpy(dup, s, len);
    dup[len] = '\0';
    return dup;
}

static AstNode* alloc_ast_node(AstNodeKind kind, SourceLocation loc) {
    AstNode* node = (AstNode*)ast_alloc(sizeof(AstNode));
    node->kind = kind;
    node->loc = loc;
    return node;
}

static TypeExpr* alloc_type_expr(TypeExprKind kind, SourceLocation loc) {
    TypeExpr* type = (TypeExpr*)ast_alloc(sizeof(TypeExpr));
    type->kind = kind;
    type->loc = loc;
    return type;
}

static Pattern* alloc_pattern(PatternKind kind, SourceLocation loc) {
    Pattern* pat = (Pattern*)ast_alloc(sizeof(Pattern));
    pat->kind = kind;
    pat->loc = loc;
    return pat;
}

/* ============================================================================
 * Constructors: AstNode
 * ========================================================================== */

AstNode* ast_new_module_decl(StringView name, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_MODULE_DECL, loc);
    node->as.module_decl.name = name;
    return node;
}

AstNode* ast_new_import_decl(StringView name, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_IMPORT_DECL, loc);
    node->as.import_decl.name = name;
    return node;
}

AstNode* ast_new_fn_decl(StringView name, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_FN_DECL, loc);
    node->as.fn_decl.name = name;
    return node;
}

AstNode* ast_new_struct_decl(StringView name, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_STRUCT_DECL, loc);
    node->as.struct_decl.name = name;
    return node;
}

AstNode* ast_new_enum_decl(StringView name, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_ENUM_DECL, loc);
    node->as.enum_decl.name = name;
    return node;
}

AstNode* ast_new_union_decl(StringView name, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_UNION_DECL, loc);
    node->as.enum_decl.name = name;
    return node;
}

AstNode* ast_new_trait_decl(StringView name, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_TRAIT_DECL, loc);
    node->as.trait_decl.name = name;
    return node;
}

AstNode* ast_new_extend_decl(StringView target, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_EXTEND_DECL, loc);
    node->as.extend_decl.target_name = target;
    return node;
}

AstNode* ast_new_type_alias(StringView name, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_TYPE_ALIAS, loc);
    node->as.type_alias.name = name;
    return node;
}

AstNode* ast_new_test_decl(StringView name, AstNode* body, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_TEST_DECL, loc);
    node->as.test_decl.name = name;
    node->as.test_decl.body = body;
    return node;
}

AstNode* ast_new_const_decl(StringView name, TypeExpr* type_, AstNode* init, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_CONST_DECL, loc);
    node->as.var_decl.name = name;
    node->as.var_decl.type_ = type_;
    node->as.var_decl.initializer = init;
    return node;
}

AstNode* ast_new_var_decl(StringView name, TypeExpr* type_, AstNode* init, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_VAR_DECL, loc);
    node->as.var_decl.name = name;
    node->as.var_decl.type_ = type_;
    node->as.var_decl.initializer = init;
    return node;
}

AstNode* ast_new_block(SourceLocation loc) {
    return alloc_ast_node(AST_BLOCK, loc);
}

AstNode* ast_new_return(AstNode* value, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_RETURN, loc);
    node->as.return_stmt.value = value;
    return node;
}

AstNode* ast_new_if(AstNode* cond, AstNode* then_, AstNode* else_, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_IF, loc);
    node->as.if_stmt.condition = cond;
    node->as.if_stmt.then_block = then_;
    node->as.if_stmt.else_block = else_;
    return node;
}

AstNode* ast_new_while(AstNode* cond, AstNode* body, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_WHILE, loc);
    node->as.while_stmt.condition = cond;
    node->as.while_stmt.body = body;
    return node;
}

AstNode* ast_new_for(StringView var, TypeExpr* type_, AstNode* iter, AstNode* body, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_FOR, loc);
    node->as.for_stmt.var_name = var;
    node->as.for_stmt.var_type = type_;
    node->as.for_stmt.iterable = iter;
    node->as.for_stmt.body = body;
    return node;
}

AstNode* ast_new_defer(AstNode* body, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_DEFER, loc);
    node->as.defer_stmt.body = body;
    return node;
}

AstNode* ast_new_errdefer(AstNode* body, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_ERRDEFER, loc);
    node->as.defer_stmt.body = body;
    return node;
}

AstNode* ast_new_break(SourceLocation loc) {
    return alloc_ast_node(AST_BREAK, loc);
}

AstNode* ast_new_continue(SourceLocation loc) {
    return alloc_ast_node(AST_CONTINUE, loc);
}

AstNode* ast_new_match(AstNode* value, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_MATCH, loc);
    node->as.match_stmt.value = value;
    return node;
}

AstNode* ast_new_assign(TokenKind op, AstNode* left, AstNode* right, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_ASSIGN, loc);
    node->as.assign.op = op;
    node->as.assign.left = left;
    node->as.assign.right = right;
    return node;
}

AstNode* ast_new_expr_stmt(AstNode* expr, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_EXPR_STMT, loc);
    node->as.expr_stmt.expr = expr;
    return node;
}

AstNode* ast_new_int_literal(int64_t value, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_INT_LITERAL, loc);
    node->as.int_literal.value = value;
    return node;
}

AstNode* ast_new_float_literal(double value, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_FLOAT_LITERAL, loc);
    node->as.float_literal.value = value;
    return node;
}

AstNode* ast_new_string_literal(StringView value, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_STRING_LITERAL, loc);
    node->as.string_literal.value = value;
    return node;
}

AstNode* ast_new_char_literal(uint8_t value, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_CHAR_LITERAL, loc);
    node->as.char_literal.value = value;
    return node;
}

AstNode* ast_new_bool_literal(bool value, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_BOOL_LITERAL, loc);
    node->as.bool_literal.value = value;
    return node;
}

AstNode* ast_new_null_literal(SourceLocation loc) {
    return alloc_ast_node(AST_NULL_LITERAL, loc);
}

AstNode* ast_new_undefined_literal(SourceLocation loc) {
    return alloc_ast_node(AST_UNDEFINED_LITERAL, loc);
}

AstNode* ast_new_identifier(StringView name, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_IDENTIFIER, loc);
    node->as.identifier.name = name;
    return node;
}

AstNode* ast_new_binary(TokenKind op, AstNode* left, AstNode* right, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_BINARY, loc);
    node->as.binary.op = op;
    node->as.binary.left = left;
    node->as.binary.right = right;
    return node;
}

AstNode* ast_new_unary(TokenKind op, AstNode* operand, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_UNARY, loc);
    node->as.unary.op = op;
    node->as.unary.operand = operand;
    return node;
}

AstNode* ast_new_field_access(AstNode* object, StringView field, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_FIELD_ACCESS, loc);
    node->as.field_access.object = object;
    node->as.field_access.field = field;
    return node;
}

AstNode* ast_new_method_call(AstNode* object, StringView method, AstNode** args, size_t arg_count, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_METHOD_CALL, loc);
    node->as.method_call.object = object;
    node->as.method_call.method = method;
    node->as.method_call.args = args;
    node->as.method_call.arg_count = arg_count;
    return node;
}

AstNode* ast_new_call(AstNode* callee, AstNode** args, size_t arg_count, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_CALL, loc);
    node->as.call.callee = callee;
    node->as.call.args = args;
    node->as.call.arg_count = arg_count;
    return node;
}

AstNode* ast_new_index(AstNode* object, AstNode* index, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_INDEX, loc);
    node->as.index.object = object;
    node->as.index.index = index;
    return node;
}

AstNode* ast_new_slice(AstNode* object, AstNode* start, AstNode* end, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_SLICE, loc);
    node->as.slice.object = object;
    node->as.slice.start = start;
    node->as.slice.end = end;
    return node;
}

AstNode* ast_new_cast(AstNode* expr, TypeExpr* type_, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_CAST, loc);
    node->as.cast.expr = expr;
    node->as.cast.type_ = type_;
    return node;
}

AstNode* ast_new_try(AstNode* expr, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_TRY, loc);
    node->as.try_expr.expr = expr;
    node->as.try_expr.has_else = false;
    return node;
}

AstNode* ast_new_try_else(AstNode* expr, StringView err_name, AstNode* else_block, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_TRY_ELSE, loc);
    node->as.try_expr.expr = expr;
    node->as.try_expr.err_name = err_name;
    node->as.try_expr.else_block = else_block;
    node->as.try_expr.has_else = true;
    return node;
}

AstNode* ast_new_unsafe_block(AstNode* block, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_UNSAFE_BLOCK, loc);
    node->as.unsafe_block.block = block;
    return node;
}

AstNode* ast_new_addr_of(AstNode* operand, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_ADDR_OF, loc);
    node->as.unary.operand = operand;
    return node;
}

AstNode* ast_new_addr_of_const(AstNode* operand, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_ADDR_OF_CONST, loc);
    node->as.unary.operand = operand;
    return node;
}

AstNode* ast_new_deref(AstNode* operand, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_DEREF, loc);
    node->as.unary.operand = operand;
    return node;
}

AstNode* ast_new_array_literal(TypeExpr* type_, AstNode** elems, size_t elem_count, AstNode* count, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_ARRAY_LITERAL, loc);
    node->as.array_literal.type_ = type_;
    node->as.array_literal.elems = elems;
    node->as.array_literal.elem_count = elem_count;
    node->as.array_literal.count = count;
    return node;
}

AstNode* ast_new_struct_literal(TypeExpr* type_, SourceLocation loc) {
    AstNode* node = alloc_ast_node(AST_STRUCT_LITERAL, loc);
    node->as.struct_literal.type_ = type_;
    return node;
}

/* ============================================================================
 * Constructors: TypeExpr
 * ========================================================================== */

TypeExpr* type_new_named(StringView name, SourceLocation loc) {
    TypeExpr* type = alloc_type_expr(TYPE_NAMED, loc);
    type->as.named.name = name;
    return type;
}

TypeExpr* type_new_primitive(StringView name, SourceLocation loc) {
    TypeExpr* type = alloc_type_expr(TYPE_PRIMITIVE, loc);
    type->as.primitive.name = name;
    return type;
}

TypeExpr* type_new_pointer(TypeExpr* pointee, bool is_const, SourceLocation loc) {
    TypeExpr* type = alloc_type_expr(TYPE_POINTER, loc);
    type->as.ptr.pointee = pointee;
    type->as.ptr.is_const = is_const;
    return type;
}

TypeExpr* type_new_reference(TypeExpr* pointee, bool is_const, SourceLocation loc) {
    TypeExpr* type = alloc_type_expr(TYPE_REFERENCE, loc);
    type->as.ref.pointee = pointee;
    type->as.ref.is_const = is_const;
    return type;
}

TypeExpr* type_new_slice(TypeExpr* element, bool is_const, SourceLocation loc) {
    TypeExpr* type = alloc_type_expr(TYPE_SLICE, loc);
    type->as.slice.element = element;
    type->as.slice.is_const = is_const;
    return type;
}

TypeExpr* type_new_array(AstNode* size, TypeExpr* element, SourceLocation loc) {
    TypeExpr* type = alloc_type_expr(TYPE_ARRAY, loc);
    type->as.array.size = size;
    type->as.array.element = element;
    return type;
}

TypeExpr* type_new_optional(TypeExpr* inner, SourceLocation loc) {
    TypeExpr* type = alloc_type_expr(TYPE_OPTIONAL, loc);
    type->as.optional.inner = inner;
    return type;
}

TypeExpr* type_new_error_union(TypeExpr* inner, SourceLocation loc) {
    TypeExpr* type = alloc_type_expr(TYPE_ERROR_UNION, loc);
    type->as.error_union.inner = inner;
    return type;
}

TypeExpr* type_new_function(TypeExpr** params, size_t param_count, TypeExpr* ret, SourceLocation loc) {
    TypeExpr* type = alloc_type_expr(TYPE_FUNCTION, loc);
    type->as.function.params = params;
    type->as.function.param_count = param_count;
    type->as.function.ret = ret;
    return type;
}

/* ============================================================================
 * Constructors: Pattern
 * ========================================================================== */

Pattern* pat_new_wildcard(SourceLocation loc) {
    return alloc_pattern(PAT_WILDCARD, loc);
}

Pattern* pat_new_literal(AstNode* literal, SourceLocation loc) {
    Pattern* pat = alloc_pattern(PAT_LITERAL, loc);
    pat->as.literal.literal = literal;
    return pat;
}

Pattern* pat_new_identifier(StringView name, SourceLocation loc) {
    Pattern* pat = alloc_pattern(PAT_IDENTIFIER, loc);
    pat->as.ident.name = name;
    return pat;
}

Pattern* pat_new_enum_variant(StringView name, Pattern** fields, size_t field_count, SourceLocation loc) {
    Pattern* pat = alloc_pattern(PAT_ENUM_VARIANT, loc);
    pat->as.enum_variant.name = name;
    pat->as.enum_variant.fields = fields;
    pat->as.enum_variant.field_count = field_count;
    return pat;
}

Pattern* pat_new_struct_field(StringView name, Pattern** fields, size_t field_count, SourceLocation loc) {
    Pattern* pat = alloc_pattern(PAT_STRUCT_FIELD, loc);
    pat->as.struct_field.name = name;
    pat->as.struct_field.fields = fields;
    pat->as.struct_field.field_count = field_count;
    return pat;
}

/* ============================================================================
 * AST Debug Printing Implementation
 * ========================================================================== */

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

static void print_sv(StringView sv) {
    if (sv.data) {
        printf("%.*s", (int)sv.len, sv.data);
    } else {
        printf("<null>");
    }
}

static void type_print(TypeExpr* type, int indent) {
    if (!type) {
        print_indent(indent);
        printf("(null type)\n");
        return;
    }

    print_indent(indent);
    switch (type->kind) {
        case TYPE_NAMED:
            printf("TypeNamed: ");
            print_sv(type->as.named.name);
            printf("\n");
            for (size_t i = 0; i < type->as.named.arg_count; i++) {
                type_print(type->as.named.args[i], indent + 1);
            }
            break;
        case TYPE_PRIMITIVE:
            printf("TypePrimitive: ");
            print_sv(type->as.primitive.name);
            printf("\n");
            break;
        case TYPE_POINTER:
            printf("TypePointer (const: %s)\n", type->as.ptr.is_const ? "true" : "false");
            type_print(type->as.ptr.pointee, indent + 1);
            break;
        case TYPE_REFERENCE:
            printf("TypeReference (const: %s)\n", type->as.ref.is_const ? "true" : "false");
            type_print(type->as.ref.pointee, indent + 1);
            break;
        case TYPE_SLICE:
            printf("TypeSlice (const: %s)\n", type->as.slice.is_const ? "true" : "false");
            type_print(type->as.slice.element, indent + 1);
            break;
        case TYPE_ARRAY:
            printf("TypeArray\n");
            type_print(type->as.array.element, indent + 1);
            if (type->as.array.size) {
                ast_print(type->as.array.size, indent + 1);
            }
            break;
        case TYPE_OPTIONAL:
            printf("TypeOptional\n");
            type_print(type->as.optional.inner, indent + 1);
            break;
        case TYPE_ERROR_UNION:
            printf("TypeErrorUnion\n");
            type_print(type->as.error_union.inner, indent + 1);
            break;
        case TYPE_FUNCTION:
            printf("TypeFunction\n");
            for (size_t i = 0; i < type->as.function.param_count; i++) {
                type_print(type->as.function.params[i], indent + 1);
            }
            type_print(type->as.function.ret, indent + 1);
            break;
    }
}

static void pat_print(Pattern* pat, int indent) {
    if (!pat) {
        print_indent(indent);
        printf("(null pattern)\n");
        return;
    }

    print_indent(indent);
    switch (pat->kind) {
        case PAT_WILDCARD:
            printf("PatWildcard (_)\n");
            break;
        case PAT_LITERAL:
            printf("PatLiteral\n");
            ast_print(pat->as.literal.literal, indent + 1);
            break;
        case PAT_IDENTIFIER:
            printf("PatIdentifier: ");
            print_sv(pat->as.ident.name);
            printf("\n");
            break;
        case PAT_ENUM_VARIANT:
            printf("PatEnumVariant: ");
            print_sv(pat->as.enum_variant.name);
            printf("\n");
            for (size_t i = 0; i < pat->as.enum_variant.field_count; i++) {
                pat_print(pat->as.enum_variant.fields[i], indent + 1);
            }
            break;
        case PAT_STRUCT_FIELD:
            printf("PatStructField: ");
            print_sv(pat->as.struct_field.name);
            printf("\n");
            for (size_t i = 0; i < pat->as.struct_field.field_count; i++) {
                pat_print(pat->as.struct_field.fields[i], indent + 1);
            }
            break;
    }
}

void ast_print(AstNode* node, int indent) {
    if (!node) {
        print_indent(indent);
        printf("(null node)\n");
        return;
    }

    print_indent(indent);

    switch (node->kind) {
        case AST_MODULE_DECL:
            printf("ModuleDecl: ");
            print_sv(node->as.module_decl.name);
            printf("\n");
            break;

        case AST_IMPORT_DECL:
            printf("ImportDecl: ");
            print_sv(node->as.import_decl.name);
            if (node->as.import_decl.has_alias) {
                printf(" as ");
                print_sv(node->as.import_decl.alias);
            }
            printf("\n");
            break;

        case AST_FN_DECL:
            printf("FnDecl: ");
            print_sv(node->as.fn_decl.name);
            printf(" (pub: %s, comptime: %s, unsafe: %s, extern: %s)\n",
                   node->as.fn_decl.is_pub ? "true" : "false",
                   node->as.fn_decl.is_comptime ? "true" : "false",
                   node->as.fn_decl.is_unsafe ? "true" : "false",
                   node->as.fn_decl.is_extern ? "true" : "false");
            if (node->as.fn_decl.return_type) {
                type_print(node->as.fn_decl.return_type, indent + 1);
            }
            if (node->as.fn_decl.body) {
                ast_print(node->as.fn_decl.body, indent + 1);
            }
            break;

        case AST_STRUCT_DECL:
            printf("StructDecl: ");
            print_sv(node->as.struct_decl.name);
            printf("\n");
            for (size_t i = 0; i < node->as.struct_decl.field_count; i++) {
                print_indent(indent + 1);
                printf("Field: ");
                print_sv(node->as.struct_decl.fields[i].name);
                printf("\n");
                if (node->as.struct_decl.fields[i].type_) {
                    type_print(node->as.struct_decl.fields[i].type_, indent + 2);
                }
            }
            break;

        case AST_ENUM_DECL:
            printf("EnumDecl: ");
            print_sv(node->as.enum_decl.name);
            printf("\n");
            for (size_t i = 0; i < node->as.enum_decl.variant_count; i++) {
                print_indent(indent + 1);
                printf("Variant: ");
                print_sv(node->as.enum_decl.variants[i].name);
                printf("\n");
                if (node->as.enum_decl.variants[i].payload_type) {
                    type_print(node->as.enum_decl.variants[i].payload_type, indent + 2);
                }
            }
            break;

        case AST_UNION_DECL:
            printf("UnionDecl: ");
            print_sv(node->as.enum_decl.name);
            printf("\n");
            break;

        case AST_TRAIT_DECL:
            printf("TraitDecl: ");
            print_sv(node->as.trait_decl.name);
            printf("\n");
            for (size_t i = 0; i < node->as.trait_decl.method_count; i++) {
                ast_print(node->as.trait_decl.methods[i], indent + 1);
            }
            break;

        case AST_EXTEND_DECL:
            printf("ExtendDecl target: ");
            print_sv(node->as.extend_decl.target_name);
            printf("\n");
            for (size_t i = 0; i < node->as.extend_decl.method_count; i++) {
                ast_print(node->as.extend_decl.methods[i], indent + 1);
            }
            break;

        case AST_TYPE_ALIAS:
            printf("TypeAlias: ");
            print_sv(node->as.type_alias.name);
            printf("\n");
            if (node->as.type_alias.type_) {
                type_print(node->as.type_alias.type_, indent + 1);
            }
            break;

        case AST_TEST_DECL:
            printf("TestDecl: ");
            print_sv(node->as.test_decl.name);
            printf("\n");
            if (node->as.test_decl.body) {
                ast_print(node->as.test_decl.body, indent + 1);
            }
            break;

        case AST_CONST_DECL:
            printf("ConstDecl: ");
            print_sv(node->as.var_decl.name);
            printf("\n");
            if (node->as.var_decl.type_) {
                type_print(node->as.var_decl.type_, indent + 1);
            }
            if (node->as.var_decl.initializer) {
                ast_print(node->as.var_decl.initializer, indent + 1);
            }
            break;

        case AST_VAR_DECL:
            printf("VarDecl: ");
            print_sv(node->as.var_decl.name);
            printf("\n");
            if (node->as.var_decl.type_) {
                type_print(node->as.var_decl.type_, indent + 1);
            }
            if (node->as.var_decl.initializer) {
                ast_print(node->as.var_decl.initializer, indent + 1);
            }
            break;

        case AST_BLOCK:
            printf("Block\n");
            for (size_t i = 0; i < node->as.block.stmt_count; i++) {
                ast_print(node->as.block.stmts[i], indent + 1);
            }
            if (node->as.block.trailing_expr) {
                print_indent(indent + 1);
                printf("TrailingExpr:\n");
                ast_print(node->as.block.trailing_expr, indent + 2);
            }
            break;

        case AST_RETURN:
            printf("ReturnStmt\n");
            if (node->as.return_stmt.value) {
                ast_print(node->as.return_stmt.value, indent + 1);
            }
            break;

        case AST_IF:
            printf("IfStmt\n");
            ast_print(node->as.if_stmt.condition, indent + 1);
            ast_print(node->as.if_stmt.then_block, indent + 1);
            if (node->as.if_stmt.else_block) {
                ast_print(node->as.if_stmt.else_block, indent + 1);
            }
            break;

        case AST_WHILE:
            printf("WhileStmt\n");
            ast_print(node->as.while_stmt.condition, indent + 1);
            ast_print(node->as.while_stmt.body, indent + 1);
            break;

        case AST_FOR:
            printf("ForStmt var: ");
            print_sv(node->as.for_stmt.var_name);
            printf("\n");
            ast_print(node->as.for_stmt.iterable, indent + 1);
            ast_print(node->as.for_stmt.body, indent + 1);
            break;

        case AST_DEFER:
            printf("DeferStmt\n");
            ast_print(node->as.defer_stmt.body, indent + 1);
            break;

        case AST_ERRDEFER:
            printf("ErrDeferStmt\n");
            ast_print(node->as.defer_stmt.body, indent + 1);
            break;

        case AST_BREAK:
            printf("Break\n");
            break;

        case AST_CONTINUE:
            printf("Continue\n");
            break;

        case AST_MATCH:
            printf("MatchStmt\n");
            if (node->as.match_stmt.value) {
                ast_print(node->as.match_stmt.value, indent + 1);
            }
            for (size_t i = 0; i < node->as.match_stmt.arm_count; i++) {
                print_indent(indent + 1);
                printf("Arm:\n");
                pat_print(node->as.match_stmt.arms[i].pat, indent + 2);
                ast_print(node->as.match_stmt.arms[i].expr, indent + 2);
            }
            break;

        case AST_ASSIGN:
            printf("Assign (op: %d)\n", node->as.assign.op);
            ast_print(node->as.assign.left, indent + 1);
            ast_print(node->as.assign.right, indent + 1);
            break;

        case AST_EXPR_STMT:
            printf("ExprStmt\n");
            ast_print(node->as.expr_stmt.expr, indent + 1);
            break;

        case AST_INT_LITERAL:
            printf("IntLiteral: %lld\n", (long long)node->as.int_literal.value);
            break;

        case AST_FLOAT_LITERAL:
            printf("FloatLiteral: %g\n", node->as.float_literal.value);
            break;

        case AST_STRING_LITERAL:
            printf("StringLiteral: \"");
            print_sv(node->as.string_literal.value);
            printf("\"\n");
            break;

        case AST_CHAR_LITERAL:
            printf("CharLiteral: '%c' (%u)\n", node->as.char_literal.value, node->as.char_literal.value);
            break;

        case AST_BOOL_LITERAL:
            printf("BoolLiteral: %s\n", node->as.bool_literal.value ? "true" : "false");
            break;

        case AST_NULL_LITERAL:
            printf("NullLiteral\n");
            break;

        case AST_UNDEFINED_LITERAL:
            printf("UndefinedLiteral\n");
            break;

        case AST_IDENTIFIER:
            printf("Identifier: ");
            print_sv(node->as.identifier.name);
            printf("\n");
            break;

        case AST_BINARY:
            printf("Binary (op: %d)\n", node->as.binary.op);
            ast_print(node->as.binary.left, indent + 1);
            ast_print(node->as.binary.right, indent + 1);
            break;

        case AST_UNARY:
            printf("Unary (op: %d)\n", node->as.unary.op);
            ast_print(node->as.unary.operand, indent + 1);
            break;

        case AST_FIELD_ACCESS:
            printf("FieldAccess: .");
            print_sv(node->as.field_access.field);
            printf("\n");
            ast_print(node->as.field_access.object, indent + 1);
            break;

        case AST_METHOD_CALL:
            printf("MethodCall: .");
            print_sv(node->as.method_call.method);
            printf("()\n");
            ast_print(node->as.method_call.object, indent + 1);
            for (size_t i = 0; i < node->as.method_call.arg_count; i++) {
                ast_print(node->as.method_call.args[i], indent + 1);
            }
            break;

        case AST_CALL:
            printf("Call\n");
            ast_print(node->as.call.callee, indent + 1);
            for (size_t i = 0; i < node->as.call.arg_count; i++) {
                ast_print(node->as.call.args[i], indent + 1);
            }
            break;

        case AST_INDEX:
            printf("Index\n");
            ast_print(node->as.index.object, indent + 1);
            ast_print(node->as.index.index, indent + 1);
            break;

        case AST_SLICE:
            printf("Slice\n");
            ast_print(node->as.slice.object, indent + 1);
            if (node->as.slice.start) ast_print(node->as.slice.start, indent + 1);
            if (node->as.slice.end) ast_print(node->as.slice.end, indent + 1);
            break;

        case AST_CAST:
            printf("Cast\n");
            ast_print(node->as.cast.expr, indent + 1);
            type_print(node->as.cast.type_, indent + 1);
            break;

        case AST_TRY:
            printf("TryExpr\n");
            ast_print(node->as.try_expr.expr, indent + 1);
            break;

        case AST_TRY_ELSE:
            printf("TryElseExpr err_name: ");
            print_sv(node->as.try_expr.err_name);
            printf("\n");
            ast_print(node->as.try_expr.expr, indent + 1);
            if (node->as.try_expr.else_block) {
                ast_print(node->as.try_expr.else_block, indent + 1);
            }
            break;

        case AST_UNSAFE_BLOCK:
            printf("UnsafeBlock\n");
            if (node->as.unsafe_block.block) {
                ast_print(node->as.unsafe_block.block, indent + 1);
            }
            break;

        case AST_ADDR_OF:
            printf("AddrOf (&)\n");
            ast_print(node->as.unary.operand, indent + 1);
            break;

        case AST_ADDR_OF_CONST:
            printf("AddrOfConst (&const)\n");
            ast_print(node->as.unary.operand, indent + 1);
            break;

        case AST_DEREF:
            printf("Deref (*)\n");
            ast_print(node->as.unary.operand, indent + 1);
            break;

        case AST_ARRAY_LITERAL:
            printf("ArrayLiteral\n");
            if (node->as.array_literal.type_) {
                type_print(node->as.array_literal.type_, indent + 1);
            }
            for (size_t i = 0; i < node->as.array_literal.elem_count; i++) {
                ast_print(node->as.array_literal.elems[i], indent + 1);
            }
            break;

        case AST_STRUCT_LITERAL:
            printf("StructLiteral\n");
            if (node->as.struct_literal.type_) {
                type_print(node->as.struct_literal.type_, indent + 1);
            }
            for (size_t i = 0; i < node->as.struct_literal.field_count; i++) {
                print_indent(indent + 1);
                printf("FieldInit: ");
                print_sv(node->as.struct_literal.fields[i].name);
                printf("\n");
                ast_print(node->as.struct_literal.fields[i].value, indent + 2);
            }
            break;
    }
}
