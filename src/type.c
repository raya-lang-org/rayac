#include "type.h"
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TYPE_TABLE_BUCKETS 256

struct TypeTable {
    Arena *arena;
    SType *buckets[TYPE_TABLE_BUCKETS];
};

static uint32_t hash_type_shape(SType *t) {
    uint32_t h = 2166136261u;
    h = (h ^ (uint8_t)t->kind) * 16777619;
    switch (t->kind) {
        case ST_INT:
            h = (h ^ (uint8_t)t->as.integer.is_signed) * 16777619;
            h = (h ^ t->as.integer.bits) * 16777619;
            break;
        case ST_FLOAT:
            h = (h ^ t->as.floating.bits) * 16777619;
            break;
        case ST_POINTER: case ST_REFERENCE: case ST_SLICE:
            h = (h ^ (uint8_t)t->as.pointer.is_const) * 16777619;
            h = (h ^ (uint32_t)(uintptr_t)t->as.pointer.base) * 16777619;
            break;
        case ST_ARRAY:
            h = (h ^ (uint32_t)(uint64_t)t->as.array.size) * 16777619;
            h = (h ^ (uint32_t)(uintptr_t)t->as.array.base) * 16777619;
            break;
        case ST_OPTIONAL: case ST_ERROR_UNION:
            h = (h ^ (uint32_t)(uintptr_t)t->as.optional.base) * 16777619;
            break;
        case ST_FUNCTION:
            h = (h ^ (uint32_t)t->as.function.param_count) * 16777619;
            h = (h ^ (uint8_t)t->as.function.is_variadic) * 16777619;
            for (size_t i = 0; i < t->as.function.param_count; i++)
                h = (h ^ (uint32_t)(uintptr_t)t->as.function.params[i]) * 16777619;
            h = (h ^ (uint32_t)(uintptr_t)t->as.function.ret) * 16777619;
            break;
        case ST_STRUCT: case ST_UNION: case ST_ENUM: case ST_TRAIT:
            for (size_t i = 0; i < t->as.struct_.name.len; i++)
                h = (h ^ (uint8_t)t->as.struct_.name.data[i]) * 16777619;
            h = (h ^ (uint32_t)t->as.struct_.generic_arg_count) * 16777619;
            break;
        case ST_GENERIC_PARAM:
            for (size_t i = 0; i < t->as.generic_param.name.len; i++)
                h = (h ^ (uint8_t)t->as.generic_param.name.data[i]) * 16777619;
            h = (h ^ (uint32_t)t->as.generic_param.index) * 16777619;
            break;
        default: break;
    }
    return h;
}

static bool type_shape_eq(SType *a, SType *b) {
    if (a->kind != b->kind) return false;
    switch (a->kind) {
        case ST_VOID: case ST_BOOL: case ST_NORETURN: case ST_OPAQUE: return true;
        case ST_INT:  return a->as.integer.is_signed == b->as.integer.is_signed && a->as.integer.bits == b->as.integer.bits;
        case ST_FLOAT: return a->as.floating.bits == b->as.floating.bits;
        case ST_POINTER: case ST_REFERENCE: case ST_SLICE:
            return a->as.pointer.is_const == b->as.pointer.is_const && a->as.pointer.base == b->as.pointer.base;
        case ST_ARRAY: return a->as.array.size == b->as.array.size && a->as.array.base == b->as.array.base;
        case ST_OPTIONAL: case ST_ERROR_UNION: return a->as.optional.base == b->as.optional.base;
        case ST_FUNCTION:
            if (a->as.function.param_count != b->as.function.param_count) return false;
            if (a->as.function.is_variadic != b->as.function.is_variadic) return false;
            if (a->as.function.ret != b->as.function.ret) return false;
            for (size_t i = 0; i < a->as.function.param_count; i++)
                if (a->as.function.params[i] != b->as.function.params[i]) return false;
            return true;
        case ST_STRUCT: case ST_UNION: case ST_ENUM: case ST_TRAIT:
            return sv_eq(a->as.struct_.name, b->as.struct_.name) && a->as.struct_.generic_arg_count == b->as.struct_.generic_arg_count;
        case ST_GENERIC_PARAM:
            return sv_eq(a->as.generic_param.name, b->as.generic_param.name) && a->as.generic_param.index == b->as.generic_param.index;
    }
    return false;
}

static SType *intern_type(TypeTable *tt, SType *proto) {
    uint32_t h = hash_type_shape(proto) % TYPE_TABLE_BUCKETS;
    proto->hash = h;
    for (SType *t = tt->buckets[h]; t; t = t->next_hash)
        if (t->hash == proto->hash && type_shape_eq(t, proto)) return t;
    proto->next_hash = tt->buckets[h];
    tt->buckets[h] = proto;
    return proto;
}

TypeTable *type_table_new(Arena *arena) {
    TypeTable *tt = arena_alloc(arena, sizeof(TypeTable));
    tt->arena = arena;
    memset(tt->buckets, 0, sizeof(tt->buckets));
    return tt;
}

