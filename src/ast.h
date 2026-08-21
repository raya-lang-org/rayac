
#ifndef RAYA_AST_H
#define RAYA_AST_H

#include "common.h"
#include "string_view.h"
#include "source_loc.h"
#include "lexer.h"
#include "arena.h"

typedef struct AstNode AstNode;
typedef struct TypeExpr TypeExpr;
typedef struct Pattern Pattern;
typedef struct SType SType;

typedef enum {
    AST_COMPILATION_UNIT,
    AST_MODULE_DECL,
    AST_IMPORT_DECL,
    AST_FN_DECL,
    AST_STRUCT_DECL,
    AST_UNION_DECL,
    AST_ENUM_DECL,
    AST_TRAITS_DECL,
    AST_EXTEND_DECL,
    AST_TYPE_ALIAS,
    AST_CONST_DECL,
    AST_VAR_DECL,
    AST_TEST_DECL,
    AST_FIELD_DECL,
    AST_VARIANT_DECL,
    AST_TRAIT_METHOD_DECL,
    AST_PARAM_DECL,
    AST_GENERIC_PARAM_DECL,
    AST_BLOCK,
    AST_EXPR_STMT,
    AST_RETURN_STMT,
    AST_IF_STMT,
    AST_WHILE_STMT,
    AST_FOR_STMT,
    AST_DEFER_STMT,
    AST_ERRDEFER_STMT,
    AST_BREAK_STMT,
    AST_CONTINUE_STMT,
    AST_MATCH_STMT,
    AST_ASSIGN_STMT,
    AST_INT_LITERAL,
    AST_FLOAT_LITERAL,
    AST_STRING_LITERAL,
    AST_CHAR_LITERAL,
    AST_BOOL_LITERAL,
    AST_NULL_LITERAL,
    AST_UNDEFINED_LITERAL,
    AST_IDENTIFIER,
    AST_BINARY_EXPR,
    AST_UNARY_EXPR,
    AST_CALL_EXPR,
    AST_METHOD_CALL_EXPR,
    AST_FIELD_ACCESS_EXPR,
    AST_INDEX_EXPR,
    AST_SLICE_EXPR,
    AST_CAST_EXPR,
    AST_TRY_EXPR,
    AST_ERROR_CAPTURE_EXPR,
    AST_UNSAFE_BLOCK_EXPR,
    AST_ARRAY_LITERAL,
    AST_STRUCT_LITERAL,
    AST_ARG_LIST,
    AST_MATCH_ARM,
} AstNodeKind;

typedef enum {
    TYPE_NAMED,
    TYPE_REFERENCE,
    TYPE_POINTER,
    TYPE_SLICE,
    TYPE_ARRAY,
    TYPE_OPTIONAL,
    TYPE_ERROR_UNION,
    TYPE_FUNCTION,
} TypeExprKind;

typedef enum {
    PATTERN_WILDCARD,
    PATTERN_LITERAL,
    PATTERN_IDENTIFIER,
    PATTERN_ENUM_VARIANT,
    PATTERN_STRUCT_FIELD,
} PatternKind;

typedef struct AstNodeList {
    AstNode** items;
    size_t count;
    size_t capacity;
} AstNodeList;

typedef struct TypeExprList {
    TypeExpr** items;
    size_t count;
    size_t capacity;
} TypeExprList;

typedef struct PatternList {
    Pattern** items;
    size_t count;
    size_t capacity;
} PatternList;

typedef struct StringViewList {
    StringView* items;
    size_t count;
    size_t capacity;
} StringViewList;

typedef struct {
    StringView name;
    AstNodeList args;
    SourceLocation loc;
} Attribute;

typedef struct {
    Attribute* items;
    size_t count;
    size_t capacity;
} AttributeList;

struct TypeExpr {
    TypeExprKind kind;
    SourceLocation loc;
    union {
        struct {
            StringView name;
            TypeExprList generic_args;
        } named;
        struct {
            TypeExpr* child;
            bool is_const;
        } unary;
        struct {
            AstNode* length;
            TypeExpr* elem;
        } array;
        struct {
            TypeExprList params;
            TypeExpr* ret;
        } func;
    };
};

struct Pattern {
    PatternKind kind;
    SourceLocation loc;
    union {
        AstNode* literal;
        StringView ident;
        struct {
            StringView name;
            Pattern* inner;
        } enum_variant;
        struct {
            StringViewList fields;
            PatternList patterns;
        } struct_field;
    };
};

