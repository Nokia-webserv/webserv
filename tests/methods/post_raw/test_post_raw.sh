#!/usr/bin/env bash
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/webserv-post-raw.XXXXXX")"
trap 'rm -rf "$BUILD_DIR"' EXIT
mkdir "$BUILD_DIR/fixtures"

echo "Checking the real production sources (C++98)..."
if ! "${CXX:-c++}" -std=c++98 -Wall -Wextra -Werror -I"$PROJECT_ROOT/include" \
    -fsyntax-only "$PROJECT_ROOT/source/Router.cpp" "$PROJECT_ROOT/source/utils.cpp"; then
    printf '\033[31m[BLOCKED]\033[0m Production sources do not compile. No runtime cases ran.\n'
    exit 2
fi

# Adapt system-call names only in the Router test translation unit. The actual
# Router.cpp is included unchanged, with its real control flow and real helpers.
cat > "$BUILD_DIR/router_under_test.cpp" <<EOF
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include "Router.hpp"
#include "ClientConnection.hpp"
#include "raw_upload_faults.hpp"
#define open rawUploadTestOpen
#define write rawUploadTestWrite
#define close rawUploadTestClose
#include "$PROJECT_ROOT/source/Router.cpp"
EOF

echo "Building raw POST component tests..."
if ! "${CXX:-c++}" -std=c++98 -Wall -Wextra -Werror \
    -I"$PROJECT_ROOT/include" -I"$SCRIPT_DIR" \
    "$SCRIPT_DIR/post_raw_test.cpp" "$SCRIPT_DIR/raw_upload_faults.cpp" \
    "$BUILD_DIR/router_under_test.cpp" \
    "$PROJECT_ROOT/source/CGI.cpp" "$PROJECT_ROOT/source/ClientConnection.cpp" \
    "$PROJECT_ROOT/source/HttpParser.cpp" \
    "$PROJECT_ROOT/source/HttpRequest.cpp" "$PROJECT_ROOT/source/HttpResponse.cpp" \
    "$PROJECT_ROOT/source/ServerConfig.cpp" "$PROJECT_ROOT/source/LocationConfig.cpp" \
    "$PROJECT_ROOT/source/utils.cpp" -o "$BUILD_DIR/post_raw_test"; then
    printf '\033[31m[BLOCKED]\033[0m Component build failed. No runtime cases ran.\n'
    exit 2
fi

# The child changes cwd only inside the temporary fixtures. This tests relative
# upload_store paths without creating any fixtures in the repository.
python3 - "$BUILD_DIR/post_raw_test" "$BUILD_DIR/fixtures" <<'PY'
import subprocess
import sys
try:
    result = subprocess.run([sys.argv[1], sys.argv[2]], cwd=sys.argv[2], timeout=30)
    sys.exit(result.returncode)
except subprocess.TimeoutExpired:
    sys.exit('[FAIL] Raw upload component run exceeded 30 seconds (possible blocked write/retry loop).')
PY
