# rayac


// ============================================================================
// RAYA PATH A — v0.7.6 Syntax Coverage Demo (Corrected)
// One file exercising every language construct.
// Fixes from v0.7.5 review:
//   - Slice syntax: []const u8 (not const []u8)
//   - void init: undefined (not {})
//   - Self parameter shorthand formally permitted
// ============================================================================

// ─── Module & Import ────────────────────────────────────────────────────────
module demo;

import std.io;
import std.mem as memory;

// ─── Type Alias ─────────────────────────────────────────────────────────────
type Byte = u8;
type Result = !i32;

// ─── Enum ───────────────────────────────────────────────────────────────────
enum Status {
    Ok,
    Err(i32),
    Unknown = 99,
}

// ─── Union ──────────────────────────────────────────────────────────────────
union Value {
    int_val: i64,
    float_val: f64,
    raw_ptr: *void,
}

// ─── Trait ──────────────────────────────────────────────────────────────────
traits Printable {
    pub fn print(&const self) -> void;
    pub fn format(&const self) -> []const u8;
}

// ─── Struct with Generics, Attributes, Bitfields ────────────────────────────
#[packed]
struct Register {
    enabled: u1,
    error_flag: u1,
    reserved: u6,
    status: u16,
}

#[align(64)]
struct Vec3(T: type) {
    x: T,
    y: T,
    z: T,
}

// ─── Inherent Methods (Generic Block) ───────────────────────────────────────
extend Vec3(T: type) {
    pub fn new(x: T, y: T, z: T) -> Self {
        return Self{ x: x, y: y, z: z };
    }

    pub fn magnitude(&const self) -> f64 {
        // ─── Block trailing expression ──────────────────────────────────────
        const dx = self.x as f64;
        const dy = self.y as f64;
        const dz = self.z as f64;
        (dx * dx + dy * dy + dz * dz) // trailing expr = block value
    }
}

// ─── Trait Implementation ───────────────────────────────────────────────────
extend Vec3(f32) with Printable {
    pub fn print(&const self) -> void {
        // method body
    }

    pub fn format(&const self) -> []const u8 {
        return "Vec3(f32)";
    }
}

// ─── Comptime Function ──────────────────────────────────────────────────────
comptime fn max_comptime(a: comptime_int, b: comptime_int) -> comptime_int {
    if a > b {
        a
    } else {
        b
    }
}

// ─── Unsafe Function ────────────────────────────────────────────────────────
unsafe fn raw_add(ptr: *i32, offset: usize) -> *i32 {
    return ptr + offset;
}

// ─── Function with Error Handling, Defer, Errdefer ──────────────────────────
pub fn process_data(buf: []const u8) -> !usize {
    // ─── Defer (block scope) ────────────────────────────────────────────────
    defer memory.release(buf);

    const fd = try open_file("/tmp/data");

    // ─── Errdefer (function scope, error path only) ─────────────────────────
    errdefer {
        close_file(fd);
        log_error("cleanup on error");
    }

    const handle = try wrap_handle(fd);

    // ─── While loop ─────────────────────────────────────────────────────────
    var idx: usize = 0;
    while idx < buf.len {
        if buf[idx] == 0 {
            break;
        }
        idx += 1;
    }

    // ─── For loop ───────────────────────────────────────────────────────────
    for ch: u8 in buf {
        if ch == '\n' {
            continue;
        }
    }

    return idx;
}

// ─── Noreturn Function ──────────────────────────────────────────────────────
pub fn panic(msg: []const u8) -> noreturn {
    unsafe {
        asm_halt();
    }
}

