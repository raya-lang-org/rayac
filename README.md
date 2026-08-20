# ⚡ Raya Compiler

**Raya** is a lightweight, high-performance systems programming language written in C. Designed with an emphasis on speed, simplicity, and explicit memory control, the compiler utilizes an **Arena-based allocation model** and a **Pratt Parser** architecture to deliver blazingly fast compilation pipelines.

---

## 🔑 Key Features

* **Zero-GC Arena Memory Architecture:** Linear memory allocation yields $O(1)$ allocations and instant teardowns via single-instruction arena resets.
* **Hand-crafted Lexer & Pratt Parser:** Highly optimized token recognition paired with Top-Down Operator Precedence parsing for flexible expression handling.
* **Rich Primitive & Built-in Types:**
  * Unsigned Integers (`u1` through `u65535`) & Signed Integers (`i1` through `i65535`)
  * Architecture-dependent Integers (`usize`, `isize`)
  * Floating-point precision (`f16`, `f32`, `f64`, `f128`)
  * Native Unicode (`u32`), Single Bytes (`u8`), Booleans, and Dynamic Slices (`[]const u8`)
* **First-Class Pattern Matching:** Expressive `match` constructs supporting wildcards, identifier bindings, and destructuring.

---

## 🛠 Project Structure

```text
.
├── src/
│   ├── main.c           # CLI Driver & Flag Parser
│   ├── lexer.c / .h     # Lexical Analyzer
│   ├── parser.c / .h    # Pratt Parser & AST Node Construction
│   ├── ast.c / .h       # Abstract Syntax Tree Data Structures
│   ├── arena.c / .h     # Low-overhead Arena Allocator
│   └── diag.c / .h      # Diagnostic Engine & Error Reporting
├── tests/               # Test Harness & Parser/Lexer Verification Cases
└── Makefile             # Compilation targets

```

---

## 🚀 Getting Started

### Prerequisites

* GCC / MinGW-w64 or Clang
* GNU Make

### Building from Source

To compile the `raya` binary:

```bash
make

```

The output binary will be compiled directly to `bin/raya`.

To clean build artifacts:

```bash
make clean

```

---

## 💻 Usage & CLI Flags

Compile or inspect a `.raya` source file:

```bash
bin/raya [options] <file.raya>

```

### Options

| Flag | Description |
| --- | --- |
| `--dump-tokens` | Print all lexed tokens with filename, line, and column coordinates |
| `--dump-ast` | Output the formatted Abstract Syntax Tree representation |
| `--expand` | Print source after compile-time evaluation |
| `-h`, `--help` | Display usage information |
| `-v`, `--version` | Display compiler version |

---

## 🧪 Running Tests

Raya includes an automated testing harness for verify lexer and parser correctness against gold standard expected outputs.

Run the test suite via `make`:

```bash
make test-parser

```

---

## 📜 Example Code


pub fn main() ->  void {
    var x: ?u32 = 42;
    var y: f64 = 3.14;

    if x == 42 {
        y = 0.0;
    } else {
        return;
    }
}

