#ifndef RAYA_SEMA_H
#define RAYA_SEMA_H

#include "common.h"
#include "arena.h"
#include "symbol.h"
#include "type.h"
#include "source_loc.h"
#include "diag.h"

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
};

Sema *sema_new(Arena *arena, DiagnosticEngine *diag);
void sema_run(Sema *s, struct AstNode *module);

SType *sema_check_expr(Sema *s, struct AstNode *expr);
SType *sema_check_stmt(Sema *s, struct AstNode *stmt);
SType *sema_check_block(Sema *s, struct AstNode *block);

void sema_report(Sema *s, SourceLocation loc, const char *fmt, ...);
SType *sema_resolve_type(Sema *s, struct TypeExpr *type_expr);

#endif
