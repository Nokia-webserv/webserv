#!/bin/sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
cd "$SCRIPT_DIR"

TRACE_SOURCE=$(mktemp --suffix=.cpp)
RESULT_OUTPUT=$(mktemp)
TRACE_OUTPUT=$(mktemp)

cleanup()
{
	rm -f "$TRACE_SOURCE" "$RESULT_OUTPUT" "$TRACE_OUTPUT" parser_test
}

trap cleanup EXIT INT TERM

# Activate only source lines explicitly marked as test tracing instructions.
sed 's@^// TEST_TRACE @@' \
	../../../source/HttpParser.cpp > "$TRACE_SOURCE"

c++ -std=c++98 -Wall -Wextra \
	-I../../../source -I../../../include \
	parser_test.cpp \
	"$TRACE_SOURCE" \
	../../../source/HttpRequest.cpp \
	../../../source/utils.cpp \
	-o parser_test

if ./parser_test fixtures > "$RESULT_OUTPUT" 2> "$TRACE_OUTPUT"; then
	cat "$RESULT_OUTPUT"
	exit 0
fi

cat "$RESULT_OUTPUT"
echo
echo "Parser diagnostics (including test assertions):"
cat "$TRACE_OUTPUT"
exit 1