# Raya: What C Should Have Been
## End-of-Day Status & Sprint Roadmap

**Date:** 2026-08-24  
**Repo:** https://github.com/raya-lang-org/rayac  
**Current Tag:** v0.8.x (frontend complete, backend functional, safety architecture pending)  
**Target Tag:** v0.9.0 (safety architecture implemented)  
**Goal:** Kernel maintainers use this. Replace C in real codebases.

---

## Current State: The Honest Picture

| Component | Tests | Status | Notes |
|-----------|-------|--------|-------|
| Lexer | 3/3 | ✅ PASS | 37 keywords. `TOK_IN` synced. |
| Parser | 1/1 | ✅ PASS | Full Pratt. All constructs. |
| Sema | 28/28 | ✅ PASS | Core types, methods, error unions. |
| Codegen | 14/14 | ✅ PASS | Basic C transpile works. `raya_rt.c` exists. |
| **Safety Architecture** | — | ❌ MISSING | The reason we started this. |

**Alignment Score: 41%**

The frontend is done. The backend transpiles. But the safety architecture — bounds checks, `unsafe` boundary, `const` protection, `errdefer` restriction — is barely started.

---

## What's Working (Don't Touch)

- ✅ Lexer, parser, sema core
- ✅ `defer` lowering (LIFO cleanup)
- ✅ Method dispatch with auto-ref
- ✅ Error union type system (`E!T`, `try`, `try else`)
- ✅ `unsafe` block recognition
- ✅ `*ptr` dereference requires `unsafe`
- ✅ C ABI compatibility
- ✅ `raya_rt.c` (panic, bounds_check, print)

---

## What's Broken (Fix These)

### 🔴 Critical (False Advertising)

| # | Bug | Why It Hurts |
|---|-----|-------------|
| 1 | **Bounds checks not emitted** | README says "bounds-checked slices." Codegen emits raw `((T*)ptr)[idx]`. No checks in ANY mode. |
| 2 | **`unsafe` is theater** | `arr.ptr`, `&T as *T`, `*T as &T` don't require `unsafe`. Safe code manufactures raw pointers. |
| 3 | **`const` is a sieve** | Only stops `x = 5`. Misses `v.x = 5`, `arr[0] = 5`, `*ptr = 5` through `&const T`. |
| 4 | **`errdefer` accepts `free()`** | Double-free vector. Worse than C because programmer thinks it's safe. |

### 🟡 High (Incomplete Backend)

| # | Bug | Why It Hurts |
|---|-----|-------------|
| 5 | **`errdefer` stubbed** | `/* TODO: errdefer */` — doesn't compile |
| 6 | **`try` stubbed** | Emits inner expr only. No tag check. No propagation. |
| 7 | **`match` stubbed** | `/* TODO: match */` — doesn't compile |
| 8 | **Error union C layout broken** | Emits error type, not `struct { uint32_t tag; T payload; }` |

### 🟢 Medium (Polish)

| # | Bug | Why It Hurts |
|---|-----|-------------|
| 9 | **No release modes** | Can't choose safety vs. speed. All builds are the same. |
| 10 | **No compiler sanitizers** | Can't catch UAF/double-free in CI. |
| 11 | **No escaping reference lint** | `return &local` passes silently. |
| 12 | **Panic handler not replaceable** | Kernel needs `oops()`, not `abort()`. |
| 13 | **`for` slice var typed as `i64`** | `for c in "hello"` — `c` is `i64`, not `u8`. |

---

## The 3 Sprints to v0.9.0

### Sprint 1: Honesty (Week 1)
**Goal:** Make the compiler do what the README promises.

- [ ] **Emit bounds checks in codegen**
  - File: `src/codegen_c.c`, `AST_INDEX_EXPR`
  - Before: `((T*)slice.ptr)[idx]`
  - After: `(raya_bounds_check(idx, slice.len, file, line), ((T*)slice.ptr)[idx])`
  - `raya_bounds_check` already exists in `src/raya_rt.c`

- [ ] **Add `--release-safe` / `--release-fast`**
  - File: `src/main.c`
  - `--release-safe`: emit `if (unlikely(idx >= len)) raya_panic(...)`
  - `--release-fast`: emit nothing (zero overhead)
  - `--debug` (default): emit `if (idx >= len) { fprintf(stderr, ...); abort(); }`

- [ ] **Make `raya_panic` a weak symbol**
  - File: `src/raya_rt.c`
  - `__attribute__((weak)) void raya_panic(...)`
  - Kernel modules override with `oops()`

**Deliverable:** `raya --release-safe --build file.raya` produces C with `unlikely` panic branches. Bounds overflow crashes with file:line in debug, panics in release-safe, UB in release-fast.

---

### Sprint 2: The Boundary (Week 2)
**Goal:** `unsafe` means something. `const` means something. `errdefer` is safe.

- [ ] **`arr.ptr` requires `unsafe`**
  - File: `src/sema.c`, `AST_FIELD_ACCESS_EXPR`
  - When `base->kind == ST_SLICE` and `fname == "ptr"`, check `s->in_unsafe`

- [ ] **`&T` → `*T` cast requires `unsafe`**
  - File: `src/sema.c`, `AST_CAST_EXPR`
  - When `from == ST_REFERENCE && to == ST_POINTER`, check `s->in_unsafe`

