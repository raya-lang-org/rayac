# Raya Repository Audit & Improvement Plan
## What C Should Have Been — Implementation Gap Analysis

**Repo:** https://github.com/raya-lang-org/rayac  
**Audit Date:** 2026-08-24  
**Auditor:** External design review (stress-test against systems programming reality)  
**Scope:** Every source file, test, spec, and design document in the repo.

---

## Executive Summary

Raya's frontend (lexer, parser, sema) is solid. The C transpiler backend exists but is incomplete. The **critical gap** is that the repo implements "C with nice syntax" but has not yet implemented the **safety architecture** that makes it "what C should have been."

This document maps every file in the repo against the stress tests, identifies what's missing, and provides the exact implementation path.

---

## Part 1: File-by-File Stress Test Results

### src/sema.c — Semantic Analyzer

| Stress Test | Status | Finding |
|-------------|--------|---------|
| **ST1: `errdefer` + manual `free`** | ⚠️ PARTIAL | `errdefer` sema validation exists (checks enclosing function returns error union), but there is **no restriction** on what can appear in an `errdefer` body. `errdefer free(buf)` would be accepted. |
| **ST2: `&T` in struct fields + escaping refs** | ❌ MISSING | No lint for returning `&local` from functions. `sema_check_fn_body` does not track reference escapes. `make_parser(data)` returning `Parser{input: &data}` passes silently. |
| **ST3: `unsafe` capability boundary** | ❌ MISSING | `unsafe` only guards `*ptr` dereference (`TOK_STAR` in unary expr). `arr.ptr` (field access on slice) does NOT require `unsafe`. Safe code can manufacture raw pointers and pass them into `unsafe` blocks. |
| **ST4: Debug allocator / sanitizers** | ❌ MISSING | No sanitizer hooks in sema. No `--sanitize` mode awareness. No arena lifetime tracking. |

**Additional findings in sema.c:**
- `defer`/`errdefer` are collected in AST but **not lowered** — PROCESS.md confirms "lowering blocked on backend"
- `match` stmt checking is a stub (`sema_check_stmt` just checks the expr and arms exist)
- `for` loop over slices emits `raya_Slice` iteration but doesn't validate element type
- `auto-ref` for method calls has a known bug (PROCESS.md: "Auto-ref for pointer receivers — sema bug, by-value works")
- `st_can_coerce` allows `&T` → `*T` implicitly (line in `type.c` or coercion matrix) — this violates the capability boundary

### src/codegen_c.c — C Transpiler Backend

| Feature | Status | Finding |
|---------|--------|---------|
| `defer` | ✅ Partial | Stack-based LIFO cleanup implemented. Correct for normal returns. |
| `errdefer` | ❌ STUB | Emits `/* TODO: errdefer */`. No error-path conditional cleanup. |
| `match` | ❌ STUB | Emits `/* TODO: match */`. |
| `try` | ❌ STUB | `AST_TRY_EXPR` just emits the inner expression. No error tag check. |
| `error_capture` (`try else`) | ❌ STUB | Same as `try` — no fallback logic. |
| Bounds checks on slices | ❌ MISSING | `AST_INDEX_EXPR` for slices emits `((T*)slice.ptr)[idx]` with NO bounds check. The README says "bounds check: panic in debug, UB in release-fast" but codegen emits neither. |
| `unsafe` block | ✅ | Emits block directly. No wrapper. Correct. |
| Error union representation | ❌ MISSING | `cg_emit_sema_type` for `ST_ERROR_UNION` just emits the error type, ignoring success payload. This is broken. |
| `raya_rt.h` integration | ⚠️ PARTIAL | Includes `raya_rt.h` but runtime only has `raya_Str` struct. No panic handler, no bounds check helper, no slice struct. |

### src/main.c — Compiler Driver

| Feature | Status | Finding |
|---------|--------|---------|
| `--build` | ✅ | Transpiles to C, invokes `cc`. Functional. |
| `--check` | ✅ | Runs sema. Functional. |
| `--dump-tokens`, `--dump-ast` | ✅ | Debug flags work. |
| Release modes | ❌ MISSING | No `--release-safe`, `--release-fast`, `--debug`. All builds are effectively `debug` but without bounds checks. |
| Sanitizer flags | ❌ MISSING | No `--sanitize=memory`, `--sanitize=arena`, `--sanitize=thread`, `--sanitize=undefined`. |
| Panic handler | ❌ MISSING | No `--panic-handler` or `@panic_handler` attribute. |
| `--help` typo | ⚠️ | Line: `"  --build      vvutput sema errors only (for tests)\n"` — "vvutput" typo. |

