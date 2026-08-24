# Raya Development Process

**Version:** 0.9.0-target  
**Status:** Frontend complete. Backend and safety architecture in progress.

---

## Current State

| Component | Status | Notes |
|-----------|--------|-------|
| Lexer | ✅ | All tokens, comments, literals |
| Parser | ✅ | Full AST for all constructs |
| Sema — Types, methods, error unions | ✅ | 30 tests passing |
| Sema — `unsafe` boundary | ⚠️ | Guards `*ptr` deref. Missing: `arr.ptr`, `&T`→`*T` casts. |
| Sema — `errdefer` restriction | ❌ | Accepts any body including `free()`. Double-free vector. |
| Sema — Escaping reference lint | ❌ | `return &local` passes silently. |
| C Backend — `defer` | ✅ | LIFO cleanup on normal returns |
| C Backend — `errdefer` | ❌ | Stubbed: `/* TODO: errdefer */` |
| C Backend — `match` | ❌ | Stubbed: `/* TODO: match */` |
| C Backend — `try` / `try else` | ❌ | Emits inner expr only. No tag check. |
| C Backend — Bounds checks | ❌ | Emits raw `((T*)ptr)[idx]`. No checks. |
| C Backend — Error union layout | ❌ | Emits error type, not tagged struct. |
| Release modes | ❌ | No `--release-safe` / `--release-fast` |
| Sanitizers | ❌ | No `--sanitize=memory/arena/undefined` |
| Stdlib — Arena | ❌ | No `std.mem.Arena` module |
| Runtime — Panic handler | ❌ | Hardcoded `abort()`. Not replaceable. |

---

## Next 3 Sprints

### Sprint 1: Honesty (Week 1)

Make the compiler do what the README promises.

- [ ] **Bounds checks in codegen** — `raya_bounds_check(idx, len, file, line)` in debug/release-safe. Nothing in release-fast.
- [ ] **Release modes** — `--debug` (default), `--release-safe`, `--release-fast`.
- [ ] **Replaceable panic handler** — `raya_panic(file, line, msg)` weak symbol. Kernel overrides with `oops()`.
- [ ] **Fix `--help` typo** — "vvutput" → "output".

**Deliverable:** `raya --release-safe --build file.raya` produces C with `unlikely` panic branches.

### Sprint 2: The `unsafe` Boundary (Week 2)

`unsafe` must mark capability creation, not just use.

- [ ] **`arr.ptr` requires `unsafe`** — Field access on slice `.ptr` needs `unsafe` context.
- [ ] **`&T` → `*T` cast requires `unsafe`** — Discarding safety guarantees is dangerous.
- [ ] **`*T` → `&T` cast requires `unsafe`** — Claiming safety where none exists.
- [ ] **Escaping reference lint** — Warn on `return &local`. `#[allow(escaping_reference)]` to suppress.
- [ ] **`errdefer` resource restriction** — Reject `free()` / `dealloc` in `errdefer` bodies. Memory cleanup uses arenas.

**Deliverable:** Safe code cannot manufacture raw pointers. `errdefer` cannot double-free.

### Sprint 3: Backend Completion (Week 3–4)

Make the language actually run correctly.

- [ ] **`errdefer` lowering** — Conditional cleanup on error-return flag.
- [ ] **`try` / `try else` lowering** — Tag check + conditional return/fallback.
- [ ] **`match` lowering** — `switch` with breaks. Wildcard, literal, enum patterns.
- [ ] **Error union C representation** — `struct { uint32_t tag; T payload; }`.
- [ ] **Arena stdlib module** — `Arena.init()`, `arena.alloc()`, `arena.reset()`. Individual `free()` is `unsafe`.

**Deliverable:** `error_union_ok.raya` compiles end-to-end and runs correctly.

---

## Principles (Read Before Adding Features)

1. **Observability > Prevention** — A sanitizer that crashes in `make test` beats a borrow checker that rejects valid kernel code.
2. **No hidden costs in `release-fast`** — If a feature adds runtime overhead, it must be strip-able.
3. **Arenas first, `free()` last** — Individual deallocation is `unsafe`. The default is scope-bound cleanup.
4. **`unsafe` marks capability creation** — If safe code can manufacture danger and pass it in, the boundary is theater.
5. **Sanitizers are compiler modes, not libraries** — Flags in CI catch bugs before merge. Opt-in libraries don't.
6. **Kernel-ready by default** — No default heap. Replaceable panic handler. C ABI compatible.

---

## Architecture

```
Source (.raya) → Lexer → Parser → Sema → C11 Emitter → Host cc → Binary
                                      ↓
                              --sanitize=memory/arena/undefined
```

**Backend strategy:** C transpile first (proves ABI, fast to iterate). LLVM later.

---

## Testing Targets

| Layer | Current | Target |
|-------|---------|--------|
| Sema | 30 | 50+ (add negative tests for unsafe boundary, errdefer) |
| Codegen | 14 | 30+ (bounds, try, errdefer, match, arena) |
| Integration | 0 | 10+ (end-to-end compile + run) |

---

> Ship the frontend. Harden the boundary. Then decide how fast.
