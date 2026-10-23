#!/bin/bash

WEBSERV="./webserv"

PASSED=0
FAILED=0

echo "======================================"
echo " Config Tests"
echo "======================================"
echo


# -----------------------
# Valid configs
# -----------------------

echo "VALID CONFIGS"
echo "--------------------------------------"

for file in tests/config/valid/*.conf
do
    OUTPUT=$($WEBSERV "$file" 2>&1)
    EXIT_CODE=$?

    if [ $EXIT_CODE -eq 0 ]; then
        echo "[PASS] $file"
        echo "       Exit: 0"
        PASSED=$((PASSED + 1))
    else
        echo "[FAIL] $file"
        echo "       Exit: $EXIT_CODE"
        echo "       Error: $OUTPUT"
        FAILED=$((FAILED + 1))
    fi

    echo
done


# -----------------------
# Invalid configs
# -----------------------

echo "INVALID CONFIGS"
echo "--------------------------------------"

for file in tests/config/invalid/*.conf
do
    OUTPUT=$($WEBSERV "$file" 2>&1)
    EXIT_CODE=$?

    if [ $EXIT_CODE -ne 0 ]; then
        echo "[PASS] $file"
        echo "       Exit: $EXIT_CODE"
        echo "       Error: $OUTPUT"
        PASSED=$((PASSED + 1))
    else
        echo "[FAIL] $file"
        echo "       Exit: 0"
        echo "       Error: No error - invalid config was accepted"
        FAILED=$((FAILED + 1))
    fi

    echo
done

# -----------------------
# Sanitization - valid
# -----------------------

echo "SANITIZATION - VALID CONFIGS"
echo "--------------------------------------"

for file in tests/config/sanitize/valid_*.conf
do
    OUTPUT=$($WEBSERV "$file" 2>&1)
    EXIT_CODE=$?

    if [ $EXIT_CODE -eq 0 ]; then
        echo "[PASS] $file"
        echo "       Exit: 0"
        PASSED=$((PASSED + 1))
    else
        echo "[FAIL] $file"
        echo "       Exit: $EXIT_CODE"
        echo "       Error: $OUTPUT"
        FAILED=$((FAILED + 1))
    fi

    echo
done


# -----------------------
# Sanitization - invalid
# -----------------------

echo "SANITIZATION - INVALID CONFIGS"
echo "--------------------------------------"

for file in tests/config/sanitize/not_valid_*.conf
do
    OUTPUT=$($WEBSERV "$file" 2>&1)
    EXIT_CODE=$?

    if [ $EXIT_CODE -ne 0 ]; then
        echo "[PASS] $file"
        echo "       Exit: $EXIT_CODE"
        echo "       Error: $OUTPUT"
        PASSED=$((PASSED + 1))
    else
        echo "[FAIL] $file"
        echo "       Exit: 0"
        echo "       Error: No error - invalid config was accepted"
        FAILED=$((FAILED + 1))
    fi

    echo
done

# -----------------------
# Tokenization - valid
# -----------------------

echo "TOKENIZATION - VALID CONFIGS"
echo "--------------------------------------"

for file in tests/config/tokenize/*.conf
do
    [ -f "$file" ] || continue

    OUTPUT=$($WEBSERV "$file" 2>&1)
    EXIT_CODE=$?

    if [ $EXIT_CODE -eq 0 ]; then
        echo "[PASS] $file"
        echo "       Exit: 0"
        PASSED=$((PASSED + 1))
    else
        echo "[FAIL] $file"
        echo "       Exit: $EXIT_CODE"
        echo "       Error: $OUTPUT"
        FAILED=$((FAILED + 1))
    fi

    echo
done

# -----------------------
# Parser Directives - valid
# -----------------------

echo "PARSER DIRECTIVES - VALID CONFIGS"
echo "--------------------------------------"

for file in tests/config/parserDirective/valid_*.conf
do
    [ -f "$file" ] || continue

    OUTPUT=$($WEBSERV "$file" 2>&1)
    EXIT_CODE=$?

    if [ $EXIT_CODE -eq 0 ]; then
        echo "[PASS] $file"
        echo "       Exit: 0"
        PASSED=$((PASSED + 1))
    else
        echo "[FAIL] $file"
        echo "       Exit: $EXIT_CODE"
        echo "       Error: $OUTPUT"
        FAILED=$((FAILED + 1))
    fi

    echo
done
# -----------------------
# Parser Directives - invalid
# -----------------------

echo "PARSER DIRECTIVES - INVALID CONFIGS"
echo "--------------------------------------"

for file in tests/config/parserDirective/not_valid_*.conf
do
    [ -f "$file" ] || continue

    OUTPUT=$($WEBSERV "$file" 2>&1)
    EXIT_CODE=$?

    if [ $EXIT_CODE -ne 0 ]; then
        echo "[PASS] $file"
        echo "       Exit: $EXIT_CODE"
        echo "       Error: $OUTPUT"
        PASSED=$((PASSED + 1))
    else
        echo "[FAIL] $file"
        echo "       Exit: 0"
        echo "       Error: No error - invalid config was accepted"
        FAILED=$((FAILED + 1))
    fi

    echo
done

# -----------------------
# Location Parser - valid
# -----------------------

echo "LOCATION PARSER - VALID CONFIGS"
echo "--------------------------------------"

file="tests/config/parserLocation/test_location.conf"

if [ -f "$file" ]; then
    OUTPUT=$($WEBSERV "$file" 2>&1)
    EXIT_CODE=$?

    if [ $EXIT_CODE -eq 0 ]; then
        echo "[PASS] $file"
        echo "       Exit: 0"
        PASSED=$((PASSED + 1))

        # Print parsed configuration
        echo "$OUTPUT"
    else
        echo "[FAIL] $file"
        echo "       Exit: $EXIT_CODE"
        echo "       Error: $OUTPUT"
        FAILED=$((FAILED + 1))
    fi
else
    echo "[FAIL] $file"
    echo "       Error: Test file not found"
    FAILED=$((FAILED + 1))
fi

echo


# -----------------------
# Results
# -----------------------

echo "======================================"
echo " Results"
echo "======================================"
echo "Passed: $PASSED"
echo "Failed: $FAILED"

if [ $FAILED -eq 0 ]; then
    echo "All tests passed."
    exit 0
else
    echo "Some tests failed."
    exit 1
fi
