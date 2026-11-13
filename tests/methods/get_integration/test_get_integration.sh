#!/usr/bin/env bash
set -eu

TEST_SCRIPT="${1:-get_integration_test.py}"
case "$TEST_SCRIPT" in
    get_integration_test.py|get_connection_test.py) ;;
    *) echo "[FAIL] Unknown test script: $TEST_SCRIPT"; exit 1 ;;
esac

if ! command -v curl >/dev/null 2>&1; then
    echo "[FAIL] curl is required for GET integration tests."
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/webserv-get-integration.XXXXXX")"
trap 'rm -rf "$BUILD_DIR"' EXIT

echo "Building real server for GET integration tests via curl (C++98)..."
"${CXX:-c++}" -std=c++98 -Wall -Wextra -Werror \
    -I"$PROJECT_ROOT/include" "$PROJECT_ROOT"/source/*.cpp \
    -o "$BUILD_DIR/webserv"
cd "$PROJECT_ROOT"
PYTHONDONTWRITEBYTECODE=1 python3 "$SCRIPT_DIR/$TEST_SCRIPT" "$BUILD_DIR/webserv"
