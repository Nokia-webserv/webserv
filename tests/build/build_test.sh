#!/bin/bash

set -e

echo "=== BUILD TEST ==="

SPECIAL_FLAGS="${1:-}"
TEST_SERVER_SOURCE="source/.test-Server.cpp"

cleanup()
{
	rm -f "$TEST_SERVER_SOURCE"
}

trap cleanup EXIT INT TERM

sed 's@^\([[:space:]]*\)// TEST_INTEGRATION @\1@' \
	source/Server.cpp > "$TEST_SERVER_SOURCE"

make fclean
make SPECIAL_FLAGS="$SPECIAL_FLAGS" SERVER_SOURCE="$TEST_SERVER_SOURCE"

echo "BUILD: PASS"