#!/usr/bin/env python3
"""Debug script for Raya sema tests — runs --check on every .raya file
and shows stdout + stderr so you can see parser crashes."""

import subprocess
import os
import sys

SEMA_DIR = "tests/sema"
RAYA_BIN = "./bin/raya"

def run(cmd, capture_stderr=True):
    """Run a command, return (stdout, stderr, returncode)."""
    result = subprocess.run(
        cmd,
        shell=True,
        capture_output=True,
        text=True
    )
    return result.stdout, result.stderr, result.returncode

def check_file(path):
    """Run raya --check on a single file."""
    stdout, stderr, rc = run(f"{RAYA_BIN} --check {path}")
    return stdout, stderr, rc

def dump_tokens(path):
    """Run raya --dump-tokens on a single file."""
    stdout, stderr, rc = run(f"{RAYA_BIN} --dump-tokens {path}")
    return stdout, stderr, rc

def main():
    if not os.path.exists(RAYA_BIN):
        print(f"ERROR: {RAYA_BIN} not found. Run 'make' first.")
        sys.exit(1)

    files = sorted([f for f in os.listdir(SEMA_DIR) if f.endswith(".raya")])

    print("=" * 70)
    print("RAYA SEMA DEBUG — checking all .raya files")
    print("=" * 70)

    ok_count = 0
    sema_error_count = 0
    parser_crash_count = 0

    for fname in files:
        path = os.path.join(SEMA_DIR, fname)
        stdout, stderr, rc = check_file(path)

        has_parser_error = "parser error" in stderr.lower()
        has_sema_error = "error:" in stderr.lower() and not has_parser_error

        if rc == 0 and not has_parser_error and not has_sema_error:
            status = "OK"
            ok_count += 1
        elif has_parser_error:
            status = "PARSER CRASH"
            parser_crash_count += 1
        elif has_sema_error:
            status = "SEMA ERROR"
            sema_error_count += 1
        else:
            status = f"EXIT {rc}"
            if rc != 0:
                parser_crash_count += 1

        print(f"\n{fname:40s}  →  {status}")
        if has_parser_error or has_sema_error:
            # Show first 3 lines of stderr
            lines = stderr.strip().split("\n")[:5]
            for line in lines:
                print(f"    {line}")

    print("\n" + "=" * 70)
    print(f"SUMMARY: {ok_count} OK, {sema_error_count} sema errors, {parser_crash_count} parser crashes")
    print("=" * 70)

    # Special deep-dive for self_outside_method
    self_file = os.path.join(SEMA_DIR, "self_outside_method.raya")
    if os.path.exists(self_file):
        print(f"\n{'='*70}")
        print("DEEP DIVE: self_outside_method.raya")
        print("=" * 70)

        print("\n--- FILE CONTENT (cat -A style) ---")
        with open(self_file, "rb") as f:
            raw = f.read()
        print(f"Raw bytes: {raw!r}")
        print(f"Length: {len(raw)}")

        print("\n--- TOKENS ---")
        stdout, stderr, rc = dump_tokens(self_file)
        if stdout:
            print(stdout[:2000])  # truncate if huge
        if stderr:
            print("STDERR:")
            print(stderr[:1000])

        print("\n--- CHECK OUTPUT ---")
        stdout, stderr, rc = check_file(self_file)
        if stdout:
            print("STDOUT:")
            print(stdout)
        if stderr:
            print("STDERR:")
            print(stderr)

if __name__ == "__main__":
    main()
