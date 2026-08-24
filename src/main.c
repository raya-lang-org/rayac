#include "common.h"
#include "arena.h"
#include "string_view.h"
#include "source_loc.h"
#include "diag.h"
#include "lexer.h"
#include "parser.h"
#include "sema.h"
#include "codegen_c.h"

bool g_dump_tokens = false;
bool g_dump_ast = false;
bool g_expand = false;
bool g_test_lexer = false;
bool g_test_parser = false;
bool g_check = false;
bool g_test_sema = false;
bool g_build = false;

static void print_usage(const char* prog) {
    fprintf(stderr, "Raya compiler %s\n", RAYA_VERSION);
    fprintf(stderr, "Usage: %s [options] <file.raya>\n", prog);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  --dump-tokens    Print all tokens after lexing\n");
    fprintf(stderr, "  --dump-ast       Print AST after parsing\n");
    fprintf(stderr, "  --expand         Print expanded source after comptime\n");
    fprintf(stderr, "  --test-lexer     Output token kinds only (for tests)\n");
    fprintf(stderr, "  --test-parser    Output AST kinds only (for tests)\n");
    fprintf(stderr, "  --check          Run semantic analysis\n");
    fprintf(stderr, "  --test-sema      Output sema errors only (for tests)\n");
    fprintf(stderr, "  --build          Compile to executable\n");
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

static void test_lexer(Lexer* lexer) {
    Token tok;
    while ((tok = lexer_next(lexer)).kind != TOK_EOF) {
        printf("%s\n", token_kind_name(tok.kind));
    }
}

static void test_parser(AstNode* node, int depth) {
    if (!node) return;
    const char* names[] = {
        "COMPILATION_UNIT", "MODULE_DECL", "IMPORT_DECL", "FN_DECL",
        "STRUCT_DECL", "UNION_DECL", "ENUM_DECL", "TRAITS_DECL",
        "EXTEND_DECL", "TYPE_ALIAS", "CONST_DECL", "VAR_DECL",
        "TEST_DECL", "FIELD_DECL", "VARIANT_DECL", "TRAIT_METHOD_DECL",
        "PARAM_DECL", "GENERIC_PARAM_DECL", "BLOCK", "EXPR_STMT",
        "RETURN_STMT", "IF_STMT", "WHILE_STMT", "FOR_STMT",
        "DEFER_STMT", "ERRDEFER_STMT", "BREAK_STMT", "CONTINUE_STMT",
        "MATCH_STMT", "ASSIGN_STMT", "INT_LITERAL", "FLOAT_LITERAL",
        "STRING_LITERAL", "CHAR_LITERAL", "BOOL_LITERAL", "NULL_LITERAL",
        "UNDEFINED_LITERAL", "IDENTIFIER", "BINARY_EXPR", "UNARY_EXPR",
        "CALL_EXPR", "METHOD_CALL_EXPR", "FIELD_ACCESS_EXPR", "INDEX_EXPR",
        "SLICE_EXPR", "CAST_EXPR", "TRY_EXPR", "ERROR_CAPTURE_EXPR",
        "UNSAFE_BLOCK_EXPR", "ARRAY_LITERAL", "STRUCT_LITERAL", "ARG_LIST",
        "MATCH_ARM"
    };
    const char* name = (node->kind >= 0 && node->kind < (int)(sizeof(names)/sizeof(names[0])))
        ? names[node->kind] : "UNKNOWN";
    for (int i = 0; i < depth; i++) printf("  ");
    printf("%s\n", name);
    switch (node->kind) {
        case AST_COMPILATION_UNIT:
            for (size_t i = 0; i < node->compilation_unit.imports.count; i++)
                test_parser(node->compilation_unit.imports.items[i], depth + 1);
            for (size_t i = 0; i < node->compilation_unit.decls.count; i++)
                test_parser(node->compilation_unit.decls.items[i], depth + 1);
            break;
        case AST_FN_DECL:
            for (size_t i = 0; i < node->fn_decl.params.count; i++)
                test_parser(node->fn_decl.params.items[i], depth + 1);
            if (node->fn_decl.body) test_parser(node->fn_decl.body, depth + 1);
            break;
        case AST_STRUCT_DECL:
        case AST_UNION_DECL:
            for (size_t i = 0; i < node->struct_decl.fields.count; i++)
                test_parser(node->struct_decl.fields.items[i], depth + 1);
            break;
        case AST_ENUM_DECL:
            for (size_t i = 0; i < node->enum_decl.variants.count; i++)
                test_parser(node->enum_decl.variants.items[i], depth + 1);
            break;
        case AST_TRAITS_DECL:
            for (size_t i = 0; i < node->traits_decl.methods.count; i++)
                test_parser(node->traits_decl.methods.items[i], depth + 1);
            break;
        case AST_EXTEND_DECL:
            for (size_t i = 0; i < node->extend_decl.methods.count; i++)
                test_parser(node->extend_decl.methods.items[i], depth + 1);
            break;
        case AST_BLOCK:
            for (size_t i = 0; i < node->block.stmts.count; i++)
                test_parser(node->block.stmts.items[i], depth + 1);
            if (node->block.trailing_expr) test_parser(node->block.trailing_expr, depth + 1);
            break;
        case AST_IF_STMT:
            test_parser(node->if_stmt.then_block, depth + 1);
            if (node->if_stmt.else_block) test_parser(node->if_stmt.else_block, depth + 1);
            break;
        case AST_WHILE_STMT:
            test_parser(node->while_stmt.body, depth + 1);
            break;
        case AST_FOR_STMT:
            test_parser(node->for_stmt.body, depth + 1);
            break;
        case AST_MATCH_STMT:
            for (size_t i = 0; i < node->match_stmt.arms.count; i++)
                test_parser(node->match_stmt.arms.items[i], depth + 1);
            break;
        case AST_CALL_EXPR:
            for (size_t i = 0; i < node->call_expr.args.count; i++)
                test_parser(node->call_expr.args.items[i], depth + 1);
            break;
        case AST_METHOD_CALL_EXPR:
            for (size_t i = 0; i < node->method_call_expr.args.count; i++)
                test_parser(node->method_call_expr.args.items[i], depth + 1);
            break;
        case AST_ARRAY_LITERAL:
            for (size_t i = 0; i < node->array_literal.elements.count; i++)
                test_parser(node->array_literal.elements.items[i], depth + 1);
            break;
        case AST_STRUCT_LITERAL:
            for (size_t i = 0; i < node->struct_literal.fields.count; i++)
                test_parser(node->struct_literal.fields.items[i], depth + 1);
            break;
        default: break;
    }
}

int main(int argc, char** argv) {
    const char* input_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dump-tokens") == 0) {
            g_dump_tokens = true;
        } else if (strcmp(argv[i], "--dump-ast") == 0) {
            g_dump_ast = true;
        } else if (strcmp(argv[i], "--expand") == 0) {
            g_expand = true;
        } else if (strcmp(argv[i], "--test-lexer") == 0) {
            g_test_lexer = true;
        } else if (strcmp(argv[i], "--test-parser") == 0) {
            g_test_parser = true;
        } else if (strcmp(argv[i], "--check") == 0) {
            g_check = true;
        } else if (strcmp(argv[i], "--test-sema") == 0) {
            g_test_sema = true;
        }  else if (strcmp(argv[i], "--build") == 0) {
            g_build = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("Raya compiler %s\n", RAYA_VERSION);
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            if (input_file) {
                fprintf(stderr, "error: multiple input files not supported\n");
                return 1;
            }
            input_file = argv[i];
        }
    }

    if (!input_file) {
        fprintf(stderr, "error: no input file\n");
        print_usage(argv[0]);
        return 1;
    }

    size_t input_len = strlen(input_file);
    if (input_len < 5 || strcmp(input_file + input_len - 5, ".raya") != 0) {
        fprintf(stderr, "warning: input file does not have .raya extension\n");
    }

    size_t source_len = 0;
    char* source = read_file(input_file, &source_len);
    if (!source) return 1;

    Arena arena;
    arena_init(&arena, 1024 * 1024);

    DiagnosticEngine diag;
    diag_init(&diag);

    Lexer lexer;
    lexer_init(&lexer, source, source_len, input_file, &diag);

    if (g_test_lexer) {
        test_lexer(&lexer);
        arena_free_all(&arena);
        free(source);
        return diag.has_errors ? 1 : 0;
    }

    if (g_dump_tokens) {
        printf("Raya compiler %s — compiling '%s' (%lu bytes)\n\n",
               RAYA_VERSION, input_file, (unsigned long)source_len);
        dump_tokens(&lexer);
        lexer_init(&lexer, source, source_len, input_file, &diag);
    }

    /* Tokenize everything into an array for the parser */
    Token* tokens = NULL;
    size_t token_count = 0;
    size_t token_cap = 0;
    Token t;
    while ((t = lexer_next(&lexer)).kind != TOK_EOF) {
        if (token_count >= token_cap) {
            token_cap = token_cap ? token_cap * 2 : 256;
            tokens = (Token*)realloc(tokens, token_cap * sizeof(Token));
        }
        tokens[token_count++] = t;
    }
    /* Append EOF token */
    if (token_count >= token_cap) {
        token_cap = token_cap ? token_cap * 2 : 256;
        tokens = (Token*)realloc(tokens, token_cap * sizeof(Token));
    }
    t.kind = TOK_EOF;
    t.text = sv_from_cstr("");
    t.loc = loc_make(input_file, 1, 1, source_len);
    tokens[token_count++] = t;

    Parser parser;
    parser_init(&parser, tokens, token_count, &arena);
    AstNode* ast = parser_parse(&parser);

    /* ===== SEMANTIC ANALYSIS ===== */
    if (g_test_sema) {
        Sema *sema = sema_new(&arena, &diag);
        sema_run(sema, ast);
        if (diag.error_count == 0) {
            printf("ok\n");
        } else {
            for (size_t i = 0; i < diag.count; i++) {
                if (diag.items[i].kind == DIAG_ERROR) {
                    printf("%.*s\n", (int)diag.items[i].message.len, diag.items[i].message.data);
                }
            }
        }
        for (size_t i = 0; i < diag.count; i++) {
            free((void*)diag.items[i].message.data);
        }
        free(diag.items);
        free(tokens);
        arena_free_all(&arena);
        free(source);
        return diag.error_count > 0 ? 1 : 0;
    }

    if (g_check) {
        Sema *sema = sema_new(&arena, &diag);
        sema_run(sema, ast);
    }
    /* ===== END SEMANTIC ANALYSIS ===== */

    if (g_build) {
        Sema *sema = sema_new(&arena, &diag);
        sema_run(sema, ast);
        if (diag.error_count > 0) {
            diag_print_all(&diag, source, source_len);
            diag_print_summary(&diag);
            int result = 1;
            for (size_t i = 0; i < diag.count; i++) {
                free((void*)diag.items[i].message.data);
            }
            free(diag.items);
            free(tokens);
            arena_free_all(&arena);
            free(source);
            return result;
        }

        char c_path[1024];
        char bin_path[1024];
        snprintf(c_path, sizeof(c_path), "%s.c", input_file);
        snprintf(bin_path, sizeof(bin_path), "%s", input_file);
        size_t flen = strlen(bin_path);
        if (flen > 5 && strcmp(bin_path + flen - 5, ".raya") == 0)
            bin_path[flen - 5] = '\0';

        FILE *out = fopen(c_path, "w");
        if (!out) {
            fprintf(stderr, "error: could not write %s\n", c_path);
            free(tokens);
            arena_free_all(&arena);
            free(source);
            return 1;
        }
        codegen_c_emit(ast, out);
        fclose(out);
        printf("Generated: %s\n", c_path);

        char cmd[2048];
        snprintf(cmd, sizeof(cmd),
            "cc -O2 -std=c11 -Isrc %s src/raya_rt.c -o %s",
            c_path, bin_path);
        printf("Running: %s\n", cmd);
        int ret = system(cmd);
        if (ret != 0) {
            fprintf(stderr, "error: C compilation failed\n");
            free(tokens);
            arena_free_all(&arena);
            free(source);
            return 1;
        }

        printf("Built: %s\n", bin_path);

        for (size_t i = 0; i < diag.count; i++) {
            free((void*)diag.items[i].message.data);
        }
        free(diag.items);
        free(tokens);
        arena_free_all(&arena);
        free(source);
        return 0;
    }

    if (g_test_parser) {
        test_parser(ast, 0);
    } else if (g_dump_ast) {
        printf("Raya compiler %s — AST dump for '%s'\n\n",
               RAYA_VERSION, input_file);
        ast_print(ast, 0);
    }

    if (diag.error_count > 0 || diag.warning_count > 0) {
        diag_print_all(&diag, source, source_len);
    }
    diag_print_summary(&diag);

    int result = diag.has_errors || parser.had_error ? 1 : 0;

    for (size_t i = 0; i < diag.count; i++) {
        free((void*)diag.items[i].message.data);
    }
    free(diag.items);
    free(tokens);
    arena_free_all(&arena);
    free(source);

    return result;
}
