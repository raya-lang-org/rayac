# Raya Standard Library: `std.mem.Arena`
## Sketch v0.1 — Arena-First Memory Model

**Status:** Design Ready for Implementation  
**Target:** Phase 4.5 Sprint 3 (see PROCESS_delta.md)  
**Philosophy:** Memory cleanup happens by scope/arena destruction. Individual `free()` is `unsafe`.

---

## 1. Module Interface

```raya
module std.mem;

// ============================================================
// Arena — Region-based allocator
// ============================================================

pub struct Arena {
    // Opaque handle. Implementation is platform-specific.
    // In C bootstrap: wraps a bump allocator or malloc chain.
    // In kernel: wraps a kmem_cache or page allocator.
    _impl: *void,
}

// Initialize an arena with a backing allocator.
// If no allocator is provided, uses the platform default (malloc in userland,
// page allocator in kernel context).
pub fn Arena.init(backing: ?&Allocator) -> Arena {
    // Platform-specific initialization
    return Arena { _impl: arena_impl_init(backing) };
}

// Allocate `count` items of type `T`.
// Returns a slice `[]T` backed by arena memory.
// Panics (in debug/safe) if allocation fails. No null return.
pub fn Arena.alloc(self: &Arena, comptime T: type, count: usize) -> []T {
    const bytes = count * @size_of(T);
    const ptr = arena_impl_alloc(self._impl, bytes, @align_of(T));
    // In debug/sanitize=arena mode: mark storage as LIVE_OBJECT
    return @ptr_to_slice(ptr, count);
}

// Allocate a single item of type `T`.
// Returns `&T` — non-null, safe reference.
pub fn Arena.alloc_one(self: &Arena, comptime T: type) -> &T {
    const slice = self.alloc(T, 1);
    return &slice[0];
}

// Reset the arena to empty.
// All previously allocated memory becomes invalid.
// In debug/sanitize=arena mode: marks all allocations as DEAD_OBJECT.
// In release-fast: zero cost (just reset bump pointer).
pub fn Arena.reset(self: &Arena) -> void {
    arena_impl_reset(self._impl);
}

// Deinitialize the arena and release all backing memory.
// After this call, the arena must not be used.
pub fn Arena.deinit(self: &Arena) -> void {
    arena_impl_deinit(self._impl);
    self._impl = null;
}

// ============================================================
// Allocator interface (for custom backing allocators)
// ============================================================

pub struct Allocator {
    alloc_fn: fn(*void, usize, usize) -> *void,   // ctx, size, align
    free_fn:  fn(*void, *void) -> void,            // ctx, ptr
    ctx:      *void,
}

// ============================================================
// Unsafe: Individual deallocation
// ============================================================

// Free a single pointer allocated from ANY source.
// This is `unsafe` because the compiler cannot verify:
// - The pointer was actually allocated
// - The pointer has not already been freed
// - No other references to the same memory exist
unsafe fn free_heap(ptr: *void) -> void {
    platform_free(ptr);
}

// Reallocate a slice. May move data.
// Returns new slice. Old slice is invalidated.
unsafe fn realloc_heap(old: []void, new_len: usize) -> []void {
    // ...
}
```

---

## 2. Usage Patterns

### Pattern A: Function-Scoped Arena (Most Common)

```raya
fn parse_config(path: []const u8) -> ParseError!Config {
    var arena = Arena.init(null);   // default backing allocator
    defer arena.deinit();            // cleanup when function returns

    const text = try read_file(path);
    const tokens = try tokenize(&arena, text);
    const ast = try parse(&arena, tokens);
    const cfg = try eval_config(&arena, ast);

    // cfg and all intermediate allocations live in `arena`
    // When function returns (success or error), defer fires and frees everything.
    return cfg;
}
```

