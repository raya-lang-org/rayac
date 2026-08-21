
# Raya Compiler (in C)

A compiler for the Raya language, written in C.

## Build Instructions
- Run `make` to create output directories and build the executable (`bin/raya`).
- Run `make clean` to remove build artifacts.



| Section          | Before                           | After                                                              |
| ---------------- | -------------------------------- | ------------------------------------------------------------------ |
| **Header**       | `Phase 0 & 2 Complete`           | `Phase 0, 2 & 3 Complete`                                          |
| **Status table** | Method call / field access = ⏳   | **Struct literal + field access = ✅**                              |
| **Architecture** | Missing `sema`, `type`, `symbol` | Added `sema.h/c`, `type.h/c`, `symbol.h/c`                         |
| **Tests**        | Only `lexer/`, `parser/`         | Added `sema/`                                                      |
| **Build table**  | No `test-sema`                   | Added `make test-sema`                                             |
| **New section**  | —                                | **Phase 3: Semantic Analyzer ✅** with two-pass design summary      |
| **What's Next**  | Phase 3 (Sema)                   | Phase 4 (Advanced Sema: methods, self, traits, generics, comptime) |
| **Notes**        | Generic parser notes             | Added sema-specific notes (two-pass, hash-consing, `st_eq()`)      |

