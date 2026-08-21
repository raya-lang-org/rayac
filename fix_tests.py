#!/usr/bin/env python3
import os

TESTDIR = "tests/sema"

# All files written with LF-only, no trailing spaces, correct content
FILES = {
    "ok.raya": "fn main() {\n    let x = 5;\n}\n",
    "ok.expected": "ok\n",

    "struct_ok.raya": "struct Point { x: f64, y: f64, }\n\nfn get_x(p: Point) -> f64 {\n    return p.x;\n}\n",
    "struct_ok.expected": "ok\n",

    "struct_literal.raya": "struct Point { x: f64, y: f64, }\n\nfn make_point() -> Point {\n    return Point{ x: 1.0, y: 2.0 };\n}\n",
    "struct_literal.expected": "ok\n",

    "bad_field.raya": "struct Point { x: f64, y: f64, }\n\nfn get_z(p: Point) -> f64 {\n    return p.z;\n}\n",
    "bad_field.expected": "struct 'Point' has no field 'z'\n",

    "bad_struct_field.raya": "struct Point { x: f64, y: f64, }\n\nfn make_point() -> Point {\n    return Point{ x: 1.0, z: 2.0 };\n}\n",
    "bad_struct_field.expected": "struct 'Point' has no field 'z'\n",

    "bad_field_type.raya": "struct Point { x: f64, y: f64, }\n\nfn make_point() -> Point {\n    return Point{ x: \"hello\", y: 2.0 };\n}\n",
    "bad_field_type.expected": "field 'x' expects 'f64', got '[]const u8'\n",

    "undeclared.raya": "fn main() {\n    let x = y;\n}\n",
    "undeclared.expected": "use of undeclared identifier 'y'\n",
}

def write_lf(path, content):
    with open(path, 'wb') as f:
        f.write(content.encode('utf-8'))

os.makedirs(TESTDIR, exist_ok=True)
for name, content in FILES.items():
    path = os.path.join(TESTDIR, name)
    write_lf(path, content)
    print(f"  wrote {name}")

print("\nDone. Run: make clean && make && make test-sema")
