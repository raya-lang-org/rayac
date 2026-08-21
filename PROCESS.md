Raya Compiler — Development Process

    Version: 0.7.6
    Last Updated: 2026-08-21
    Status: Phase 3 Complete → Phase 4 In Progress

Table of Contents

    Project Overview
    Completed Phases
    Current State
    Known Issues & Fixes
    Phase 4 Roadmap
    Phase 5+ Roadmap
    Architecture Decisions
    Testing Strategy
    Backend Strategy

1. Project Overview
Raya is a systems programming language positioned as a "better C" — explicit, zero-cost, and hardware-friendly, with modern type system conveniences (nullability, slices, traits, comptime) without the complexity of full memory safety enforcement.
The compiler (rayac) is written in C11 and follows a classic multi-phase design:
plain

Source (.raya) → Lexer → Parser → Semantic Analyzer → [Backend] → Object Code

Repository: https://github.com/raya-lang-org/rayac
2. Completed Phases
Phase 0: Bootstrap & Infrastructure

    Arena allocator — deterministic, region-based memory management. No individual free calls.
    StringView — zero-copy string slices throughout the compiler.
    SourceLocation — precise filename/line/column tracking for diagnostics.
    DiagnosticEngine — structured error/warning reporting with color support.

Phase 1: Lexer

    UTF-8 source encoding with BOM stripping.
    Line comments (//) and nestable block comments (/* */, max depth 16).
    30 keywords, primitive type recognition, contextual identifiers (self, Self).
    Integer literals (decimal, hex 0x, octal 0o, binary 0b).
    Float literals with exponent support.
    String and character literals with escape sequences (\n, \t, \", \', \0).
    Full operator and punctuation tokenization.
    Attribute syntax: #[name(args)].

Phase 2: Parser

    Pratt parser for expressions with correct precedence.
    Full AST construction for all language constructs:
        Module declarations, imports with aliases.
        Functions (with comptime, unsafe, pub, generics, default params, self).
        Structs, unions, enums (with payloads and discriminants).
        Traits and extend blocks (inherent methods + trait impls).
        Type aliases.
        Top-level const and var.
        Test declarations.
    Statements: if/else, while, for, match, defer, errdefer, return, break, continue.
    Expressions: literals, identifiers, binary/unary ops, field access, index, slice, call, method call, casts (as), try, error capture, unsafe blocks.
    Patterns for match: wildcard, literal, binding, enum variant, struct field.
    Trailing expression support in blocks.
    Attribute parsing on declarations and fields.

Phase 3: Semantic Analyzer (Two-Pass)

    Pass 1 — Symbol Collection:
        Scans all top-level declarations.
        Inserts symbols into module scope: functions, structs, unions, enums, traits, type aliases, constants, variables.
        Detects redefinitions.
        Builds function type signatures from AST parameters.
    Pass 2 — Body Resolution:
        Type-checks function bodies.
        Scope chain for locals, parameters, generic params.
        self receiver binding with Self type resolution context.
        Expression type inference and checking.
        Assignment compatibility via st_can_coerce().
        Struct literal field validation against declared fields.
        Field access resolution through pointer/reference indirection.
        Index and slice expression validation.
        Return type checking against declared return type.
    Type System:
        Hash-consed SType with pointer equality (st_eq).
        Primitive types: void, bool, all integer and float widths.
        Pointer taxonomy: &T, &const T, *T, *const T, ?T, !T.
        Slices: []T, []const T.
        Arrays: [N]T.
        Functions: param/return types, variadic flag.
        Named types (structs/unions/enums) with placeholder resolution.
        Coercion matrix: integer widening, float widening, int→float, &T→&const T, T→?T, array→slice.

3. Current State
Table
Component	Status	Notes
Lexer	✅ Complete	All tokens, comments, literals
Parser	✅ Complete	Full AST for all constructs
Sema — Symbol Collection	✅ Complete	Two-pass design working
Sema — Type Resolution	✅ Partial	Basic types, structs, functions
Sema — Method Dispatch	⏳ Pending	extend methods not registered
Sema — Trait Conformance	⏳ Pending	Not yet implemented
Sema — Generics	⏳ Pending	Parsed but not sema'd
Sema — unsafe Enforcement	⏳ Pending	Parsed but not enforced
Sema — Defer/Errdefer	⏳ Pending	Collected but not lowered
Sema — Comptime	⏳ Pending	Spec'd but not built
Backend	❌ Not Started	Decision pending
Standard Library	❌ Not Started	Post-backend
Test Coverage:

    Lexer: 3 tests
    Parser: 1 test
    Sema: 7 tests (struct literals, field access, undeclared identifiers, type mismatches)

4. Known Issues & Fixes
Critical (Fix This Sprint)

    Method calls are stubs — AST_METHOD_CALL_EXPR always returns void type. Need MethodTable and dispatch.
    Return type checking breaks on if/else with returns — last_is_return logic misses control-flow returns.
    extend blocks are invisible — sema_collect_decls skips AST_EXTEND_DECL. Methods never registered.
    Self type never resolved — Self in method signatures creates placeholder struct instead of binding to enclosing type.
    unsafe not enforced — Raw pointer dereference (*ptr) allowed everywhere. Spec requires unsafe context.
    Function calls don't check arguments — add(5, "hello") passes sema silently.
    Top-level const/var init never checked — sema_resolve_bodies only walks functions.
    Unknown types create silent placeholders — Vec3x (typo) becomes a new struct type instead of error.

Medium (Fix Next Sprint)

    Slice bounds checking not yet implemented (runtime panic/abort vs UB modes).
    errdefer semantics: only runs on error-return paths — needs control-flow analysis.
    Generic monomorphization not hooked into sema.
    No volatile, align(N), packed, extern("C"), noinline attribute enforcement.

5. Phase 4 Roadmap
Goal: Complete semantic analysis. Make the language actually mean something.
Sprint 1: Sema Hardening (Week 1)

    [ ] Fix return type checking for if/else with returns
    [ ] Fix Self type resolution in method contexts
    [ ] Add unsafe boundary enforcement (in_unsafe flag in Sema)
    [ ] Check function call argument count and types
    [ ] Fix unknown type names → error instead of placeholder
    [ ] Check top-level const/var initializer types
    [ ] Add 20+ sema tests (method calls, unsafe, generics, errors)

Sprint 2: Method Dispatch (Week 2)

    [ ] Build MethodTable keyed by SType* (pointer equality via hash-consing)
    [ ] Process AST_EXTEND_DECL in Pass 1: register methods against target type
    [ ] Resolve AST_METHOD_CALL_EXPR in Pass 2: lookup method, verify signature, prepend self
    [ ] Desugar method calls to regular CALL nodes (simplifies codegen)
    [ ] Handle self shorthand: &self → &const Self, self → Self

Sprint 3: Traits (Week 3)

    [ ] Trait object-safety checking (spec C.3)
    [ ] Trait conformance validation in extend Type with Trait
        Every trait method must be implemented
        Method signatures must match (param types, return type, receiver)
    [ ] Vtable layout design (frozen ABI for trait objects &Trait)
    [ ] Static vtable generation per Type + Trait combo

Sprint 4: Generics (Week 4–5)

    [ ] Type substitution map for generic arguments
    [ ] ast_clone() with type substitution
    [ ] Monomorphization cache: Vec3(f32) → Vec3_f32 sema'd once
    [ ] Generic constraint checking (T: type with Copyable)
    [ ] Hook into sema_check_expr for generic function calls and struct literals

Sprint 5: Defer & Errdefer (Week 5)

    [ ] Collect defers per block during sema
    [ ] Validate defer expressions don't reference dying locals
    [ ] Design defer lowering strategy (AST transform vs codegen insertion)
    [ ] errdefer control-flow analysis: only execute on error return paths

6. Phase 5+ Roadmap
Phase 5: Backend (8–12 weeks)
Decision needed: C transpile vs LLVM vs custom.
Recommended: C Transpile First

    Raya AST → C11 source → host cc → binary
    Proves ABI compatibility, fast to implement, easy to debug
    defer lowers to goto cleanup blocks
    Slices lower to struct { T* ptr; size_t len; }
    Method calls already desugared to regular functions

LLVM Backend (later)

    Direct IR emission for performance-critical builds
    Debug info via llvm::DIBuilder
    Cross-compilation support

Phase 6: Self-Hosting

    Rewrite rayac in Raya
    Requires: full language working, C FFI, file I/O, memory allocator in stdlib

Phase 7: Standard Library

    std.io, std.mem, std.fmt, std.os, std.math
    Growable strings, hash maps, vectors
    Platform abstractions (Windows/Linux/macOS)

Phase 8: Tooling

    Language server protocol (LSP)
    Package manager (raya fetch, raya build)
    Formatter (raya fmt)
    Documentation generator

7. Architecture Decisions
Table
Decision	Rationale
Arena allocator	Compiler lifetime is a single invocation. No fragmentation, no leaks, deterministic.
Hash-consed types	Pointer equality for type comparison is O(1). Critical for method table lookup and monomorphization cache.
Two-pass sema	Pass 1 collects all symbols so forward references work without forward declarations.
Pratt parser	Handles precedence naturally without massive recursive-descent boilerplate.
No hidden lifetimes	Programmer manages memory explicitly. Honest about what the compiler can and cannot prove.
Monomorphization for generics	Zero-cost abstraction. No vtable overhead for generic code. Binary bloat is acceptable tradeoff for systems code.
Trait objects as fat pointers	{ data: *void, vtable: *void }. Compatible with C ABI, no hidden allocations.
C transpile first	Fastest path to a working compiler. Validates language semantics before committing to LLVM.
8. Testing Strategy
Current Test Layout
plain

tests/
  lexer/
    *.raya      — source input
    *.expected  — expected token output
  parser/
    *.raya
    *.expected  — expected AST dump
  sema/
    *.raya
    *.expected  — expected error messages or "ok"

Target Coverage
Table
Layer	Current	Target	Priority
Lexer	3	15	P2
Parser	1	20	P2
Sema	7	100+	P1
Integration	0	30	P0 (post-backend)
Test Categories Needed

    Positive: Valid code that should pass sema cleanly.
    Negative: Invalid code that should produce specific error messages.
    Edge cases: Empty structs, zero-arg functions, empty blocks, nested comments max depth.
    Type coercion: Integer widening, float promotion, optional wrapping, reference constness.
    Method dispatch: Valid calls, missing methods, wrong receiver type, static vs instance.
    Traits: Conformance ok, missing method, signature mismatch, object-safety violations.
    Unsafe: Valid unsafe blocks, raw deref outside unsafe, pointer arithmetic rules.
    Generics: Instantiation ok, type mismatch, constraint failure, recursion limits.

9. Backend Strategy
Option A: C Transpile (Recommended for v0.8)
Mapping:
Table
Raya	C11
i32, u64, f32, etc.	int32_t, uint64_t, float
bool	_Bool
void	void
&T	T*
&const T	T const*
*T	T*
*const T	T const*
?&T	T* (NULL = null)
[]T	struct { T* ptr; size_t len; }
[]const T	struct { T const* ptr; size_t len; }
[N]T	T[N]
fn(...) -> T	T (*)(...)
struct { ... }	struct Name { ... }
union { ... }	union Name { ... }
enum { A, B }	enum Name { A, B }
defer expr;	goto to cleanup label
errdefer { ... }	Conditional goto on error flag
match	switch with breaks
try expr	Error tag check + return
unsafe { ... }	Direct C equivalent (no wrapper)
extend Type { fn m(...) }	Type_m(Type self, ...)
T as U	(U)T for numeric, *(U*)&T for pointer
Runtime Library (small):

    Slice bounds check function (debug/safe mode)
    Panic/abort handler
    Optional allocator interface

Option B: LLVM IR (v0.9+)

    Direct llvm::IRBuilder emission
    Native debug info
    Link-time optimization (LTO)
    Cross-compilation

Option C: Custom x86-64 (Future)

    Not recommended until self-hosting

Changelog
Table
Date	Version	Change
2026-08-21	0.7.6	Phase 3 complete. Sema two-pass design, struct literal + field access working. Process document created.
2026-08-21	0.7.5	Formal spec v0.7.5: EBNF grammar, typed builder API, comptime VM bytecode, fat-pointer ABI.
2026-08-20	0.7.0	Parser complete. Pratt expressions, all declarations, patterns, attributes.
2026-08-19	0.6.0	Lexer complete. All keywords, literals, operators, nested comments.
2026-08-18	0.5.0	Bootstrap: arena, stringview, diagnostics, source locations.
Contributors

    Lead: [Your Name]
    Language Design & Spec: Team consensus via GitHub discussions
    Compiler Engineering: C11 implementation, arena + hash-consed types

"Ship the frontend. Harden the sema. Then decide how fast you want it to go."
