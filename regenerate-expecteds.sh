#!/bin/bash
set -e
RAYA="${1:-./bin/raya.exe}"
TEST_DIR="${2:-tests/sema}"

echo "Regenerating .expected files using $RAYA ..."
for f in "$TEST_DIR"/*.raya; do
    [ -e "$f" ] || continue
    base=$(basename "$f" .raya)
    expected="$TEST_DIR/${base}.expected"
    [ -f "$expected" ] || { echo "  SKIP $base"; continue; }
    "$RAYA" --check "$f" 2>&1 | grep -E '^error' > "$expected" || true
    [ -s "$expected" ] || echo "ok" > "$expected"
    echo "  GEN  $base.expected"
done
echo "Done."
