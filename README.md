Raya Compiler (rayac) — Phase 0, 2 & 3 Complete
plain

Language version: Raya Path A v0.7.6
Repo: https://github.com/raya-lang-org/rayac
Lead: You | Engine: Kimi (Moonshot AI)

Table
Feature	Status
Symbol tables (module + function + block scopes)	✅
Primitive type canonicalization (i32, f64, etc.)	✅
Composite types from AST (*T, &T, []T, ?T, !T, fn)	✅
Undeclared identifier errors	✅
Binary expression type checking (+, -, ==, etc.)	✅
Prefix operators (&, *, -, !, ~)	✅
Return type checking	✅
Variable declaration + inference	✅
if/while/for/defer/break/continue	✅
as casts, try, unsafe blocks	✅
Generic params (stub)	✅
Self parameter resolution	⏳ (needs struct context)
Struct/union/enum body checking	⏳
Trait checking / extend validation	⏳
Generic monomorphization	⏳
Comptime evaluation	⏳
Method call / field access type checking	⏳
Quick Start
bash

# Build
make

# Run lexer tests (fast)
make test-lexer

# Run parser tests
make test-parser

# Run semantic analysis tests
make test-sema

# Run everything
make test

# Debug build with ASan
make debug

Architecture
plain

rayac/
├── src/
│   ├── main.c          # CLI entry, --dump-tokens, --dump-ast, --check, --test-lexer, --test-parser, --test-sema
│   ├── lexer.h/c       # Phase 0: Tokenizer
│   ├── parser.h/c      # Phase 2: Recursive descent + Pratt parser
│   ├── ast.h/c         # Phase 2: AST nodes, types, patterns, S-expr printer
│   ├── type.h/c        # Phase 3: Canonical type graph, unification, coercion
│   ├── symbol.h/c      # Phase 3: Symbol kinds, scope chains, lookup tables
│   ├── sema.h/c        # Phase 3: Semantic analyzer — two-pass resolution
│   ├── arena.h/c       # Arena allocator (linked chunks)
│   ├── diag.h/c        # Diagnostic engine with source context
│   ├── string_view.h   # StringView + SV_FMT/SV_ARG macros
│   ├── source_loc.h    # SourceLocation (file, line, col, offset)
│   └── common.h        # RAYA_VERSION, std includes, extern flags
├── tests/
│   ├── lexer/*.raya + .expected    # Token-kind regression tests
│   ├── parser/*.raya + .expected   # AST-kind regression tests
│   └── sema/*.raya + .expected     # Semantic error regression tests
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
Phase 3: Semantic Analysis / Type Checker ✅
Status: Complete, compiles clean, sema tests passing.
New Files

    type.h/c — Canonical type graph with hash-consing (deduplicated SType* per concrete shape)
    symbol.h/c — Symbol kinds, scope chains, hash-table lookups
    sema.h/c — Two-pass semantic analyzer

Design

    Arena-everything — Types and symbols live in the same arena as AST nodes. No free() during checking.
    Canonical types — Every concrete type is hashed and deduplicated in TypeTable. *i32 in function A and *i32 in function B point to the same SType*.
    Two-pass resolution:
        Pass 1 (Collect): Walk top-level decls, register symbols in module scope.
        Pass 2 (Resolve): Walk bodies, resolve identifiers, infer types, check constraints.
    Error recovery — On unknown identifier, bind to error state and keep checking. Don't cascade.

Implemented Features

    Symbol tables: module scope, function scope, block scope, for-loop scope
    Primitive type canonicalization: void, bool, i8–i128, u8–u128, isize, usize, f32, f64, noreturn
    Composite type resolution from AST: *T, &T, []T, [N]T, ?T, !T, fn(...) -> T
    Undeclared identifier detection
    Binary expression type checking: arithmetic, comparison, logical, bitwise
    Prefix operator type checking: &, *, -, !, ~
    Return type compatibility checking
    Variable declaration with explicit type or inference
    Assignment type compatibility
    if/while condition boolean checks
    for loop variable binding
    as cast, try, unsafe block validation
    Generic parameter stubs (collected, not yet monomorphized)
    Self parameter binding in function scopes

Test Mode
bash

./bin/raya --test-sema file.raya

Outputs ok if no errors, or one error message per line (for cmp -s regression testing).
Check Mode
bash

./bin/raya --check file.raya

Runs full semantic analysis and prints diagnostics via the existing DiagnosticEngine.
Build System
Table
Target	Action
make	Release build (-O2)
make debug	Debug build (-g -fsanitize=address)
make test-lexer	Run all tests/lexer/*.raya vs .expected
make test-parser	Run all tests/parser/*.raya vs .expected
make test-sema	Run all tests/sema/*.raya vs .expected
make test	Run lexer + parser + sema tests
make clean	Wipe obj/ and bin/
Tests use cmp -s (binary compare) — much faster than the old awk + diff pipeline.
What's Next: Phase 4 (Advanced Semantic Analysis)
Planned work:

    Struct literal type checking — resolve Type{ field: value } against struct definition
    Field access resolution — obj.field lookup in struct field tables
    Method call resolution — obj.method(args) dispatch
    Self resolution — resolve Self to enclosing struct/union/enum/extend type
    Trait checking — verify extend implements all required trait methods
    Generic monomorphization — instantiate Vec3(f32) into concrete types
    Error propagation — validate try / !T usage across call chains
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
    Sema uses two passes. Pass 1 collects top-level symbols; Pass 2 resolves bodies. If adding new top-level decls, update both sema_collect_decls and sema_resolve_bodies.
    Type table is hash-consed. All st_* constructors return canonical pointers. Use st_eq(a, b) (pointer compare) rather than deep comparison.
