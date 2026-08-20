#ifndef TYPE_H
#define TYPE_H

#include "common.h"
#include "string_view.h"

// Forward-declare AST types to avoid including ast.h
struct AstNode;

typedef struct SemaType SemaType;
typedef struct TypeField TypeField;

typedef enum {
    ST_VOID,
    ST_BOOL,
    ST_INT,
    ST_FLOAT,
    ST_NORETURN,
    ST_POINTER,
    ST_REFERENCE,
    ST_SLICE,
    ST_ARRAY,
    ST_OPTIONAL,
    ST_ERROR_UNION,
    ST_FUNCTION,
    ST_STRUCT,
    ST_UNION,
    ST_ENUM,
    ST_TRAIT,
    ST_GENERIC_PARAM,
    ST_OPAQUE,
} SemaTypeKind;

struct TypeField {
    StringView name;
    SemaType *type;
    struct AstNode *default_value;
};

struct SemaType {
    SemaTypeKind kind;
    uint32_t hash;
    union {
        struct { bool is_signed; uint8_t bits; } integer;
        struct { uint8_t bits; } floating;
        struct { bool is_const; SemaType *base; } pointer;
        struct { bool is_const; SemaType *base; } reference;
        struct { bool is_const; SemaType *base; } slice;
        struct { uint64_t size; SemaType *base; } array;
        struct { SemaType *base; } optional;
        struct { SemaType *base; } error_union;
        struct {
            SemaType **params;
            size_t param_count;
            SemaType *ret;
            bool is_variadic;
        } function;
        struct {
            StringView name;
            SemaType **generic_args;
            size_t generic_arg_count;
            TypeField *fields;
            size_t field_count;
            struct AstNode *decl;
        } struct_;
        struct {
            StringView name;
            TypeField *fields;
            size_t field_count;
            struct AstNode *decl;
        } union_;
        struct {
            StringView name;
            StringView *variants;
            size_t variant_count;
            struct AstNode *decl;
        } enum_;
        struct {
            StringView name;
            SemaType **required_methods;
            size_t method_count;
        } trait;
        struct {
            StringView name;
            size_t index;
        } generic_param;
    } as;
};

typedef struct TypeTable TypeTable;

TypeTable *type_table_new(Arena *arena);
SemaType *st_void(TypeTable *tt);
SemaType *st_bool(TypeTable *tt);
SemaType *st_int(TypeTable *tt, bool is_signed, uint8_t bits);
SemaType *st_float(TypeTable *tt, uint8_t bits);
SemaType *st_pointer(TypeTable *tt, bool is_const, SemaType *base);
SemaType *st_reference(TypeTable *tt, bool is_const, SemaType *base);
SemaType *st_slice(TypeTable *tt, bool is_const, SemaType *base);
SemaType *st_array(TypeTable *tt, uint64_t size, SemaType *base);
SemaType *st_optional(TypeTable *tt, SemaType *base);
SemaType *st_error_union(TypeTable *tt, SemaType *base);
SemaType *st_function(TypeTable *tt, SemaType **params, size_t pc, SemaType *ret, bool variadic);

SemaType *st_from_ast(TypeTable *tt, struct AstNode *type_expr);

bool st_eq(SemaType *a, SemaType *b);
bool st_is_integer(SemaType *t);
bool st_is_numeric(SemaType *t);
bool st_is_const_ptr(SemaType *t);
bool st_can_coerce(SemaType *from, SemaType *to);

const char *st_name(SemaType *t);
void st_print(SemaType *t);

#endif
