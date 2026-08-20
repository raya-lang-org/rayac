#include "arena.h"
#include "lexer.h"
#include "parser.h"
#include "diag.h"

// Custom test loop function in your parser test runner
void run_parser_test_suite(const char** test_files, size_t file_count) {
    // 1. Initialize reusable core structures ONCE outside the loop
    Arena arena;
    arena_init(&arena, 2 * 1024 * 1024); // 2 MB reusable block
    
    DiagnosticEngine diag;
    diag_init(&diag);

    for (size_t i = 0; i < file_count; i++) {
        size_t source_len = 0;
        char* source = read_file(test_files[i], &source_len);
        if (!source) continue;

        // 2. Lex file into arena
        Lexer lexer;
        lexer_init(&lexer, source, source_len, test_files[i], &diag);

        size_t tok_cap = 256;
        size_t tok_count = 0;
        Token* tokens = arena_alloc(&arena, tok_cap * sizeof(Token));

        for (;;) {
            Token tok = lexer_next(&lexer);
            if (tok_count >= tok_cap) {
                size_t old_cap = tok_cap;
                tok_cap *= 2;
                Token* new_toks = arena_alloc(&arena, tok_cap * sizeof(Token));
                memcpy(new_toks, tokens, old_cap * sizeof(Token));
                tokens = new_toks;
            }
            tokens[tok_count++] = tok;
            if (tok.kind == TOK_EOF) break;
        }

        // 3. Parse file using shared arena
        Parser parser;
        parser_init(&parser, tokens, tok_count, &arena);
        AstNode* ast = parser_parse(&parser);

        // ... Execute verification / diff logic against expected AST output ...

        // 4. INSTANT RESET: Reuse underlying arena memory without freeing OS pages
        arena_reset(&arena); 
        
        // Clear diagnostic counters for next test case
        diag.count = 0;
        diag.error_count = 0;
        diag.warning_count = 0;
        diag.has_errors = false;

        free(source);
    }

    // 5. Free OS memory once upon test suite completion
    arena_free_all(&arena);
}
