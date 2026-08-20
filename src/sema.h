#ifndef SEMA_H
#define SEMA_H

#include "common.h"
#include "arena.h"
#include "symbol.h"
#include "type.h"

// Forward declarations to avoid pulling in full headers
struct AstNode;
struct DiagEngine;   // <-- Replace with your actual diagnostic struct name

typedef struct {
    Arena *arena;
    TypeTable *types;
    Scope *module_scope;
    Scope *current_scope;
    struct DiagEngine *diag;
    
    SemaType *current_self_type;
    struct AstNode *current_fn;
    
    size_t error_count;
} Sema;

Sema *sema_new(Arena *arena, struct DiagEngine *diag);
void sema_run(Sema *s, struct AstNode *module);

void sema_collect_decls(Sema *s, struct AstNode *module);
void sema_resolve_bodies(Sema *s, struct AstNode *module);

SemaType *sema_check_expr(Sema *s, struct AstNode *expr);
SemaType *sema_check_stmt(Sema *s, struct AstNode *stmt);
SemaType *sema_check_block(Sema *s, struct AstNode *block);

void sema_report(Sema *s, SourceLocation loc, const char *fmt, ...);
SemaType *sema_resolve_type(Sema *s, struct AstNode *type_expr);

#endif