// ─── Main Function: Expression Kitchen Sink ─────────────────────────────────
pub fn main() -> !void {
    // ─── Var / Const declarations ───────────────────────────────────────────
    var count: i32 = 0;
    const limit: i32 = 10;

    // ─── All primitive types ────────────────────────────────────────────────
    var a_i8: i8 = 1;
    var a_i16: i16 = 2;
    var a_i32: i32 = 3;
    var a_i64: i64 = 4;
    var a_i128: i128 = 5;
    var a_isize: isize = 6;
    var a_u8: u8 = 7;
    var a_u16: u16 = 8;
    var a_u32: u32 = 9;
    var a_u64: u64 = 10;
    var a_u128: u128 = 11;
    var a_usize: usize = 12;
    var a_f32: f32 = 1.5;
    var a_f64: f64 = 2.5;
    var a_bool: bool = true;
    var a_void: void = undefined;  // CORRECTED: void has no literal {}

    // ─── Type constructors ──────────────────────────────────────────────────
    var r_mut: &i32 = &count;
    var r_const: &const i32 = &limit;
    var r_opt: ?&i32 = null;
    var slice_mut: []u8 = undefined;
    var slice_const: []const u8 = "hello";         // CORRECTED: []const u8
    var raw_mut: *i32 = undefined;
    var raw_const: *const i32 = undefined;
    var raw_opt: ?*i32 = null;
    var fixed: [4]u8 = [4]u8{ 1, 2, 3, 4 };
    var maybe: ?i32 = null;
    var result_val: !i32 = 42;

    // ─── Coercions ──────────────────────────────────────────────────────────
    var coerced_slice: []u8 = fixed;
    var coerced_const_slice: []const u8 = fixed;   // CORRECTED: []const u8

    // ─── Array repeat-fill ──────────────────────────────────────────────────
    var zeros: [1024]u8 = [1024]u8{ 0, };

    // ─── Struct literal ─────────────────────────────────────────────────────
    var v = Vec3(f32){ x: 1.0, y: 2.0, z: 3.0 };

    // ─── Field access, method call ──────────────────────────────────────────
    const mag = v.magnitude();
    const vx = v.x;

    // ─── Index, slice ───────────────────────────────────────────────────────
    const b0 = fixed[0];
    const sub = fixed[1..3];

    // ─── Address-of ─────────────────────────────────────────────────────────
    var ptr_to_count: &i32 = &count;
    var ptr_to_const: &const i32 = &limit;

    // ─── Dereference (unsafe) ───────────────────────────────────────────────
    unsafe {
        *raw_mut = 99;
        const deref_val = *raw_const;
    }

    // ─── Unary operators ────────────────────────────────────────────────────
    var neg_val = -a_i32;
    var not_val = !a_bool;
    var bit_not = ~a_u32;

    // ─── Binary operators ───────────────────────────────────────────────────
    var sum = a_i32 + limit;
    var diff = a_i32 - 1;
    var prod = a_i32 * 2;
    var quot = a_i32 / 2;
    var rem = a_i32 % 2;
    var shl = a_u32 << 1;
    var shr = a_u32 >> 1;
    var band = a_u32 & 0xFF;
    var bxor = a_u32 ^ 0xFF;
    var bor = a_u32 | 0xFF;
    var eq = a_i32 == limit;
    var ne = a_i32 != limit;
    var lt = a_i32 < limit;
    var gt = a_i32 > limit;
    var le = a_i32 <= limit;
    var ge = a_i32 >= limit;
    var land = a_bool && false;
    var lor = a_bool || false;

    // ─── Assignment operators ───────────────────────────────────────────────
    count += 1;
    count -= 1;
    count *= 2;
    count /= 2;
    count %= 3;
    a_u32 &= 0xFF;
    a_u32 |= 0x10;
    a_u32 ^= 0x01;
    a_u32 <<= 1;
    a_u32 >>= 1;

    // ─── Cast ───────────────────────────────────────────────────────────────
    var as_f64 = a_i32 as f64;
    var as_u32 = a_i32 as u32;

    // ─── Try (expression position) ──────────────────────────────────────────
    const len = try process_data(slice_const);

    // ─── Error capture ──────────────────────────────────────────────────────
    const maybe_len = process_data(slice_const) else |err| {
        log_error("failed");
        0
    };

    // ─── Unsafe block ───────────────────────────────────────────────────────
    unsafe {
        var raw: *i32 = ptr_to_count as *i32;
        *raw = 42;
    }

    // ─── Match (all pattern kinds) ──────────────────────────────────────────
    var st = Status.Ok;
    const code = match st {
        .Ok => 0,
        .Err(e) => e,
        _ => -1,
    };

    // ─── If / Else with trailing expression ─────────────────────────────────
    const msg = if count > 0 {
        "positive"
    } else {
        "non-positive"
    };

    // ─── While with break/continue ──────────────────────────────────────────
    var i: usize = 0;
    while i < 10 {
        if i == 5 {
            i += 1;
            continue;
        }
        if i == 8 {
            break;
        }
        i += 1;
    }

    // ─── For loop ───────────────────────────────────────────────────────────
    for byte: u8 in slice_const {
        if byte == 0 {
            break;
        }
    }

    // ─── Null, undefined, true, false, self ─────────────────────────────────
    var n: ?i32 = null;
    var uninit: i32 = undefined;
    var t = true;
    var f = false;

    // ─── Test declaration ───────────────────────────────────────────────────
    test "math works" {
        const two = 1 + 1;
        assert(two == 2);
    }

    // ─── Return ─────────────────────────────────────────────────────────────
    return {};
}

// ─── Extern / FFI ───────────────────────────────────────────────────────────
#[extern("C")]
pub fn c_callback(ptr: *void) -> void;

// ─── Helper stubs (so the demo type-checks conceptually) ────────────────────
fn open_file(path: []const u8) -> !i32 { return 0; }    // CORRECTED
fn close_file(fd: i32) -> void {}
fn wrap_handle(fd: i32) -> !i32 { return fd; }
fn log_error(msg: []const u8) -> void {}               // CORRECTED
fn assert(cond: bool) -> void {}
fn asm_halt() -> void {}

// ─── Inherent Extension Mechanics ───────────────────────────────────────────
// Generic Binding: Writing extend Vec3(T: type) binds T across all inherent
// functions inside the block, matching the parameterization in struct Vec3(T: type).
//
// Specialized Extensions: Writing extend Vec3(f32) with Printable allows
// target-specialized trait implementations without polluting the generic core
// struct definition.