struct AstNode {
    AstNodeKind kind;
    SourceLocation loc;
    SType *sema_type;
    union {
        struct {
            StringView module_name;
            AstNodeList imports;
            AstNodeList decls;
        } compilation_unit;
        struct { StringView name; } module_decl;
        struct {
            StringViewList parts;
            StringView alias;
        } import_decl;
        struct {
            StringView name;
            bool is_pub;
            bool is_comptime;
            bool is_unsafe;
            bool is_extern;
            StringView extern_abi;
            AstNodeList generic_params;
            AstNodeList params;
            TypeExpr* ret_type;
            AstNode* body;
            AttributeList attrs;
        } fn_decl;
        struct {
            StringView name;
            bool is_pub;
            AstNodeList fields;
            AttributeList attrs;
        } union_decl;
        struct {
            StringView name;
            bool is_pub;
            AstNodeList generic_params;
            AstNodeList fields;
            AttributeList attrs;
        } struct_decl;
        struct {
            StringView name;
            bool is_pub;
            AstNodeList variants;
            AttributeList attrs;
        } enum_decl;
        struct {
            StringView name;
            bool is_pub;
            AstNodeList methods;
            AttributeList attrs;
        } traits_decl;
        struct {
            StringView target_name;
            AstNodeList generic_params;
            StringViewList trait_names;
            AstNodeList methods;
            AttributeList attrs;
        } extend_decl;
        struct {
            StringView name;
            bool is_pub;
            TypeExpr* type;
            AttributeList attrs;
        } type_alias;
        struct {
            StringView name;
            bool is_pub;
            bool is_comptime;
            TypeExpr* type;
            AstNode* init;
            AttributeList attrs;
        } var_decl;
        struct {
            StringView name;
            AstNode* body;
            AttributeList attrs;
        } test_decl;
        struct {
            StringView name;
            bool is_pub;
            TypeExpr* type;
            AstNode* default_value;
            AttributeList attrs;
        } field_decl;
        struct {
            StringView name;
            TypeExpr* payload_type;
            AstNode* discriminant;
        } variant_decl;
        struct {
            StringView name;
            bool is_pub;
            AstNodeList params;
            TypeExpr* ret_type;
        } trait_method_decl;
        struct {
            StringView name;
            TypeExpr* type;
            AstNode* default_value;
            bool is_comptime;
            bool is_self;
        } param_decl;
        struct {
            StringView name;
            StringViewList trait_constraints;
        } generic_param_decl;
        struct {
            AstNodeList stmts;
            AstNode* trailing_expr;
        } block;
        struct { AstNode* expr; } expr_stmt;
        struct { AstNode* value; } return_stmt;
        struct {
            AstNode* condition;
            AstNode* then_block;
            AstNode* else_block;
        } if_stmt;
        struct {
            AstNode* condition;
            AstNode* body;
        } while_stmt;
        struct {
            StringView var_name;
            TypeExpr* var_type;
            AstNode* iterable;
            AstNode* body;
        } for_stmt;
        struct { AstNode* expr; } defer_stmt;
        struct { AstNode* body; } errdefer_stmt;
        struct { /* nothing */ } break_stmt;
        struct {
            AstNode* expr;
            AstNodeList arms;
        } match_stmt;
        struct {
            Pattern* pattern;
            AstNode* expr;
        } match_arm;
        struct {
            TokenKind op;
            AstNode* lhs;
            AstNode* rhs;
        } assign_stmt;
        struct { int64_t value; } int_literal;
        struct { double value; } float_literal;
        struct { StringView value; } string_literal;
        struct { StringView value; } char_literal;
        struct { bool value; } bool_literal;
        struct { StringView name; struct Symbol *sym;} identifier;
        struct {
            TokenKind op;
            AstNode* left;
            AstNode* right;
        } binary_expr;
        struct {
            TokenKind op;
            AstNode* operand;
        } unary_expr;
        struct {
            AstNode* callee;
            AstNodeList args;
        } call_expr;
        struct {
            AstNode* receiver;
            StringView method_name;
            AstNodeList args;
        } method_call_expr;
        struct {
            AstNode* object;
            StringView field_name;
        } field_access_expr;
        struct {
            AstNode* object;
            AstNode* index;
        } index_expr;
        struct {
            AstNode* object;
            AstNode* start;
            AstNode* end;
        } slice_expr;
        struct {
            AstNode* expr;
            TypeExpr* type;
        } cast_expr;
        struct {
            AstNode* expr;
        } try_expr;
        struct {
            AstNode* expr;
            StringView err_name;
            AstNode* fallback;
        } error_capture_expr;
        struct { AstNode* body; } unsafe_block_expr;
        struct {
            TypeExpr* explicit_type;
            AstNode* length;
            AstNodeList elements;
            bool sentinel;
        } array_literal;
        struct {
            TypeExpr* type;
            AstNodeList fields;
        } struct_literal;
        struct { AstNodeList args; } arg_list;
    };
};

