
#!/bin/sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
cd "$SCRIPT_DIR"

# Portable temporary directory for macOS and Linux.
TEMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/webserv_parser_XXXXXX")
TRACE_SOURCE="$TEMP_DIR/HttpParser.cpp"
RESULT_OUTPUT="$TEMP_DIR/result.txt"
TRACE_OUTPUT="$TEMP_DIR/trace.txt"

cleanup()
{
    rm -rf "$TEMP_DIR"
    rm -f parser_test uri_normalization_test
}

trap cleanup EXIT
trap 'exit 1' INT TERM

# Activate only source lines explicitly marked as test tracing instructions.
sed 's@^// TEST_TRACE @@' \
    ../../../source/HttpParser.cpp > "$TRACE_SOURCE"


# Common parser dependencies.
PARSER_SOURCES="
$TRACE_SOURCE
../../../source/HttpRequest.cpp
../../../source/utils.cpp
"

compile_test()
{
    c++ -std=c++98 -Wall -Wextra -Werror \
        -I../../../source -I../../../include \
        "$1" \
        $PARSER_SOURCES \
        -o "$2"
}

run_test()
{
    name="$1"
    executable="$2"
    shift 2

    echo
    echo "=== $name ==="

    if "./$executable" "$@" > "$RESULT_OUTPUT" 2> "$TRACE_OUTPUT"; then
        cat "$RESULT_OUTPUT"
        return 0
    fi

    cat "$RESULT_OUTPUT"
    echo
    echo "Test diagnostics:"
    cat "$TRACE_OUTPUT"
    return 1
}

FAILED=0

# 1. Existing HTTP parser tests
compile_test parser_test.cpp parser_test

if ! run_test "HTTP PARSER TESTS" parser_test fixtures; then
    FAILED=1
fi

# 2. URI normalization tests
compile_test uri_normalization.cpp uri_normalization_test

if ! run_test "URI NORMALIZATION TESTS" uri_normalization_test; then
    FAILED=1
fi

echo
if [ "$FAILED" -eq 0 ]; then
    echo "[PASS] All HTTP parser tests passed"
else
    echo "[FAIL] HTTP parser tests failed"
fi

exit "$FAILED"
