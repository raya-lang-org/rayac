#!/bin/bash
# Raya Sema Test Runner
# Usage: ./run_sema_tests.sh [path_to_raya_executable]

RAYA="${1:-./bin/raya}"
TEST_DIR="${2:-tests/sema}"
PASSED=0
FAILED=0

if [ ! -f "$RAYA" ]; then
    echo "Error: $RAYA not found"
    echo "Usage: $0 [path_to_raya] [test_dir]"
    exit 1
fi

echo "=== Raya Sema Tests ==="
echo "Compiler: $RAYA"
echo "Test dir: $TEST_DIR"
echo ""

for test_file in "$TEST_DIR"/*.raya; do
    [ -e "$test_file" ] || continue
    base=$(basename "$test_file" .raya)
    expected_file="$TEST_DIR/${base}.expected"

    if [ ! -f "$expected_file" ]; then
        echo "SKIP: $base (no .expected file)"
        continue
    fi

    output=$($RAYA --check "$test_file" 2>&1)
    exit_code=$?

    expected=$(cat "$expected_file")

    # Check if expected contains "error:"
    if echo "$expected" | grep -q "error:"; then
        # Expect errors — check that at least one expected error appears
        match=true
        while IFS= read -r line; do
            [ -z "$line" ] && continue
            if ! echo "$output" | grep -Fq "$line"; then
                match=false
                missing="$line"
                break
            fi
        done <<< "$expected"

        if [ "$match" = true ]; then
            echo "PASS: $base"
            PASSED=$((PASSED + 1))
        else
            echo "FAIL: $base"
            echo "  Missing expected error: $missing"
            echo "  Output:"
            echo "$output" | sed 's/^/    /'
            FAILED=$((FAILED + 1))
        fi
    else
        # Expect ok — check no errors in output
        if echo "$output" | grep -q "error:"; then
            echo "FAIL: $base"
            echo "  Expected: ok"
            echo "  Got errors:"
            echo "$output" | sed 's/^/    /'
            FAILED=$((FAILED + 1))
        else
            echo "PASS: $base"
            PASSED=$((PASSED + 1))
        fi
    fi
done

echo ""
echo "=== Results ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"

if [ $FAILED -gt 0 ]; then
    exit 1
fi
