# Raya Compiler Sanitizer Instrumentation Specification
## v0.1 — Memory, Arena, Undefined Behavior

**Status:** Design Ready for Implementation  
**Target:** Phase 4.5 Sprint 2 (see PROCESS_delta.md)  
**Philosophy:** Sanitizers are compiler modes, not libraries. They catch bugs in `make test`, not in production.

---

## 1. Overview

Raya sanitizers are **compiler instrumentation passes** that insert runtime checks into the generated code. They are analogous to:
- **KASAN** (Kernel Address Sanitizer) — memory errors
- **KCSAN** (Kernel Concurrency Sanitizer) — data races
- **UBSan** — undefined behavior

Unlike opt-in library allocators, sanitizers are enabled via compiler flags and apply to **all code** in the compilation unit.

---

## 2. Sanitizer Flags

```bash
# Development / CI
rayac --sanitize=memory    main.raya   # Shadow memory for UAF, double-free, overflow
rayac --sanitize=arena     main.raya   # Arena lifetime tracking
rayac --sanitize=undefined main.raya   # Integer overflow, shift checks, alignment
rayac --sanitize=thread    main.raya   # Data race detection (future)

# Production
rayac -O3 --release-safe main.raya   # Defined panics on bounds/overflow
rayac -O3 --release-fast main.raya     # Zero overhead. Zero checks. UB on failure.
```

**Flag combinations:**
- `--sanitize=*` implies `--debug` (full stack traces, no optimization)
- `--release-safe` can be combined with `--sanitize=memory` for extra debugging
- `--release-fast` ignores all sanitizer flags

---

## 3. Memory Sanitizer (`--sanitize=memory`)

### 3.1 Shadow Memory Design

Inspired by KASAN. Every 8 bytes of application memory has 1 byte of shadow memory:

| Shadow Value | Meaning |
|-------------|---------|
| 0x00 | All 8 bytes are accessible |
| 0x01-0x07 | Only first N bytes are accessible (e.g., 0x03 = first 3 bytes) |
| 0xFF | All 8 bytes are poisoned (unallocated/freed) |

**Shadow memory layout:**
```
App memory:    0x1000 - 0x1FFF  (4KB page)
Shadow memory: 0x7FFF1000 - 0x7FFF1200  (512 bytes = 4KB / 8)
```

### 3.2 Instrumentation Points

#### A. Stack Variables

```raya
fn example() -> void {
    var x: i32 = undefined;   // 4 bytes uninitialized
    // ...
    const y = x + 1;         // ERROR: reading undefined
}
```

**Instrumentation (C output):**
```c
void example(void) {
    int32_t x;
    raya_msan_poison(&x, sizeof(x), "x", "main.raya", 42);
    // ...
    raya_msan_check(&x, sizeof(x), "main.raya", 45);  // before read
    int32_t y = x + 1;
}
```

#### B. Heap Allocations (Arena)

```raya
var arena = Arena.init(null);
const buf = arena.alloc(u8, 64);
// buf[0..63] are marked LIVE
arena.reset();
// buf[0..63] are marked DEAD
buf[0] = 1;  // ERROR: use-after-free
```

**Instrumentation:**
```c
RayaArena arena;
raya_arena_init(&arena, 65536);
uint8_t *buf = raya_arena_alloc(&arena, 64, 1);
raya_msan_mark_live(buf, 64, "main.raya", 10);   // after alloc

raya_arena_reset(&arena);
raya_msan_mark_dead(buf, 64, "main.raya", 12);   // after reset

raya_msan_check(buf, 1, "main.raya", 14);        // before store
buf[0] = 1;
```

#### C. Raw Pointer Dereferences

```raya
unsafe fn raw_access(ptr: *u8) -> u8 {
    return unsafe { *ptr };   // Check even in unsafe block!
}
```

**Instrumentation:**
```c
uint8_t raw_access(uint8_t *ptr) {
    raya_msan_check(ptr, 1, "main.raya", 20);  // MSAN checks even unsafe blocks
    return *ptr;
}
```

