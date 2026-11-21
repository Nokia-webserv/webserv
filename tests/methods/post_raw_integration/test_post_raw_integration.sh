#!/usr/bin/env bash
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/webserv-post-http.XXXXXX")"
trap 'rm -rf "$BUILD_DIR"' EXIT

if ! command -v curl >/dev/null 2>&1; then
    echo "[FAIL] curl is required for raw POST integration tests."
    exit 1
fi

echo "Building real server for raw POST integration tests via curl (C++98)..."
"${CXX:-c++}" -std=c++98 -Wall -Wextra -Werror \
    -I"$PROJECT_ROOT/include" "$PROJECT_ROOT"/source/*.cpp \
    -o "$BUILD_DIR/webserv"
cd "$PROJECT_ROOT"
PYTHONDONTWRITEBYTECODE=1 python3 "$SCRIPT_DIR/post_raw_integration_test.py" "$BUILD_DIR/webserv"
