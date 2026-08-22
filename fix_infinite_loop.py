#!/usr/bin/env python3
"""Fix infinite loop in parser when extend/traits block contains unexpected tokens."""

import re
import sys

def read_file(path):
    with open(path, 'r') as f:
        return f.read()

def write_file(path, content):
    with open(path, 'w') as f:
        f.write(content)

def fix_extend_infinite_loop():
    path = 'src/parser.c'
    src = read_file(path)

    # Fix extend decl: after parser_expect(TOK_FN) fails, advance to avoid infinite loop
    old = (
        '        parser_expect(p, TOK_FN, "expected \'fn\'");\n\n'
        '        const Token *fn_name = parser_current(p);'
    )
    new = (
        '        if (!parser_expect(p, TOK_FN, "expected \'fn\'")) {\n'
        '            if (!parser_at_end(p)) p->pos++;\n'
        '            continue;\n'
        '        }\n\n'
        '        const Token *fn_name = parser_current(p);'
    )

    if old not in src:
        print("ERROR: Could not find extend decl pattern")
        return False

    src = src.replace(old, new)
    write_file(path, src)
    print("OK: src/parser.c — extend decl no longer infinite loops on bad tokens")
    return True

def fix_traits_infinite_loop():
    path = 'src/parser.c'
    src = read_file(path)

    # Fix traits decl: same issue
    old = (
        '        parser_expect(p, TOK_FN, "expected \'fn\' in trait method");\n\n'
        '        const Token *method_name = parser_current(p);'
    )
    new = (
        '        if (!parser_expect(p, TOK_FN, "expected \'fn\' in trait method")) {\n'
        '            if (!parser_at_end(p)) p->pos++;\n'
        '            continue;\n'
        '        }\n\n'
        '        const Token *method_name = parser_current(p);'
    )

    if old not in src:
        print("ERROR: Could not find traits decl pattern")
        return False

    src = src.replace(old, new)
    write_file(path, src)
    print("OK: src/parser.c — traits decl no longer infinite loops on bad tokens")
    return True

def main():
    ok = True
    ok &= fix_extend_infinite_loop()
    ok &= fix_traits_infinite_loop()

    if ok:
        print("\nAll fixes applied. Rebuild with: make clean && make CC=gcc")
        return 0
    else:
        print("\nSome fixes failed.")
        return 1

if __name__ == '__main__':
    sys.exit(main())