> **Rule:** Sanitizers instrument `unsafe` blocks too. Never trust the programmer in debug mode.

#### D. Slices

```raya
fn slice_access(s: []u8, idx: usize) -> u8 {
    return s[idx];   // Bounds check + MSAN check
}
```

**Instrumentation:**
```c
uint8_t slice_access(raya_Slice s, size_t idx) {
    raya_bounds_check(idx, s.len, "main.raya", 25);
    raya_msan_check((uint8_t*)s.ptr + idx, 1, "main.raya", 25);
    return ((uint8_t*)s.ptr)[idx];
}
```

### 3.3 Runtime Functions

```c
// raya_rt.h — Memory Sanitizer API

// Mark memory as accessible (live)
void raya_msan_mark_live(void *ptr, size_t size, const char *file, size_t line);

// Mark memory as inaccessible (dead/poisoned)
void raya_msan_mark_dead(void *ptr, size_t size, const char *file, size_t line);

// Mark memory as partially accessible (for struct padding)
void raya_msan_mark_partial(void *ptr, size_t accessible_bytes, const char *file, size_t line);

// Check if memory is accessible before read/write
void raya_msan_check(void *ptr, size_t size, const char *file, size_t line);

// Poison a region (for uninitialized stack variables)
void raya_msan_poison(void *ptr, size_t size, const char *name, const char *file, size_t line);
```

### 3.4 Panic Output

```
panic: use-after-free at driver.raya:142
  accessed: 0x7fff2000 (1 bytes)
  allocation: 0x7fff2000 - 0x7fff2040 (64 bytes)
  allocated at driver.raya:100
  freed at driver.raya:138
  shadow: 0xFF (all bytes poisoned)
```

---

## 4. Arena Sanitizer (`--sanitize=arena`)

### 4.1 Design

