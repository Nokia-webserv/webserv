#!/usr/bin/env bash
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/webserv-files-list.XXXXXX")"
trap 'rm -rf "$BUILD_DIR"' EXIT
mkdir "$BUILD_DIR/fixtures"

# Compile the actual three methods together, without the unrelated server/router.
python3 - "$PROJECT_ROOT/source/Router.cpp" "$BUILD_DIR/files_list_under_test.hpp" <<'PY'
from pathlib import Path
import re
import sys

source = Path(sys.argv[1]).read_text()
definitions = []
tokens = re.compile(r"""//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[{}]""", re.S)
for name in ('generateFilesList', 'escapeHtml', 'urlEncodePath'):
    pattern = r'\bstd::\s*string\s+Router::' + name + r'\s*\([^)]*\)\s*\{'
    matches = list(re.finditer(pattern, source))
    if len(matches) != 1:
        sys.exit('[FAIL] Expected exactly one Router::' + name + ' definition.')
    match = matches[0]
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
        sys.exit('[FAIL] Unmatched braces in Router::' + name + '.')
    definitions.append(source[match.start():end].replace(
        'Router::' + name, 'ListingComponent::' + name, 1))

Path(sys.argv[2]).write_text('''#include <string>
#include <sstream>
#include <iomanip>
#include <cerrno>
#include <dirent.h>

class ListingComponent {
public:
    std::string generateFilesList(std::string&, std::string&);
    std::string escapeHtml(std::string);
    std::string urlEncodePath(std::string);
};

''' + '\n\n'.join(definitions) + '\n')
PY

echo "Building directory listing component tests (C++98)..."
"${CXX:-c++}" -std=c++98 -Wall -Wextra -Werror \
    -I"$BUILD_DIR" "$SCRIPT_DIR/generate_files_list_test.cpp" \
    -o "$BUILD_DIR/generate_files_list_test"
"$BUILD_DIR/generate_files_list_test" "$BUILD_DIR/fixtures"