- [ ] **`*T` → `&T` cast requires `unsafe`**
  - File: `src/sema.c`, `AST_CAST_EXPR`
  - When `from == ST_POINTER && to == ST_REFERENCE`, check `s->in_unsafe`

- [ ] **Escaping reference lint**
  - File: `src/sema.c`, `sema_check_fn_body`
  - Warn when `&local` or `&parameter` escapes function scope
  - Add `#[allow(escaping_reference)]` suppression (attribute system)

- [ ] **`errdefer` rejects memory deallocation**
  - File: `src/sema.c`, `AST_ERRDEFER_STMT`
  - Walk body. Reject calls to `free`, `dealloc`, `arena.reset`, etc.
  - Error message: "`errdefer` cannot contain memory deallocation. Use arena allocation."

- [ ] **Fix `const` mutation protection**
  - File: `src/sema.c`, `AST_ASSIGN_STMT`
  - Add `expr_is_mutable()` helper that walks the expression tree
  - Check `const` on struct fields, slice elements, pointer derefs

**Deliverable:** Safe code cannot manufacture raw pointers. `const` is actually protected. `errdefer` cannot double-free.

---

### Sprint 3: Backend Completion (Weeks 3–4)
**Goal:** The language actually runs correctly.

- [ ] **Fix error union C representation**
  - File: `src/codegen_c.c`, `cg_emit_sema_type`
  - `ST_ERROR_UNION` → `struct { uint32_t __tag; T __payload; }`

- [ ] **Lower `try`**
  - File: `src/codegen_c.c`, `AST_TRY_EXPR`
  - Emit: `{ auto __tmp = expr; if (__tmp.__tag != 0) return __tmp; __tmp.__payload; }`

- [ ] **Lower `try else`**
  - File: `src/codegen_c.c`, `AST_ERROR_CAPTURE_EXPR`
  - Emit tag check + fallback block execution

- [ ] **Lower `errdefer`**
  - File: `src/codegen_c.c`, `AST_ERRDEFER_STMT`
  - Use thread-local error flag: `if (__raya_err_flag) { /* cleanup */ }`
  - Set flag on error-return paths

- [ ] **Lower `match`**
  - File: `src/codegen_c.c`, `AST_MATCH_STMT`
  - Emit `switch` with breaks
  - Handle: wildcard, literal, enum variant patterns

- [ ] **Fix `for` slice iteration type**
  - File: `src/sema.c`, `AST_FOR_STMT`
  - Infer variable type from iterable: slice → `slice.base`, array → `array.base`

**Deliverable:** `error_union_ok.raya` compiles end-to-end and runs. `match`, `try`, `errdefer` all produce correct C.

---

## Test Additions Needed

### New Sema Tests (Negative — Must Fail)
```
tests/sema/const_field_mutation.raya
tests/sema/const_index_mutation.raya
tests/sema/const_ptr_deref.raya
tests/sema/unsafe_slice_ptr.raya
tests/sema/unsafe_ref_to_raw_cast.raya
tests/sema/unsafe_raw_to_ref_cast.raya
tests/sema/errdefer_free_rejected.raya
tests/sema/escaping_reference_warn.raya
```

### New Codegen Tests (Positive — Must Pass)
```
tests/codegen/bounds_check_debug.raya
tests/codegen/bounds_check_release_safe.raya
tests/codegen/error_union_try.raya
tests/codegen/error_union_try_else.raya
tests/codegen/errdefer_cleanup.raya
tests/codegen/match_enum.raya
tests/codegen/for_slice_type.raya
```

---

## Design Principles (Read Before Every Commit)

1. **Observability > Prevention** — A sanitizer that crashes in `make test` beats a borrow checker that rejects valid kernel code.
2. **No hidden costs in `release-fast`** — If a feature adds runtime overhead, it must be strip-able.
3. **Arenas first, `free()` last** — Individual deallocation is `unsafe`. Default is scope-bound cleanup.
4. **`unsafe` marks capability creation** — If safe code can manufacture danger and pass it in, the boundary is theater.
5. **Sanitizers are compiler modes, not libraries** — Flags in CI catch bugs before merge.
6. **Kernel-ready by default** — No default heap. Replaceable panic handler. C ABI compatible.

---

## The Deal

Raya trusts you to write the code. It does not trust you to test it.

| Mode | What it does | When to use |
|------|-------------|-------------|
| `debug` | Full checks, stack traces, abort on panic | Development, unit tests |
| `release-safe` | `unlikely` panic branches on bounds/overflow/null | Production kernels, drivers, servers |
| `release-fast` | Zero overhead. UB on failure. | Interrupt handlers, crypto inner loops |

---

## Summary

**Don't ship v0.8.x.** It says "bounds-checked" but isn't. It says `unsafe` marks danger but doesn't. It says `const` protects but it's a sieve.

**Do the 3 sprints.** Four weeks of focused backend work. No new syntax. No new features. Just tighten the screws.

After Sprint 3, you have a language where:
- Bounds overflows panic with file:line in debug/release-safe
- `unsafe` actually marks where danger is created
- `const` actually prevents mutation
- `errdefer` cleans up resources without double-free risk
- Error unions propagate correctly
- `match` compiles to efficient `switch`

**Then tag v0.9.0.** Then write the blog post. Then ask kernel maintainers to try it.

This is what C should have been. Not syntax sugar. Not a borrow checker. A language that respects your expertise but doesn't require you to be perfect.

---

*End of Day — 2026-08-24*  
*Next: Sprint 1, Week 1*
