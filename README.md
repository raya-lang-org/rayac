Raya Compiler (rayac) — Phase 0 & 2 Complete

    Language version: Raya Path A v0.7.6
    Repo: https://github.com/raya-lang-org/rayac
    Lead: You | Engine: Kimi (Moonshot AI)

Quick Start
bash

# Build
make

# Run lexer tests (fast)
make test-lexer

# Run parser tests
make test-parser

# Run everything
make test

# Debug build with ASan
make debug

Architecture
plain

rayac/
├── src/
│   ├── main.c          # CLI entry, --dump-tokens, --dump-ast, --test-lexer, --test-parser
│   ├── lexer.h/c       # Phase 0: Tokenizer
│   ├── parser.h/c      # Phase 2: Recursive descent + Pratt parser
│   ├── ast.h/c         # Phase 2: AST nodes, types, patterns, S-expr printer
│   ├── arena.h/c       # Arena allocator (linked chunks)
│   ├── diag.h/c        # Diagnostic engine with source context
│   ├── string_view.h   # StringView + SV_FMT/SV_ARG macros
│   ├── source_loc.h    # SourceLocation (file, line, col, offset)
│   └── common.h        # RAYA_VERSION, std includes, extern flags
├── tests/
│   ├── lexer/*.raya + .expected   # Token-kind regression tests
│   └── parser/*.raya + .expected  # AST-kind regression tests
├── bin/raya            # Compiler binary
└── Makefile

Phase 0: Lexer ✅
Status: Complete, all tests passing.
Features

    Keywords: 30 keywords (module, fn, pub, const, var, comptime, defer, errdefer, test, return, if, else, while, for, try, break, continue, match, struct, union, enum, traits, extend, type, unsafe, noreturn, as, with, in, undefined, self, Self)
    Primitives: Recognized as identifiers (i8–i128, u8–u128, isize, usize, f32, f64, bool, void, noreturn)
    Literals: Int (dec/hex/octal/binary), float (with exponent), string "...", char '...'
    Operators: Full set including compound assignments (+=, -=, *=, /=, %=, &=, |=, ^=, <<=, >>=)
    Comments: // line and /* nestable block */ (max depth 16)
    Diagnostics: Source-location tracking, error messages with line context
    Special: TOK_UNDERSCORE for _ wildcard, TOK_EOF, TOK_ERROR

Test Mode
bash

./bin/raya --test-lexer file.raya

Outputs one token kind per line (fast, no headers).
Phase 2: Parser & AST ✅
Status: Complete, compiles clean with -Wall -Wextra -Werror, parser tests passing.
Parser Features
Top-level declarations:

    module name;
    import a.b.c as alias;
    pub? comptime? unsafe? fn name(T: type with Trait)?(params) -> Type? { block | ; }
    pub? struct Name(T: type)? { pub? field: Type = def?, }
    pub? union Name(T: type)? { ... }
    pub? enum Name { Variant(Type)? = disc?, }
    pub? traits Name { pub? fn name(params) -> Type?; }
    extend Name with Trait? { fn ... }
    pub? type Name = Type;
    pub? const/var name: Type = expr;
    test "name" { block }
    Attributes: #[name(args)] on all decls

Statements:

    const / var declarations (with optional : Type)
    return expr?;
    if expr block (else block | else if)?
    while expr block
    for ident: Type in expr block
    defer expr;
    errdefer { block }
    break; / continue;
    match expr { pattern => expr, }
    Assignment ops: =, +=, -=, *=, /=, %=, &=, |=, ^=, <<=, >>=
    Block with trailing expression (C.15: last expr without ; = block value)

Expressions (Pratt parser, precedence-climbing):

    Prefix: &, &const, *, -, !, ~
    Binary: ||, &&, ==, !=, <, >, <=, >=, |, ^, &, <<, >>, +, -, *, /, %
    Postfix: .field, .method(args), (args), [idx], [start..end]
    as Type cast
    try expr and try expr else |err| { block }
    unsafe { block }
    Array literals: []T{...} / [N]T{...} (with sentinel comma support)
    Struct literals: Type{ field: value, }

Types:

    Named types + generic instantiation: Vec3(f32)
    &const? T, *const? T, []const? T, [N]T
    ?T, !T
    Function type: fn(params) -> T

Self parameter (C.2):

    self → self: Self
    &self → self: &Self
    &const self → self: &const Self
    self: Type → explicit

Patterns:

    _ wildcard
    Literals, identifiers
    .Variant, .Variant(pat)
    .{ field: pat }

AST Structure

    AstNode — tagged union, 40+ node kinds
    TypeExpr — 8 type kinds (named, ref, ptr, slice, array, optional, error_union, function)
    Pattern — 5 pattern kinds
    Attribute + AttributeList for #[...]
    Arena-allocated with growable lists (AstNodeList, TypeExprList, etc.)

Test Mode
bash

./bin/raya --test-parser file.raya

Outputs indented AST node kind tree (fast, no S-expr bloat).
Debug Mode
bash

./bin/raya --dump-ast file.raya

Pretty-prints full S-expression AST with types and values.
Build System
Table
Target	Action
make	Release build (-O2)
make debug	Debug build (-g -fsanitize=address)
make test-lexer	Run all tests/lexer/*.raya vs .expected
make test-parser	Run all tests/parser/*.raya vs .expected
make test	Run both lexer and parser tests
make clean	Wipe obj/ and bin/
Tests use cmp -s (binary compare) — much faster than the old awk + diff pipeline.
What's Next: Phase 3 (Semantic Analysis / Type Checker)
Planned work:

    Symbol table — scopes, declarations, lookups
    Name resolution — resolve identifiers to decls, check imports
    Type inference — infer var and missing return types
    Type checking — validate expressions, assignments, function calls
    Self resolution — resolve Self to enclosing struct/union/enum/extend
    Trait checking — verify extend implements all trait methods
    Generic monomorphization — instantiate Vec3(f32) into concrete types
    Error propagation — validate try / !T usage
    Comptime evaluation — constant-fold comptime expressions
    Builder API — generate DeclDescriptor / Expr / Stmt structs for comptime VM

Language Spec Reference
See raya_v0.7.6_cheatsheet.txt for a 1-page quick reference covering:

    Keywords, types, literals
    Declaration syntax
    Expression precedence
    Self parameter rules
    Pattern grammar
    Coercion rules
    Fat pointer ABI
    Error tag layout

Notes for Future Sessions

    Lexer is frozen. Do not modify token kinds without updating parser tests.
    Parser handles the full v0.7.6 grammar. If adding new syntax, update parser_parse_stmt / parser_parse_top_level_decl.
    AST uses arena allocation. All nodes/lists live in the arena passed to parser_init. Free with arena_free_all().
    Generic params use a 3-token lookahead (( ident : type) to distinguish from regular params. Fragile — document if grammar changes.
    Struct literal ambiguity: if foo { ... } could be parsed as struct literal + block. Currently resolved by greedy struct literal parsing. If this breaks, add context-sensitive check.
