#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

cd "$PROJECT_ROOT" || exit 1

SERVER="${1:-./webserv}"
CONFIG="${2:-tests/integration/basic.conf}"
TIMEOUT="${3:-${TEST_TIMEOUT:-10}}"

if ! [[ "$TIMEOUT" =~ ^[0-9]+$ ]] || [ "$TIMEOUT" -le 0 ]; then
    echo "[FAIL] Timeout must be a positive integer"
    exit 1
fi

SPECIAL_FLAGS="-DWEBSERV_CLIENT_TIMEOUT_SECONDS=$TIMEOUT"
export TEST_TIMEOUT="$TIMEOUT"
mkdir -p tests/integration/logs

cleanup()
{
    make fclean >/dev/null 2>&1 || true
}

trap cleanup EXIT INT TERM

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

    return "$status"
}

echo "============================================================"
echo "              WEBSERV INTEGRATION TESTS"
echo "============================================================"
echo "[INFO] Client timeout: $TIMEOUT seconds"

echo "[INFO] Rebuilding server"
if ! KEEP_BUILD_ARTIFACTS=1 bash tests/build/build_test.sh "$SPECIAL_FLAGS"; then
    echo "[FAIL] Server rebuild"
    exit 1
fi

echo "[PASS] Server rebuild"

for test in tests/integration/*.py; do
    test_name="$(basename "$test")"

    # Skip shared helper modules
    if [[ "$test_name" == "common.py" ||
          "$test_name" == "stalled_write_common.py" ]]; then
        continue
    fi

    LOG_FILE="$(cd tests/integration/logs && pwd)/${test_name%.py}.server.log"
    : > "$LOG_FILE"

    case "$test_name" in
        c6_stalled_write_timeout_test.py|c7_stalled_write_incoming_activity_test.py)
            run_test "INTEGRATION: $test_name" \
                env WEBSERV_TEST_LOG="$LOG_FILE" \
                python3 "$test" "$SERVER"
            ;;
        h1_delete.py)
            run_test "INTEGRATION: $test_name" \
                env WEBSERV_TEST_LOG="$LOG_FILE" \
                python3 "$test" "$SERVER" "tests/integration/delete.conf"
            ;;
        g2_method_not_allowed.py|g3_vhost_selection.py)
            run_test "INTEGRATION: $test_name" \
                env WEBSERV_TEST_LOG="$LOG_FILE" \
                python3 "$test" "$SERVER" "tests/integration/g_routing.conf"
            ;;
        *)
            run_test "INTEGRATION: $test_name" \
                env WEBSERV_TEST_LOG="$LOG_FILE" \
                python3 "$test" "$SERVER" "$CONFIG"
            ;;
    esac
done
echo
echo "============================================================"
echo "            INTEGRATION TEST REPORT"
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
    echo "[RESULT] INTEGRATION TESTS FAILED"
    exit 1
fi

echo
echo "[RESULT] ALL INTEGRATION TESTS PASSED"
exit 0