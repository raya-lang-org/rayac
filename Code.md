Raya Language Reference

    Version: 0.7.5
    Status: Formal Specification — Path A
    This document defines the complete syntax, type system, and semantics of the Raya language.

Table of Contents

    Lexical Specification
    Syntax Specification (EBNF)
    Type System & Pointer Taxonomy
    Semantics
    Attributes
    Memory Model
    ABI Layout
    Comptime Execution Model

1. Lexical Specification
1.1 Source Encoding

    UTF-8 only. BOM (0xEF 0xBB 0xBF) is stripped by the lexer.
    Line endings: \n (Unix) or \r\n (Windows). \r alone is an error.

1.2 Comments
raya

// Line comment: extends to end of line

/*
 * Block comment
 * Nestable to depth 16
 */

/* outer /* inner */ still outer */

1.3 Whitespace
Space (0x20), Tab (0x09), Newline (0x0A), Carriage Return (0x0D), Form Feed (0x0C), Vertical Tab (0x0B).
1.4 Keywords (30 — Frozen)
Table
#	Keyword	Category	#	Keyword	Category
1	module	Module	16	try	Error handling
2	import	Module	17	break	Control flow
3	fn	Function	18	continue	Control flow
4	pub	Visibility	19	match	Control flow
5	const	Declaration	20	struct	Type
6	var	Declaration	21	union	Type
7	comptime	Compile-time	22	enum	Type
8	defer	Cleanup	23	traits	Type system
9	errdefer	Error cleanup	24	extend	Type system
10	test	Testing	25	type	Type system
11	return	Control flow	26	unsafe	Safety
12	if	Control flow	27	noreturn	Type / control
13	else	Control flow	28	as	Conversion
14	while	Loop	29	with	Syntax / context
15	for	Loop	30	undefined	Value
1.5 Built-in Primitive Types (Reserved, NOT Keywords)
Signed integers: i8, i16, i32, i64, i128, isize
Unsigned integers: u8, u16, u32, u64, u128, usize
Floating point: f32, f64
Other: bool, void

    Cannot be shadowed by user declarations.
    Valid only in type-expression context.

