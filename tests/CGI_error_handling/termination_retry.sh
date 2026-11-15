#!/bin/bash
# Test 4: imitate termination retry (source/Server.cpp)
FILE="source/Server.cpp"
SERVER_CMD="${SERVER_CMD:-./webserv test1.conf}"
BACKUP="$(mktemp)"
SERVER_PID=""

[ -f "$FILE" ] || { echo "File not found: $FILE" >&2; exit 1; }
cp -p "$FILE" "$BACKUP"

# Runs on any exit: stop server, restore the source file
cleanup() {
    [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null && wait "$SERVER_PID" 2>/dev/null
    cp "$BACKUP" "$FILE"
    rm -f "$BACKUP"
    echo "$FILE restored."
}
trap cleanup EXIT INT TERM

# 1) comment out the tStatus check (only the if/return, not the call or the declaration)
perl -0777 -i -pe '
    $n += s{(if\s*\(\s*tStatus\s*==\s*stillRunning\s*\|\|\s*tStatus\s*==\s*interrupted\s*\|\|\s*tStatus\s*==\s*otherError\s*\)\s*return;)}{/* $1 */};
    END { exit($n ? 0 : 1) }
' "$FILE" || { echo "Block 1 (tStatus check) not found" >&2; exit 1; }

# 2) uncomment the test block
perl -0777 -i -pe '
    $n += s{/\*\s*(static\s+int\s+count\s*=\s*0;.*?std::cout\s*<<\s*"Attempt 2\\n";)\s*\*/}{$1}s;
    END { exit($n ? 0 : 1) }
' "$FILE" || { echo "Block 2 (test block) not found" >&2; exit 1; }

echo "--- diff ---"
diff "$BACKUP" "$FILE"
echo "------------"

# 3) build
make CXXFLAGS="-Wall -Wextra -std=c++98" || { echo "make failed" >&2; exit 1; }

# 4) start server
$SERVER_CMD &
SERVER_PID=$!
sleep 1

# 5) request
curl -v --http1.0 --max-time 10 http://localhost:8080/f.py

# 6) cleanup() runs via trap: kills server, restores the file