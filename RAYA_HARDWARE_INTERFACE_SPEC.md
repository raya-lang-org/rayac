# Raya Hardware Interface & Attribute System Specification
## ("Trust the Programmer" Boundary — v0.8.0-pre)

**Status:** Design Complete → Implementation Ready  
**Target:** Phase 4 Sprint 3+ (Traits) & Phase 5 (Backend)  
**Based on:** PROCESS.md v0.7.7 + Pre-session comptime attribute discussions  
**Author:** Lead Engineering Review  
**Date:** 2026-08-22

---

## 1. Executive Summary

This document defines the **hardware-facing surface** of Raya: `volatile`, atomics, inline assembly, linkage/section attributes, and the `#[]` attribute system that drives them. These are not "advanced features for later." They are **language primitives** that dictate IR design, backend lowering, and whether Raya can write a UART driver or a spinlock.

The design follows one rule: **Safe syntax, explicit semantics.** The compiler validates constraints and catches mistakes. The programmer owns the consequences of wrong memory ordering or bad assembly.

> **Note on the "Liar Compiler":** We have validated the `#[]` attribute parsing and comptime evaluation pipeline via a liar compiler (stub backend, full sema). All attribute forms parse correctly and resolve at comptime. This spec now hardens the semantics.

---

## 2. The `#[]` Attribute System

### 2.1 Core Design

Attributes in Raya are **comptime-evaluated metadata** attached to declarations, expressions, types, and fields. They are not decorators — they are **semantic directives** that the compiler must understand.

```raya
// On declarations
#[no_mangle]
#[section(".init.text")]
fn init_driver() -> void { ... }

// On expressions
let x = #[atomic_load(Acquire)] ptr.*;

// On types (type constructor attributes)
let reg: &volatile u32 = &MMIO_BASE;

// On fields
struct IrqDesc {
    #[align(64)]
    handler: fn() -> void,
}
```

### 2.2 Attribute Grammar

```ebnf
Attribute      ::= "#[" AttributeName ( "(" AttributeArgs ")" )? "]"
AttributeName  ::= identifier ( "::" identifier )*
AttributeArgs  ::= Arg ( "," Arg )*
Arg            ::= Expr | identifier "=" Expr
```

### 2.3 Comptime Resolution

All attribute arguments are evaluated at **compile time** via the comptime VM. This means:

```raya
const CACHE_LINE: usize = 64;

struct CacheAligned {
    #[align(CACHE_LINE)]   // Resolved to 64 at comptime
    data: [8]u64,
}
```

**Implementation note:** The comptime evaluator runs during Pass 1 of semantic analysis. Attribute arguments must be `comptime`-knowable: literals, const variables, or comptime function calls. If an attribute argument cannot be resolved at comptime, sema emits a hard error.

### 2.4 Attribute Categories

| Category | Examples | Target | Phase |
|----------|----------|--------|-------|
| **Linkage** | `no_mangle`, `weak`, `used`, `linkage("internal")` | Functions, globals | Phase 5 |
| **Section** | `section(".init.text")`, `section(".data.read_mostly")` | Functions, globals | Phase 5 |
| **Memory** | `volatile`, `atomic_*`, `align(N)`, `packed` | Types, expressions | Phase 5 |
| **Optimization** | `noinline`, `always_inline`, `cold`, `hot` | Functions | Phase 5+ |
| **Safety** | `naked`, `interrupt` | Functions | Phase 5 |
| **FFI** | `extern("C")`, `call_conv("stdcall")` | Functions | Phase 5 |
| **Testing** | `test`, `bench` | Functions | Phase 6+ |

---

## 3. Volatile — "Don't Optimize This Away"

### 3.1 Philosophy

C made `volatile` a **storage-class qualifier** and it was a 40-year mistake. In Raya, volatility is a property of the **access**, not the variable. We provide two equivalent forms:

| Form | Syntax | Use Case |
|------|--------|----------|
| **Access attribute** | `#[volatile] ptr.*` | One-off MMIO reads/writes |
| **Volatile pointer type** | `&volatile T` | APIs, registers, device descriptors |