#define ST_SIMPLE(K) do { SType t = {.kind = K}; SType *p = arena_alloc(tt->arena, sizeof(SType)); memcpy(p, &t, sizeof(t)); return intern_type(tt, p); } while(0)
SType *st_void(TypeTable *tt) { ST_SIMPLE(ST_VOID); }
SType *st_bool(TypeTable *tt) { ST_SIMPLE(ST_BOOL); }

SType *st_int(TypeTable *tt, bool is_signed, uint8_t bits) {
    SType t = {.kind = ST_INT, .as.integer = {.is_signed = is_signed, .bits = bits}};
    SType *p = arena_alloc(tt->arena, sizeof(SType)); memcpy(p, &t, sizeof(t)); return intern_type(tt, p);
}

SType *st_float(TypeTable *tt, uint8_t bits) {
    SType t = {.kind = ST_FLOAT, .as.floating = {.bits = bits}};
    SType *p = arena_alloc(tt->arena, sizeof(SType)); memcpy(p, &t, sizeof(t)); return intern_type(tt, p);
}

SType *st_pointer(TypeTable *tt, bool is_const, SType *base) {
    SType t = {.kind = ST_POINTER, .as.pointer = {.is_const = is_const, .base = base}};
    SType *p = arena_alloc(tt->arena, sizeof(SType)); memcpy(p, &t, sizeof(t)); return intern_type(tt, p);
}

SType *st_reference(TypeTable *tt, bool is_const, SType *base) {
    SType t = {.kind = ST_REFERENCE, .as.reference = {.is_const = is_const, .base = base}};
    SType *p = arena_alloc(tt->arena, sizeof(SType)); memcpy(p, &t, sizeof(t)); return intern_type(tt, p);
}

SType *st_slice(TypeTable *tt, bool is_const, SType *base) {
    SType t = {.kind = ST_SLICE, .as.slice = {.is_const = is_const, .base = base}};
    SType *p = arena_alloc(tt->arena, sizeof(SType)); memcpy(p, &t, sizeof(t)); return intern_type(tt, p);
}

SType *st_array(TypeTable *tt, uint64_t size, SType *base) {
    SType t = {.kind = ST_ARRAY, .as.array = {.size = size, .base = base}};
    SType *p = arena_alloc(tt->arena, sizeof(SType)); memcpy(p, &t, sizeof(t)); return intern_type(tt, p);
}

SType *st_optional(TypeTable *tt, SType *base) {
    SType t = {.kind = ST_OPTIONAL, .as.optional = {.base = base}};
    SType *p = arena_alloc(tt->arena, sizeof(SType)); memcpy(p, &t, sizeof(t)); return intern_type(tt, p);
}

SType *st_error_union(TypeTable *tt, SType *base) {
    SType t = {.kind = ST_ERROR_UNION, .as.error_union = {.base = base}};
    SType *p = arena_alloc(tt->arena, sizeof(SType)); memcpy(p, &t, sizeof(t)); return intern_type(tt, p);
}

SType *st_function(TypeTable *tt, SType **params, size_t pc, SType *ret, bool variadic) {
    SType t = {.kind = ST_FUNCTION};
    t.as.function.params = arena_alloc(tt->arena, pc * sizeof(SType*));
    memcpy(t.as.function.params, params, pc * sizeof(SType*));
    t.as.function.param_count = pc;
    t.as.function.ret = ret;
    t.as.function.is_variadic = variadic;
    SType *p = arena_alloc(tt->arena, sizeof(SType)); memcpy(p, &t, sizeof(t)); return intern_type(tt, p);
}

bool st_eq(SType *a, SType *b) { return a == b; }
bool st_is_integer(SType *t) { return t->kind == ST_INT; }
bool st_is_numeric(SType *t) { return t->kind == ST_INT || t->kind == ST_FLOAT; }

bool st_can_coerce(SType *from, SType *to) {
    if (st_eq(from, to)) return true;
    if (from->kind == ST_INT && to->kind == ST_INT) {
        if (from->as.integer.bits == 64) return true;
        return from->as.integer.is_signed == to->as.integer.is_signed && from->as.integer.bits <= to->as.integer.bits;
    }
    if (from->kind == ST_FLOAT && to->kind == ST_FLOAT) {
        if (from->as.floating.bits == 64) return true;
        return from->as.floating.bits <= to->as.floating.bits;
    }
    if (from->kind == ST_INT && to->kind == ST_INT && from->as.integer.bits == 64) return true;
    if (from->kind == ST_FLOAT && to->kind == ST_FLOAT && from->as.floating.bits == 64) return true;
    if (from->kind == ST_INT && to->kind == ST_FLOAT) return true;
    if (to->kind == ST_OPTIONAL && st_can_coerce(from, to->as.optional.base)) return true;
    if (from->kind == ST_REFERENCE && to->kind == ST_REFERENCE)
        return !from->as.reference.is_const && to->as.reference.is_const && st_eq(from->as.reference.base, to->as.reference.base); 
    if (from->kind == ST_REFERENCE && st_eq(from->as.reference.base, to)) return true;
    if (from->kind == ST_POINTER && st_eq(from->as.pointer.base, to)) return true;
    return false;
}

