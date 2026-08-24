# Raya Design Document: What C Should Have Been

**Status:** Draft v0.1  
**Target:** Systems programming with debuggability-first safety  
**Philosophy:** Trust the programmer to write the code. Don't trust them to test it.

---

## 1. Core Principles

These principles are non-negotiable. Every language feature must justify itself against this list.

| # | Principle | Rationale |
|---|-----------|-----------|
| 1 | **Observability > Prevention** | A sanitizer that crashes loudly in `make test` is more valuable than a borrow checker that rejects valid kernel code. Kernel maintainers debug panics, not type errors. |
| 2 | **No Hidden Global State** | No default heap. No implicit allocations. Every allocation site must be explicit: `arena.alloc()`, `page_alloc()`, `kmalloc()`. |
| 3 | **Zero-Cost Is Non-Negotiable** | Debug builds can instrument. Release-safe can panic. Release-fast must compile to the same assembly as C. No reference counting, no borrow-checker runtime. |
| 4 | **C Interop Must Be Mechanical** | Wrapping Linux headers must not require thunks, marshalling, or hidden costs. `unsafe` blocks around C calls are acceptable. Hidden costs are not. |
| 5 | **Arenas First, Individual Free Last** | The default mental model for memory is scope-bound arenas. Individual `free()` is an `unsafe` operation for exceptional cases. |
| 6 | **Lint the Obvious, Don't Ban the Necessary** | Self-referential structs and escaping references are sometimes required (device trees, early boot, IRQ contexts). Warn loudly, allow suppression. |

---

## 2. Memory Model: Arenas as the Default

### 2.1 The Problem with `free()`

Individual `free()` in error-prone paths is the #1 source of use-after-free and double-free bugs in systems code. In kernel development, you don't `kfree()` every `kmalloc()` — you tear down the arena, slab, or request context.

### 2.2 Arena-First Design

```raya
// The default, safe way to get memory
fn parse_packet(arena: &Arena, raw: []const u8) -> ParseError!Packet {
    const header = try arena.alloc<Header>(1);
    const payload = try arena.alloc<u8>(raw.len);
    // ... parse ...
    return Packet { header: header, payload: payload };
    // No errdefer needed. Memory is reclaimed when arena is destroyed.
}
```

### 2.3 Individual Deallocation is `unsafe`

```raya
// This is possible, but marked as dangerous
unsafe fn free_heap(ptr: *void) -> void {
    // platform-specific deallocation
}
```

**Rationale:** Making `free()` unsafe forces the programmer to acknowledge they are doing something the language does not track. It discourages the pattern that leads to UAF/double-free without adding ownership complexity.

### 2.4 Arena Safety in Debug Builds

In `--sanitize=arena` mode, the compiler instruments:
- Use-after-arena-free: accessing memory after `arena.reset()` or `arena.deinit()`
- Arena escape: returning `&T` into arena-allocated memory past the arena's lifetime
- Double-reset: calling `arena.reset()` twice without intervening allocation

---

## 3. `errdefer`: For OS Resources, Not Memory

### 3.1 The Double-Free Trap

Current Raya allows `errdefer free(buf)` to coexist with manual `free(buf)`. This is a double-free bomb:

```raya
// BROKEN — do not allow this pattern
fn broken() -> Error![]u8 {
    const buf = try alloc(1024);
    errdefer free(buf);    // registered
    // ... work ...
    free(buf);             // manual free on success path
    return something_else;
    // if an error happens after free(buf), errdefer fires again
}
```

### 3.2 `errdefer` Semantics

`errdefer` is for **non-memory resources** that must be cleaned up on error paths:
- File descriptors: `errdefer close(fd)`
- Locks: `errdefer spin_unlock(&lock)`
- Device state: `errdefer pci_disable_device(dev)`
- Kernel objects: `errdefer kobject_put(kobj)`

**Memory cleanup happens by scope/arena destruction.** `errdefer` must **not** accept memory deallocation expressions. Attempting `errdefer free(ptr)` is a compile-time error.

### 3.3 Nested `errdefer` Ordering

`errdefer` stacks execute in LIFO order (reverse of registration), matching C++ destructor semantics and kernel cleanup patterns.

