#!/usr/bin/env bash
set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export PYTHONDONTWRITEBYTECODE=1
passed=0
failed=0

run_suite() {
    local name="$1"
    shift
    echo
    echo "=== $name ==="
    if "$@"; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
        printf '\033[31m[FAIL]\033[0m Suite: %s\n' "$name"
    fi
}

run_suite "Raw POST: unit" bash "$SCRIPT_DIR/test_post_raw.sh" --unit
run_suite "Raw POST: component" bash "$SCRIPT_DIR/test_post_raw.sh" --component
run_suite "Raw POST: real server via curl" bash "$SCRIPT_DIR/test_post_raw.sh" --integration
run_suite "Multipart: boundary" python3 "$SCRIPT_DIR/test_multipart_boundary.py"
run_suite "Multipart: splitting" python3 "$SCRIPT_DIR/test_multipart_parts.py"
run_suite "Multipart: one part before saving" python3 "$SCRIPT_DIR/test_multipart_part.py"
run_suite "Multipart: complete component with file saving" bash "$SCRIPT_DIR/test_multipart.sh"

printf '\nPOST suites passed: %s; failed: %s\n' "$passed" "$failed"
if [ "$failed" -ne 0 ]; then
    exit 1
fi
printf '\033[32m[PASS]\033[0m All POST suites passed\n'
