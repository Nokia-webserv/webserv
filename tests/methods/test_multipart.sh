#!/usr/bin/env bash
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/webserv-multipart.XXXXXX")"
trap 'rm -rf "$BUILD_DIR"' EXIT

echo "Checking production Router.cpp (C++98)..."
"${CXX:-c++}" -std=c++98 -Wall -Wextra -Werror -I"$PROJECT_ROOT/include" \
    -fsyntax-only "$PROJECT_ROOT/source/Router.cpp"

# Same real filesystem/error wrappers used by the raw-upload component suite.
# Router.cpp itself is included unchanged; ordinary calls do real disk I/O.
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

echo "Building complete multipart component tests..."
"${CXX:-c++}" -std=c++98 -Wall -Wextra -Werror \
    -I"$PROJECT_ROOT/include" -I"$SCRIPT_DIR/post_raw" \
    "$SCRIPT_DIR/multipart/multipart_component.cpp" "$SCRIPT_DIR/post_raw/raw_upload_faults.cpp" \
    "$BUILD_DIR/router_under_test.cpp" \
    "$PROJECT_ROOT/source/CGI.cpp" "$PROJECT_ROOT/source/ClientConnection.cpp" \
    "$PROJECT_ROOT/source/HttpParser.cpp" \
    "$PROJECT_ROOT/source/HttpRequest.cpp" "$PROJECT_ROOT/source/HttpResponse.cpp" \
    "$PROJECT_ROOT/source/ServerConfig.cpp" "$PROJECT_ROOT/source/LocationConfig.cpp" \
    "$PROJECT_ROOT/source/utils.cpp" -o "$BUILD_DIR/multipart_component"
PYTHONDONTWRITEBYTECODE=1 python3 "$SCRIPT_DIR/multipart/test_multipart.py" "$BUILD_DIR/multipart_component"
