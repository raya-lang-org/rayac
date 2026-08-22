# Raya

    A systems programming language that respects the programmer.

raya

module main;
```
  fn main() -> void {
     const msg: []const u8 = "Hello, Raya";
     std.io.print(msg);
 }

```
What is Raya?
Raya is a C-like systems language with modern conveniences. It gives you:

    Explicit control over memory, types, and calling conventions
    Zero-cost abstractions — what you write is what runs
    Null safety without hidden allocations (?&T is just a null pointer)
    Bounds-checked slices as first-class fat pointers
    Compile-time metaprogramming (comptime) for zero-overhead generics
    Traits for polymorphism without hidden vtables on every object

Raya does not enforce memory safety through a borrow checker. It trusts the programmer — like C — but provides better tools to avoid the common mistakes.
Quick Example
raya
```
module main;

struct Vec3 {
    x: f32,
    y: f32,
    z: f32,
}

extend Vec3 {
    pub fn length(self: &const Vec3) -> f32 {
        return std.math.sqrt(self.x * self.x + self.y * self.y + self.z * self.z);
    }
}

fn main() -> void {
    const v = Vec3{ x: 1.0, y: 2.0, z: 3.0 };
    const len = v.length();
    std.io.print_f32(len);
}
```
Primitive Data Types
Raya has no hidden complexity in its type system. Every primitive has a fixed size and predictable layout.
Integers
Table
Type	Size	Signed	Min	Max
i8	1 byte	Yes	-128	127
i16	2 bytes	Yes	-32,768	32,767
i32	4 bytes	Yes	-2³¹	2³¹-1
i64	8 bytes	Yes	-2⁶³	2⁶³-1
i128	16 bytes	Yes	-2¹²⁷	2¹²⁷-1
isize	pointer-sized	Yes	arch-dependent	arch-dependent
u8	1 byte	No	0	255
u16	2 bytes	No	0	65,535
u32	4 bytes	No	0	4,294,967,295
u64	8 bytes	No	0	2⁶⁴-1
u128	16 bytes	No	0	2¹²⁸-1
usize	pointer-sized	No	0	arch-dependent

    No implicit widening. u8 to u32 requires an explicit cast: x as u32.
    Checked in debug builds, wrapping in release builds.

Floating Point
Table
Type	Size	IEEE 754
f32	4 bytes	binary32
f64	8 bytes	binary64 (default)
Other Primitives
Table
Type	Size	Values
bool	1 byte	true, false
void	0 bytes	No value
noreturn	—	Function never returns

    No char type. Character literals yield u8.
    No string type. String literals yield []const u8.

