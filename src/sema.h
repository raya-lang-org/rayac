#ifndef RAYA_SEMA_H
#define RAYA_SEMA_H
#define METHOD_TABLE_BUCKETS 64

#include "common.h"
#include "arena.h"
#include "symbol.h"
#include "type.h"
#include "source_loc.h"
#include "diag.h"

typedef struct AstNode AstNode;

typedef struct MethodEntry {
    StringView type_name;
    StringView method_name;
    AstNode *fn_decl;
    struct MethodEntry *next;
} MethodEntry;

typedef struct {
    MethodEntry *buckets[METHOD_TABLE_BUCKETS];
} MethodTable;

typedef struct Sema Sema;

struct Sema {
    Arena *arena;
    TypeTable *types;
    Scope *module_scope;
    Scope *current_scope;
    SType *current_self_type;
    struct AstNode *current_fn;
    DiagnosticEngine *diag;
    size_t error_count;
    bool in_unsafe;
    bool current_fn_has_return;
    bool in_collect_decls;
    MethodTable method_table;   // ← was MethodEntry, now MethodTable
};

Sema *sema_new(Arena *arena, DiagnosticEngine *diag);
void sema_run(Sema *s, AstNode *module);

SType *sema_check_expr(Sema *s, AstNode *expr);
SType *sema_check_stmt(Sema *s, AstNode *stmt);
SType *sema_check_block(Sema *s, AstNode *block);

void sema_report(Sema *s, SourceLocation loc, const char *fmt, ...);
SType *sema_resolve_type(Sema *s, TypeExpr *type_expr);

#endif
