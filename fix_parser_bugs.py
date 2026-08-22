#!/usr/bin/env python3
"""Fix parser bugs in rayac source files. Run from rayac repo root."""

import re
import sys

def read_file(path):
    with open(path, 'r') as f:
        return f.read()

def write_file(path, content):
    with open(path, 'w') as f:
        f.write(content)

def fix_parser_toplevel_const():
    path = 'src/parser.c'
    src = read_file(path)

    # Find: parser_expect(p, TOK_COLON, "expected ':'"); followed by TypeExpr *type = parser_parse_type(p);
    # Replace with optional type
    pattern = r'(parser_expect\(p, TOK_IDENTIFIER,\s*"expected identifier"\);)\s*\n\s*parser_expect\(p, TOK_COLON,\s*"expected \':\'"\);\s*\n\s*TypeExpr \*type = parser_parse_type\(p\);'

    def repl(m):
        return (
            m.group(1) + '\n'
            '            TypeExpr *type = NULL;\n'
            '            if (parser_match(p, TOK_COLON))\n'
            '                type = parser_parse_type(p);'
        )

    new_src, count = re.subn(pattern, repl, src, count=1)
    if count == 0:
        print("ERROR: Could not find top-level const pattern in parser.c")
        return False

    write_file(path, new_src)
    print("OK: src/parser.c — top-level const/var type is now optional")
    return True

def fix_parser_trailing_comma():
    path = 'src/parser.c'
    src = read_file(path)

    # Struct: parser_expect(p, TOK_COMMA, "...");\n ast_struct_add_field(p->arena, s, field);
    pattern1 = r'parser_expect\(p, TOK_COMMA,\s*"expected \',\' after field declaration"\);\s*\n\s*ast_struct_add_field\(p->arena, s, field\);'
    repl1 = 'ast_struct_add_field(p->arena, s, field);\n        if (!parser_match(p, TOK_COMMA))\n            break;'
    new_src, count1 = re.subn(pattern1, repl1, src, count=1)
    if count1 == 0:
        print("ERROR: Could not find struct trailing-comma pattern")
        return False

    # Union
    pattern2 = r'parser_expect\(p, TOK_COMMA,\s*"expected \',\' after field declaration"\);\s*\n\s*ast_struct_add_field\(p->arena, u, field\);'
    repl2 = 'ast_struct_add_field(p->arena, u, field);\n        if (!parser_match(p, TOK_COMMA))\n            break;'
    new_src, count2 = re.subn(pattern2, repl2, new_src, count=1)
    if count2 == 0:
        print("ERROR: Could not find union trailing-comma pattern")
        return False

    # Enum
    pattern3 = r'parser_expect\(p, TOK_COMMA,\s*"expected \',\' after variant"\);\s*\n\s*ast_enum_add_variant\(p->arena, e, variant\);'
    repl3 = 'ast_enum_add_variant(p->arena, e, variant);\n        if (!parser_match(p, TOK_COMMA))\n            break;'
    new_src, count3 = re.subn(pattern3, repl3, new_src, count=1)
    if count3 == 0:
        print("ERROR: Could not find enum trailing-comma pattern")
        return False

    write_file(path, new_src)
    print("OK: src/parser.c — trailing comma now optional in struct/union/enum")
    return True

def fix_sema_local_init_name():
    path = 'src/sema.c'
    src = read_file(path)

    pattern = r'(if \(stmt->var_decl\.init && !st_can_coerce\(init_type, decl_type\)\))\s*\n\s*sema_report\(s, stmt->loc, "cannot initialize variable of type \'%s\' with \'%s\'", st_name\(decl_type\), st_name\(init_type\)\);'

    def repl(m):
        return (
            m.group(1) + '\n'
            '                    sema_report(s, stmt->loc, "cannot initialize variable \'%.*s\' of type \'%s\' with \'%s\'",\n'
            '                        SV_ARG(stmt->var_decl.name), st_name(decl_type), st_name(init_type));'
        )

    new_src, count = re.subn(pattern, repl, src, count=1)
    if count == 0:
        print("ERROR: Could not find local init error pattern in sema.c")
        return False

    write_file(path, new_src)
    print("OK: src/sema.c — local init errors now include variable name")
    return True

def fix_test_runner():
    path = 'tests/sema/run_sema_tests.c'
    src = read_file(path)

    # Insert helpers before run_test
    pattern = r'(static int run_test\(const char \*raya)'

    def repl(m):
        return (
            '/* Normalize an error line by stripping [E####] codes */\n'
            'static const char *normalize_error(const char *s) {\n'
            '    if (strncmp(s, "error[", 6) == 0) {\n'
            '        const char *p = strchr(s, \']\');\n'
            '        if (p && p[1] == \':\') return p + 3;\n'
            '    }\n'
            '    if (strncmp(s, "error: ", 7) == 0) return s + 7;\n'
            '    return s;\n'
            '}\n'
            '\n'
            'static int is_error_line(const char *s) {\n'
            '    return strncmp(s, "error:", 6) == 0 || strncmp(s, "error[", 6) == 0;\n'
            '}\n'
            '\n'
            'static int output_contains(const char *haystack, const char *needle) {\n'
            '    return strstr(haystack, needle) != NULL;\n'
            '}\n'
            '\n'
            + m.group(1)
        )

    new_src, count = re.subn(pattern, repl, src, count=1)
    if count == 0:
        print("ERROR: Could not find run_test function")
        return False

    # Fix expect_errors
    pattern2 = r'if \(strstr\(exp_line,\s*"error:"\)\)\s*expect_errors = 1;'
    new_src, count2 = re.subn(pattern2, 'if (is_error_line(exp_line)) expect_errors = 1;', new_src, count=1)
    if count2 == 0:
        print("ERROR: Could not find expect_errors pattern")
        return False

    # Fix missing error check
    pattern3 = r'if \(strlen\(line\) > 0 && !strstr\(output, line\)\) \{'
    repl3 = 'if (strlen(line) > 0 && !output_contains(output, line)\n                && !output_contains(output, normalize_error(line))) {'
    new_src, count3 = re.subn(pattern3, repl3, new_src, count=1)
    if count3 == 0:
        print("ERROR: Could not find missing-error check pattern")
        return False

    write_file(path, new_src)
    print("OK: tests/sema/run_sema_tests.c — handles error[E0001]: format")
    return True

def main():
    ok = True
    ok &= fix_parser_toplevel_const()
    ok &= fix_parser_trailing_comma()
    ok &= fix_sema_local_init_name()
    ok &= fix_test_runner()

    if ok:
        print("\nAll fixes applied. Rebuild with: make clean && make CC=gcc")
        return 0
    else:
        print("\nSome fixes failed. Check messages above.")
        return 1

if __name__ == '__main__':
    sys.exit(main())