Tracks arena-allocated memory separately from general heap. Focuses on:
- Use-after-arena-free (access after `arena.reset()` or `arena.deinit()`)
- Arena escape (returning `&T` into arena memory past arena's lifetime)
- Double-reset (calling `arena.reset()` twice without intervening allocation)

### 4.2 Arena Tracking Table

```c
typedef struct RayaArenaTrack {
    void *arena_id;           // Unique arena identifier
    void *ptr;                // Allocation start
    size_t size;              // Allocation size
    const char *alloc_file;   // Where allocated
    size_t alloc_line;
    bool is_live;             // Current state
    struct RayaArenaTrack *next;
} RayaArenaTrack;
```

### 4.3 Instrumentation Points

#### A. Arena Allocation

```raya
const buf = arena.alloc(u8, 64);
```
↓
```c
uint8_t *buf = raya_arena_alloc(arena._impl, 64, 1);
raya_asan_track(arena._impl, buf, 64, "main.raya", 10);
```

#### B. Arena Reset

```raya
arena.reset();
```
↓
```c
raya_arena_reset(arena._impl);
raya_asan_reset(arena._impl, "main.raya", 15);  // marks all allocations DEAD
```

#### C. Access to Arena Memory

```raya
buf[0] = 1;
```
↓
```c
raya_asan_check(arena._impl, buf, 1, "main.raya", 20);
buf[0] = 1;
```

#### D. Reference Escape Detection

```raya
fn bad(arena: &Arena) -> &u8 {
    const buf = arena.alloc(u8, 1);
    return &buf[0];   // ERROR: reference escapes arena scope
}
```

**Sema-level detection (not runtime):**
```c
// In sema.c: when checking return statements
if (expr_is_arena_allocated(s, return_expr)) {
    sema_report(s, return_expr->loc,
        "reference to arena-allocated memory escapes function scope. "
        "Arena will be destroyed when function returns.");
}
```

### 4.4 Runtime Functions

```c
void raya_asan_track(void *arena_id, void *ptr, size_t size, const char *file, size_t line);
void raya_asan_reset(void *arena_id, const char *file, size_t line);
void raya_asan_deinit(void *arena_id, const char *file, size_t line);
void raya_asan_check(void *arena_id, void *ptr, size_t size, const char *file, size_t line);
```

---

## 5. Undefined Behavior Sanitizer (`--sanitize=undefined`)

### 5.1 Checks

| Check | Raya Trigger | C Instrumentation |
|-------|-------------|-------------------|
| Integer overflow | `x + y` on signed ints | `raya_ubsan_add_overflow(x, y, file, line)` |
| Shift out of range | `x << y` where `y >= bitwidth` | `raya_ubsan_shift(x, y, bitwidth, file, line)` |
| Division by zero | `x / 0` | `raya_ubsan_divzero(x, file, line)` |
| Null reference | `*ref` where ref is null | `raya_ubsan_null(ref, file, line)` |
| Alignment violation | `*(unaligned_ptr)` | `raya_ubsan_align(ptr, align, file, line)` |
| Invalid enum value | `match` on invalid tag | `raya_ubsan_enum(val, max, file, line)` |

### 5.2 Integer Overflow Example

```raya
fn add(x: i32, y: i32) -> i32 {
    return x + y;   // May overflow
}
```

**Debug mode:**
```c
int32_t add(int32_t x, int32_t y) {
    int32_t result;
    if (__builtin_add_overflow(x, y, &result)) {
        raya_panic("main.raya", 10, "signed integer overflow");
    }
    return result;
}
```

**Release-safe mode:**
```c
int32_t add(int32_t x, int32_t y) {
    int32_t result;
    if (unlikely(__builtin_add_overflow(x, y, &result))) {
        raya_panic("main.raya", 10, "signed integer overflow");
    }
    return result;
}
```

**Release-fast mode:**
```c
int32_t add(int32_t x, int32_t y) {
    return x + y;  // UB on overflow. Zero cost.
}
```

### 5.3 Runtime Functions

```c
void raya_ubsan_add_overflow(int64_t a, int64_t b, const char *file, size_t line);
void raya_ubsan_shift(uint64_t val, uint64_t shift, uint8_t width, const char *file, size_t line);
void raya_ubsan_divzero(int64_t divisor, const char *file, size_t line);
void raya_ubsan_null(void *ptr, const char *file, size_t line);
void raya_ubsan_align(void *ptr, size_t align, const char *file, size_t line);
```

---

## 6. Codegen Integration

### 6.1 CGen Mode Flags

```c
typedef struct CGen {
    // ... existing fields ...
    CGenMode mode;              // MODE_DEBUG, MODE_RELEASE_SAFE, MODE_RELEASE_FAST
    bool sanitize_memory;       // --sanitize=memory
    bool sanitize_arena;        // --sanitize=arena
    bool sanitize_undefined;    // --sanitize=undefined
} CGen;
```

### 6.2 Emission Helpers

```c
static void cg_emit_sanitize_check(CGen *cg, const char *func, 
                                   const char *ptr_expr, size_t size,
                                   SourceLocation loc) {
    if (cg->mode == MODE_RELEASE_FAST) return;
    cg_indent(cg);
    fprintf(cg->out, "%s(%s, %zu, "%s", %lu);\n",
            func, ptr_expr, size, loc.filename, (unsigned long)loc.line);
}

static void cg_emit_bounds_check(CGen *cg, AstNode *idx_expr, AstNode *len_expr, 
                                  SourceLocation loc) {
    if (cg->mode == MODE_RELEASE_FAST) return;
    cg_indent(cg);
    if (cg->mode == MODE_DEBUG) {
        fprintf(cg->out, "if (");
        cg_emit_expr(cg, idx_expr);
        fprintf(cg->out, " >= ");
        cg_emit_expr(cg, len_expr);
        fprintf(cg->out, ") { raya_panic(\"%s\", %lu, "index out of bounds"); }\n",
                loc.filename, (unsigned long)loc.line);
    } else { // MODE_RELEASE_SAFE
        fprintf(cg->out, "if (unlikely(");
        cg_emit_expr(cg, idx_expr);
        fprintf(cg->out, " >= ");
        cg_emit_expr(cg, len_expr);
        fprintf(cg->out, ")) { raya_panic(\"%s\", %lu, "index out of bounds"); }\n",
                loc.filename, (unsigned long)loc.line);
    }
}
```

### 6.3 Instrumentation Decision Matrix

| Operation | Debug | Release-Safe | Release-Fast |
|-----------|-------|-------------|--------------|
| Slice index | Full bounds check + panic | `unlikely` bounds check + panic | Nothing |
| Array index | Full bounds check + panic | `unlikely` bounds check + panic | Nothing |
| Sub-slice | Bounds check on start/end | `unlikely` bounds check | Nothing |
| `*ptr` (raw deref) | MSAN check | Nothing | Nothing |
| `arr.ptr` | MSAN check on access | Nothing | Nothing |
| Integer `+` | Overflow check | `unlikely` overflow check | Nothing |
| Integer `<<` | Shift width check | `unlikely` shift check | Nothing |
| Division | Divzero check | `unlikely` divzero check | Nothing |
| Arena alloc | ASAN track | Nothing | Nothing |
| Arena reset | ASAN mark dead | Nothing | Nothing |
| Arena access | ASAN check | Nothing | Nothing |

---

## 7. Kernel Context Adaptations

In kernel builds (`--target=kernel` or `#[kernel_module]`):

| Feature | Userland | Kernel |
|---------|----------|--------|
| Panic handler | `abort()` → coredump | `oops()` / `BUG()` |
| Shadow memory | `mmap` anonymous pages | `vmalloc` or static array |
| Stack traces | `libbacktrace` | `dump_stack()` |
| Printf | `fprintf(stderr, ...)` | `printk(KERN_ERR ...)` |

**Kernel panic handler:**
```c
__attribute__((weak))
void raya_panic(const char *file, size_t line, const char *msg) {
    printk(KERN_ERR "raya panic at %s:%lu: %s\n", file, (unsigned long)line, msg);
    dump_stack();
    BUG();  // or panic() for fatal errors
}
```

**Kernel module override:**
```raya
#[no_mangle]
#[linkage("strong")]  // override weak default
fn raya_panic_handler(file: []const u8, line: usize, msg: []const u8) -> noreturn {
    printk("raya panic at %.*s:%zu: %.*s\n",
           file.len, file.ptr, line, msg.len, msg.ptr);
    dump_stack();
    BUG();
}
```

---

## 8. Performance Expectations

| Sanitizer | Runtime Overhead | Memory Overhead | Use Case |
|-----------|-----------------|-----------------|----------|
| `--sanitize=memory` | 2-3x | 2x (shadow memory) | CI, fuzzing, bug hunting |
| `--sanitize=arena` | 1.5-2x | 1.5x (tracking table) | CI, unit tests |
| `--sanitize=undefined` | 1.2-1.5x | Minimal | CI, always-on in debug |
| `--release-safe` | 1.05-1.1x | None | Production kernels, servers |
| `--release-fast` | 1.0x | None | Interrupt handlers, crypto |

---

## 9. Implementation Order

1. **Week 1:** `raya_panic()` + `raya_bounds_check()` + release modes
2. **Week 2:** `--sanitize=undefined` (integer overflow, shift, divzero)
3. **Week 3:** `--sanitize=arena` (arena tracking table)
4. **Week 4:** `--sanitize=memory` (shadow memory — hardest, most value)
5. **Week 5:** Kernel adaptations (panic handler override, printk integration)

---

## 10. Summary

Sanitizers are not optional tooling. They are the **foundation of Raya's debuggability promise.** A systems language that asks programmers to be perfect is just C with syntax sugar. A systems language that crashes immediately with a file:line on every bug is what C should have been.

The implementation is straightforward:
- Add flags to the driver
- Add runtime functions to `raya_rt.c`
- Add conditional emission to `codegen_c.c`
- Write tests that verify panics happen at the right lines

The hard part is the **discipline to keep them on by default** in debug builds and CI, and to never let `release-fast` become the default.

---

*This spec is ready for implementation. All functions, flags, and emission patterns are defined.*
