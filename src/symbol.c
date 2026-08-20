#include "symbol.h"
#include <string.h>

#define SCOPE_BUCKETS 32

static uint32_t hash_sv(StringView sv) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < sv.len; i++) h = (h ^ (uint8_t)sv.data[i]) * 16777619;
    return h;
}

Scope *scope_new(Arena *arena, Scope *parent, struct AstNode *node) {
    Scope *s = arena_alloc(arena, sizeof(Scope));
    s->parent = parent;
    s->bucket_count = SCOPE_BUCKETS;
    s->buckets = arena_alloc(arena, SCOPE_BUCKETS * sizeof(Symbol*));
    memset(s->buckets, 0, SCOPE_BUCKETS * sizeof(Symbol*));
    s->children = NULL;
    s->next_sibling = parent ? parent->children : NULL;
    s->node = node;
    if (parent) parent->children = s;
    return s;
}

Symbol *scope_lookup(Scope *s, StringView name) {
    for (Scope *sc = s; sc; sc = sc->parent) {
        Symbol *sym = scope_lookup_current(sc, name);
        if (sym) return sym;
    }
    return NULL;
}

Symbol *scope_lookup_current(Scope *s, StringView name) {
    uint32_t h = hash_sv(name) % s->bucket_count;
    for (Symbol *sym = s->buckets[h]; sym; sym = sym->next)
        if (sym->name.len == name.len && memcmp(sym->name.data, name.data, name.len) == 0) return sym;
    return NULL;
}

void scope_insert(Arena *arena, Scope *s, Symbol *sym) {
    (void)arena;
    uint32_t h = hash_sv(sym->name) % s->bucket_count;
    sym->next = s->buckets[h];
    s->buckets[h] = sym;
}

Symbol *symbol_new(Arena *arena, SymbolKind kind, StringView name, SType *type, struct AstNode *decl) {
    Symbol *sym = arena_alloc(arena, sizeof(Symbol));
    sym->kind = kind; sym->name = name; sym->type = type; sym->decl = decl;
    sym->next = NULL; sym->is_pub = false; sym->is_comptime = false;
    return sym;
}