```raya
fn open_device() -> Error!Device {
    const fd = try open("/dev/foo");
    errdefer close(fd);           // fires second

    const map = try mmap(fd);
    errdefer munmap(map);         // fires first

    return Device { fd: fd, map: map };
}
```

---

## 4. The `unsafe` Boundary

`unsafe` must mark the **creation of capability**, not just its use. If safe code can manufacture raw pointers and pass them into `unsafe` blocks, the keyword is theater.

### 4.1 Operations Requiring `unsafe`

| Operation | Requires `unsafe`? | Rationale |
|-----------|-------------------|-----------|
| Dereference `*T` | Yes | Raw pointer access is unchecked |
| Pointer arithmetic on `*T` | Yes | Can walk off allocated regions |
| Cast `&T` → `*T` | Yes | Discards safety guarantees |
| Cast `*T` → `&T` | Yes | Claims safety where none exists |
| Cast between pointer types | Yes | Type punning breaks aliasing |
| Call C function | No | FFI is normal systems work |
| Read C function pointer into `*T` | Yes | C code may return raw pointers |
| `free_heap()`, `realloc()` | Yes | Individual deallocation |

### 4.2 `unsafe` Blocks Must Be Greedy

The `unsafe` keyword should encompass the entire expression that creates danger, not just the final dereference:

```raya
// GOOD: the entire dangerous operation is marked
const raw = unsafe { arr.ptr as *u8 };
unsafe { raw[10000] = 0xFF; }

// BAD: safe code manufactures the weapon, unsafe only pulls the trigger
const raw: *u8 = arr.ptr;     // This should NOT be allowed outside unsafe
unsafe { raw[10000] = 0xFF; }
```

---

## 5. Sanitizers as Compiler Modes, Not Libraries

### 5.1 The Library Trap

Opt-in debug allocators (`std.heap.DebugAllocator`) fail because:
- Nobody enables them until after the CVE
- They can't understand language semantics (fat pointers, error unions, arena lifetimes)
- They add friction to the build system

### 5.2 Compiler Sanitizer Flags

Raya sanitizers are compiler instrumentation passes, analogous to KASAN/KCSAN in the Linux kernel:

```bash
# Development / CI
rayac --sanitize=memory main.raya      # Shadow memory for UAF, double-free, buffer overflow
rayac --sanitize=arena main.raya        # Arena lifetime tracking
rayac --sanitize=thread main.raya       # Data race detection (TSAN-like)
rayac --sanitize=undefined main.raya    # Integer overflow, shift checks, alignment

# Production kernel module
rayac -O3 --release-safe main.raya      # Defined panics on bounds/overflow

# Bare-metal interrupt handler
rayac -O3 --release-fast main.raya      # Zero overhead. Zero checks. UB on failure.
```

### 5.3 Sanitizer-Aware Semantics

The memory sanitizer must understand Raya's type system:
- `[]T` slices: instrument both `.ptr` and `.len` on every index
- `?&T` optionals: skip redundant null checks (zero address is already the null bit)
- `E!T` error unions: track payload liveness separately from error tag
- `unsafe` blocks: instrument them anyway. Never trust the programmer in debug mode.

---

## 6. Release Modes: Defined Behavior by Default

### 6.1 The Debug-Only Trap

Bounds checks that "panic in debug, UB in release-fast" are worse than no checks at all. They give false confidence:
- Tests pass in debug
- Production kernel panics (or silently corrupts memory)

### 6.2 Three Release Modes

| Mode | Bounds Check | Integer Overflow | Null Check | Use Case |
|------|-------------|------------------|------------|----------|
| `debug` | Panic + stack trace | Panic | Panic | Development, unit tests |
| `release-safe` | `unlikely` panic branch | `unlikely` panic branch | `unlikely` panic branch | Production kernels, drivers, servers |
| `release-fast` | UB | UB | UB | Interrupt handlers, crypto inner loops, benchmarks |

### 6.3 Panic Handler is User-Defined

In kernel context, "panic" means calling the kernel's `oops()` or `BUG()` mechanism, not `abort()`. Raya's panic handler must be replaceable:

```raya
// In kernel module entry point
@panic_handler = kernel_oops;
```

---

## 7. Struct Fields with `&T`: Allow, But Lint

### 7.1 The Kernel Reality

