#!/bin/bash

CONFIG_TEST="tests/config/config_test"
LOG="tests/config/config_test.log"
PASSED=0
FAILED=0

: > "$LOG"

log_case()
{
    local file="$1"
    local expected="$2"
    local output
    local exit_code

    {
        echo
        echo "============================================================"
        echo "CASE: $file"
        echo "EXPECTED: $expected"
    } >> "$LOG"

    output=$("$CONFIG_TEST" "$file" 2>&1)
    exit_code=$?
    printf '%s\n' "$output" >> "$LOG"

    if [ "$expected" = "valid" ]; then
        local expected_file="${file%.conf}.expected"

        if [ "$exit_code" -ne 0 ]; then
            echo "[FAIL] $file"
            echo "       parser exited with code $exit_code"
            echo "       details: $LOG"
            FAILED=$((FAILED + 1))
            return
        fi

        if [ ! -f "$expected_file" ]; then
            echo "[FAIL] $file"
            echo "       missing expected file: $expected_file"
            echo "       details: $LOG"
            FAILED=$((FAILED + 1))
            return
        fi

        while IFS= read -r line; do
            [ -n "$line" ] || continue
            if ! printf '%s\n' "$output" | grep -F -x -- "$line" >/dev/null; then
                echo "[FAIL] $file"
                echo "       expected value not found: $line"
                echo "       details: $LOG"
                FAILED=$((FAILED + 1))
                return
            fi
        done < "$expected_file"

        echo "[PASS] $file"
        PASSED=$((PASSED + 1))
        return
    fi

    if [ "$exit_code" -ne 0 ]; then
        echo "[PASS] $file"
        PASSED=$((PASSED + 1))
    else
        echo "[FAIL] $file"
        echo "       invalid config was accepted"
        echo "       details: $LOG"
        FAILED=$((FAILED + 1))
    fi
}

run_valid_glob()
{
    local pattern="$1"
    local file

    for file in $pattern; do
        [ -f "$file" ] || continue
        log_case "$file" valid
    done
}

run_invalid_glob()
{
    local pattern="$1"
    local file

    for file in $pattern; do
        [ -f "$file" ] || continue
        log_case "$file" invalid
    done
}

echo "============================================================"
echo "              CONFIG PARSER UNIT TESTS"
echo "============================================================"
echo "[INFO] Detailed output: $LOG"
echo

echo "[INFO] Building parser test"
if ! c++ -std=c++98 -Wall -Wextra -Werror -Iinclude \
    tests/config/config_test.cpp source/Configs.cpp \
    source/ServerConfig.cpp source/LocationConfig.cpp source/utils.cpp \
    -o "$CONFIG_TEST" >> "$LOG" 2>&1; then
    echo "[FAIL] Could not build parser test"
    echo "       details: $LOG"
    exit 1
fi

trap 'rm -f "$CONFIG_TEST"' EXIT

run_valid_glob "tests/config/valid/*.conf"
run_invalid_glob "tests/config/invalid/*.conf"
run_valid_glob "tests/config/sanitize/valid_*.conf"
run_invalid_glob "tests/config/sanitize/not_valid_*.conf"
run_valid_glob "tests/config/tokenize/*.conf"
run_valid_glob "tests/config/parserDirective/valid_*.conf"
run_invalid_glob "tests/config/parserDirective/not_valid_*.conf"
log_case "tests/config/parserLocation/test_location.conf" valid
log_case "tests/config/parserLocation/invalid_test_location.conf" invalid

echo
echo "============================================================"
echo "                  CONFIG TEST SUMMARY"
echo "============================================================"
echo "PASSED: $PASSED"
echo "FAILED: $FAILED"
echo "LOG: $LOG"

if [ "$FAILED" -eq 0 ]; then
    echo "[RESULT] ALL CONFIG TESTS PASSED"
    exit 0
fi

echo "[RESULT] CONFIG TESTS FAILED"
exit 1
