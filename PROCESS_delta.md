# PROCESS.md Delta — Updated Roadmap to "What C Should Have Been"

**Base:** PROCESS.md v0.8.0 (2026-08-23)  
**Delta Author:** Design review audit  
**Status:** Proposed changes to align with safety architecture

---

## Changes to Section 3 (Current State)

### Add new rows to the Component table:

| Component | Status | Notes |
|-----------|--------|-------|
| Sema — `errdefer` resource restriction | ❌ Not Started | Must reject memory deallocation in `errdefer` bodies |
| Sema — `unsafe` capability boundary | ❌ Not Started | `arr.ptr`, `&T`→`*T` casts must require `unsafe` |
| Sema — Escaping reference lint | ❌ Not Started | Warn on `return &local`. `#[allow(escaping_reference)]` to suppress. |
| Sema — `&volatile T` type | ❌ Not Started | Spec'd in RAYA_HARDWARE_INTERFACE_SPEC.md |
| Backend — Bounds check codegen | ❌ Not Started | `raya_bounds_check()` in debug/release-safe |
| Backend — `defer` lowering | ✅ Partial | Normal returns work. Error-path conditional cleanup missing. |
| Backend — `errdefer` lowering | ❌ Not Started | Stubbed in codegen_c.c |
| Backend — `match` lowering | ❌ Not Started | Stubbed in codegen_c.c |
| Backend — Error union representation | ❌ Not Started | `cg_emit_sema_type` emits wrong type for `ST_ERROR_UNION` |
| Backend — `try`/`try else` lowering | ❌ Not Started | Emits inner expression only. No tag check. |
| Driver — Release modes | ❌ Not Started | No `--release-safe` / `--release-fast` |
| Driver — Sanitizer flags | ❌ Not Started | No `--sanitize=memory/arena/thread/undefined` |
| Stdlib — Arena allocator | ❌ Not Started | No `std.mem.Arena` module |
| Runtime — Replaceable panic handler | ❌ Not Started | `raya_panic()` hardcoded to `abort()` |

---

## Changes to Section 4 (Known Issues)

### Add to "Critical (Fix Before v0.9.0)":

- 🔴 `errdefer` accepts any block including `free()` calls — double-free vector.
- 🔴 `unsafe` only guards `*ptr` dereference, not `arr.ptr` or `&T as *T` — safe code manufactures raw pointers.
- 🔴 Bounds checks promised in README but not emitted in codegen.
- 🔴 `release-fast` mode would have UB on bounds overflow, but there is no `release-safe` mode with defined panics.
- 🔴 Error union C representation is broken — emits error type instead of tagged struct.

### Add to "Medium (Fix Next Sprint)":

- `match` lowering stubbed in C backend.
- `try`/`try else` lowering stubbed in C backend.
- `for` loop over slices uses `void*` cast — loses type safety in C output.
- `--help` typo: "vvutput" → "output".

---

## Changes to Section 5 (Phase 4 Roadmap)

### Insert new Sprint 0 before existing Sprint 1:

#### Sprint 0: Safety Architecture Foundation (Week 0) — NEW

- [ ] Add `release-safe` / `release-fast` / `debug` modes to compiler driver
- [ ] Emit bounds checks via `raya_bounds_check()` in debug/release-safe modes
- [ ] Restrict `errdefer` to non-memory resources (compile error on `free`/`dealloc`)
- [ ] Expand `unsafe` boundary: `arr.ptr`, `&T`→`*T`, `*T`→`&T` casts require `unsafe`
- [ ] Add escaping reference lint with `#[allow(escaping_reference)]`
- [ ] Fix error union C representation: emit `{ uint32_t tag; T payload; }`
- [ ] Implement `try` lowering: tag check + conditional return
- [ ] Implement `errdefer` lowering: conditional cleanup on error flag

**Rationale:** These are prerequisites for all other backend work. Without them, the C backend produces incorrect or dangerous code.

### Update Sprint 5 (Error Unions & Defer):

#### Sprint 5: Error Unions & Defer (Week 5) — UPDATED

- [x] `E!S` syntax: binary `!` in type expressions, parser + AST support
- [x] Error union type resolution with hash-consed `SType`
- [x] Error type restricted to struct/union/enum
- [x] Auto-wrap on return: success branch, error branch, pass-through
- [x] `try` propagation with enclosing function validation
- [x] `try else |e|` capture with divergence analysis
- [x] `errdefer` sema validation (lowering ~~blocked on backend~~ **NOW IMPLEMENTED**)
- [ ] `errdefer` resource restriction (NEW — reject memory deallocation)
- [x] Coercion: `T -> E!T`, `E -> E!T`, `E!T -> E!T` identity
- [ ] `try` C lowering (NEW — tag check + return)
- [ ] `errdefer` C lowering (NEW — conditional cleanup)
- [ ] 3 new sema tests, 30 total passing → **target 40+ with new negative tests**

---

## Changes to Section 6 (Phase 5+ Roadmap)

### Insert new Phase 4.5: Sanitizers & Runtime (4 weeks)

**Goal:** Make bugs observable in `make test`, not in production.

#### Sprint 1: Runtime Foundation (Week 1)
- [ ] `raya_rt.c`: `raya_panic(file, line, msg)` — replaceable weak symbol
- [ ] `raya_rt.c`: `raya_bounds_check(idx, len, file, line)`
- [ ] `raya_rt.c`: `raya_slice_sub(start, end, len, file, line)` — sub-slicing bounds check
- [ ] `raya_rt.h`: `raya_Slice`, `raya_Str` structs (already partial)

