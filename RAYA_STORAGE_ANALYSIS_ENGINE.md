# Raya Storage Analysis Engine (SAE)
## Design Discussion Document — v0.1.0-draft

**Status:** Design Exploration — Not Part of Active Development  
**Scope:** Post-v1.0 Language Feature / Optional Diagnostic Layer  
**Date:** 2026-08-22  
**Classification:** Architecture Decision Record (ADR) — Pending Team Review

---

## 1. Purpose

This document captures the **Storage Analysis Engine (SAE)** concept for potential integration into Raya at a future date. It is **not** part of the v0.8–v1.0 development roadmap. It exists to ensure today's design decisions do not accidentally block tomorrow's capabilities.

> **Rule:** We design the contracts now. We implement after v1.0.

---

## 2. Core Idea

Instead of tracking ownership or borrow relationships, the compiler tracks the **lifecycle state of storage**:

```
NO_STORAGE → RAW_STORAGE → LIVE_OBJECT → DEAD_OBJECT
```

- **NO_STORAGE:** No memory backing exists (null, unallocated).
- **RAW_STORAGE:** Memory is allocated but contains no valid object (uninitialized).
- **LIVE_OBJECT:** Memory contains a valid, initialized object.
- **DEAD_OBJECT:** Memory has been deallocated. Any access is a bug.

This gives **temporal safety** (when can you touch memory?) without **aliasing safety** (who else is touching it?). Aliasing remains the programmer's responsibility — matching Raya's philosophy.

---

## 3. Why This Fits Raya

Raya's existing slogan: *"Trust the programmer, but give them better tools."*

| Approach | Aliasing | Temporal Safety | Complexity |
|----------|----------|----------------|------------|
| C | Programmer's problem | None | Zero |
| **SAE (proposed)** | Programmer's problem | Debug-mode enforcement | Low |
| Rust borrow checker | Compiler-enforced | Compiler-enforced | High |

SAE occupies the gap between "C trusts you completely" and "Rust watches your every alias." It asks: *"Did you touch dead memory?"* without asking *"Did you have exclusive access?"*

---

## 4. Design Contracts (Locked for v1.0)

These contracts are **not implemented today**. They are design locks to prevent future breakage.

### 4.1 Pointer Semantics

| Raya Type | v0.8–v0.9 Meaning | Reserved v1.0+ Meaning |
|-----------|------------------|------------------------|
| `*T` / `*const T` | Raw C pointer, `unsafe` to deref | **State-agnostic.** Bypasses SAE entirely. C interop lives here forever. |
| `&T` / `&const T` | Safe, non-null reference | **LIVE_OBJECT guarantee.** SAE proves storage is live before binding. |
| `?&T` | Nullable safe reference | **NO_STORAGE or LIVE_OBJECT.** Null is the `NO_STORAGE` state. |
| `[]T` / `[]const T` | Fat pointer (ptr + len) | **LIVE_OBJECT + bounds.** SAE tracks both state and length. |

### 4.2 The `unsafe` Boundary

`unsafe` blocks and `unsafe fn` serve two purposes:

1. **Today:** Permit raw pointer dereference and raw pointer arithmetic.
2. **v1.0+:** Disable SAE state tracking for the lexical scope.

```raya
unsafe fn mmio_read(reg: *u32) -> u32 {
    return unsafe { *reg };   // SAE is off here. No state checks.
}
```

This keeps kernel code and C FFI clean of state annotations.

### 4.3 Allocation / Deallocation as Reserved Transitions

The names `alloc` and `free` in `std.mem` are reserved for compiler-known state transitions:

```raya
// Today: thin wrappers around C malloc/free
// v1.0+: compiler intrinsics with state tracking

fn alloc(size: usize) -> *u8;     // Returns RAW_STORAGE (today: just *u8)
fn free(ptr: *u8) -> void;       // Consumes *u8, transitions to DEAD_OBJECT
```

Users cannot shadow or redefine these in module scope.

### 4.4 C ABI Boundary

Any pointer crossing the C ABI is state-agnostic. Promotion to a tracked state requires an explicit operation:

```raya
const c_ptr: *u8 = c_function();           // State: unknown (treated as RAW)
const raya_ref: &u8 = promote(c_ptr);      // v1.0+: SAE verifies, marks LIVE
```

Today, `promote()` is an `unsafe` cast. In v1.0, it becomes a tracked state transition.

---

## 5. Hypothetical Syntax (v1.0+ — Not Active)

The following syntax examples use **Raya's actual keywords** (`const`, `var`, `fn`) and illustrate how SAE would behave if activated. These are **not valid Raya today**.

### 5.1 Basic Lifecycle Tracking

```raya
module main;

fn example() -> void {
    const raw: *u8 = alloc(64);        // raw → RAW_STORAGE
    const live: &u8 = init(raw);       // SAE: raw was RAW, now live → LIVE_OBJECT

    // In debug build:
    // *raw;                           // ERROR: raw was consumed by init()

    *live = 42;                        // OK: live is LIVE_OBJECT

    free(live);                        // live → DEAD_OBJECT

    // In debug build:
    // *live;                           // ERROR: use of DEAD_OBJECT
}
```

### 5.2 Null as NO_STORAGE

