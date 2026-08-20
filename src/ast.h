#ifndef RAYA_AST_H
#define RAYA_AST_H

#include "common.h"
#include "string_view.h"
#include "source_loc.h"
#include "arena.h"
#include "lexer.h"

typedef struct AstNode AstNode;
typedef struct TypeExpr TypeExpr;
typedef struct Pattern Pattern;

/* ============================================================================
 * Type expressions
 * ========================================================================== */

typedef enum {
    TYPE_NAMED,
    TYPE_PRIMITIVE,
    TYPE_POINTER,
    TYPE_REFERENCE,
    TYPE_SLICE,
    TYPE_ARRAY,
    TYPE_OPTIONAL,
    TYPE_ERROR_UNION,
    TYPE_FUNCTION,
} TypeExprKind;

struct TypeExpr {
    TypeExprKind kind;
    SourceLocation loc;
    union {
        struct { StringView name; TypeExpr** args; size_t arg_count; } named;
        struct { StringView name; } primitive;
        struct { TypeExpr* pointee; bool is_const; } ptr;
        struct { TypeExpr* pointee; bool is_const; } ref;
        struct { TypeExpr* element; bool is_const; } slice;
        struct { AstNode* size; TypeExpr* element; } array;
        struct { TypeExpr* inner; } optional;
        struct { TypeExpr* inner; } error_union;
        struct { TypeExpr** params; size_t param_count; TypeExpr* ret; } function;
    } as;
};

/* ============================================================================
 * Patterns (for match arms)
 * ========================================================================== */

typedef enum {
    PAT_WILDCARD,
    PAT_LITERAL,
    PAT_IDENTIFIER,
    PAT_ENUM_VARIANT,
    PAT_STRUCT_FIELD,
} PatternKind;

struct Pattern {
    PatternKind kind;
    SourceLocation loc;
    union {
        struct { AstNode* literal; } literal;
        struct { StringView name; } ident;
        struct { StringView name; Pattern** fields; size_t field_count; } enum_variant;
        struct { StringView name; Pattern** fields; size_t field_count; } struct_field;
    } as;
};

/* ============================================================================
 * AST node kinds
 * ========================================================================== */

typedef enum {
    AST_MODULE_DECL,
    AST_IMPORT_DECL,
    AST_FN_DECL,
    AST_STRUCT_DECL,
    AST_ENUM_DECL,
    AST_UNION_DECL,
    AST_TRAIT_DECL,
    AST_EXTEND_DECL,
    AST_TYPE_ALIAS,
    AST_TEST_DECL,
    AST_CONST_DECL,
    AST_VAR_DECL,
    AST_BLOCK,
    AST_RETURN,
    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_DEFER,
    AST_ERRDEFER,
    AST_BREAK,
    AST_CONTINUE,
    AST_MATCH,
    AST_ASSIGN,
    AST_EXPR_STMT,
    AST_INT_LITERAL,
    AST_FLOAT_LITERAL,
    AST_STRING_LITERAL,
    AST_CHAR_LITERAL,
    AST_BOOL_LITERAL,
    AST_NULL_LITERAL,
    AST_UNDEFINED_LITERAL,
    AST_IDENTIFIER,
    AST_BINARY,
    AST_UNARY,
    AST_FIELD_ACCESS,
    AST_METHOD_CALL,
    AST_CALL,
    AST_INDEX,
    AST_SLICE,
    AST_CAST,
    AST_TRY,
    AST_TRY_ELSE,
    AST_UNSAFE_BLOCK,
    AST_ADDR_OF,
    AST_ADDR_OF_CONST,
    AST_DEREF,
    AST_ARRAY_LITERAL,
    AST_STRUCT_LITERAL,
} AstNodeKind;

/* ============================================================================
 * Attribute  #[name(args)]
 * ========================================================================== */

typedef struct {
    StringView name;
    AstNode** args;
    size_t arg_count;
    SourceLocation loc;
} Attribute;

/* ============================================================================
 * Parameter / Field / Variant descriptors
 * ========================================================================== */

typedef struct {
    StringView name;
    TypeExpr* type_;
    AstNode* default_value;
    bool is_self;
    bool is_const_self;
    bool has_type;
    SourceLocation loc;
} ParamDesc;

typedef struct {
    StringView name;
    TypeExpr* type_;
    AstNode* default_value;
    bool is_pub;
    SourceLocation loc;
} FieldDesc;

typedef struct {
    StringView name;
    TypeExpr* payload_type;
    AstNode* discriminant;
    SourceLocation loc;
} VariantDesc;

/* ============================================================================
 * AST node (tagged union)
 * ========================================================================== */

