#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

cd "$PROJECT_ROOT" || exit 1

SERVER="${1:-./webserv}"
CONFIG="${2:-tests/integration/basic.conf}"
SPECIAL_FLAGS="${3:-}"

cleanup()
{
    make fclean >/dev/null 2>&1 || true
}

trap cleanup EXIT INT TERM

TESTS=(
    startup_test.sh
    listening_test.sh
    port_conflict_test.sh
)

PASSED=0
FAILED=0

echo "============================================================"
echo "              WEBSERV STARTUP TESTS"
echo "============================================================"

echo "[INFO] Rebuilding server"
if ! make fclean || ! make SPECIAL_FLAGS="$SPECIAL_FLAGS"; then
    echo "[FAIL] Server rebuild"
    exit 1
fi

echo "[PASS] Server rebuild"

for test in "${TESTS[@]}"; do
    echo
    echo "============================================================"
    echo "RUNNING: STARTUP: $test"
    echo "============================================================"

    if bash "$SCRIPT_DIR/$test" "$SERVER" "$CONFIG"; then
        echo "[PASS] STARTUP: $test"
        PASSED=$((PASSED + 1))
    else
        echo "[FAIL] STARTUP: $test"
        FAILED=$((FAILED + 1))
    fi
done

echo
echo "============================================================"
echo "              STARTUP TEST REPORT"
echo "============================================================"
echo
echo "PASSED: $PASSED"
echo "FAILED: $FAILED"
echo "TOTAL: $((PASSED + FAILED))"

if [ "$FAILED" -gt 0 ]; then
    echo
    echo "[RESULT] STARTUP TESTS FAILED"
    exit 1
fi

echo
echo "[RESULT] ALL STARTUP TESTS PASSED"
exit 0