### 3.2 Syntax

```raya
// Form A: Attribute on dereference
let status: u32 = #[volatile] device_status_reg.*;
#[volatile] tx_fifo.* = data;

// Form B: Volatile reference type
fn read_status(reg: &volatile u32) -> u32 {
    return reg.*;   // Every deref is volatile — no attribute needed
}

fn write_cmd(reg: &volatile u32, cmd: u32) -> void {
    reg.* = cmd;    // Volatile store
}
```

### 3.3 Semantics

- A volatile load guarantees **exactly one memory access instruction** is emitted. The compiler may not:
  - Eliminate redundant loads
  - Reorder volatile loads relative to other volatile accesses
  - Cache the value in a register across volatile accesses
- A volatile store guarantees **exactly one write instruction**.
- Volatile does **NOT** provide atomicity or memory ordering. It is not a synchronization primitive.
- Volatile accesses are **not** thread-safe by default.

### 3.4 Type System Integration

```
&volatile T     →  safe deref, volatile semantics
&volatile const T → safe deref, volatile, read-only
```

`&volatile T` is a **distinct type** from `&T`. Coercion rules:
- `&volatile T` → `&T`: **Forbidden.** Volatility is contagious. If you pass a volatile register to a function expecting a normal reference, the function might optimize away accesses.
- `&T` → `&volatile T`: **Allowed with explicit cast.** You can escalate, never downgrade.

```raya
let normal: &u32 = &x;
let vol: &volatile u32 = normal as &volatile u32;  // OK

let bad: &u32 = vol;  // ERROR: cannot discard volatile qualifier
```

### 3.5 IR Primitive

```c
// IR op: VolatileLoad(ptr, type)
// IR op: VolatileStore(ptr, val)
```

**C Bootstrap Lowering:**
```c
// Raya: let x = #[volatile] ptr.*;
uint32_t x = *((volatile uint32_t*)ptr);

// Raya: #[volatile] ptr.* = 42;
*((volatile uint32_t*)ptr) = 42;
```

**LLVM Lowering:**
```llvm
%x = load volatile i32, i32* %ptr
store volatile i32 42, i32* %ptr
```

---

## 4. Atomics — "Ordering Is the Programmer's Choice"

### 4.1 Philosophy

Raya adopts the **C11 memory model** exactly. We do not invent new ordering names, new semantics, or new guarantees. A kernel hacker who knows `smp_mb()` and `atomic_inc()` should map directly to Raya syntax.

**Mandatory memory ordering:** Every atomic operation requires an explicit ordering argument. There are no defaults. Wrong ordering is a logic bug, not a safety violation — the compiler enforces syntax, not strategy.

### 4.2 Memory Ordering Enum

```raya
enum MemoryOrder {
    Relaxed,    // Atomicity only, no ordering
    Acquire,    // Synchronize-with: loads only
    Release,    // Synchronize-with: stores only
    AcqRel,     // Acquire + Release: RMW only
    SeqCst,     // Sequential consistency: total order
}
```

### 4.3 Syntax

```raya
// Loads
let x = #[atomic_load(Acquire)] ptr.*;
let y = #[atomic_load(Relaxed)] counter.*;

// Stores
#[atomic_store(Release)] flag.* = 1;
#[atomic_store(SeqCst)] done.* = true;

// Read-Modify-Write
#[atomic_rmw(Add, AcqRel)] counter.* += 1;
#[atomic_rmw(And, Relaxed)] mask.* &= 0xFF;
#[atomic_rmw(Xchg, SeqCst)] old.* = new_val;

// Compare-Exchange
let success = #[atomic_cmpxchg(AcqRel, Acquire)] ptr.*
    .compare(expected, desired);

// Fence (CPU barrier)
#[fence(Acquire)];
#[fence(Release)];
#[fence(SeqCst)];    // Full barrier
```

### 4.4 Semantics