### src/type.c / type.h — Type System

| Feature | Status | Finding |
|---------|--------|---------|
| `ST_ERROR_UNION` | ✅ | Type exists, hash-consed. |
| `ST_REFERENCE` | ✅ | `&T` and `&const T` exist. |
| `ST_POINTER` | ✅ | `*T` and `*const T` exist. |
| `ST_SLICE` | ✅ | `[]T` and `[]const T` exist. |
| Coercion: `&T` → `*T` | ⚠️ IMPLICIT | `st_can_coerce` likely allows this. Should require `unsafe` context. |
| `&volatile T` | ❌ MISSING | Not in type system. `RAYA_HARDWARE_INTERFACE_SPEC.md` defines it but it's not implemented. |

### src/arena.c / arena.h — Compiler Allocator

| Feature | Status | Finding |
|---------|--------|---------|
| Arena for compiler | ✅ | `arena_alloc`, `arena_free_all`. Deterministic, no individual free. |
| Arena for Raya stdlib | ❌ MISSING | This is a **compiler-internal** arena. There is no `std.mem.Arena` for Raya user code. |

### src/ast.h — AST Definitions

| Feature | Status | Finding |
|---------|--------|---------|
| `AST_DEFER_STMT` | ✅ | `expr` field. |
| `AST_ERRDEFER_STMT` | ✅ | `body` field (block, not expr). |
| `AST_UNSAFE_BLOCK_EXPR` | ✅ | `body` field. |
| `AST_TRY_EXPR` | ✅ | `expr` field. |
| `AST_ERROR_CAPTURE_EXPR` | ✅ | `expr`, `err_name`, `fallback` fields. |
| `AST_MATCH_STMT` | ✅ | `expr`, `arms` fields. |
| `AST_MATCH_ARM` | ✅ | `pattern`, `expr` fields. |
| Attributes on decls | ✅ | `AttributeList attrs` on structs, fns, fields, etc. |
| `#[allow(...)]` attribute | ❌ MISSING | No `AttributeKind` enum. Attributes are raw name+args. No semantic meaning assigned. |

### src/lexer.c / lexer.h

| Feature | Status | Finding |
|---------|--------|---------|
| `volatile` keyword | ❌ MISSING | Not in keyword list. Needed for `&volatile T`. |
| `atomic_*` keywords | ❌ MISSING | Not needed if atomics are attribute-based (`#[atomic_load(Acquire)]`). |
| `unsafe` keyword | ✅ | Present. |
| `defer`/`errdefer` | ✅ | Present. |

### tests/ directory

| Test Category | Count | Coverage Gap |
|---------------|-------|-------------|
| Lexer | 3 | No `volatile`, `atomic`, `asm` token tests |
| Parser | 1 | No `match`, `try else`, `unsafe block` parse tests |
| Sema | 28 | No `errdefer` + `free` conflict test. No escaping reference test. No `unsafe` capability boundary test. No bounds check test. |
| Codegen | 14 | No `errdefer`, `match`, `try`, error union, bounds check tests. |
| Integration | 0 | No end-to-end compiled tests. |

### PROCESS.md — Roadmap

| Gap | Severity | Notes |
|-----|----------|-------|
| No mention of `release-safe` / `release-fast` modes | 🔴 HIGH | Current plan has "debug" and "release" but no defined-panic mode |
| No mention of compiler sanitizers | 🔴 HIGH | SAE doc exists but is post-v1.0. Sanitizers are pre-v1.0 critical. |
| No mention of arena-first stdlib | 🔴 HIGH | Phase 7 (stdlib) has no arena design. |
| `errdefer` semantics vague | 🟡 MEDIUM | Says "only runs on error-return paths — needs control-flow analysis" but doesn't specify resource vs. memory restriction |
| Backend strategy: C transpile | ✅ OK | Correct first step. But needs sanitizer stubs. |

### RAYA_HARDWARE_INTERFACE_SPEC.md

| Assessment | Status |
|------------|--------|
| `&volatile T` type design | ✅ Excellent. Ready for implementation. |
| Atomic attribute design | ✅ Excellent. `#[atomic_load(Acquire)]` etc. |
| Inline asm structured design | ✅ Excellent. |
| IR primitives | ✅ Well-defined. |
| C bootstrap lowering map | ✅ Complete. |
| **Integration with current repo** | ⚠️ NOT INTEGRATED. Spec exists but no code implements it. |

