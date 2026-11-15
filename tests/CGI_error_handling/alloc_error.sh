#!/bin/bash
# Test 1: imitate CGI allocation error.
# Usage: ./test_alloc_error.sh [path/to/Router.cpp]
# Server is started with ./webserv test1.conf (override with SERVER_CMD="...")

ROUTER="${1:-source/Router.cpp}"
SERVER_CMD="${SERVER_CMD:-./webserv test1.conf}"
BACKUP="$(mktemp)"
SERVER_PID=""

[ -f "$ROUTER" ] || { echo "File not found: $ROUTER" >&2; exit 1; }

cp -p "$ROUTER" "$BACKUP"

# Runs on any exit (normal, error, Ctrl+C): stop server, restore Router.cpp
cleanup() {
    [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null && wait "$SERVER_PID" 2>/dev/null
    cp "$BACKUP" "$ROUTER"
    rm -f "$BACKUP"
    echo "Router.cpp restored."
}
trap cleanup EXIT INT TERM

# 1) comment out the try/catch block
perl -0777 -i -pe '
    s{(try\s*\{\s*cgi\s*=\s*new\s+CGI\(request,\s*response,\s*result,\s*extension,\s*cgiMap\);\s*\}\s*catch\s*\(\.\.\.\)\s*\{\s*result\.errorCode\s*=\s*500;\s*return;\s*\})}{/* $1 */}
' "$ROUTER"

# 2) uncomment the test block
perl -0777 -i -pe '
    s{/\*\s*(std::cout\s*<<\s*"=== TEST: Imitating allocation error ===\\n";\s*result\.errorCode\s*=\s*500;\s*return;)\s*\*/}{$1}
' "$ROUTER"

# sanity check: both edits must have happened
if cmp -s "$ROUTER" "$BACKUP"; then
    echo "Router.cpp was not modified - patterns did not match." >&2
    exit 1
fi
echo "--- diff ---"
diff "$BACKUP" "$ROUTER"
echo "------------"

# 3) build
make CXXFLAGS="-Wall -Wextra -std=c++98" || { echo "make failed" >&2; exit 1; }

# 4) start the server so curl has something to talk to
$SERVER_CMD &
SERVER_PID=$!
sleep 1

# 5) request
curl -v --http1.0 http://localhost:8080/f.py

# 6) cleanup() restores Router.cpp via trap