| Operation | Allowed Orderings | Notes |
|-----------|-------------------|-------|
| `atomic_load` | Relaxed, Acquire, SeqCst | Acquire loads synchronize-with Release stores |
| `atomic_store` | Relaxed, Release, SeqCst | Cannot use Acquire on a store |
| `atomic_rmw` | Relaxed, Acquire, Release, AcqRel, SeqCst | Always reads the old value |
| `atomic_cmpxchg` | success: any, fail: Relaxed/Acquire/SeqCst | Failure ordering must be ≤ success ordering |
| `fence` | Acquire, Release, AcqRel, SeqCst | Standalone barrier, no data transfer |

**Sema validation:**
- `atomic_store(Acquire)` → **ERROR**: Acquire is not valid for stores.
- `atomic_cmpxchg(Release, Release)` → **ERROR**: Failure ordering cannot be Release.
- `atomic_load(Relaxed)` on `*T` (raw pointer) → **ERROR**: Atomics require `&T` or `&volatile T`, not raw pointers.

### 4.5 IR Primitives

```c
// IR ops for atomics
AtomicLoad(ptr, type, ordering)
AtomicStore(ptr, val, ordering)
AtomicRMW(ptr, op, val, ordering)   // op: Add, Sub, And, Or, Xor, Xchg, Max, Min
AtomicCmpXchg(ptr, expected, desired, succ_ord, fail_ord)
Fence(ordering)
```

**C Bootstrap Lowering (GCC/Clang intrinsics):**
```c
// Raya: let x = #[atomic_load(Acquire)] ptr.*;
uint32_t x = __atomic_load_n(ptr, __ATOMIC_ACQUIRE);

// Raya: #[atomic_store(Release)] ptr.* = 42;
__atomic_store_n(ptr, 42, __ATOMIC_RELEASE);

// Raya: #[atomic_rmw(Add, AcqRel)] ptr.* += 1;
uint32_t old = __atomic_fetch_add(ptr, 1, __ATOMIC_ACQ_REL);

// Raya: #[atomic_cmpxchg(AcqRel, Acquire)] ptr.*.compare(exp, des)
_Bool success = __atomic_compare_exchange_n(
    ptr, &expected, desired,
    false,  // weak = false
    __ATOMIC_ACQ_REL,
    __ATOMIC_ACQUIRE
);

// Raya: #[fence(SeqCst)];
__atomic_thread_fence(__ATOMIC_SEQ_CST);
```

**LLVM Lowering:**
```llvm
%x = load atomic i32, i32* %ptr acquire, align 4
store atomic i32 42, i32* %ptr release, align 4
%old = atomicrmw add i32* %ptr, i32 1 acq_rel
%success = cmpxchg i32* %ptr, i32 %exp, i32 %des acq_rel acquire
fence seq_cst
```

### 4.6 Compiler Fence vs. CPU Fence

Linux kernel code uses compiler barriers extensively. We separate them:

```raya
#[compiler_fence];   // Prevents compiler reordering only. No CPU instruction.
#[fence(Acquire)];    // CPU fence + compiler barrier
```

**C Bootstrap:**
```c
// #[compiler_fence]
__asm__ __volatile__("" ::: "memory");

// #[fence(Acquire)]
__atomic_thread_fence(__ATOMIC_ACQUIRE);
```

---

## 5. Inline Assembly — "The Compiler Must Know Everything"

### 5.1 Philosophy