const char *st_name(SType *t) {
    static char bufs[8][256];
    static int buf_idx = 0;
    char *buf = bufs[buf_idx++ % 8];
    switch (t->kind) {
        case ST_VOID: return "void";
        case ST_BOOL: return "bool";
        case ST_INT: snprintf(buf, 256, "%c%u", t->as.integer.is_signed ? 'i' : 'u', t->as.integer.bits); return buf;
        case ST_FLOAT: snprintf(buf, 256, "f%u", t->as.floating.bits); return buf;
        case ST_NORETURN: return "noreturn";
        case ST_POINTER: snprintf(buf, 256, "*%s%s", t->as.pointer.is_const ? "const " : "", st_name(t->as.pointer.base)); return buf;
        case ST_REFERENCE: snprintf(buf, 256, "&%s%s", t->as.reference.is_const ? "const " : "", st_name(t->as.reference.base)); return buf;
        case ST_SLICE: snprintf(buf, 256, "[]%s%s", t->as.slice.is_const ? "const " : "", st_name(t->as.slice.base)); return buf;
        case ST_ARRAY: snprintf(buf, 256, "[%llu]%s", (unsigned long long)t->as.array.size, st_name(t->as.array.base)); return buf;
        case ST_OPTIONAL: snprintf(buf, 256, "?%s", st_name(t->as.optional.base)); return buf;
        case ST_ERROR_UNION: snprintf(buf, 256, "!%s", st_name(t->as.error_union.base)); return buf;
        case ST_FUNCTION: snprintf(buf, 256, "fn(...) -> %s", st_name(t->as.function.ret)); return buf;
        case ST_STRUCT: case ST_UNION: case ST_ENUM: case ST_TRAIT:
            if (t->as.struct_.name.data) { snprintf(buf, 256, "%.*s", (int)t->as.struct_.name.len, t->as.struct_.name.data); return buf; }
            return "<anon>";
        case ST_GENERIC_PARAM:
            if (t->as.generic_param.name.data) { snprintf(buf, 256, "%.*s", (int)t->as.generic_param.name.len, t->as.generic_param.name.data); return buf; }
            return "<T>";
        case ST_OPAQUE: return "<opaque>";
    }
    return "<unknown>";
}

void st_print(SType *t) { printf("%s", st_name(t)); }

SType *st_from_ast(TypeTable *tt, TypeExpr *te) {
    if (!te) return st_void(tt);
    switch (te->kind) {
        case TYPE_NAMED: {
            StringView n = te->named.name;
            if (sv_eq_cstr(n, "void")) return st_void(tt);
            if (sv_eq_cstr(n, "bool")) return st_bool(tt);
            if (sv_eq_cstr(n, "isize")) return st_int(tt, true, 64);
            if (sv_eq_cstr(n, "usize")) return st_int(tt, false, 64);
            if (n.len > 1 && n.data[0] == 'i') { int b = atoi(n.data+1); if (b==8||b==16||b==32||b==64||b==128) return st_int(tt, true, b); }
            if (n.len > 1 && n.data[0] == 'u') { int b = atoi(n.data+1); if (b==8||b==16||b==32||b==64||b==128) return st_int(tt, false, b); }
            if (n.len > 1 && n.data[0] == 'f') { int b = atoi(n.data+1); if (b==32||b==64) return st_float(tt, b); }
            if (sv_eq_cstr(n, "noreturn")) { SType t = {.kind = ST_NORETURN}; SType *p = arena_alloc(tt->arena, sizeof(SType)); memcpy(p, &t, sizeof(t)); return intern_type(tt, p); }
            SType t = {.kind = ST_STRUCT}; t.as.struct_.name = n; SType *p = arena_alloc(tt->arena, sizeof(SType)); memcpy(p, &t, sizeof(t)); return intern_type(tt, p);
        }
        case TYPE_POINTER: return st_pointer(tt, te->unary.is_const, st_from_ast(tt, te->unary.child));
        case TYPE_REFERENCE: return st_reference(tt, te->unary.is_const, st_from_ast(tt, te->unary.child));
        case TYPE_SLICE: return st_slice(tt, te->unary.is_const, st_from_ast(tt, te->unary.child));
        case TYPE_ARRAY: {
            uint64_t sz = 0;
            if (te->array.length && te->array.length->kind == AST_INT_LITERAL) sz = (uint64_t)te->array.length->int_literal.value;
            return st_array(tt, sz, st_from_ast(tt, te->array.elem));
        }
        case TYPE_OPTIONAL: return st_optional(tt, st_from_ast(tt, te->unary.child));
        case TYPE_ERROR_UNION: return st_error_union(tt, st_from_ast(tt, te->unary.child));
        case TYPE_FUNCTION: {
            size_t pc = te->func.params.count;
            SType **ps = arena_alloc(tt->arena, pc * sizeof(SType*));
            for (size_t i = 0; i < pc; i++) ps[i] = st_from_ast(tt, te->func.params.items[i]);
            return st_function(tt, ps, pc, st_from_ast(tt, te->func.ret), false);
        }
        default: return st_void(tt);
    }
}
