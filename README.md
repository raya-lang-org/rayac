# Raya

Raya is a systems programming language designed around explicit control,
predictable memory behavior, and a compiler architecture that makes storage
operations visible rather than hidden.

> **Current status: Phase 0 — Lexer Foundation**

The compiler is currently being built incrementally from the bottom up.
Phase 0 establishes the executable compiler, source handling, diagnostics,
lexer, and regression-test infrastructure before moving to parsing and
semantic analysis.

---

## Project Status

| Component | Status |
|---|---|
| C compiler bootstrap | ✅ |
| Windows / MinGW build | ✅ |
| Project structure | ✅ |
| Arena allocator | ✅ |
| StringView | ✅ |
| Source locations | ✅ |
| Diagnostic engine | ✅ |
| Lexer | ✅ |
| Keywords | ✅ |
| Identifiers | ✅ |
| Integer literals | ✅ |
| Floating-point literals | ✅ |
| String literals | ✅ |
| Character literals | ✅ |
| Operators | ✅ |
| Punctuation | ✅ |
| Comments | ✅ |
| Lexer regression tests | ✅ |
| Parser | ⏳ |
| AST | ⏳ |
| Type system | ⏳ |
| SAIR | ⏳ |
| SSA | ⏳ |
| SAE | ⏳ |
| C backend | ⏳ |
| Self-hosting | ⏳ |

---

# 1. Design Direction

Raya is intended to be a systems language with:

- explicit memory operations
- deterministic cleanup
- explicit ownership
- explicit borrowing/views
- no garbage collector
- no hidden ownership model
- storage-aware compiler analysis
- predictable generated code

The compiler architecture is intended to eventually follow:

```text
Raya source
    │
    ▼
 Lexer
    │
    ▼
 Parser
    │
    ▼
 Typed AST
    │
    ▼
 SAIR
    │
    ├──────────────► SSA
    │
    ▼
 SAE
    │
    ▼
 C Backend
    │
    ▼
 C Compiler
    │
    ▼
 Executable
