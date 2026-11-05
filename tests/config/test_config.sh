#!/bin/bash

CONFIG_TEST="tests/config/config_test"
LOG="tests/config/config_test.log"
PASSED=0
FAILED=0

: > "$LOG"

normalize_file()
{
    if [ "$#" -eq 0 ]; then
        sed -e 's/\r$//' -e '/^[[:space:]]*$/d'
    else
        sed -e 's/\r$//' -e '/^[[:space:]]*$/d' "$1"
    fi
}

log_case()
{
    local file="$1"
    local expected="$2"
    local output
    local exit_code
    local expected_file
    local actual_normalized
    local expected_normalized

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
        expected_file="${file%.conf}.expected"

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

        actual_normalized=$(mktemp)
        expected_normalized=$(mktemp)
        printf '%s\n' "$output" | normalize_file > "$actual_normalized"
        normalize_file "$expected_file" > "$expected_normalized"

        if ! diff -u "$expected_normalized" "$actual_normalized" >> "$LOG"; then
            echo "[FAIL] $file"
            echo "       complete parsed output differs from expected"
            echo "       details: $LOG"
            rm -f "$actual_normalized" "$expected_normalized"
            FAILED=$((FAILED + 1))
            return
        fi

        rm -f "$actual_normalized" "$expected_normalized"

        echo "[PASS] $file"
        PASSED=$((PASSED + 1))
        return
    fi

    expected_file="${file%.conf}.error"

    if [ ! -f "$expected_file" ]; then
        echo "[FAIL] $file"
        echo "       missing expected error file: $expected_file"
        echo "       details: $LOG"
        FAILED=$((FAILED + 1))
        return
    fi

    if [ "$exit_code" -ne 1 ]; then
        echo "[FAIL] $file"
        echo "       expected parser exit code 1, got $exit_code"
        echo "       details: $LOG"
        FAILED=$((FAILED + 1))
        return
    fi

    actual_normalized=$(mktemp)
    expected_normalized=$(mktemp)
    printf '%s\n' "$output" | normalize_file > "$actual_normalized"
    normalize_file "$expected_file" > "$expected_normalized"

    if ! diff -u "$expected_normalized" "$actual_normalized" >> "$LOG"; then
        echo "[FAIL] $file"
        echo "       parser error differs from expected"
        echo "       details: $LOG"
        rm -f "$actual_normalized" "$expected_normalized"
        FAILED=$((FAILED + 1))
        return
    fi

    rm -f "$actual_normalized" "$expected_normalized"
    echo "[PASS] $file"
    PASSED=$((PASSED + 1))
}

run_valid_glob()
{
    local pattern="$1"
    local file
    local files=( $pattern )

    if [ "${#files[@]}" -eq 0 ] || [ ! -f "${files[0]}" ]; then
        echo "[FAIL] no files matched: $pattern"
        FAILED=$((FAILED + 1))
        return
    fi

    for file in "${files[@]}"; do
        [ -f "$file" ] || continue
        log_case "$file" valid
    done
}

run_invalid_glob()
{
    local pattern="$1"
    local file
    local files=( $pattern )

    if [ "${#files[@]}" -eq 0 ] || [ ! -f "${files[0]}" ]; then
        echo "[FAIL] no files matched: $pattern"
        FAILED=$((FAILED + 1))
        return
    fi

    for file in "${files[@]}"; do
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
