
#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT" || exit 1

SERVER="./webserv"
CONFIG="tests/integration/basic.conf"
TIMEOUT="${1:-10}"

if ! [[ "$TIMEOUT" =~ ^[0-9]+$ ]] || [ "$TIMEOUT" -le 0 ]; then
    echo "[FAIL] Timeout must be a positive integer"
    exit 1
fi

SPECIAL_FLAGS="-DWEBSERV_CLIENT_TIMEOUT_SECONDS=$TIMEOUT"
export TEST_TIMEOUT="$TIMEOUT"

PASSED=()
FAILED=()

run_test()
{
    local name="$1"
    shift

    echo
    echo "============================================================"
    echo "RUNNING: $name"
    echo "============================================================"

    "$@"
    local status=$?

    if [ "$status" -eq 0 ]; then
        echo "[PASS] $name"
        PASSED+=("$name")
    else
        echo "[FAIL] $name (exit code: $status)"
        FAILED+=("$name")
    fi
}

echo "============================================================"
echo "              WEBSERV FULL TEST SUITE"
echo "============================================================"

# 1. BUILD
echo "[INFO] Client timeout: $TIMEOUT seconds"

run_test "BUILD" bash tests/build/build_test.sh "$SPECIAL_FLAGS"

# 2. STARTUP
for test in tests/startup/*.sh; do
    run_test "STARTUP: $(basename "$test")" \
        bash "$test" "$SERVER" "$CONFIG"
done

# 3. CONFIG PARSER UNIT TEST
run_test "CONFIG PARSER" bash tests/config/test_config.sh

# 4. TCP
run_test "TCP ACCEPT" bash tests/tcp/accept_test/accept_test.sh \
    "$SERVER" "$CONFIG"

# 5. HTTP PARSER
run_test "HTTP PARSER" bash tests/http/parser/parser_test.sh

# 6. INTEGRATION
for test in tests/integration/*.py; do
    run_test "INTEGRATION: $(basename "$test")" \
        python3 "$test" "$SERVER" "$CONFIG"
done

# TEST ARTIFACT CLEANUP
run_test "TEST ARTIFACT CLEANUP" bash -c '
    rm -f tests/tcp/accept_test/accept_test
    rm -f tests/http/parser/parser_test
    rm -f tests/config/config_test
    find tests -type d -name objects -prune -exec rm -rf {} +
'

# FINAL CLEANUP
run_test "FINAL CLEANUP (make fclean)" make fclean

# FINAL REPORT
echo
echo "============================================================"
echo "                  FINAL TEST REPORT"
echo "============================================================"

echo
echo "PASSED: ${#PASSED[@]}"
for test in "${PASSED[@]}"; do
    echo "  [PASS] $test"
done

echo
echo "FAILED: ${#FAILED[@]}"
for test in "${FAILED[@]}"; do
    echo "  [FAIL] $test"
done

echo
echo "TOTAL: $((${#PASSED[@]} + ${#FAILED[@]}))"

if [ "${#FAILED[@]}" -gt 0 ]; then
    echo
    echo "[RESULT] TEST SUITE FAILED"
    exit 1
fi

echo
echo "[RESULT] ALL TESTS PASSED"
exit 0
