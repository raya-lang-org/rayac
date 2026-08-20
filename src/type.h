#ifndef RAYA_TYPE_H
#define RAYA_TYPE_H

#include "common.h"
#include "string_view.h"
#include "arena.h"

typedef struct AstNode AstNode;
typedef struct TypeExpr TypeExpr;

typedef struct SType SType;
typedef struct TypeField TypeField;

typedef enum {
    ST_VOID, ST_BOOL, ST_INT, ST_FLOAT, ST_NORETURN,
    ST_POINTER, ST_REFERENCE, ST_SLICE, ST_ARRAY,
    ST_OPTIONAL, ST_ERROR_UNION, ST_FUNCTION,
    ST_STRUCT, ST_UNION, ST_ENUM, ST_TRAIT,
    ST_GENERIC_PARAM, ST_OPAQUE,
} STypeKind;

struct TypeField {
    StringView name;
    SType *type;
    struct AstNode *default_value;
};

struct SType {
    STypeKind kind;
    uint32_t hash;
    SType *next_hash;
    union {
        struct { bool is_signed; uint8_t bits; } integer;
        struct { uint8_t bits; } floating;
        struct { bool is_const; SType *base; } pointer;
        struct { bool is_const; SType *base; } reference;
        struct { bool is_const; SType *base; } slice;
        struct { uint64_t size; SType *base; } array;
        struct { SType *base; } optional;
        struct { SType *base; } error_union;
        struct {
            SType **params;
            size_t param_count;
            SType *ret;
            bool is_variadic;
        } function;
        struct {
            StringView name;
            SType **generic_args;
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
            SType **required_methods;
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
SType *st_void(TypeTable *tt);
SType *st_bool(TypeTable *tt);
SType *st_int(TypeTable *tt, bool is_signed, uint8_t bits);
SType *st_float(TypeTable *tt, uint8_t bits);
SType *st_pointer(TypeTable *tt, bool is_const, SType *base);
SType *st_reference(TypeTable *tt, bool is_const, SType *base);
SType *st_slice(TypeTable *tt, bool is_const, SType *base);
SType *st_array(TypeTable *tt, uint64_t size, SType *base);
SType *st_optional(TypeTable *tt, SType *base);
SType *st_error_union(TypeTable *tt, SType *base);
SType *st_function(TypeTable *tt, SType **params, size_t pc, SType *ret, bool variadic);

SType *st_from_ast(TypeTable *tt, TypeExpr *type_expr);

bool st_eq(SType *a, SType *b);
bool st_is_integer(SType *t);
bool st_is_numeric(SType *t);
bool st_can_coerce(SType *from, SType *to);

const char *st_name(SType *t);
void st_print(SType *t);

#endif
