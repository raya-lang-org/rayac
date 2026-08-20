#ifndef SYMBOL_H
#define SYMBOL_H

#include "common.h"
#include "string_view.h"
#include "type.h"

typedef struct Symbol Symbol;
typedef struct Scope Scope;

typedef enum {
    SYM_MODULE,
    SYM_FUNCTION,
    SYM_VAR,
    SYM_CONST,
    SYM_STRUCT,
    SYM_UNION,
    SYM_ENUM,
    SYM_TRAIT,
    SYM_TYPE_ALIAS,
    SYM_GENERIC_PARAM,
    SYM_FIELD,
    SYM_VARIANT,
    SYM_POISON,
} SymbolKind;

struct Symbol {
    SymbolKind kind;
    StringView name;
    SemaType *type;
    struct AstNode *decl;
    Symbol *next;
    bool is_pub;
    bool is_comptime;
};

struct Scope {
    Scope *parent;
    Symbol **buckets;
    size_t bucket_count;
    Scope *children;
    Scope *next_sibling;
    struct AstNode *node;
};

Scope *scope_new(Arena *arena, Scope *parent, struct AstNode *node);
Symbol *scope_lookup(Scope *s, StringView name);
Symbol *scope_lookup_current(Scope *s, StringView name);
void scope_insert(Arena *arena, Scope *s, Symbol *sym);

Symbol *symbol_new(Arena *arena, SymbolKind kind, StringView name, SemaType *type, struct AstNode *decl);

#endif