struct AstNode {
    AstNodeKind kind;
    SourceLocation loc;
    union {
        struct { StringView name; } module_decl;
        struct { StringView name; StringView* parts; size_t part_count; StringView alias; bool has_alias; } import_decl;
        struct {
            StringView name;
            bool is_pub;
            bool is_comptime;
            bool is_unsafe;
            bool is_extern;
            StringView extern_abi;
            TypeExpr** generic_params; size_t generic_param_count;
            ParamDesc* params; size_t param_count;
            TypeExpr* return_type;
            AstNode* body;
            Attribute* attrs; size_t attr_count;
        } fn_decl;
        struct {
            StringView name;
            bool is_pub;
            TypeExpr** generic_params; size_t generic_param_count;
            FieldDesc* fields; size_t field_count;
            Attribute* attrs; size_t attr_count;
        } struct_decl;
        struct {
            StringView name;
            bool is_pub;
            VariantDesc* variants; size_t variant_count;
            Attribute* attrs; size_t attr_count;
        } enum_decl;
        struct {
            StringView name;
            bool is_pub;
            AstNode** methods; size_t method_count;
            Attribute* attrs; size_t attr_count;
        } trait_decl;
        struct {
            StringView target_name;
            TypeExpr** generic_params; size_t generic_param_count;
            StringView* trait_names; size_t trait_count;
            AstNode** methods; size_t method_count;
        } extend_decl;
        struct { StringView name; bool is_pub; TypeExpr* type_; } type_alias;
        struct { StringView name; AstNode* body; } test_decl;
        struct { StringView name; TypeExpr* type_; AstNode* initializer; bool is_pub; } var_decl;
        struct { AstNode** stmts; size_t stmt_count; AstNode* trailing_expr; } block;
        struct { AstNode* value; } return_stmt;
        struct { AstNode* condition; AstNode* then_block; AstNode* else_block; } if_stmt;
        struct { AstNode* condition; AstNode* body; } while_stmt;
        struct { StringView var_name; TypeExpr* var_type; AstNode* iterable; AstNode* body; } for_stmt;
        struct { AstNode* body; } defer_stmt;
        struct { AstNode* value; struct { Pattern* pat; AstNode* expr; SourceLocation arrow_loc; }* arms; size_t arm_count; } match_stmt;
        struct { TokenKind op; AstNode* left; AstNode* right; } assign;
        struct { AstNode* expr; } expr_stmt;
        struct { int64_t value; } int_literal;
        struct { double value; } float_literal;
        struct { StringView value; } string_literal;
        struct { uint8_t value; } char_literal;
        struct { bool value; } bool_literal;
        struct { StringView name; } identifier;
        struct { TokenKind op; AstNode* left; AstNode* right; } binary;
        struct { TokenKind op; AstNode* operand; } unary;
        struct { AstNode* object; StringView field; } field_access;
        struct { AstNode* object; StringView method; AstNode** args; size_t arg_count; } method_call;
        struct { AstNode* callee; AstNode** args; size_t arg_count; } call;
        struct { AstNode* object; AstNode* index; } index;
        struct { AstNode* object; AstNode* start; AstNode* end; } slice;
        struct { AstNode* expr; TypeExpr* type_; } cast;
        struct { AstNode* expr; StringView err_name; AstNode* else_block; bool has_else; } try_expr;
        struct { AstNode* block; } unsafe_block;
        struct { TypeExpr* type_; AstNode** elems; size_t elem_count; AstNode* count; } array_literal;
        struct { TypeExpr* type_; struct { StringView name; AstNode* value; }* fields; size_t field_count; } struct_literal;
    } as;
};

/* ============================================================================
 * Arena helpers
 * ========================================================================== */

void ast_set_arena(Arena* arena);
void* ast_alloc(size_t size);
void* ast_alloc_n(size_t count, size_t size);
char* ast_strdup(const char* s);

/* ============================================================================
 * Constructors
 * ========================================================================== */