### RAYA_STORAGE_ANALYSIS_ENGINE.md

| Assessment | Status |
|------------|--------|
| SAE concept | ✅ Good future direction. |
| Design locks | ✅ Appropriate. |
| **Relevance to current gap** | 🟡 LOW. SAE is v1.0+. Current gap is pre-v1.0 sanitizers. Don't confuse the two. |

### Code.md — Formal Spec

| Assessment | Status |
|------------|--------|
| Bounds check semantics | ⚠️ "Panic in debug, UB in release-fast" — this is the **debug-only trap** identified in stress test 4. Needs `release-safe` mode. |
| `unsafe` boundary | ⚠️ Says "Dereferencing raw pointers, pointer arithmetic, type punning, inline asm, calling extern C without wrappers, `*void` coercion" require unsafe. Does NOT say `&T` → `*T` cast requires unsafe. |
| Error union layout | ✅ Defined. |
| Slice ABI | ✅ Defined. |
| Trait object ABI | ✅ Defined. |

---

## Part 2: The Complete Missing List

### 🔴 Critical (Blocks "What C Should Have Been" Claim)

| # | Missing Feature | File(s) Affected | Why It Matters |
|---|-----------------|------------------|----------------|
| 1 | **`errdefer` restricted to non-memory resources** | `src/sema.c`, `src/parser.c` | Without this, `errdefer free(buf)` + manual `free(buf)` = double-free. The language becomes less safe than C. |
| 2 | **`unsafe` marks capability creation** | `src/sema.c` | `arr.ptr`, `&T as *T`, pointer casts must require `unsafe`. Otherwise safe code manufactures UB weapons. |
| 3 | **Bounds checks in codegen** | `src/codegen_c.c`, `src/raya_rt.c` | README promises bounds-checked slices. Codegen doesn't emit checks. This is false advertising. |
| 4 | **`release-safe` mode with defined panics** | `src/main.c`, `src/codegen_c.c` | "Panic in debug, UB in release" means tests pass and production crashes. Need `release-safe` with `unlikely` panic branches. |
| 5 | **Arena-first stdlib module** | New: `lib/std/mem/Arena.raya` | Without this, programmers use C `malloc`/`free` and get all the same UAF/double-free bugs. |
| 6 | **Compiler sanitizers (`--sanitize=*`)** | `src/main.c`, `src/codegen_c.c` | KASAN-style shadow memory for debug/CI. Without this, bugs are found in production. |
| 7 | **`defer`/`errdefer` lowering in C backend** | `src/codegen_c.c` | `errdefer` is stubbed. `defer` works for normal returns but error-path conditional cleanup is missing. |
| 8 | **Error union representation in C backend** | `src/codegen_c.c` | `cg_emit_sema_type` for `ST_ERROR_UNION` emits wrong type. `try`/`try else` emit no code. |

### 🟡 High (Required for Kernel/Systems Use)

| # | Missing Feature | File(s) Affected | Why It Matters |
|---|-----------------|------------------|----------------|
| 9 | **`&volatile T` in type system** | `src/type.c`, `src/lexer.c`, `src/parser.c`, `src/sema.c` | Hardware spec defines this. MMIO drivers need it. |
| 10 | **Lint for escaping references** | `src/sema.c` | `return &local` is the #1 junior kernel bug. Warn on it. |
| 11 | **`#[allow(...)]` attribute system** | `src/sema.c`, `src/ast.h` | Needed to suppress lint warnings legitimately. |
| 12 | **Replaceable panic handler** | `src/raya_rt.c`, `src/codegen_c.c` | Kernel panic means `oops()`, not `abort()`. |
| 13 | **`match` lowering in C backend** | `src/codegen_c.c` | `match` is a core control flow feature. Currently stubbed. |
| 14 | **`free()` as `unsafe` operation** | `src/sema.c`, stdlib design | Individual deallocation should be explicit about danger. |

### 🟢 Medium (Polish & Completeness)