AstNode* ast_new_compilation_unit(Arena* arena, SourceLocation loc);
AstNode* ast_new_module_decl(Arena* arena, StringView name, SourceLocation loc);
AstNode* ast_new_import_decl(Arena* arena, StringView first, SourceLocation loc);
AstNode* ast_new_fn_decl(Arena* arena, StringView name, SourceLocation loc);
AstNode* ast_new_struct_decl(Arena* arena, StringView name, SourceLocation loc);
AstNode* ast_new_union_decl(Arena* arena, StringView name, SourceLocation loc);
AstNode* ast_new_enum_decl(Arena* arena, StringView name, SourceLocation loc);
AstNode* ast_new_traits_decl(Arena* arena, StringView name, SourceLocation loc);
AstNode* ast_new_extend_decl(Arena* arena, StringView target, SourceLocation loc);
AstNode* ast_new_type_alias(Arena* arena, StringView name, SourceLocation loc);
AstNode* ast_new_const_decl(Arena* arena, StringView name, SourceLocation loc);
AstNode* ast_new_var_decl(Arena* arena, StringView name, SourceLocation loc);
AstNode* ast_new_test_decl(Arena* arena, StringView name, SourceLocation loc);
AstNode* ast_new_field_decl(Arena* arena, StringView name, SourceLocation loc);
AstNode* ast_new_variant_decl(Arena* arena, StringView name, SourceLocation loc);
AstNode* ast_new_trait_method_decl(Arena* arena, StringView name, SourceLocation loc);
AstNode* ast_new_param_decl(Arena* arena, StringView name, SourceLocation loc);
AstNode* ast_new_generic_param_decl(Arena* arena, StringView name, SourceLocation loc);

AstNode* ast_new_block(Arena* arena, SourceLocation loc);
AstNode* ast_new_expr_stmt(Arena* arena, AstNode* expr, SourceLocation loc);
AstNode* ast_new_return(Arena* arena, AstNode* value, SourceLocation loc);
AstNode* ast_new_if(Arena* arena, AstNode* cond, AstNode* then_block, AstNode* else_block, SourceLocation loc);
AstNode* ast_new_while(Arena* arena, AstNode* cond, AstNode* body, SourceLocation loc);
AstNode* ast_new_for(Arena* arena, StringView var, TypeExpr* type, AstNode* iterable, AstNode* body, SourceLocation loc);
AstNode* ast_new_defer(Arena* arena, AstNode* expr, SourceLocation loc);
AstNode* ast_new_errdefer(Arena* arena, AstNode* body, SourceLocation loc);
AstNode* ast_new_break(Arena* arena, SourceLocation loc);
AstNode* ast_new_continue(Arena* arena, SourceLocation loc);
AstNode* ast_new_match(Arena* arena, AstNode* expr, SourceLocation loc);
AstNode* ast_new_match_arm(Arena* arena, Pattern* pat, AstNode* expr, SourceLocation loc);
AstNode* ast_new_assign(Arena* arena, TokenKind op, AstNode* lhs, AstNode* rhs, SourceLocation loc);

AstNode* ast_new_int_literal(Arena* arena, int64_t value, SourceLocation loc);
AstNode* ast_new_float_literal(Arena* arena, double value, SourceLocation loc);
AstNode* ast_new_string_literal(Arena* arena, StringView value, SourceLocation loc);
AstNode* ast_new_char_literal(Arena* arena, StringView value, SourceLocation loc);
AstNode* ast_new_bool_literal(Arena* arena, bool value, SourceLocation loc);
AstNode* ast_new_null_literal(Arena* arena, SourceLocation loc);
AstNode* ast_new_undefined_literal(Arena* arena, SourceLocation loc);
AstNode* ast_new_identifier(Arena* arena, StringView name, SourceLocation loc);
AstNode* ast_new_binary(Arena* arena, TokenKind op, AstNode* left, AstNode* right, SourceLocation loc);
AstNode* ast_new_unary(Arena* arena, TokenKind op, AstNode* operand, SourceLocation loc);
AstNode* ast_new_call(Arena* arena, AstNode* callee, SourceLocation loc);
AstNode* ast_new_method_call(Arena* arena, AstNode* receiver, StringView method, SourceLocation loc);
AstNode* ast_new_field_access(Arena* arena, AstNode* object, StringView field, SourceLocation loc);
AstNode* ast_new_index(Arena* arena, AstNode* object, AstNode* index, SourceLocation loc);
AstNode* ast_new_slice(Arena* arena, AstNode* object, AstNode* start, AstNode* end, SourceLocation loc);
AstNode* ast_new_cast(Arena* arena, AstNode* expr, TypeExpr* type, SourceLocation loc);
AstNode* ast_new_try(Arena* arena, AstNode* expr, SourceLocation loc);
AstNode* ast_new_error_capture(Arena* arena, AstNode* expr, StringView err_name, AstNode* fallback, SourceLocation loc);
AstNode* ast_new_unsafe_block(Arena* arena, AstNode* body, SourceLocation loc);
AstNode* ast_new_array_literal(Arena* arena, SourceLocation loc);
AstNode* ast_new_struct_literal(Arena* arena, TypeExpr* type, SourceLocation loc);