#### Sprint 2: Compiler Sanitizer Flags (Week 2)
- [ ] `--sanitize=memory`: KASAN-style shadow memory (stubs)
- [ ] `--sanitize=arena`: Arena lifetime tracking (stubs)
- [ ] `--sanitize=undefined`: Integer overflow, shift checks (stubs)
- [ ] `--sanitize=thread`: Data race detection (stubs, future)

#### Sprint 3: Arena Standard Library (Week 3)
- [ ] `lib/std/mem/Arena.raya`: `Arena.init()`, `arena.alloc(n)`, `arena.reset()`
- [ ] `lib/std/mem/Arena.raya`: `Arena.deinit()` — marks all allocations DEAD in sanitize mode
- [ ] Document: arenas are the default; individual `free()` is `unsafe`

#### Sprint 4: Integration Tests (Week 4)
- [ ] 10+ integration tests: compile `.raya` → run binary → verify output
- [ ] Test bounds check panic in debug mode
- [ ] Test bounds check `unlikely` branch in release-safe mode
- [ ] Test no bounds check overhead in release-fast mode

### Update Phase 5 (Backend):

**Add to C Transpile mapping table:**

| Raya | C11 (debug/release-safe) | C11 (release-fast) |
|------|---------------------------|-------------------|
| `arr[i]` | `raya_bounds_check(i, arr.len, file, line), ((T*)arr.ptr)[i]` | `((T*)arr.ptr)[i]` |
| `arr[a..b]` | `raya_slice_sub(a, b, arr.len, file, line), (raya_Slice){...}` | `(raya_Slice){...}` |
| `try expr` | `{ auto __tmp = expr; if (__tmp.__tag) return __tmp; __tmp.__payload; }` | Same (tag check is required for correctness) |
| `errdefer { ... }` | `if (__error_flag) { ... }` at cleanup label | Same (error flag is required) |
| `E!T` | `struct { uint32_t __tag; T __payload; }` | Same (ABI must be stable) |

**Add to "Runtime Library (small)":**
- `raya_panic()` — user-replaceable panic handler
- `raya_bounds_check()` — slice/array index validation
- `raya_slice_sub()` — sub-slicing validation
- `raya_errdefer_flag` — thread-local error flag for `errdefer` conditional cleanup

### Update Phase 7 (Standard Library):

**Add to stdlib modules:**
- `std.mem.Arena` — arena allocator (default allocation strategy)
- `std.mem.PageAllocator` — page-granularity allocator (kernel use)
- `std.panic` — panic handler registration
- `std.atomic` — `#[atomic_*]` wrappers (post-hardware-spec)

---

## Changes to Section 7 (Architecture Decisions)

### Add new decisions:

| Decision | Rationale |
|----------|-----------|
| `release-safe` as default production mode | Systems code should fail loudly before it fails silently. `release-fast` is opt-in, not default. |
| `errdefer` restricted to non-memory resources | Prevents double-free when `errdefer free(x)` coexists with manual `free(x)`. Memory cleanup uses arenas. |
| `unsafe` guards capability creation, not just use | If safe code can manufacture `*T` and pass it to `unsafe`, the boundary is theater. |
| Compiler sanitizers as flags, not libraries | Opt-in debug allocators fail because nobody enables them. Flags in CI catch bugs before merge. |
| Arenas as default allocation model | Individual `free()` is the #1 CVE factory. Kernel code doesn't `kfree` every `kmalloc` — it tears down contexts. |
| Replaceable panic handler | Kernel panic is `oops()`, not `abort()`. Bare-metal needs `while(1);`. |

---

## Changes to Section 8 (Testing Strategy)

### Update Target Coverage:

| Layer | Current | Target | Priority |
|-------|---------|--------|----------|
| Lexer | 3 | 15 | P2 |
| Parser | 1 | 20 | P2 |
| Sema | **28** | **50+** | **P0** |
| Codegen | **14** | **30+** | **P0** |
| Integration | **0** | **20+** | **P0** |

### Add test categories:

- **Bounds check**: Debug panic, release-safe `unlikely` branch, release-fast omission
- **Error union**: `try` tag check, `try else` fallback, `errdefer` conditional cleanup
- **Unsafe boundary**: `arr.ptr` outside unsafe, `&T`→`*T` outside unsafe, `*T`→`&T` outside unsafe
- **Arena**: Allocation, reset, use-after-reset in sanitize mode
- **Release modes**: Same source compiles to three different outputs

---

## Changes to Section 9 (Backend Strategy)

### Add to "C Transpile First":

**Release Mode Implementation:**

```c
// In codegen_c.c, add mode field to CGen:
typedef enum {
    MODE_DEBUG,         // Full checks, stack traces, abort on panic
    MODE_RELEASE_SAFE,  // unlikely branches to panic handler
    MODE_RELEASE_FAST,  // Zero overhead. UB on failure.
} CGenMode;
```

**Bounds check emission:**
```c
// MODE_DEBUG
if (idx >= len) {
    fprintf(stderr, "panic: index out of bounds at %s:%lu\n", file, line);
    abort();
}

// MODE_RELEASE_SAFE
if (unlikely(idx >= len)) {
    raya_panic(file, line, "index out of bounds");
}

// MODE_RELEASE_FAST
// Nothing emitted
```

---

## New Section: Design Principles Checklist

Before any new feature is merged, it must answer:

1. **Does it add hidden cost in release-fast mode?** If yes, reject or gate behind flag.
2. **Does it prevent a bug that would become a CVE?** If yes, prioritize.
3. **Does it require the programmer to be perfect?** If yes, add a sanitizer or lint.
4. **Does it break C ABI compatibility?** If yes, require explicit attribute.
5. **Does it work in a kernel module context?** If no, make it optional or replaceable.

---

*This delta is a living document. Revisit after each sprint to ensure alignment with the "what C should have been" goal.*
