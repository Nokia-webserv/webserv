#!/usr/bin/env bash
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/webserv-delete.XXXXXX")"
trap 'rm -rf "$BUILD_DIR"' EXIT
mkdir "$BUILD_DIR/fixtures"

echo "Building DELETE component tests (C++98)..."
if ! "${CXX:-c++}" -std=c++98 -Wall -Wextra -Werror \
    -I"$PROJECT_ROOT/include" \
    "$SCRIPT_DIR/delete_test.cpp" \
    "$PROJECT_ROOT/source/Router.cpp" \
    "$PROJECT_ROOT/source/HttpRequest.cpp" \
    "$PROJECT_ROOT/source/HttpResponse.cpp" \
    "$PROJECT_ROOT/source/ServerConfig.cpp" \
    "$PROJECT_ROOT/source/LocationConfig.cpp" \
    "$PROJECT_ROOT/source/utils.cpp" \
    -o "$BUILD_DIR/delete_test"; then
    echo "[FAIL] Component build failed; no runtime tests were executed."
    exit 1
fi

cd "$PROJECT_ROOT"
"$BUILD_DIR/delete_test" "$BUILD_DIR/fixtures"