void ast_node_list_init(Arena* arena, AstNodeList* list);
void ast_node_list_push(Arena* arena, AstNodeList* list, AstNode* node);
void type_expr_list_init(Arena* arena, TypeExprList* list);
void type_expr_list_push(Arena* arena, TypeExprList* list, TypeExpr* type);
void string_view_list_init(Arena* arena, StringViewList* list);
void string_view_list_push(Arena* arena, StringViewList* list, StringView sv);
void pattern_list_init(Arena* arena, PatternList* list);
void pattern_list_push(Arena* arena, PatternList* list, Pattern* pat);
void attribute_list_init(Arena* arena, AttributeList* list);
void attribute_list_push(Arena* arena, AttributeList* list, Attribute attr);

void ast_import_add_part(Arena* arena, AstNode* import_decl, StringView part);
void ast_import_set_alias(AstNode* import_decl, StringView alias);
void ast_set_module(AstNode* unit, StringView name);
void ast_add_import(Arena* arena, AstNode* unit, AstNode* import_decl);
void ast_add_decl(Arena* arena, AstNode* unit, AstNode* decl);
void ast_block_add_stmt(Arena* arena, AstNode* block, AstNode* stmt);
void ast_block_set_trailing(AstNode* block, AstNode* expr);
void ast_fn_add_param(Arena* arena, AstNode* fn, AstNode* param);
void ast_fn_add_generic(Arena* arena, AstNode* fn, AstNode* param);
void ast_struct_add_field(Arena* arena, AstNode* s, AstNode* field);
void ast_struct_add_generic(Arena* arena, AstNode* s, AstNode* param);
void ast_enum_add_variant(Arena* arena, AstNode* e, AstNode* variant);
void ast_traits_add_method(Arena* arena, AstNode* traits, AstNode* method);
void ast_extend_add_method(Arena* arena, AstNode* extend, AstNode* method);
void ast_extend_add_generic(Arena* arena, AstNode* extend, AstNode* param);
void ast_call_add_arg(Arena* arena, AstNode* call, AstNode* arg);
void ast_array_add_elem(Arena* arena, AstNode* arr, AstNode* elem);
void ast_struct_add_field_init(Arena* arena, AstNode* lit, AstNode* field_init);
void ast_match_add_arm(Arena* arena, AstNode* match, AstNode* arm);
void ast_extend_add_trait(Arena* arena, AstNode* extend, StringView trait);

TypeExpr* type_new_named(Arena* arena, StringView name, SourceLocation loc);
TypeExpr* type_new_reference(Arena* arena, TypeExpr* child, bool is_const, SourceLocation loc);
TypeExpr* type_new_pointer(Arena* arena, TypeExpr* child, bool is_const, SourceLocation loc);
TypeExpr* type_new_slice(Arena* arena, TypeExpr* child, bool is_const, SourceLocation loc);
TypeExpr* type_new_array(Arena* arena, AstNode* length, TypeExpr* elem, SourceLocation loc);
TypeExpr* type_new_optional(Arena* arena, TypeExpr* child, SourceLocation loc);
TypeExpr* type_new_error_union(Arena* arena, TypeExpr* child, SourceLocation loc);
TypeExpr* type_new_function(Arena* arena, SourceLocation loc);
void type_add_generic_arg(Arena* arena, TypeExpr* type, TypeExpr* arg);
void type_func_add_param(Arena* arena, TypeExpr* func, TypeExpr* param);
void type_func_set_ret(TypeExpr* func, TypeExpr* ret);

Pattern* pattern_new_wildcard(Arena* arena, SourceLocation loc);
Pattern* pattern_new_literal(Arena* arena, AstNode* lit, SourceLocation loc);
Pattern* pattern_new_identifier(Arena* arena, StringView name, SourceLocation loc);
Pattern* pattern_new_enum_variant(Arena* arena, StringView name, Pattern* inner, SourceLocation loc);
Pattern* pattern_new_struct_field(Arena* arena, SourceLocation loc);
void pattern_struct_add_field(Arena* arena, Pattern* pat, StringView name, Pattern* field_pat);

bool sv_is_primitive_type(StringView sv);
bool token_is_assignment_op(TokenKind kind);
void ast_print(AstNode* node, int indent);

#endif
