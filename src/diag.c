#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "diag.h"
#include <stdarg.h>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#else
#include <unistd.h>
#endif

static bool stderr_is_tty(void) {
    return isatty(fileno(stderr));
}

static const char* color_red(void)    { return stderr_is_tty() ? "\033[1;31m" : ""; }
static const char* color_yellow(void) { return stderr_is_tty() ? "\033[1;33m" : ""; }
static const char* color_blue(void)   { return stderr_is_tty() ? "\033[1;34m" : ""; }
static const char* color_reset(void)  { return stderr_is_tty() ? "\033[0m"  : ""; }
static const char* color_bold(void)   { return stderr_is_tty() ? "\033[1m"  : ""; }

void diag_init(DiagnosticEngine* d) {
    d->items = NULL;
    d->count = 0;
    d->capacity = 0;
    d->error_count = 0;
    d->warning_count = 0;
    d->use_color = stderr_is_tty();
    d->has_errors = false;
}

void diag_emit(DiagnosticEngine* d, DiagKind kind, SourceLocation loc,
               const char* fmt, ...) {
    if (d->count >= d->capacity) {
        d->capacity = d->capacity ? d->capacity * 2 : 16;
        d->items = (Diagnostic*)realloc(d->items, d->capacity * sizeof(Diagnostic));
    }
    Diagnostic* diag = &d->items[d->count++];
    diag->kind = kind;
    diag->loc = loc;

    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    diag->message = sv_from_cstr(strdup(buf));
    diag->source = NULL;
    diag->source_len = 0;

    if (kind == DIAG_ERROR) {
        d->error_count++;
        d->has_errors = true;
    } else if (kind == DIAG_WARNING) {
        d->warning_count++;
    }
}

static void print_context_line(const char* source, size_t source_len,
                                size_t line_num, size_t target_line) {
    size_t current_line = 1;
    size_t pos = 0;
    while (pos < source_len && current_line < target_line) {
        if (source[pos] == '\n') current_line++;
        pos++;
    }
    if (current_line != target_line) return;
    size_t end = pos;
    while (end < source_len && source[end] != '\n' && source[end] != '\r') end++;
    fprintf(stderr, " %4lu | %.*s\n", (unsigned long)line_num, (int)(end - pos), source + pos);
}

void diag_print_all(DiagnosticEngine* d, const char* source, size_t source_len) {
    for (size_t i = 0; i < d->count; i++) {
        Diagnostic* diag = &d->items[i];
        const char* kind_str = "";
        const char* kind_color = "";
        const char* code = "";

        switch (diag->kind) {
            case DIAG_ERROR:
                kind_str = "error";
                kind_color = color_red();
                code = "E0001";
                break;
            case DIAG_WARNING:
                kind_str = "warning";
                kind_color = color_yellow();
                code = "W0001";
                break;
            case DIAG_NOTE:
                kind_str = "note";
                kind_color = color_blue();
                code = "";
                break;
        }

        fprintf(stderr, "%s%s%s[%s]%s: %.*s\n",
                kind_color, color_bold(), kind_str, code, color_reset(),
                (int)diag->message.len, diag->message.data);

        fprintf(stderr, " %s--> %s:%lu:%lu%s\n",
                color_blue(), diag->loc.filename,
                (unsigned long)diag->loc.line,
                (unsigned long)diag->loc.column,
                color_reset());

        fprintf(stderr, " %s |%s\n", color_blue(), color_reset());
        size_t ctx_start = diag->loc.line > 1 ? diag->loc.line - 1 : 1;
        for (size_t ln = ctx_start; ln <= diag->loc.line + 1; ln++) {
            print_context_line(source, source_len, ln, ln);
            if (ln == diag->loc.line) {
                fprintf(stderr, " %s |%s ", color_blue(), color_reset());
                for (size_t j = 0; j < diag->loc.column - 1; j++) fprintf(stderr, " ");
                fprintf(stderr, "%s^%s", color_red(), color_reset());
                fprintf(stderr, "%s~~~~%s", color_red(), color_reset());
                fprintf(stderr, "\n");
            }
        }
        fprintf(stderr, "\n");
    }
}

void diag_print_summary(DiagnosticEngine* d) {
    if (d->error_count > 0) {
        fprintf(stderr, "%serror%s: could not compile due to %lu previous error%s\n",
                color_red(), color_reset(),
                (unsigned long)d->error_count,
                d->error_count == 1 ? "" : "s");
    }
}