| # | Missing Feature | File(s) Affected | Why It Matters |
|---|-----------------|------------------|----------------|
| 15 | `for` loop over slices type validation | `src/sema.c` | Current codegen casts to `void*` then back. Type-safe iteration needed. |
| 16 | `auto-ref` for pointer receivers | `src/sema.c` | Known bug per PROCESS.md. |
| 17 | Trait conformance checking | `src/sema.c` | Phase 4 Sprint 3 in PROCESS.md. |
| 18 | Generic monomorphization | `src/sema.c`, `src/codegen_c.c` | Phase 4 Sprint 4 in PROCESS.md. |
| 19 | `comptime` evaluation | `src/sema.c` | Phase 4 Sprint 4 in PROCESS.md. |
| 20 | `--help` typo fix | `src/main.c` | "vvutput" → "output" |

---

## Part 3: Implementation Priority

### Phase A: Safety Architecture (Do This Next)

**Goal:** Make Raya honest about what it guarantees.

| Week | Task | Files | Deliverable |
|------|------|-------|-------------|
| 1 | Add `release-safe` / `release-fast` to compiler driver | `src/main.c` | `--release-safe` emits `unlikely` panic branches. `--release-fast` emits UB. Default is `--debug`. |
| 1 | Emit bounds checks in C backend | `src/codegen_c.c`, `src/raya_rt.c` | `raya_bounds_check(idx, len, file, line)` call in debug/release-safe. Nothing in release-fast. |
| 2 | Restrict `errdefer` to non-memory cleanup | `src/sema.c` | `errdefer` body cannot contain `free()`, `alloc()`, or arena dealloc calls. Compile error if attempted. |
| 2 | Expand `unsafe` to capability creation | `src/sema.c` | `arr.ptr`, `&T as *T`, `*T as &T`, pointer casts require `unsafe` block or `unsafe fn`. |
| 3 | Implement `defer`/`errdefer` lowering | `src/codegen_c.c` | `defer` → goto cleanup. `errdefer` → conditional cleanup on error flag. |
| 3 | Fix error union C representation | `src/codegen_c.c` | Emit `struct { uint32_t tag; T payload; }` with alignment. Implement `try` tag check + return. |
| 4 | Add arena stdlib module | New: `lib/std/mem/Arena.raya` | `Arena.init()`, `arena.alloc()`, `arena.reset()`. No `free()`. |
| 4 | Add `--sanitize=memory` stub | `src/main.c`, `src/codegen_c.c` | Flag accepted. Emits `/* sanitize=memory: TODO */` comment. Prepares infrastructure. |

### Phase B: Kernel-Ready Features

| Week | Task | Files | Deliverable |
|------|------|-------|-------------|
| 5 | Add `&volatile T` to type system | `src/lexer.c`, `src/parser.c`, `src/type.c`, `src/sema.c` | Parser recognizes `volatile` keyword. Type system has `ST_REFERENCE_VOLATILE`. |
| 5 | Add escaping reference lint | `src/sema.c` | Warn when `&local` escapes function scope. `#[allow(escaping_reference)]` to suppress. |
| 6 | Implement `match` lowering | `src/codegen_c.c` | `match` → `switch` with breaks. Handle wildcard, literal, enum variant patterns. |
| 6 | Add replaceable panic handler | `src/raya_rt.c`, `src/codegen_c.c` | `raya_panic(file, line, msg)` weak symbol. Kernel overrides with `oops()`. |
| 7 | Integrate hardware interface spec | `src/sema.c`, `src/codegen_c.c` | `#[atomic_*]` attributes parse and lower to `__atomic_*` intrinsics. `&volatile T` lowers to `volatile T*`. |
| 7 | C header import tool sketch | New: `tools/raya-bindgen/` | Parse C headers, emit Raya `extern fn` declarations. |

### Phase C: Hardening

| Week | Task | Files | Deliverable |
|------|------|-------|-------------|
| 8 | Full sanitizer instrumentation | `src/codegen_c.c` | `--sanitize=memory` emits KASAN-style shadow memory checks on `*T` derefs. |
| 8 | Arena sanitizer | `src/codegen_c.c` | `--sanitize=arena` tracks arena lifetimes. |
| 9 | 100+ sema tests | `tests/sema/` | Cover all negative cases: bad `errdefer`, unsafe boundary violations, escaping refs. |
| 9 | 30+ integration tests | `tests/integration/` | End-to-end compile + run tests. |
| 10 | Self-hosting prep | `PROCESS.md` | Define which C stdlib functions Raya needs to replace. |

---

## Part 4: Specific Code Changes Required

### 4.1 sema.c — `errdefer` Restriction