Pointer Taxonomy
Raya has eight pointer-like types, each with clear semantics. No guesswork.
Table
Syntax	Null?	Size	Auto-deref?	Arithmetic?	Deref Safety
&T	No	sizeof(usize)	Yes	No	Safe
&const T	No	sizeof(usize)	Yes	No	Safe (read-only)
?&T	Yes	sizeof(usize)	Yes	No	Safe (null-checked)
[]T	No (empty ok)	2 × sizeof(usize)	No	No	Bounds-checked
[]const T	No (empty ok)	2 × sizeof(usize)	No	No	Bounds-checked (read-only)
*T	Yes	sizeof(usize)	No	Yes (scaled)	unsafe required
*const T	Yes	sizeof(usize)	No	Yes (scaled)	unsafe required
?*T	Yes	sizeof(usize)	No	Yes (scaled)	unsafe required
References &T
Non-null, safe pointers. Used for function parameters and struct fields.
raya
```
fn add_one(r: &i32) -> void {
    r.* = r.* + 1;   // auto-deref: same as (*r) = (*r) + 1
}

const x: i32 = 5;
var r: &i32 = &x;
r = &y;            // rebindable — not a C++ reference

Slices []T
Fat pointers: { ptr: *T, len: usize }. Bounds-checked on every index.
raya

fn sum(arr: []const i32) -> i32 {
    var total: i32 = 0;
    for i: usize in 0..arr.len {
        total += arr[i];   // bounds check: panic in debug, UB in release-fast
    }
    return total;
}

const buf: [5]i32 = [5]i32{ 1, 2, 3, 4, 5 };
const slice = buf[0..5];   // sub-slicing with bounds check

Raw Pointers *T
C-compatible, unrestricted. Dereference requires unsafe.
raya

fn read_mmio(addr: *u32) -> u32 {
    return unsafe { *addr };   // must be inside unsafe block
}

const ptr: *u8 = raw_ptr;
const next = ptr + 1;         // scaled by sizeof(u8) = 1 byte
const word_ptr = ptr as *u32; // pointer cast

Optional References ?&T
Zero-cost optional. The null bit is the zero address — no extra tag.
raya

fn maybe_find(arr: []i32, target: i32) -> ?&i32 {
    for i: usize in 0..arr.len {
        if arr[i] == target {
            return &arr[i];
        }
    }
    return null;
}

const result = maybe_find(arr, 42);
if const r = result {
    std.io.print_i32(r.*);   // r is &i32 inside this block
}

Syntax at a Glance
Variables
raya

const x: i32 = 5;      // immutable, type explicit
const y = 10;          // immutable, type inferred
var z: i32 = 0;        // mutable
var w = 3.14;          // mutable, inferred as f64

Functions
raya

fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

pub fn sub(a: i32, b: f32) -> i32 {
    return a - b as i32;
}

// No return type = void
fn greet(name: []const u8) {
    std.io.print(name);
}

// Unsafe function
unsafe fn raw_deref(ptr: *i32) -> i32 {
    return *ptr;
}

Control Flow
raya

// if — no parentheses around condition
if x > 0 {
    std.io.print("positive");
} else if x < 0 {
    std.io.print("negative");
} else {
    std.io.print("zero");
}

// while
while running {
    process();
}

// for — iterate over range or slice
for i: usize in 0..10 {
    std.io.print_usize(i);
}

// match
match color {
    .R => std.io.print("Red"),
    .G => std.io.print("Green"),
    .B => std.io.print("Blue"),
    _ => std.io.print("Unknown"),
}

Structs and Methods
raya

struct Player {
    name: []const u8,
    health: f32,
}

extend Player {
    pub fn new() -> Self {
        return Self { name: "Unknown", health: 100.0 };
    }

    pub fn take_damage(self: &Self, amount: f32) -> void {
        self.health -= amount;
    }
}

Error Handling
raya

fn might_fail() -> !i32 {
    if something_wrong {
        return error.BadInput;
    }
    return 42;
}

const x = try might_fail();           // propagates error upward
const y = might_fail() else |err| {   // capture error
    std.io.print("failed!");
    return;
};

Defer
raya

fn process_file(path: []const u8) -> void {
    const f = open_file(path);
    defer close_file(f);              // runs when block exits

    if error_condition {
        return;                       // close_file(f) runs here
    }
    read_data(f);
}                                     // close_file(f) runs here too
```
Building the Compiler
bash

# Clone the repository
git clone https://github.com/raya-lang-org/rayac.git
cd rayac

# Build
make

# Run tests
make test

# Run a specific file
./bin/raya --check myfile.raya

Requirements:

    C11 compiler (GCC, Clang, MSVC)
    Make
    No external dependencies

Project Status
Table
Phase	Status
Lexer	✅ Complete
Parser	✅ Complete
Semantic Analyzer	🚧 In Progress
Backend	📋 Planned
Standard Library	📋 Planned
See PROCESS.md for the full development roadmap.
Philosophy

    "Trust the programmer, but give them better tools."

Raya does not pretend to solve memory safety at compile time. Instead, it:

    Makes dangerous operations explicit (unsafe, raw pointers)
    Makes common mistakes impossible (null references, unchecked array access in debug)
    Keeps the mental model simple — a pointer is an address, a struct is bytes in memory
    Respects C ABI compatibility so you can call C and be called by C

License
MIT / Apache-2.0 dual license. See LICENSE for details.
Raya — light, fast, and honest.