**Why this works:**
- No `errdefer` needed for memory cleanup
- No individual `free()` calls
- No double-free possible
- No use-after-free of intermediate data (it's all freed together at the end)

### Pattern B: Request-Scoped Arena (Server/Kernel)

```raya
fn handle_request(req: &Request) -> Response {
    var arena = Arena.init(&page_allocator);
    defer arena.deinit();

    const parsed = try parse_request(&arena, req);
    const result = try process(&arena, parsed);
    return try serialize_response(&arena, result);
    // All request-specific memory freed here
}
```

### Pattern C: Nested Arenas

```raya
fn outer() -> void {
    var outer_arena = Arena.init(null);
    defer outer_arena.deinit();

    const data = outer_arena.alloc(u8, 1024);

    {
        var inner_arena = Arena.init(&outer_arena);  // inner backed by outer
        defer inner_arena.deinit();

        const temp = inner_arena.alloc(u8, 256);
        // temp freed when inner_arena deinits
    }

    // data still valid here
}
```

### Pattern D: The Exception — Individual Free

```raya
// Only when you MUST interoperate with C APIs that take ownership
unsafe fn transfer_to_c(arena: &Arena, data: &[]u8) -> *u8 {
    const raw = data.ptr;       // requires unsafe: manufacturing raw pointer
    // Detach from arena tracking (in sanitize mode)
    arena_impl_detach(arena._impl, raw);
    return raw;
}
```

---

## 3. C Bootstrap Lowering

### Arena Implementation (C11)

```c
// raya_rt.h additions

typedef struct RayaArenaBlock {
    struct RayaArenaBlock *next;
    size_t used;
    size_t capacity;
    alignas(16) char data[];
} RayaArenaBlock;

typedef struct RayaArena {
    RayaArenaBlock *current;
    RayaArenaBlock *head;
    size_t block_size;
} RayaArena;

void raya_arena_init(RayaArena *a, size_t block_size);
void *raya_arena_alloc(RayaArena *a, size_t size, size_t align);
void raya_arena_reset(RayaArena *a);
void raya_arena_deinit(RayaArena *a);
```

### Raya → C Mapping

```raya
var arena = Arena.init(null);
```
↓
```c
RayaArena arena;
raya_arena_init(&arena, 65536);
```

```raya
const buf = arena.alloc(u8, 1024);
```
↓
```c
uint8_t *buf = raya_arena_alloc(&arena, 1024, 1);
// In debug/sanitize mode, also: raya_sanitize_mark_live(buf, 1024);
```

```raya
defer arena.deinit();
```
↓
```c
// At block exit:
raya_arena_deinit(&arena);
```

---

## 4. Sanitizer Integration

### `--sanitize=arena` Mode

In this mode, the arena tracks every allocation:

```c
typedef struct RayaArenaSanitizeEntry {
    void *ptr;
    size_t size;
    const char *file;
    size_t line;
    bool is_live;
} RayaArenaSanitizeEntry;

void raya_sanitize_arena_alloc(void *ptr, size_t size, const char *file, size_t line);
void raya_sanitize_arena_free(void *ptr);           // mark DEAD
void raya_sanitize_arena_check(void *ptr, size_t access_size, const char *file, size_t line);
```

**Instrumentation points in codegen:**

| Raya Operation | Sanitizer Call |
|---------------|----------------|
| `arena.alloc(T, n)` | `raya_sanitize_arena_alloc(ptr, n * sizeof(T), file, line)` |
| `arena.reset()` | `raya_sanitize_arena_free(all_entries)` |
| `arena.deinit()` | `raya_sanitize_arena_free(all_entries)` |
| `slice[i]` (arena-allocated) | `raya_sanitize_arena_check(slice.ptr + i * sizeof(T), sizeof(T), file, line)` |
| `*ref` (arena-allocated) | `raya_sanitize_arena_check(ref, sizeof(*ref), file, line)` |

**Panic message example:**
```
panic: use-after-free at main.raya:42
  allocation was freed by arena.reset() at main.raya:38
  allocated at main.raya:35
```

---

## 5. Kernel Context

In Linux kernel modules:

```raya
// Kernel module entry
const kernel_arena = Arena.init(&page_allocator);

fn init_module() -> i32 {
    const state = kernel_arena.alloc(DeviceState, 1);
    // state lives until module exit
    return 0;
}

fn exit_module() -> void {
    kernel_arena.deinit();  // frees all module state
}
```

**Kernel-specific allocator:**
```raya
pub const page_allocator = Allocator {
    alloc_fn: kernel_alloc_pages,
    free_fn:  kernel_free_pages,
    ctx:      null,
};

unsafe fn kernel_alloc_pages(ctx: *void, size: usize, align: usize) -> *void {
    return __get_free_pages(GFP_KERNEL, order);
}
```

---

## 6. Why No `std.heap.GeneralPurposeAllocator`?

Zig's GPA is excellent for userland. But Raya targets kernel and bare-metal contexts where:
- There is no `malloc` (or it's `kmalloc` with different semantics)
- Fragmentation is unacceptable in real-time paths
- Page granularity is the natural allocation unit

**Raya provides:**
- `Arena` — deterministic, no fragmentation, default
- `PageAllocator` — kernel-friendly, page-granularity
- `unsafe free_heap()` — escape hatch for C interop

**Raya does NOT provide:**
- A general-purpose heap allocator in safe code
- `malloc`/`free` as first-class operations
- Reference counting

---

## 7. Open Questions

1. **Should `Arena.alloc` return `[]T` or `?[]T`?**
   - Current sketch: returns `[]T`, panics on failure.
   - Alternative: return `?[]T`, let caller handle OOM.
   - **Recommendation:** Panic in debug/safe. In kernel context, panics are `BUG()`. Caller must pre-check capacity.

2. **Should arenas be `const` or `var`?**
   - `var arena = Arena.init(...)` — mutable, can reset/deinit
   - `const arena = Arena.init(...)` — immutable, can alloc but not reset
   - **Recommendation:** Both valid. `const` arena is "grow-only".

3. **How does `comptime` interact with arenas?**
   - `comptime` allocations should use a compiler-managed arena.
   - No user-visible `comptime Arena` needed.

---

*This sketch is ready for implementation. The C bootstrap runtime (`raya_rt.c`) needs the `RayaArena` struct and functions. The codegen backend needs to lower `Arena` method calls to these C functions.*