```raya
fn maybe_alloc(cond: bool) -> ?&u32 {
    if cond {
        const raw: *u32 = alloc(4);
        return init(raw);              // LIVE_OBJECT
    }
    return null;                       // NO_STORAGE
}

fn caller() -> void {
    const ptr = maybe_alloc(true);
    if const r = ptr {                 // r is &u32 (LIVE_OBJECT) inside block
        *r = 42;                       // OK
    }
    // ptr is ?&u32 — SAE knows it may be NO_STORAGE or LIVE_OBJECT
}
```

### 5.3 Reuse After Free (Rebirth)

```raya
fn rebirth() -> void {
    var p: *u8 = alloc(64);            // p → RAW_STORAGE
    p = init(p);                       // p promoted to LIVE_OBJECT
    free(p);                           // p → DEAD_OBJECT

    // v1.0 behavior options:
    // A) p is permanently DEAD. Reassignment forbidden without new variable.
    // B) p = alloc(64);               // Rebirth: DEAD → RAW. SAE allows this.
}
```

**Open question:** Is `DEAD_OBJECT` terminal (linear/affine) or can it transition back to `RAW_STORAGE`? Decision deferred to v1.0 design phase.

### 5.4 SAE in Debug vs. Release

```raya
// Debug build: state checks are enforced (compile-time + runtime assertions)
// Release-fast: all SAE checks stripped. Zero cost.
//
// This mirrors Raya's existing philosophy:
//   - Bounds checks: panic in debug, UB in release-fast
//   - State checks: error in debug, UB in release-fast
```

---

## 6. What SAE Does NOT Do

To be explicit about scope boundaries:

| Problem | SAE Handles? | Notes |
|---------|-----------|-------|
| Use-after-free | **Yes** | DEAD_OBJECT access is caught |
| Double-free | **Yes** | Second `free()` sees DEAD_OBJECT |
| Uninitialized read | **Yes** | RAW_STORAGE read is caught |
| Null dereference | **Yes** | NO_STORAGE → LIVE_OBJECT check |
| Data races | **No** | Aliasing is still the programmer's problem |
| Iterator invalidation | **No** | Requires alias tracking |
| Use-after-move | **Partial** | If move transitions to RAW, yes. If aliased, no. |
| Buffer overflow | **No** | SAE tracks state, not spatial bounds. Slices handle bounds. |

---

## 7. Implementation Strategy (Future)

### Phase 1: SAE as a Separate Pass (Post-v1.0)

After semantic analysis is stable, add a **StorageStatePass** that:

1. Walks the AST after Pass 2 (body resolution).
2. Maintains a `HashMap<VarId, StorageState>`.
3. Emits **warnings/errors** in debug builds.
4. Generates **no code** — purely diagnostic.

This proves the concept without touching codegen.

### Phase 2: IR Intrinsics (Post-v1.0)

Reserve IR opcodes today, implement lowering later:

```c
// Reserved IR opcodes (no-op today, active in v1.0+):
IR_STATE_CHECK_LIVE,          // Assert variable is LIVE_OBJECT
IR_STATE_CHECK_RAW,           // Assert variable is RAW_STORAGE
IR_STATE_PROMOTE_RAW_TO_LIVE,   // Transition marker for optimization
IR_STATE_DEMOTE_LIVE_TO_DEAD,  // Transition marker for optimization
```

### Phase 3: Optional Enforcement (v1.2+)

Add a compiler flag `--enforce-storage-state` that turns SAE diagnostics into hard errors. Default: warning-only.

---

## 8. Design Locks for Today's Development

To ensure SAE can land later without breaking changes, follow these rules **now**:

| Rule | Rationale |
|------|-----------|
| `&T` must never be creatable from `*T` without explicit cast | In v1.0, that cast becomes a state promotion |
| `unsafe` blocks disable all safety checks | v1.0 extends this to disable SAE |
| `alloc` and `free` are reserved in `std.mem` | v1.0 makes them compiler-known transitions |
| Do not add `&raw T`, `&live T`, `&dead T` to type syntax | Keeps surface language clean; state is tracked internally |
| Do not add `#[track_lifecycle]` yet | Attribute system is unstable; defer to v1.0 |
| C ABI pointers are always state-agnostic | Prevents ABI breakage |

---

## 9. Recommendation

**Do not implement SAE before v1.0.** The priority is:

1. Backend (C bootstrap → LLVM)
2. Kernel proof-of-concept (MMIO, spinlock, interrupt handler)
3. Standard library
4. Self-hosting

**After v1.0**, revisit SAE as an optional diagnostic layer. It aligns with Raya's philosophy and fills a genuine gap in systems programming. But it is not on the critical path to a working compiler.

---

## 10. Open Questions

1. **Is `DEAD_OBJECT` terminal?** Can a variable be reborn, or is linear/affine typing required?
2. **How does SAE interact with `defer`?** Does `defer free(ptr)` transition state at block exit?
3. **Sub-object tracking:** If I move one field out of a struct, what state do the remaining fields have?
4. **Thread-local vs. shared storage:** Does SAE need to understand `thread_local` or shared heap?
5. **Interaction with comptime:** Can storage states be manipulated at compile time?

These are deferred to the v1.0 design phase.

---

> *"Trust the programmer with aliasing, but don't let them touch dead memory."*  
> — Potential Raya SAE motto

---

**Document Status:** Draft for discussion. Not a commitment. Not a roadmap item.  
**Next Review:** After v1.0 backend is stable.