Self-referential and externally-referenced structs are unavoidable in kernel code:
- `list_head`, `hlist_node`, `rb_node` — all effectively self-referential
- Device trees with parent pointers
- IRQ handlers with references to device state

### 7.2 Lint Strategy

```raya
fn make_parser(data: []const u8) -> Parser {
    const p = Parser { input: &data };
    return p;    // WARN: address of parameter `data` escapes function scope
}
```

- **Default:** Emit a warning at compile time
- **Suppression:** `#[allow(escaping_reference)]` or similar attribute
- **Sanitizer:** In `--sanitize=memory`, instrument the escape and crash if the referenced stack frame is popped

This respects the programmer's knowledge ("I know the caller's frame outlives me") while catching the 95% case of accidental dangling references.

---

## 8. C Interop: Mechanical, Zero-Cost

### 8.1 Header Wrapping

Wrapping Linux kernel headers must not require generated thunks or marshalling layers:

```raya
// Direct C struct usage
const task_struct = @c_include("linux/sched.h");

fn get_current_pid() -> i32 {
    const t = @c_call("get_current")();   // returns struct task_struct*
    return t.pid;                          // direct field access
}
```

### 8.2 Calling Conventions

- Raya functions are C-ABI-compatible by default
- `extern "C"` is implicit for `pub fn` at module boundary
- `unsafe` is required when C functions return `*T` (see §4.1)

### 8.3 No Hidden Allocations in FFI

If a C function returns a string, Raya must not implicitly allocate a `[]u8` copy. The programmer must explicitly choose:
- `const s: *const u8 = c_function();` — zero-cost, unsafe to use
- `const s: []u8 = try arena.dupe(c_function());` — explicit copy into arena

---

## 9. Implementation Roadmap

### Phase 1: Foundation (Current — v0.8.x)
- [x] Lexer, Parser, Semantic Analyzer
- [x] `E!T` error unions (Phase 1 & 2 complete)
- [x] Pointer taxonomy (`&T`, `?&T`, `[]T`, `*T`)
- [ ] Bounds checks in codegen (⚠️ in progress)

### Phase 2: Safety Architecture (v0.9.x)
- [ ] Arena allocator in stdlib
- [ ] `errdefer` restricted to non-memory resources
- [ ] `unsafe` boundary enforcement (capability creation)
- [ ] `#[allow(...)]` attribute system for lint suppression
- [ ] Stack escape warnings

### Phase 3: Sanitizers (v0.10.x)
- [ ] `--sanitize=memory` (KASAN-style shadow memory)
- [ ] `--sanitize=arena` (arena lifetime tracking)
- [ ] `--sanitize=undefined` (integer overflow, alignment)
- [ ] Replaceable panic handler

### Phase 4: Production Modes (v0.11.x)
- [ ] `release-safe` mode with `unlikely` panic branches
- [ ] `release-fast` mode (zero overhead)
- [ ] Kernel panic handler integration
- [ ] C header import tool (`raya-bindgen`)

### Phase 5: Ecosystem (v1.0.x)
- [ ] Standard library (kernel-safe subset)
- [ ] Package manager with kernel module support
- [ ] Documentation generator
- [ ] Formal specification

---

## 10. Summary: The Contract with the Programmer

Raya does not promise memory safety. It promises **memory observability**.

| What Raya Gives You | What Raya Does Not Give You |
|---------------------|----------------------------|
| Bounds-checked slices that panic safely | Protection against use-after-free |
| Non-null references by default | Protection against data races |
| Explicit `unsafe` around all raw pointer ops | A borrow checker or ownership system |
| Compiler sanitizers that catch bugs in `make test` | Garbage collection or reference counting |
| Arena-first memory model | Hidden allocations or default heap |
| Defined panics in `release-safe` mode | Zero-cost safety guarantees |

**The deal:** Raya trusts you to write the code. It does not trust you to test it. In debug and CI, the compiler is your adversary — it instruments every dangerous operation and crashes immediately on violation. In production, you choose your tradeoff: defined panics (`release-safe`) or zero overhead (`release-fast`).

This is what C should have been: a language that respects the programmer's expertise, but doesn't require them to be perfect.

---

*Document maintained by the Raya core team. Last updated: 2026-08-24*
