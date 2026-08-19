#ifndef RAYA_DIAG_H
#define RAYA_DIAG_H

#include "common.h"
#include "string_view.h"
#include "source_loc.h"

typedef enum {
    DIAG_ERROR,
    DIAG_WARNING,
    DIAG_NOTE,
} DiagKind;

typedef struct {
    DiagKind kind;
    SourceLocation loc;
    StringView message;
    const char* source;
    size_t source_len;
} Diagnostic;

typedef struct {
    Diagnostic* items;
    size_t count;
    size_t capacity;
    size_t error_count;
    size_t warning_count;
    bool use_color;
    bool has_errors;
} DiagnosticEngine;

void diag_init(DiagnosticEngine* d);
void diag_emit(DiagnosticEngine* d, DiagKind kind, SourceLocation loc,
               const char* fmt, ...);
void diag_print_all(DiagnosticEngine* d, const char* source, size_t source_len);
void diag_print_summary(DiagnosticEngine* d);

#define diag_error(d, loc, ...) diag_emit((d), DIAG_ERROR, (loc), __VA_ARGS__)
#define diag_warn(d, loc, ...)  diag_emit((d), DIAG_WARNING, (loc), __VA_ARGS__)

#endif