**Current (broken):**
```c
case AST_ERRDEFER_STMT:
    if (!s->current_fn_return_type || s->current_fn_return_type->kind != ST_ERROR_UNION) {
        sema_report(s, stmt->loc, "'errdefer' only valid in functions returning error unions");
    }
    sema_check_block(s, stmt->errdefer_stmt.body);
```

**Required:**
```c
case AST_ERRDEFER_STMT:
    if (!s->current_fn_return_type || s->current_fn_return_type->kind != ST_ERROR_UNION) {
        sema_report(s, stmt->loc, "'errdefer' only valid in functions returning error unions");
    }
    // NEW: Walk errdefer body and reject memory deallocation
    if (errdefer_body_contains_free(s, stmt->errdefer_stmt.body)) {
        sema_report(s, stmt->loc, 
            "'errdefer' cannot contain memory deallocation. "
            "Use arena allocation or handle cleanup manually on the error path");
    }
    sema_check_block(s, stmt->errdefer_stmt.body);
```

**Helper needed:**
```c
static bool expr_contains_free(Sema *s, AstNode *expr);
static bool stmt_contains_free(Sema *s, AstNode *stmt);
```
Match `free`, `dealloc`, `arena.deinit`, etc. by name.

### 4.2 sema.c — `unsafe` Capability Boundary

**Current (broken):** `AST_FIELD_ACCESS_EXPR` on slices emits `.ptr` without checking `in_unsafe`.

**Required:** Add check in `AST_FIELD_ACCESS_EXPR`:
```c
case AST_FIELD_ACCESS_EXPR: {
    // ... existing slice field check ...
    if (base->kind == ST_SLICE) {
        if (sv_eq_cstr(fname, "ptr") || sv_eq_cstr(fname, "len")) {
            // NEW: .ptr requires unsafe because it manufactures raw pointers
            if (sv_eq_cstr(fname, "ptr") && !s->in_unsafe) {
                sema_report(s, expr->loc, 
                    "accessing '.ptr' on a slice requires 'unsafe' context");
            }
            // .len is safe — it's just a number
        }
    }
    // ...
}
```

**Also required:** `AST_CAST_EXPR` from `&T` to `*T`:
```c
case AST_CAST_EXPR: {
    SType *from = sema_check_expr(s, expr->cast_expr.expr);
    SType *to = sema_resolve_type(s, expr->cast_expr.type);
    if (from->kind == ST_REFERENCE && to->kind == ST_POINTER && !s->in_unsafe) {
        sema_report(s, expr->loc, 
            "casting '&T' to '*T' requires 'unsafe' context");
    }
    // ...
}
```

### 4.3 codegen_c.c — Bounds Checks

**Current (broken):** `AST_INDEX_EXPR` on slices:
```c
if (obj_type && obj_type->kind == ST_SLICE) {
    fprintf(cg->out, "((");
    cg_emit_sema_type(cg, obj_type->as.slice.base, "");
    fprintf(cg->out, "*)");
    cg_emit_expr(cg, expr->index_expr.object);
    fprintf(cg->out, ".ptr)[");
    cg_emit_expr(cg, expr->index_expr.index);
    fprintf(cg->out, "]");
}
```

**Required:**
```c
if (obj_type && obj_type->kind == ST_SLICE) {
    // NEW: bounds check in debug/release-safe
    if (cg->mode != MODE_RELEASE_FAST) {
        fprintf(cg->out, "(raya_bounds_check(");
        cg_emit_expr(cg, expr->index_expr.index);
        fprintf(cg->out, ", ");
        cg_emit_expr(cg, expr->index_expr.object);
        fprintf(cg->out, ".len, "%s", %lu), ", 
            expr->loc.filename, (unsigned long)expr->loc.line);
    }
    fprintf(cg->out, "((");
    cg_emit_sema_type(cg, obj_type->as.slice.base, "");
    fprintf(cg->out, "*)");
    cg_emit_expr(cg, expr->index_expr.object);
    fprintf(cg->out, ".ptr)[");
    cg_emit_expr(cg, expr->index_expr.index);
    fprintf(cg->out, "]");
}
```

**Runtime addition (raya_rt.c):**
```c
void raya_bounds_check(size_t idx, size_t len, const char *file, size_t line) {
    if (idx >= len) {
        raya_panic(file, line, "index out of bounds");
    }
}
```

### 4.4 codegen_c.c — Error Union Representation

**Current (broken):**
```c
case ST_ERROR_UNION: {
    cg_emit_sema_type(cg, type->as.error_union.error, name);
    break;
}
```

