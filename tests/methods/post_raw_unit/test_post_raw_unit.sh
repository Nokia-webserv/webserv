#!/usr/bin/env bash
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/webserv-post-unit.XXXXXX")"
trap 'rm -rf "$BUILD_DIR"' EXIT

# Test the current pure method in isolation. Change its qualified name only;
# link the real getMime/trimStr/strTolower implementations from utils.cpp.
python3 - "$PROJECT_ROOT/source/Router.cpp" "$BUILD_DIR/get_file_format_under_test.hpp" <<'PY'
from pathlib import Path
import re
import sys

source = Path(sys.argv[1]).read_text()
matches = list(re.finditer(r'\bstd::\s*string\s+Router::getFileFormat\s*\([^)]*\)\s*\{', source))
if len(matches) != 1:
    sys.exit('[FAIL] Expected exactly one Router::getFileFormat definition.')
match = matches[0]
tokens = re.compile(r'''//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[{}]''', re.S)
depth = 0
for token in tokens.finditer(source, match.end() - 1):
    if token.group() == '{':
        depth += 1
    elif token.group() == '}':
        depth -= 1
        if depth == 0:
            definition = source[match.start():token.end()]
            definition = definition.replace('Router::getFileFormat', 'getFileFormatUnderTest', 1)
            Path(sys.argv[2]).write_text('#include "utils.hpp"\n' + definition + '\n')
            break
else:
    sys.exit('[FAIL] Unmatched braces in Router::getFileFormat.')
PY

echo "Building getFileFormat unit tests with real utils.cpp (C++98)..."
"${CXX:-c++}" -std=c++98 -Wall -Wextra -Werror \
    -I"$PROJECT_ROOT/include" -I"$BUILD_DIR" \
    "$SCRIPT_DIR/get_file_format_test.cpp" "$PROJECT_ROOT/source/utils.cpp" \
    -o "$BUILD_DIR/get_file_format_test"
"$BUILD_DIR/get_file_format_test"
