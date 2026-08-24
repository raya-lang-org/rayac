# Raya Package Manifest Specification
## raya.toml — v0.1

**Status:** Design Ready for Implementation  
**Purpose:** Multi-file builds, dependency resolution, project metadata.

---

## 1. Minimal raya.toml

```toml
[package]
name = "myproject"
version = "0.1.0"
edition = "2026"

[build]
src = "src"           # Source root directory
entry = "main.raya"   # Main file (relative to src/)
out = "bin"           # Output directory

[build.release-safe]
opt = 2
panic = "handler"     # call raya_panic() instead of abort

[build.release-fast]
opt = 3
panic = "trap"        # ud2 / __builtin_trap()
```

---

## 2. Full raya.toml

```toml
[package]
name = "rayhttp"
version = "0.3.1"
authors = ["bryan <bryan@raya.dev>"]
description = "HTTP server in Raya"
license = "MIT"
edition = "2026"
repository = "https://github.com/raya-lang-org/rayhttp"

[build]
src = "src"
entry = "main.raya"
out = "bin"
target = "x86_64-linux-gnu"   # default: host triple

# Release profiles
[build.debug]
opt = 0
checks = true
sanitize = ["memory", "undefined"]

[build.release-safe]
opt = 2
checks = "unlikely"     # unlikely branches to panic handler
sanitize = []

[build.release-fast]
opt = 3
checks = false        # zero overhead. UB on failure.
sanitize = []

# Dependencies from registry or git
dependencies = [
    { name = "raynet", version = "^0.2.0" },
    { name = "raycrypto", git = "https://github.com/raya-lang-org/raycrypto", branch = "main" },
    { name = "linux-headers", path = "../linux-headers-raya" },
]

# Kernel module build
[[module]]
name = "raydriver"
entry = "driver/main.raya"
target = "kernel"
out = "raydriver.ko"
```

---

## 3. Module Resolution Rules

```
import std.io;        →  RAYA_PATH/std/io.raya  or  ~/.raya/std/0.9.0/io.raya
import net.http;      →  src/net/http.raya  (relative to project src root)
import vendor.foo;    →  deps/vendor/foo.raya  (fetched dependency)
```

**Search order:**
1. Project `src/` tree
2. `deps/` directory (local path deps)
3. `RAYA_PATH` environment variable (colon-separated paths)
4. `~/.raya/registry/` (fetched registry deps)

---

## 4. CLI Integration

```bash
# Build default target (debug)
raya build

# Build specific profile
raya build --release-safe
raya build --target riscv64-none-elf

# Fetch dependencies
raya fetch

# Run tests
raya test
raya test --release-safe

# Run binary
raya run

# Check only (no codegen)
raya check

# Clean build artifacts
raya clean
```

---

## 5. Dependency Format

```toml
dependencies = [
    # Registry dependency
    { name = "raynet", version = "^0.2.0", registry = "default" },

    # Git dependency
    { name = "raycrypto", git = "https://github.com/raya-lang-org/raycrypto", rev = "abc123" },

    # Local path dependency
    { name = "mylib", path = "../mylib" },
]
```

**Version specifiers:**
- `"0.2.0"` — exact
- `"^0.2.0"` — compatible (≥0.2.0, <0.3.0)
- `"~0.2.0"` — patch only (≥0.2.0, <0.2.1)
- `"*"` — any

---

## 6. Lock File

`raya.lock` — auto-generated, committed to version control.

```toml
# raya.lock
lock_version = 1

[[package]]
name = "raynet"
version = "0.2.3"
source = "registry+https://raya.dev/registry"
checksum = "sha256:abc..."

[[package]]
name = "raycrypto"
version = "0.1.0"
source = "git+https://github.com/raya-lang-org/raycrypto#abc123"
```

---

## 7. Implementation Order

| Week | Task | Deliverable |
|------|------|-------------|
| 1 | Parse `raya.toml`, resolve `src/` imports | `raya build` compiles multi-file projects |
| 1 | `RAYA_PATH` env var | External stdlib discovery |
| 2 | `raya fetch` for git deps | `deps/` directory populated |
| 2 | `raya.lock` generation | Reproducible builds |
| 3 | Registry protocol sketch | `raya publish` / `raya fetch` from registry |

---

*This spec enables real multi-file Raya projects. Start with Week 1 — src/ resolution is enough to unblock everything else.*
