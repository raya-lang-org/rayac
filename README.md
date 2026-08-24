# Raya

A systems programming language that respects the programmer.

```raya
module main;

fn main() -> void {
    const msg: []const u8 = "Hello, Raya";
    std.io.print(msg);
}
```

## What Raya Is

C-like control. Modern tools. No hidden costs.

- **Explicit memory**: You choose arenas or raw pointers. No GC, no borrow checker, no hidden allocations.
- **Bounds-checked slices**: Fat pointers `[]T` with checks in debug and `release-safe`. Zero cost in `release-fast`.
- **Null-safe references**: `&T` is non-null. `?&T` is explicit optional. Zero overhead.
- **Error unions**: `E!T` with `try`, `try else |e|`, `errdefer`. No exceptions, no hidden unwinding.
- **Zero-cost abstractions**: `comptime`, traits, generics via monomorphization. What you write is what runs.
- **C ABI compatible**: Call C, be called by C. No marshalling.

## What Raya Is Not

- **Not memory-safe**: Use-after-free, double-free, and data races are still possible. Raya gives you tools to catch them in debug/CI, not a proof at compile time.
- **Not Rust**: No borrow checker. No ownership tracking. If you want that, use Rust.
- **Not a managed language**: No runtime. No garbage collector. You manage memory explicitly.

## The Deal

Raya trusts you to write the code. It does not trust you to test it.

| Mode | What it does | When to use |
|------|-------------|-------------|
| `debug` | Full checks, stack traces, abort on panic | Development, unit tests |
| `release-safe` | `unlikely` panic branches on bounds/overflow/null | Production kernels, drivers, servers |
| `release-fast` | Zero overhead. UB on failure. | Interrupt handlers, crypto inner loops |

In debug and CI, the compiler is your adversary. In production, you choose your tradeoff.

## Quick Tour

### Pointers: Eight Types, Zero Guesswork

| Syntax | Null? | Arithmetic? | Deref Safety |
|--------|-------|-------------|-------------|
| `&T` | No | No | Safe (auto-deref) |
| `&const T` | No | No | Safe (read-only) |
| `?&T` | Yes | No | Safe (null-checked) |
| `[]T` | No (empty ok) | No | Bounds-checked |
| `[]const T` | No (empty ok) | No | Bounds-checked |
| `*T` | Yes | Yes (scaled) | `unsafe` required |
| `*const T` | Yes | Yes (scaled) | `unsafe` required |
| `?*T` | Yes | Yes (scaled) | `unsafe` required |

### Error Handling

```raya
struct FileError { code: i32, msg: []const u8 }

fn read_file(path: []const u8) -> FileError![]u8 {
    if (path.len == 0) {
        return FileError{ code: 1, msg: "empty path" };
    }
    return buffer;
}

const data = try read_file("config.txt");
const data2 = try read_file("other.txt") else |e| {
    std.io.print(e.msg);
    return e;
};
```

### Arenas: The Default Way to Allocate

```raya
fn parse(arena: &Arena, input: []const u8) -> ParseError!Ast {
    const tokens = try arena.alloc(Token, 256);
    const ast = try arena.alloc(AstNode, 128);
    // ... parse ...
    return ast;
    // All memory freed when caller destroys the arena.
}
```

Individual `free()` is `unsafe`. Use arenas. Tear down by scope.

### `unsafe` Means Something

`unsafe` marks the **creation of danger**, not just its use:

```raya
const raw = unsafe { arr.ptr as *u8 };   // Manufacturing raw pointer: unsafe
unsafe { raw[10000] = 0xFF; }            // Using it: also unsafe
```

Safe code cannot manufacture raw pointers and pass them into `unsafe` blocks. The boundary is real.

## Building

```bash
git clone https://github.com/raya-lang-org/rayac.git
cd rayac
make
make test
./bin/raya --check myfile.raya
./bin/raya --build myfile.raya
```

## Project Status

| Component | Status |
|-----------|--------|
| Lexer | ✅ Complete |
| Parser | ✅ Complete |
| Semantic Analyzer | ✅ Core complete (types, methods, error unions, unsafe) |
| C Transpiler Backend | 🚧 Functional. `defer` works. `errdefer`, `match`, `try` lowering in progress. |
| Release Modes | 📋 Planned |
| Compiler Sanitizers | 📋 Planned |
| Standard Library (Arena, IO) | 📋 Planned |

## Philosophy

> "Trust the programmer, but give them better tools."

Raya does not pretend to solve memory safety at compile time. It makes dangerous operations explicit, common mistakes impossible in debug, and gives you compiler sanitizers that turn bugs into immediate crashes during `make test`.

This is what C should have been: a language that respects your expertise, but doesn't require you to be perfect.

---

MIT / Apache-2.0 dual license.
