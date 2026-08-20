#include "common.h"
#include "arena.h"
#include "string_view.h"
#include "source_loc.h"
#include "diag.h"
#include "lexer.h"

bool g_dump_tokens = false;
bool g_dump_ast = false;
bool g_expand = false;

static void print_usage(const char* prog) {
    fprintf(stderr, "Raya compiler %s\n", RAYA_VERSION);
    fprintf(stderr, "Usage: %s [options] <file.raya>\n", prog);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  --dump-tokens    Print all tokens after lexing\n");
    fprintf(stderr, "  --dump-ast       Print AST after parsing\n");
    fprintf(stderr, "  --expand         Print expanded source after comptime\n");
    fprintf(stderr, "  -h, --help       Show this help\n");
    fprintf(stderr, "  -v, --version    Show version\n");
}

static char* read_file(const char* path, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "error: cannot open file '%s'\n", path);
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fprintf(stderr, "error: failed to seek file '%s'\n", path);
        fclose(f);
        return NULL;
    }

    long file_len = ftell(f);

    if (file_len < 0) {
        fprintf(stderr, "error: failed to determine size of '%s'\n", path);
        fclose(f);
        return NULL;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fprintf(stderr, "error: failed to seek file '%s'\n", path);
        fclose(f);
        return NULL;
    }

    size_t len = (size_t)file_len;

    char* buf = (char*)malloc(len + 1);

    if (!buf) {
        fprintf(stderr, "error: out of memory\n");
        fclose(f);
        return NULL;
    }

    size_t read = fread(buf, 1, len, f);

    if (read != len && ferror(f)) {
        fprintf(stderr, "error: failed to read '%s'\n", path);
        free(buf);
        fclose(f);
        return NULL;
    }

    buf[read] = '\0';

    fclose(f);

    *out_len = read;
    return buf;
}

static void dump_tokens(Lexer* lexer) {
    printf("=== TOKENS ===\n");
    printf("%-20s %-30s %s\n", "KIND", "TEXT", "LOCATION");
    printf("%-20s %-30s %s\n", "----", "----", "--------");

    Token tok;
    int count = 0;
    while ((tok = lexer_next(lexer)).kind != TOK_EOF) {
        printf("%-20s %-30.*s %s:%lu:%lu\n",
               token_kind_name(tok.kind),
               (int)tok.text.len, tok.text.data,
               tok.loc.filename,
               (unsigned long)tok.loc.line,
               (unsigned long)tok.loc.column);
        count++;
        if (count > 10000) {
            printf("... (truncated after 10000 tokens)\n");
            break;
        }
    }
    printf("\nTotal: %d tokens\n", count);
}

int main(int argc, char** argv) {
    const char* input_file = NULL;
    
     /*
     * ------------------------------------------------------------------
     * Command-line arguments
     * ------------------------------------------------------------------
     */

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dump-tokens") == 0) {
            g_dump_tokens = true;

        } else if (strcmp(argv[i], "--dump-ast") == 0) {
            g_dump_ast = true;

        } else if (strcmp(argv[i], "--expand") == 0) {
            g_expand = true;

        } else if (strcmp(argv[i], "-h") == 0 ||
                   strcmp(argv[i], "--help") == 0) {

            print_usage(argv[0]);
            return 0;

        } else if (strcmp(argv[i], "-v") == 0 ||
                   strcmp(argv[i], "--version") == 0) {

            printf("Raya compiler %s\n", RAYA_VERSION);
            return 0;

        } else if (argv[i][0] == '-') {
            fprintf(stderr,
                    "error: unknown option '%s'\n",
                    argv[i]);

            print_usage(argv[0]);
            return 1;

        } else {
            if (input_file) {
                fprintf(stderr,
                        "error: multiple input files not supported\n");

                return 1;
            }

            input_file = argv[i];
        }
    }

    /*
     * ------------------------------------------------------------------
     * Input validation
     * ------------------------------------------------------------------
     */

    if (!input_file) {
        fprintf(stderr, "error: no input file\n");
        print_usage(argv[0]);
        return 1;
    }

    size_t input_len = strlen(input_file);

    if (input_len < 5 ||
        strcmp(input_file + input_len - 5, ".raya") != 0) {

        fprintf(stderr,
                "warning: input file does not have .raya extension\n");
    }

    /*
     * ------------------------------------------------------------------
     * Read source
     * ------------------------------------------------------------------
     */

    size_t source_len = 0;

    char* source = read_file(input_file, &source_len);

    if (!source) {
        return 1;
    }

    /*
     * ------------------------------------------------------------------
     * Compiler state
     * ------------------------------------------------------------------
     */

    Arena arena;
    arena_init(&arena, 1024 * 1024);

    DiagnosticEngine diag;
    diag_init(&diag);

    Lexer lexer;
    lexer_init(
        &lexer,
        source,
        source_len,
        input_file,
        &diag
    );

    /*
     * ------------------------------------------------------------------
     * Phase 0: lexing
     * ------------------------------------------------------------------
     */

    printf(
        "Raya compiler %s — compiling '%s' (%lu bytes)\n\n",
        RAYA_VERSION,
        input_file,
        (unsigned long)source_len
    );

    if (g_dump_tokens) {
        dump_tokens(&lexer);

        /*
         * Reset lexer so diagnostics / later phases can
         * consume the source from the beginning.
         */
        lexer_init(
            &lexer,
            source,
            source_len,
            input_file,
            &diag
        );
    }

    /*
     * ------------------------------------------------------------------
     * Diagnostics
     * ------------------------------------------------------------------
     */

    if (diag.error_count > 0 ||
        diag.warning_count > 0) {

        diag_print_all(
            &diag,
            source,
            source_len
        );
    }

    diag_print_summary(&diag);
    int result = diag.has_errors ? 1 : 0;
    /*
     * ------------------------------------------------------------------
     * Cleanup
     * ------------------------------------------------------------------
     */
    
    for (size_t i = 0; i < diag.count; i++) {
        free((void*)diag.items[i].message.data);
    }

    free(diag.items);
    arena_free_all(&arena);
    free(source);

    return result; 
}