1.6 Reserved Literals (NOT Keywords)

    true | false — boolean literals
    null — null literal
    undefined — uninitialized memory (IS a keyword, see #30)

1.7 Identifiers
ebnf

identifier ::= [a-zA-Z_][a-zA-Z0-9_]*

    Must not match any keyword, primitive type name, or reserved literal.
    Identifiers beginning with __ (double underscore) are reserved for compiler-generated symbols.

1.8 Contextual Identifiers

    self — method receiver. Lexes as IDENTIFIER; gains special meaning inside struct/union/enum/trait methods.
    Self — current enclosing type. Lexes as IDENTIFIER; gains special meaning as a type alias to the enclosing type.

1.9 Literals
Integer:
ebnf

decimal ::= [0-9]+
hex     ::= 0x[0-9a-fA-F]+
octal   ::= 0o[0-7]+
binary  ::= 0b[01]+

No suffixes. Type inferred from context or explicit cast.
Float:
ebnf

float ::= [0-9]+ "." [0-9]+ ([eE] [+-]? [0-9]+)?
      | [0-9]+ [eE] [+-]? [0-9]+

Default type is f64.
String:
ebnf

string ::= " ([^"] | \ [nt"\0])* "

Type is []const u8. No multi-line strings.
Character:
ebnf

char ::= ' ([^'] | \ [nt'\0]) '

Type is u8. No distinct char type.
1.10 Operators and Punctuation
plain

+  -  *  /  %
== != <  >  <= >=
=  += -= *= /= %= &= |= ^= <<= >>=
&  |  ^  <<  >>  &&  ||  !  ~
.  .. -> => :: #
( ) [ ] { }
, ; : ? &

1.11 Attributes
raya

#[packed]
#[align(16)]
#[extern("C")]
#[section(".text.boot")]

2. Syntax Specification (EBNF)
2.1 Compilation Unit
ebnf

compilation_unit ::= module_decl? import_stmt* top_level_decl*

module_decl ::= "module" identifier ";"

2.2 Import Statement
ebnf

import_stmt ::= "import" module_path ( "as" identifier )? ";"
module_path ::= identifier ( "." identifier )*

2.3 Top-Level Declarations
ebnf

top_level_decl ::= pub_decl | private_decl

pub_decl    ::= "pub" visibility_decl
private_decl ::= visibility_decl

visibility_decl ::= fn_decl
                  | struct_decl
                  | enum_decl
                  | union_decl
                  | trait_decl
                  | extend_decl
                  | type_alias
                  | const_decl
                  | var_decl
                  | test_decl

2.4 Function Declaration
ebnf

fn_decl ::= attribute* "comptime"? "fn" identifier generic_params?
          "(" param_list? ")" return_type? block

generic_params ::= "(" generic_param ( "," generic_param )* ")"
generic_param  ::= identifier ":" "type" ( "with" trait_list )?
trait_list     ::= identifier ( "," identifier )*

param_list ::= param ( "," param )*
param      ::= identifier ":" type_expr ( "=" expr )?

return_type ::= "->" type_expr

2.5 Block
ebnf

block ::= "{" stmt* trailing_expr? "}"

trailing_expr ::= expr

    If present, the trailing expression (no semicolon) is the value of the block.
    If absent, block value is void.

2.6 Statements
ebnf

stmt ::= decl_stmt
       | expr_stmt
       | assignment_stmt
       | return_stmt
       | if_stmt
       | while_stmt
       | for_stmt
       | defer_stmt
       | errdefer_stmt
       | break_stmt
       | continue_stmt
       | match_stmt
       | block

decl_stmt ::= "const" identifier ":" type_expr? "=" expr ";"
            | "var" identifier ":" type_expr? "=" expr ";"

expr_stmt      ::= expr ";"
assignment_stmt ::= expr assign_op expr ";"

assign_op ::= "=" | "+=" | "-=" | "*=" | "/=" | "%="
            | "&=" | "|=" | "^=" | "<<=" | ">>="

return_stmt ::= "return" expr? ";"

if_stmt   ::= "if" expr block ( "else" ( block | if_stmt ) )?
while_stmt ::= "while" expr block

for_stmt ::= "for" identifier ":" type_expr "in" expr block

defer_stmt  ::= "defer" expr ";"
errdefer_stmt ::= "errdefer" block

break_stmt    ::= "break" ";"
continue_stmt ::= "continue" ";"

match_stmt ::= "match" expr "{" match_arm* "}"
match_arm  ::= pattern "=>" expr ","

2.7 Expressions (Precedence, Highest to Lowest)
ebnf

expr ::= primary_expr
       | expr "." identifier                    /* field access */
       | expr "." identifier "(" arg_list? ")"  /* method call */
       | expr "(" arg_list? ")"                 /* function call */
       | expr "[" expr "]"                      /* index */
       | expr "[" expr ".." expr "]"           /* slice */
       | "&" expr                               /* address-of */
       | "&" "const" expr                       /* immutable address */
       | "*" expr                               /* dereference (unsafe) */
       | "-" expr                               /* unary negation */
       | "!" expr                               /* logical not */
       | "~" expr                               /* bitwise not */
       | expr "as" type_expr                    /* cast */
       | expr "*" expr                          /* multiplication */
       | expr "/" expr                          /* division */
       | expr "%" expr                          /* modulo */
       | expr "+" expr                          /* addition */
       | expr "-" expr                          /* subtraction */
       | expr "<<" expr                         /* shift left */
       | expr ">>" expr                         /* shift right */
       | expr "&" expr                          /* bitwise and */
       | expr "^" expr                          /* bitwise xor */
       | expr "|" expr                          /* bitwise or */
       | expr "==" expr                         /* equality */
       | expr "!=" expr                         /* inequality */
       | expr "<" expr                          /* less than */
       | expr ">" expr                          /* greater than */
       | expr "<=" expr                         /* less equal */
       | expr ">=" expr                         /* greater equal */
       | expr "&&" expr                         /* logical and */
       | expr "||" expr                         /* logical or */
       | "try" expr                             /* error propagation */
       | expr "else" "|" identifier "|" block   /* error capture */
       | "unsafe" block                         /* unsafe context */

primary_expr ::= literal
               | identifier
               | "null"
               | "true"
               | "false"
               | "undefined"
               | array_literal
               | struct_literal
               | "(" expr ")"

arg_list ::= expr ( "," expr )*

2.8 Type Expressions
ebnf

type_expr ::= primary_type
            | type_expr "!"                     /* error union */
            | "?" type_expr                     /* optional */
            | "&" type_expr                     /* mutable reference */
            | "&" "const" type_expr             /* immutable reference */
            | "[]" type_expr                    /* mutable slice */
            | "[]" "const" type_expr            /* immutable slice */
            | "*" type_expr                     /* mutable raw pointer */
            | "*" "const" type_expr             /* immutable raw pointer */
            | "[" expr "]" type_expr            /* fixed array */

primary_type ::= identifier
               | identifier "(" type_arg_list ")"  /* generic instantiation */
               | "fn" "(" type_list? ")" return_type?  /* function type */
               | "type"                              /* meta-type */
               | primitive_type

primitive_type ::= "void"
                 | "bool"
                 | "i8" | "i16" | "i32" | "i64" | "i128" | "isize"
                 | "u8" | "u16" | "u32" | "u64" | "u128" | "usize"
                 | "f32" | "f64"

type_arg_list ::= type_expr ( "," type_expr )*
type_list     ::= type_expr ( "," type_expr )*

2.9 Array and Struct Literals
ebnf

array_literal ::= "[]" type_expr "{" arg_list? "}"
                | "[" expr "]" type_expr "{" arg_list? "}"
                | "[" expr "]" type_expr "{" expr "," "}"   /* repeat-fill */

struct_literal ::= identifier "{" field_init_list? "}"
field_init_list ::= field_init ( "," field_init )*
field_init      ::= identifier ":" expr

2.10 Struct Declaration
ebnf

struct_decl ::= attribute* "struct" identifier generic_params?
              "{" field_decl* "}"

field_decl ::= attribute* "pub"? identifier ":" type_expr ( "=" expr )? ","

2.11 Enum Declaration
ebnf

enum_decl ::= attribute* "enum" identifier "{" variant_decl* "}"

variant_decl ::= identifier ( "(" type_expr ")" )? ( "=" expr )? ","

2.12 Union Declaration
ebnf

union_decl ::= attribute* "union" identifier generic_params?
             "{" field_decl* "}"

2.13 Trait Declaration
ebnf

trait_decl ::= "traits" identifier "{" trait_method* "}"

trait_method ::= "pub"? "fn" identifier "(" param_list? ")"
               return_type? ";"

2.14 Extend Declaration (Trait Implementation)
ebnf

extend_decl ::= "extend" identifier "with" trait_list block

2.15 Type Alias
ebnf

type_alias ::= "type" identifier "=" type_expr ";"

2.16 Test Declaration
ebnf

test_decl ::= "test" string_literal block

3. Type System & Pointer Taxonomy
3.1 References (&T, &const T, ?&T)

    Non-null, address-aligned memory locations.
    Auto-dereferencing on field access and method call.
    NO pointer arithmetic permitted.
    Rebindable: var r: &i32 = &x; r = &y; is legal.
    Aliasing: Multiple &T to the same address is permitted. The programmer is responsible for synchronization. This is not a compiler error.
    Struct fields: A struct MAY contain &T or []T fields. The compiler tracks nothing about the referent's lifetime. Dangling references are programmer bugs.

Table
Type	Null?	Size	Auto-deref?	Core Use
&T	No	sizeof(usize)	Yes	Safe, mutable reference. Function params, struct fields.
&const T	No	sizeof(usize)	Yes	Read-only view. Multiple &const T to same address allowed.
?&T	Yes	sizeof(usize)	Yes	Null-pointer optimized: 0x0 is null. No extra tag.
3.2 Slices ([]T, []const T)

    Fat pointer: struct { ptr: *T, len: usize } in target native endianness, ptr-first.
    Bounds checking on index: slice[i] checks 0 <= i < len.
        Panic/abort in Debug and ReleaseSafe.
        Undefined behavior in ReleaseFast.
    Sub-slicing: slice[start..end] produces []T with len = end - start, ptr = slice.ptr + start. Bounds-checked at construction.
    Empty slice is valid: ptr may be undefined/0x0, len == 0.

3.3 Raw Pointers (*T, *const T, ?*T)

    C-style unrestricted access.
    Arithmetic: ptr + n advances by n * sizeof(T) bytes (scaled, C-style).
    Byte-level: For unscaled byte arithmetic, cast through *u8: ptr as *u8 + 5.
    Dereference: Requires unsafe block or unsafe fn. *ptr is the deref syntax.
    Coercion: &T implicitly coerces to *T in unsafe contexts. *T to &T requires explicit unsafe assertion of validity.

3.4 Fixed Arrays ([N]T)

    Stack-allocated contiguous sequence.
    Implicitly coerces to []T: buf → buf[0..N]
    Implicitly coerces to []const T.

3.5 Opaque Pointers (*void, *const void) — FFI Only

    Valid only in #[extern("C")] contexts and unsafe blocks.
    Coerce to/from any *T or *const T in unsafe contexts.
    Cannot be dereferenced (void has no size).

3.6 Coercion Matrix
Table
From → To	Mechanism
[N]T → []T	Implicit
[N]T → []const T	Implicit
&T → &const T	Implicit (freezes mutability)
&T → *T	Implicit inside unsafe only
*T → &T	Explicit unsafe assertion required
[]T → *T	Explicit: slice.ptr
*void → *T	Explicit unsafe ptr_cast only
T → !T	Implicit on return (wraps success tag)
!T → T	Explicit: try, or else |err| unwrap
4. Semantics
4.1 Control Flow — No Outer Parentheses
raya

while true { }      // OK
while (true) { }    // SYNTAX ERROR

4.2 No Implicit Self
Methods must declare self explicitly:
raya

pub fn length(self: &const Vec3) -> f32 { ... }

4.3 Trait Object-Safety Rules
A trait is object-safe (can form &Trait) iff ALL methods:

    Have no comptime parameters (except implicit self)
    Have no generic parameters
    Do not return T by value
    Have receiver self: &T or self: &const T

4.4 Error Union Layout
!T is represented as:
c

struct {
    tag: u32,           /* padding to alignof(T) */
    payload: T
}

    Tag 0x00000000 = Success.
    Non-zero = (module_id: u16 << 16) | error_code: u16.

4.5 Casting Rules
x as T is allowed for:

    Integer ↔ Integer (checked in debug, wrapping in release)
    Integer ↔ Float
    Pointer ↔ Pointer (in unsafe only)
    Pointer ↔ Integer (in unsafe only)
    Enum ↔ Integer

4.6 Defer / Errdefer Scope

    defer executes its expression when the enclosing BLOCK exits (not function).
    errdefer executes its block when the enclosing FUNCTION exits via error.
    Multiple defers in the same block execute in LIFO order.
    errdefer blocks execute in LIFO order before the error is propagated.

4.7 Unsafe Contexts
The following require unsafe { ... } or unsafe fn:

    Dereferencing raw pointers (*ptr)
    Pointer arithmetic
    Type punning between pointer types
    Inline assembly
    Calling extern C functions without wrappers
    Coercing *void to typed pointers

4.8 Generic Constraints
raya

fn sort(T: type with Comparable, Serializable)(arr: []T) -> void { ... }

The compiler checks structural implementation before monomorphization.
4.9 Undefined Semantics

    undefined represents uninitialized memory. It is not a specific bit pattern.
    Reading undefined memory in safe code is a safety-checked compile error if the compiler can prove the read occurs; otherwise it is immediate undefined behavior at runtime in debug builds.
    undefined is valid only in initialization contexts:

raya

var x: u32 = undefined;     // OK
var y: u32 = x + 1;         // ERROR: x is undefined

    undefined may not be passed to comptime functions or used in compile-time constant evaluation.

4.10 Self and Self

    self is a contextual identifier referring to the receiver instance inside struct/union/enum/trait methods.
    Self is a contextual identifier referring to the enclosing type within a struct/union/enum/trait definition.
    Neither is a keyword. Outside method/type context they are ordinary identifiers.

4.11 Block Trailing Expressions
raya

const x = if cond {
    42                          // trailing expression: block value is 42
} else {
    0
};

4.12 Try in Expression Position
raya

const x = try might_fail();                     // OK
const y = (try a()) + (try b());               // OK

4.13 Generic Instantiation Syntax
raya

const v = Vec3(f32){ x: 1.0, y: 2.0, z: 3.0 };  // No angle brackets

4.14 Array Literal Repeat-Fill
raya

const zeros = [100]u32{ 0, };   // fills all 100 elements with 0
const exact = [3]u32{ 1, 2, 3 }; // requires exactly 3 elements

4.15 Bitfield Packing
Fields of type u1, u2, u3, etc. in a #[packed] struct are automatically bit-packed by the compiler. Byte-aligned fields maintain normal alignment even inside #[packed].
raya

#[packed]
struct StatusReg {
    ready: u1,
    error: u1,
    reserved: u6,    // packed into first byte
    status: u16,     // starts at byte offset 1, aligned
}

4.16 Pointer Dereference Syntax

    *ptr is the only dereference syntax. There is NO ptr.* sugar.
    Auto-dereference applies only to &T and &const T, not *T.
    To access a field through a raw pointer: (*ptr).field inside unsafe.

5. Attributes
Attributes decorate the declaration that immediately follows them.
raya

#[packed]
struct Header {
    flags: u8,
    len: u16,
}

#[align(64)]
struct CacheLine {
    data: [64]u8,
}

#[extern("C")]
fn c_function(x: i32) -> i32;

#[section(".text.boot")]
fn _start() -> noreturn;

Table
Attribute	Target	Effect
#[packed]	struct, union	Disable padding; bit-pack sub-byte fields
#[align(N)]	struct, union, var	Force N-byte alignment
#[extern("C")]	fn	C calling convention, no name mangling
#[section("name")]	fn	Place in specific linker section
#[noinline]	fn	Prevent inlining
#[always_inline]	fn	Force inlining
Multiple attributes may be stacked. Order is significant only for user-defined comptime attributes. Unknown attributes are a compile error unless permitted by a comptime plugin hook.
6. Memory Model
6.1 References

    &T and &const T are non-null pointers.
    They are rebindable — var r: &i32 = &x; r = &y; is legal.
    The compiler tracks nothing about lifetimes. Dangling references are programmer bugs.

6.2 Slices

    []T is a fat pointer: { ptr: *T, len: usize }.
    Bounds checking on every index operation.
    Empty slices are valid: ptr may be undefined, len == 0.

6.3 Raw Pointers

    *T permits arbitrary pointer arithmetic (scaled by sizeof(T)).
    Dereference requires unsafe.
    &T coerces to *T implicitly in unsafe contexts.

7. ABI Layout
7.1 Fat-Pointer ABI (Frozen)
A trait object &Trait is a fat pointer:
plain

┌─────────────────┬─────────────────┐
│   data: *void   │  vtable: *void  │
└─────────────────┴─────────────────┘

    Native endianness.
    data points to the concrete object.
    vtable points to a static vtable for the Type + Trait combination.

7.2 Error Union ABI
plain

┌────────┬────────┬─────────────────┐
│ tag: u32 │ padding │   payload: T    │
└────────┴────────┴─────────────────┘

    Tag 0x00000000 = Success.
    Non-zero = (module_id: u16 << 16) | error_code: u16.

7.3 Slice ABI
plain

┌─────────────────┬─────────────────┐
│   ptr: *T       │   len: usize    │
└─────────────────┴─────────────────┘

    Native endianness, ptr-first.

8. Comptime Execution Model
The comptime interpreter is a stack-based bytecode VM.
8.1 Value Types in the Interpreter

    Integer (i128) — for all integer math
    Float (f128) — for all float math
    Bool
    String (heap-allocated, reference-counted)
    Type (ComptimeType handle)
    Descriptor (DeclDescriptor handle)
    Expr / Stmt (AST node handles)
    Pointer (opaque heap pointer for composite values)
    Void

8.2 Instruction Set (v1.0 Minimum)
Table
Opcode	Operands	Description
NOP	—	No operation
PUSH_INT	imm: i128	Push integer literal
PUSH_FLT	imm: f128	Push float literal
PUSH_STR	idx: u32	Push string from constant pool
PUSH_TYP	idx: u32	Push type from type table
PUSH_BOOL	imm: u8	Push true/false
DUP	—	Duplicate top of stack
POP	—	Pop and discard
LD_LOCAL / ST_LOCAL	idx: u16	Local variable load/store
LD_GLOBAL / ST_GLOBAL	idx: u16	Global load/store
ADD_I / SUB_I / MUL_I / DIV_I / MOD_I	—	Integer arithmetic
ADD_F / SUB_F / MUL_F / DIV_F	—	Float arithmetic
AND / OR / XOR / SHL / SHR / NOT	—	Bitwise operations
NEG_I / NEG_F	—	Negation
EQ / NE / LT_I / GT_I / LE_I / GE_I	—	Integer comparisons
LT_F / GT_F / LE_F / GE_F	—	Float comparisons
JMP	offset: i32	Unconditional jump
JZ	offset: i32	Jump if false/zero
CALL	idx: u32	Call comptime function
RET	—	Return from function
CALL_BUILTIN	idx: u16	Compiler builtin (size_of, etc.)
MAKE_ARRAY	len: u32	Pop N values, make array
MAKE_STRUCT	idx: u32	Pop N values, make struct
INDEX_ARRAY	—	Array index
INDEX_STRUCT	field: u16	Struct field access
FIELD_ACCESS	name: u32	Field access by name
CONCAT_STR	—	String concatenation
CAST_INT	target_bits: u8	Integer cast
CAST_FLT	target_bits: u8	Float cast
HALT	—	Terminate execution
8.3 Execution Limits (Enforced)

    Max instructions per evaluation: 10⁸
    Max heap memory: 2GB
    Max recursion depth: 1024
    Max string length: 1MB
    Max array length: 10⁶ elements

8.4 Host Interface (Compiler Callbacks)
raya

builtin_size_of(T: ComptimeType) -> usize
builtin_align_of(T: ComptimeType) -> usize
builtin_offset_of(T: ComptimeType, field: []const u8) -> usize
builtin_type_name(T: ComptimeType) -> []const u8
builtin_compile_error(msg: []const u8) -> noreturn
builtin_add_method(desc: DeclDescriptor, method: FnDecl) -> DeclDescriptor
builtin_add_trait_impl(desc: DeclDescriptor, trait: []const u8, methods: []FnDecl) -> DeclDescriptor

Appendix A: Complete Keyword Reference
plain

module    import    fn        pub       const     var
comptime  defer     errdefer  test      return    if
else      while     for       try       break     continue
match     struct    union     enum      traits    extend
type      unsafe    noreturn  as        with      undefined

Appendix B: Complete Operator Precedence (High to Low)
Table
Precedence	Operators	Associativity
1	() [] . method call	Left
2	& &const * - ! ~ try	Right (prefix)
3	as	Left
4	* / %	Left
5	+ -	Left
6	<< >>	Left
7	&	Left
8	^	Left
9	|	Left
10	== != < > <= >=	Left
11	&&	Left
12	||	Left
13	else |x| (error capture)	Right
14	= += -= etc.	Right
This document is the authoritative language specification for Raya Path A. For implementation status, see PROCESS.md.
