#!/bin/bash
# Test 2: imitate pipe error in source/CGI.cpp
FILE="source/CGI.cpp"
SERVER_CMD="${SERVER_CMD:-./webserv test1.conf}"
BACKUP="$(mktemp)"
SERVER_PID=""

[ -f "$FILE" ] || { echo "File not found: $FILE" >&2; exit 1; }
cp -p "$FILE" "$BACKUP"

cleanup() {
    [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null && wait "$SERVER_PID" 2>/dev/null
    cp "$BACKUP" "$FILE"
    rm -f "$BACKUP"
    echo "$FILE restored."
}
trap cleanup EXIT INT TERM

# 1) comment out the pipe() check block
perl -0777 -i -pe '
    $n += s{(if\s*\(\s*pipe\(pipeRead\)\s*==\s*-1\s*\|\|\s*pipe\(pipeWrite\)\s*==\s*-1\s*\)\s*\{\s*error_code\s*=\s*500;\s*return;\s*\})}{/* $1 */};
    END { exit($n ? 0 : 1) }
' "$FILE" || { echo "Block 1 (pipe check) not found" >&2; exit 1; }

# 2) uncomment the test block
perl -0777 -i -pe '
    $n += s{/\*\s*(std::cout\s*<<\s*"=== TEST: Imitating pipe error ===\\n";\s*error_code\s*=\s*500;\s*return;)\s*\*/}{$1};
    END { exit($n ? 0 : 1) }
' "$FILE" || { echo "Block 2 (test block) not found" >&2; exit 1; }

diff "$BACKUP" "$FILE"

make CXXFLAGS="-Wall -Wextra -std=c++98" || { echo "make failed" >&2; exit 1; }

$SERVER_CMD &
SERVER_PID=$!
sleep 1

curl -v --http1.0 http://localhost:8080/f.py