AstNode* ast_new_module_decl(StringView name, SourceLocation loc);
AstNode* ast_new_import_decl(StringView name, SourceLocation loc);
AstNode* ast_new_fn_decl(StringView name, SourceLocation loc);
AstNode* ast_new_struct_decl(StringView name, SourceLocation loc);
AstNode* ast_new_enum_decl(StringView name, SourceLocation loc);
AstNode* ast_new_union_decl(StringView name, SourceLocation loc);
AstNode* ast_new_trait_decl(StringView name, SourceLocation loc);
AstNode* ast_new_extend_decl(StringView target, SourceLocation loc);
AstNode* ast_new_type_alias(StringView name, SourceLocation loc);
AstNode* ast_new_test_decl(StringView name, AstNode* body, SourceLocation loc);
AstNode* ast_new_const_decl(StringView name, TypeExpr* type_, AstNode* init, SourceLocation loc);
AstNode* ast_new_var_decl(StringView name, TypeExpr* type_, AstNode* init, SourceLocation loc);
AstNode* ast_new_block(SourceLocation loc);
AstNode* ast_new_return(AstNode* value, SourceLocation loc);
AstNode* ast_new_if(AstNode* cond, AstNode* then_, AstNode* else_, SourceLocation loc);
AstNode* ast_new_while(AstNode* cond, AstNode* body, SourceLocation loc);
AstNode* ast_new_for(StringView var, TypeExpr* type_, AstNode* iter, AstNode* body, SourceLocation loc);
AstNode* ast_new_defer(AstNode* body, SourceLocation loc);
AstNode* ast_new_errdefer(AstNode* body, SourceLocation loc);
AstNode* ast_new_break(SourceLocation loc);
AstNode* ast_new_continue(SourceLocation loc);
AstNode* ast_new_match(AstNode* value, SourceLocation loc);
AstNode* ast_new_assign(TokenKind op, AstNode* left, AstNode* right, SourceLocation loc);
AstNode* ast_new_expr_stmt(AstNode* expr, SourceLocation loc);

AstNode* ast_new_int_literal(int64_t value, SourceLocation loc);
AstNode* ast_new_float_literal(double value, SourceLocation loc);
AstNode* ast_new_string_literal(StringView value, SourceLocation loc);
AstNode* ast_new_char_literal(uint8_t value, SourceLocation loc);
AstNode* ast_new_bool_literal(bool value, SourceLocation loc);
AstNode* ast_new_null_literal(SourceLocation loc);
AstNode* ast_new_undefined_literal(SourceLocation loc);
AstNode* ast_new_identifier(StringView name, SourceLocation loc);
AstNode* ast_new_binary(TokenKind op, AstNode* left, AstNode* right, SourceLocation loc);
AstNode* ast_new_unary(TokenKind op, AstNode* operand, SourceLocation loc);
AstNode* ast_new_field_access(AstNode* object, StringView field, SourceLocation loc);
AstNode* ast_new_method_call(AstNode* object, StringView method, AstNode** args, size_t arg_count, SourceLocation loc);
AstNode* ast_new_call(AstNode* callee, AstNode** args, size_t arg_count, SourceLocation loc);
AstNode* ast_new_index(AstNode* object, AstNode* index, SourceLocation loc);
AstNode* ast_new_slice(AstNode* object, AstNode* start, AstNode* end, SourceLocation loc);
AstNode* ast_new_cast(AstNode* expr, TypeExpr* type_, SourceLocation loc);
AstNode* ast_new_try(AstNode* expr, SourceLocation loc);
AstNode* ast_new_try_else(AstNode* expr, StringView err_name, AstNode* else_block, SourceLocation loc);
AstNode* ast_new_unsafe_block(AstNode* block, SourceLocation loc);
AstNode* ast_new_addr_of(AstNode* operand, SourceLocation loc);
AstNode* ast_new_addr_of_const(AstNode* operand, SourceLocation loc);
AstNode* ast_new_deref(AstNode* operand, SourceLocation loc);
AstNode* ast_new_array_literal(TypeExpr* type_, AstNode** elems, size_t elem_count, AstNode* count, SourceLocation loc);
AstNode* ast_new_struct_literal(TypeExpr* type_, SourceLocation loc);

TypeExpr* type_new_named(StringView name, SourceLocation loc);
TypeExpr* type_new_primitive(StringView name, SourceLocation loc);
TypeExpr* type_new_pointer(TypeExpr* pointee, bool is_const, SourceLocation loc);
TypeExpr* type_new_reference(TypeExpr* pointee, bool is_const, SourceLocation loc);
TypeExpr* type_new_slice(TypeExpr* element, bool is_const, SourceLocation loc);
TypeExpr* type_new_array(AstNode* size, TypeExpr* element, SourceLocation loc);
TypeExpr* type_new_optional(TypeExpr* inner, SourceLocation loc);
TypeExpr* type_new_error_union(TypeExpr* inner, SourceLocation loc);
TypeExpr* type_new_function(TypeExpr** params, size_t param_count, TypeExpr* ret, SourceLocation loc);

Pattern* pat_new_wildcard(SourceLocation loc);
Pattern* pat_new_literal(AstNode* literal, SourceLocation loc);
Pattern* pat_new_identifier(StringView name, SourceLocation loc);
Pattern* pat_new_enum_variant(StringView name, Pattern** fields, size_t field_count, SourceLocation loc);
Pattern* pat_new_struct_field(StringView name, Pattern** fields, size_t field_count, SourceLocation loc);

/* ============================================================================
 * AST debug print
 * ========================================================================== */

void ast_print(AstNode* node, int indent);

#endif
