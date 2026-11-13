#!/usr/bin/env bash
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/webserv-url-encode.XXXXXX")"
trap 'rm -rf "$BUILD_DIR"' EXIT

# Isolate the current pure method body from unrelated Router build dependencies.
# Change only its qualified name; never copy or replace the encoding algorithm.
python3 - "$PROJECT_ROOT/source/Router.cpp" "$BUILD_DIR/url_encode_path_under_test.hpp" <<'PY'
from pathlib import Path
import re
import sys

source = Path(sys.argv[1]).read_text()
pattern = r'\bstd::\s*string\s+Router::urlEncodePath\s*\([^)]*\)\s*\{'
matches = list(re.finditer(pattern, source))
if len(matches) != 1:
    sys.exit('[FAIL] Expected exactly one Router::urlEncodePath definition.')

match = matches[0]
tokens = re.compile(r"""//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[{}]""", re.S)
depth = 0
end = None
for token in tokens.finditer(source, match.end() - 1):
    if token.group() == '{':
        depth += 1
    elif token.group() == '}':
        depth -= 1
        if depth == 0:
            end = token.end()
            break
if end is None:
    sys.exit('[FAIL] Unmatched braces in Router::urlEncodePath.')

definition = source[match.start():end]
definition = definition.replace('Router::urlEncodePath', 'urlEncodePathUnderTest', 1)
Path(sys.argv[2]).write_text(
    '#include <string>\n#include <sstream>\n#include <iomanip>\n\n' + definition + '\n'
)
PY

echo "Building isolated URL path encoding tests (C++98)..."
"${CXX:-c++}" -std=c++98 -Wall -Wextra -Werror \
    -I"$BUILD_DIR" "$SCRIPT_DIR/url_encode_path_test.cpp" \
    -o "$BUILD_DIR/url_encode_path_test"
"$BUILD_DIR/url_encode_path_test"
