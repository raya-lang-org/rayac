#!/bin/bash
# Raya codegen test runner
# Run from repo root: bash tests/codegen/run_tests.sh

set -e

RAYAC="./bin/raya"
TESTDIR="tests/codegen"
PASSED=0
FAILED=0

run_test() {
    local file="$1"
    local name="$(basename "$file" .raya)"
    local cfile="${file}.c"
    local bin="${file%.raya}"
    local expected_exit=""
    local expected_stdout=""

    while IFS= read -r line; do
        if [[ "$line" =~ ^[[:space:]]*//[[:space:]]*expect_exit:[[:space:]]*([0-9]+) ]]; then
            expected_exit="${BASH_REMATCH[1]}"
        fi
        if [[ "$line" =~ ^[[:space:]]*//[[:space:]]*expect_stdout:[[:space:]]*\"(.*)\" ]]; then
            expected_stdout="${BASH_REMATCH[1]}"
        fi
    done < "$file"

    [[ -z "$expected_exit" ]] && expected_exit=0

    # Compile
    if ! "$RAYAC" --build "$file" > /dev/null 2>&1; then
        echo "  FAIL  $name (compilation failed)"
        FAILED=$((FAILED + 1))
        rm -f "$bin" "$cfile"
        return
    fi

    # Run
    local actual_stdout=""
    local actual_exit=0
    actual_stdout="$("$bin" 2>/dev/null)" || actual_exit=$?

    rm -f "$bin" "$cfile"

    if [[ "$actual_exit" != "$expected_exit" ]]; then
        echo "  FAIL  $name (exit: expected $expected_exit, got $actual_exit)"
        FAILED=$((FAILED + 1))
        return
    fi

    if [[ -n "$expected_stdout" ]]; then
        local norm_expected
        local norm_actual
        norm_expected=$(printf '%s' "$expected_stdout" | sed 's/\\n/\n/g')
        norm_actual=$(printf '%s' "$actual_stdout" | sed 's/\r\n/\n/g')
        if [[ "$norm_actual" != "$norm_expected" ]]; then
            echo "  FAIL  $name (stdout mismatch)"
            echo "    expected: $(printf %q "$expected_stdout")"
            echo "    actual:   $(printf %q "$actual_stdout")"
            FAILED=$((FAILED + 1))
            return
        fi
    fi

    echo "  PASS  $name"
    PASSED=$((PASSED + 1))
}

echo "Running codegen tests..."

if [[ $# -eq 1 ]]; then
    run_test "$1"
else
    for f in "$TESTDIR"/*.raya; do
        [[ -f "$f" ]] && run_test "$f"
    done
fi

echo "Codegen: $PASSED passed, $FAILED failed"
[[ $FAILED -eq 0 ]]