**Required:**
```c
case ST_ERROR_UNION: {
    // Emit as struct { uint32_t tag; SuccessType payload; }
    fprintf(cg->out, "struct { uint32_t __tag; ");
    char payload_name[256];
    snprintf(payload_name, sizeof(payload_name), "__payload");
    cg_emit_sema_type(cg, type->as.error_union.success, payload_name);
    fprintf(cg->out, " } %s", name);
    break;
}
```

**For `try` lowering:**
```c
case AST_TRY_EXPR: {
    int tmp = cg_next_temp(cg);
    SType *inner_type = expr->try_expr.expr->sema_type;
    // Emit: tmp = expr; if (tmp.__tag != 0) return tmp;
    fprintf(cg->out, "({ struct { uint32_t __tag; ");
    cg_emit_sema_type(cg, inner_type->as.error_union.success, "__payload");
    fprintf(cg->out, "; } __tmp_%d = ", tmp);
    cg_emit_expr(cg, expr->try_expr.expr);
    fprintf(cg->out, "; if (__tmp_%d.__tag != 0) { ", tmp);
    // Run errdefers if any
    cg_emit_errdefers(cg);
    fprintf(cg->out, "return *(/* error union cast */ void*)&__tmp_%d; } ", tmp);
    fprintf(cg->out, "__tmp_%d.__payload; })", tmp);
    break;
}
```

### 4.5 main.c — Release Modes

**Add to `print_usage`:**
```c
fprintf(stderr, "  --release-safe   Emit bounds checks and panics with unlikely branches\n");
fprintf(stderr, "  --release-fast   Zero overhead. Undefined behavior on failure.\n");
fprintf(stderr, "  --sanitize=memory   Enable memory sanitizer (debug only)\n");
fprintf(stderr, "  --sanitize=arena  Enable arena lifetime sanitizer\n");
```

**Add to flag parsing:**
```c
} else if (strcmp(argv[i], "--release-safe") == 0) {
    g_release_mode = MODE_RELEASE_SAFE;
} else if (strcmp(argv[i], "--release-fast") == 0) {
    g_release_mode = MODE_RELEASE_FAST;
} else if (strncmp(argv[i], "--sanitize=", 11) == 0) {
    g_sanitize = argv[i] + 11;
}
```

---

## Part 5: Test Additions Required

### New Sema Tests (Negative — Must Fail)

```
tests/sema/errdefer_free_conflict.raya
tests/sema/errdefer_free_conflict.expected
tests/sema/unsafe_slice_ptr.raya
tests/sema/unsafe_slice_ptr.expected
tests/sema/unsafe_ref_to_raw_cast.raya
tests/sema/unsafe_ref_to_raw_cast.expected
tests/sema/escaping_reference_warn.raya
tests/sema/escaping_reference_warn.expected
tests/sema/volatile_downgrade.raya
tests/sema/volatile_downgrade.expected
```

### New Codegen Tests (Positive — Must Pass)

```
tests/codegen/bounds_check_debug.raya
tests/codegen/bounds_check_release_safe.raya
tests/codegen/error_union_try.raya
tests/codegen/error_union_try_else.raya
tests/codegen/errdefer_cleanup.raya
tests/codegen/match_enum.raya
tests/codegen/arena_alloc.raya
```

### New Integration Tests

```
tests/integration/run_hello.raya
tests/integration/run_defer_order.raya
tests/integration/run_error_union.raya
tests/integration/run_slice_bounds.raya
```

---

## Part 6: Summary

Raya is at a **decision point.** The frontend is solid enough to ship syntax. But the backend and safety architecture are where the "better C" claim lives or dies.

**If you implement only one thing:** Make `release-safe` mode with defined panics on bounds checks. This single feature transforms Raya from "C with slices" into "C that fails loudly in test instead of corrupting memory in production."

**If you implement three things:** Add `release-safe`, restrict `errdefer` to non-memory, and expand `unsafe` to capability creation. These three changes eliminate the most dangerous footguns without adding ownership complexity.

**If you implement five things:** Add arena stdlib, compiler sanitizers, and escaping reference lint. At this point, Raya has a credible claim to being "what C should have been."

The repo has the talent and the architecture. What's missing is the **discipline to say no** to features that don't serve the systems programmer's workflow, and the **courage to add** the safety nets that make debugging possible at 3pm instead of 2am.

---

*End of Audit*