String-based inline assembly (C's `asm volatile(...)`) is a text-substitution hack. The compiler doesn't understand register constraints, clobbers, or memory effects. It guesses. In kernel code, a wrong guess means corrupted registers or reordered memory operations around a critical section.

Raya inline assembly is **structured**. The compiler knows every input, output, and destroyed register. It validates constraints at compile time and integrates with the register allocator.

### 5.2 Syntax

```raya
asm {
    // Inputs: register name + variable
    in("eax") cpu_type,
    in("ecx") 0,           // Immediate or variable

    // Outputs: register name + variable (must be mutable)
    out("ebx") ebx_out,
    out("edx") edx_out,

    // Clobbers: registers destroyed but not input/output
    clobber("eax"),         // "eax" is input AND clobbered? ERROR.
    clobber("memory"),      // This asm touches memory unpredictably
    clobber("cc"),          // Flags register modified

    // Options
    options(volatile),      // Do not delete or move this asm block
    options(pure),          // No side effects (can delete if unused)
    options(nomem),         // Does not touch memory (enables reordering)
    options(noreturn),      // Does not fall through (e.g., ud2, hlt)

    // Template string: the only raw text
    "cpuid"
};
```

### 5.3 Constraint Syntax

| Constraint | Meaning | Example |
|------------|---------|---------|
| `in("reg") val` | Read-only register input | `in("eax") val` |
| `out("reg") val` | Write-only register output | `out("ebx") result` |
| `inout("reg") val` | Read/write register | `inout("eax") counter` |
| `in("m") val` | Memory input | `in("m") buffer` |
| `out("m") val` | Memory output | `out("m") result` |
| `clobber("reg")` | Destroyed register | `clobber("ecx")` |
| `clobber("memory")` | Memory barrier | Prevents reordering across asm |
| `clobber("cc")` | Flags modified | Condition codes |

### 5.4 Semantic Validation (Hard Errors)

The semantic analyzer **must** enforce:

1. **Register overlap:** If `in("eax")` is declared, `clobber("eax")` is an error. Use `inout` instead.
2. **Mutability:** `out` and `inout` variables must be `var`, not `const`.
3. **Definite assignment:** `out` variables must be initialized before the asm block (they are overwritten, but the compiler needs a declaration).
4. **Option conflicts:** `options(nomem)` + `clobber("memory")` = **ERROR**.
5. **Option conflicts:** `options(pure)` + `options(volatile)` = **ERROR**.
6. **noreturn placement:** If `options(noreturn)` is set, the asm block must be the last statement in the function. No code after it.
7. **Valid registers:** Register names must exist on the target architecture. `in("rax")` on AArch64 = **ERROR**.
8. **noreturn + outputs:** `options(noreturn)` with `out` declarations = **ERROR** (the outputs are never written if we don't return).

### 5.5 IR Primitive

```c
AsmBlock {
    StringView template;           // "cpuid"
    AsmConstraint[] inputs;        // { reg: "eax", val: operand }
    AsmConstraint[] outputs;       // { reg: "ebx", val: operand }
    StringView[] clobbers;         // ["ecx", "memory", "cc"]
    AsmOptions options;            // volatile, pure, nomem, noreturn
}
```

**C Bootstrap Lowering:**
```c
// Raya asm block
uint32_t cpu_type = 0;
uint32_t ebx_out = 0;
uint32_t edx_out = 0;

__asm__ __volatile__ (
    "cpuid"
    : "=b"(ebx_out), "=d"(edx_out)    // outputs
    : "a"(cpu_type), "c"(0)             // inputs
    : "memory", "cc"                    // clobbers
);
```

**LLVM Lowering:**
```llvm
%asm_result = call { i32, i32 } asm sideeffect "cpuid",
    "={ebx},={edx},{eax},{ecx},~{memory},~{cc}"
    (i32 %cpu_type, i32 0)
```

### 5.6 Architecture-Specific Register Names

Register names are validated against the target triple:

| Architecture | Valid Registers |
|--------------|-----------------|
| x86_64 | `rax`, `rbx`, `rcx`, `rdx`, `rsi`, `rdi`, `rbp`, `rsp`, `r8`–`r15`, `eax`, `ebx`, etc., `xmm0`–`xmm15` |
| AArch64 | `x0`–`x30`, `w0`–`w30`, `sp`, `v0`–`v31`, `xzr`, `wzr` |
| RISC-V | `x0`–`x31`, `f0`–`f31`, `zero`, `ra`, `sp`, `gp`, `tp` |

---

## 6. Linkage & Section Attributes

Kernel builds require explicit control over binary layout. These attributes are **non-negotiable** for Linux kernel module development.

### 6.1 Section Control

```raya
#[section(".init.text")]
fn init_module() -> i32 { ... }

#[section(".exit.text")]
fn cleanup_module() -> void { ... }

#[section(".data.read_mostly")]
static let config_table: [16]u32 = ...;

#[section(".rodata")]
const let version: []const u8 = "1.0.0";
```

**C Bootstrap:**
```c
__attribute__((section(".init.text")))
int init_module(void) { ... }
```

### 6.2 Symbol Visibility

```raya
#[no_mangle]              // Exact symbol name, no type mangling
fn raya_driver_probe() -> i32 { ... }

#[weak]                   // Weak symbol — overridable
fn arch_specific_hook() -> void { ... }

#[used]                   // Prevent dead-code elimination / LTO stripping
static let idt: [256]IdtEntry = ...;

#[linkage("internal")]     // Static linkage, not exported
fn helper() -> void { ... }
```

### 6.3 Special Function Types

```raya
#[naked]                   // No prologue/epilogue. Body must be single asm block.
unsafe fn syscall_entry() -> void {
    asm {
        options(noreturn),
        "swapgs",
        "mov %rsp, %gs:0x10",
        // ...
    };
}

#[interrupt]               // Interrupt calling convention (iret, not ret)
fn timer_isr() -> void {
    // Compiler emits: pusha; ... ; popa; iret
    ack_interrupt();
}
```

**Safety boundary:** `#[naked]` requires `unsafe fn`. The programmer is writing raw stack manipulation without compiler-generated frame setup.

### 6.4 Calling Convention

```raya
#[extern("C")]            // C ABI (default for extern functions)
fn c_callback(cb: fn(i32) -> void) -> void { ... }

#[call_conv("stdcall")]   // Windows x86 stdcall
fn winapi_func() -> void { ... }

#[call_conv("fastcall")]  // Architecture-dependent fastcall
fn fast_func() -> void { ... }
```

---

## 7. The Safety Boundary Table

| Feature | Syntax | Safety Level | Rationale |
|---------|--------|-------------|-----------|
| `&volatile T` | `let r: &volatile u32 = ...` | **Safe syntax, unsafe semantics** | Volatility is explicit in the type. You cannot deref without knowing it's volatile. Compiler emits exactly one access. |
| `#[atomic_*]` | `#[atomic_load(Acquire)] ptr.*` | **Safe syntax, programmer chooses ordering** | Wrong ordering is a logic bug (data race), not a safety violation. The compiler validates valid orderings but cannot know your synchronization strategy. |
| `asm {}` | `asm { in("eax") x, out("ebx") y, "cpuid" }` | **Safe syntax, unsafe power** | The compiler validates register constraints, detects overlaps, and enforces clobber lists. The template string can do anything — that's the programmer's responsibility. |
| `*T` raw pointer | `*ptr` | **Requires `unsafe` block** | Unrestricted memory access. Dereference requires `unsafe { ... }` or `unsafe fn`. |
| `#[naked]` | `#[naked] unsafe fn ...` | **Requires `unsafe` function** | No stack frame, no prologue/epilogue. You're writing raw machine code entry points. |
| `#[interrupt]` | `#[interrupt] fn ...` | **Safe** | Compiler generates the interrupt frame save/restore. Programmer writes normal function body. |

---

## 8. IR Design: The Eight Mandatory Operations

Before any backend work proceeds, these IR primitives must exist. They are **not library calls** — they are first-class operations that the semantic analyzer generates and the backend recognizes.

```c
// === Volatile ===
IR_VOLATILE_LOAD   { ptr: IROperand, type: IRType }
IR_VOLATILE_STORE  { ptr: IROperand, val: IROperand, type: IRType }

// === Atomics ===
IR_ATOMIC_LOAD     { ptr: IROperand, type: IRType, order: MemoryOrder }
IR_ATOMIC_STORE    { ptr: IROperand, val: IROperand, type: IRType, order: MemoryOrder }
IR_ATOMIC_RMW      { ptr: IROperand, op: RMWOp, val: IROperand, type: IRType, order: MemoryOrder }
IR_ATOMIC_CMPXCHG  { ptr: IROperand, expected: IROperand, desired: IROperand, 
                     type: IRType, succ_ord: MemoryOrder, fail_ord: MemoryOrder }
IR_FENCE           { order: MemoryOrder }

// === Inline Assembly ===
IR_ASM_BLOCK       { template: StringView, inputs: AsmConstraint[], 
                     outputs: AsmConstraint[], clobbers: StringView[], 
                     options: AsmOptions }
```

### 8.1 C Bootstrap Lowering Map

| IR Op | C11 Emission | Header Needed |
|-------|-------------|---------------|
| `IR_VOLATILE_LOAD` | `*((volatile T*)ptr)` | `<stdatomic.h>` for types |
| `IR_VOLATILE_STORE` | `*((volatile T*)ptr) = val` | — |
| `IR_ATOMIC_LOAD` | `__atomic_load_n(ptr, ORDER)` | — |
| `IR_ATOMIC_STORE` | `__atomic_store_n(ptr, val, ORDER)` | — |
| `IR_ATOMIC_RMW` | `__atomic_fetch_op(ptr, val, ORDER)` | — |
| `IR_ATOMIC_CMPXCHG` | `__atomic_compare_exchange_n(ptr, &exp, des, false, succ, fail)` | — |
| `IR_FENCE` | `__atomic_thread_fence(ORDER)` | — |
| `IR_ASM_BLOCK` | `__asm__ __volatile__(template : outputs : inputs : clobbers)` | — |

### 8.2 LLVM Lowering Map

| IR Op | LLVM Instruction |
|-------|-----------------|
| `IR_VOLATILE_LOAD` | `load volatile <type>, <type>* <ptr>` |
| `IR_VOLATILE_STORE` | `store volatile <val>, <type>* <ptr>` |
| `IR_ATOMIC_LOAD` | `load atomic <type>, <type>* <ptr> <ordering>, align <N>` |
| `IR_ATOMIC_STORE` | `store atomic <val>, <type>* <ptr> <ordering>, align <N>` |
| `IR_ATOMIC_RMW` | `atomicrmw <op> <type>* <ptr>, <type> <val> <ordering>` |
| `IR_ATOMIC_CMPXCHG` | `cmpxchg <type>* <ptr>, <type> <exp>, <type> <des> <succ> <fail>` |
| `IR_FENCE` | `fence <ordering>` |
| `IR_ASM_BLOCK` | `call <ty> asm <sideeffect> "<template>", "<constraints>" (...)` |

---

## 9. Implementation Roadmap

### Phase A: Semantic Foundation (Now — Week 1-2)

**Goal:** The parser and sema understand the syntax. The IR has the ops. C emission is stubbed.

| Task | Owner | Deliverable |
|------|-------|-------------|
| Add `&volatile T` to type system | Compiler | `SType` variant, parser rule, sema coercion |
| Add `#[volatile]` expression attribute | Compiler | AST node, sema validation |
| Add `#[atomic_*]` expression attributes | Compiler | AST nodes, ordering validation |
| Add `MemoryOrder` enum to builtin types | Compiler | Recognized in sema, comptime-evaluable |
| Add `AsmBlock` AST node | Compiler | Full structured asm parsing |
| Add 8 IR primitives | Compiler | IR opcodes, C emission stubs |
| Add linkage/section attributes to decls | Compiler | `#[section]`, `#[no_mangle]`, `#[weak]`, `#[used]` |
| Liar compiler tests | Testing | Parse + sema only, no codegen |

### Phase B: C Bootstrap Validation (Week 3-6)

**Goal:** Real C code is emitted. We compile a kernel module.

| Task | Owner | Deliverable |
|------|-------|-------------|
| C emission for all 8 IR ops | Backend | `__atomic_*`, `volatile`, `__asm__` |
| Kernel PoC: MMIO read/write | Driver | `#[volatile]` reads a device ID register |
| Kernel PoC: Spinlock | Driver | `#[atomic_rmw]` implements `spin_lock()` / `spin_unlock()` |
| Kernel PoC: Inline asm barrier | Driver | `#[compiler_fence]` or `asm { "mfence" }` |
| Kbuild integration | Build | `.raya` → `.c` → `.o` → `.ko` |
| 10+ integration tests | Testing | Kernel modules that load and pass `insmod` |

### Phase C: LLVM Backend (Month 3-6)

**Goal:** Flip the switch. Same Raya source, LLVM backend, identical runtime behavior.

| Task | Owner | Deliverable |
|------|-------|-------------|
| LLVM IR emission for 8 ops | Backend | `load atomic`, `atomicrmw`, `cmpxchg`, `fence`, `call asm` |
| Register constraint validation | Backend | Map Raya constraints to LLVM constraint strings |
| Debug info for asm blocks | Backend | Source location mapping through inline asm |
| Feature parity tests | Testing | All Phase B tests pass with LLVM backend |

---

## 10. Open Questions & Decisions

| Question | Recommendation | Status |
|----------|---------------|--------|
| `&volatile T` → `&T` coercion? | **Forbidden.** Volatility is never silently discarded. | Decided |
| Default memory ordering for atomics? | **None.** Mandatory explicit ordering. | Decided |
| `asm` template syntax? | String literal only. No string interpolation (security). | Decided |
| `#[interrupt]` on which architectures? | x86_64 first (IRET), AArch64 later (ERET). | TBD |
| `#[naked]` + non-asm body? | **ERROR.** Naked functions must contain exactly one `asm` block. | Decided |
| Should `#[atomic_*]` work on `*T`? | **No.** Atomics require `&T` or `&volatile T`. Raw pointers need `unsafe` + explicit C interop. | Decided |
| `#[compiler_fence]` as attribute or builtin? | Attribute on statement: `#[compiler_fence];` | Decided |

---

## 11. Testing Strategy

### 11.1 Sema Tests (Negative — Must Fail)

```raya
// test: atomic_store_acquire_invalid.raya
fn bad() -> void {
    let ptr: &u32 = &x;
    #[atomic_store(Acquire)] ptr.* = 1;  // ERROR: Acquire not valid for stores
}
```

```raya
// test: volatile_downgrade_invalid.raya
fn bad() -> void {
    let v: &volatile u32 = &mmio;
    let p: &u32 = v;  // ERROR: cannot discard volatile qualifier
}
```

```raya
// test: asm_clobber_overlap.raya
fn bad() -> void {
    asm {
        in("eax") x,
        clobber("eax"),   // ERROR: register "eax" is already an input
        "nop"
    };
}
```

### 11.2 Integration Tests (Positive — Must Pass)

```raya
// test: mmio_volatile_read.raya
// Compile to .o, link with C test harness, verify exact load count
fn read_device_id(base: &volatile u32) -> u32 {
    return base.*;  // Must emit exactly one load
}
```

```raya
// test: atomic_spinlock.raya
// Compile, run in multi-threaded C harness, verify mutual exclusion
fn spin_lock(lock: &u32) -> void {
    while (#[atomic_rmw(Xchg, Acquire)] lock.* == 1) {
        #[atomic_load(Relaxed)] lock.*;  // Spin
    }
}

fn spin_unlock(lock: &u32) -> void {
    #[atomic_store(Release)] lock.* = 0;
}
```

---

## 12. Summary

This spec transforms Raya from a "C-like language with nice syntax" into a **kernel-capable systems language.** The decisions here are irreversible — once we commit to a memory model or asm syntax, changing it breaks all kernel code written in Raya.

The `#[]` attribute system, combined with comptime evaluation, gives us a clean extensible surface. The eight IR primitives give the backend a concrete target. The safety boundary table makes explicit what the compiler guarantees and what the programmer owns.

**Next action:** Implement `&volatile T` in the type system and add the `IR_VOLATILE_LOAD` / `IR_VOLATILE_STORE` ops. That's the smallest vertical slice that proves the pipeline works end-to-end.

---

> *"Trust the programmer, but give them better tools."* — Raya Philosophy
