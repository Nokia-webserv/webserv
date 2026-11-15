#!/bin/bash
# Test 3: imitate error in child process (source/CGI.cpp)
FILE="source/CGI.cpp"
SERVER_CMD="${SERVER_CMD:-./webserv test1.conf}"
BACKUP="$(mktemp)"
SERVER_PID=""

[ -f "$FILE" ] || { echo "File not found: $FILE" >&2; exit 1; }
cp -p "$FILE" "$BACKUP"

# Runs on any exit: stop server and frozen child, restore the source file
cleanup() {
    [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null && wait "$SERVER_PID" 2>/dev/null
    pkill -f "$SERVER_CMD" 2>/dev/null
    cp "$BACKUP" "$FILE"
    rm -f "$BACKUP"
    echo "$FILE restored."
}
trap cleanup EXIT INT TERM

# 1) disable the dup2 block (#if 0 survives the inner comments)
perl -0777 -i -pe '
    $n += s{(if\s*\(\s*dup2\(pipeRead\[0\],\s*STDIN_FILENO\)\s*==\s*-1\s*\|\|\s*dup2\(pipeWrite\[1\],\s*STDOUT_FILENO\)\s*==\s*-1\s*\)\s*\{.*?while\s*\(true\)\s*poll\(NULL,\s*0,\s*-1\);\s*\})}{#if 0\n$1\n#endif}s;
    END { exit($n ? 0 : 1) }
' "$FILE" || { echo "Block 1 (dup2 check) not found" >&2; exit 1; }

# 2) uncomment the test block
perl -0777 -i -pe '
    $n += s{/\*\s*(std::cout\s*<<\s*"=== TEST: Imitating error in subprocess ===\\n";.*?)\s*\*/}{$1}s;
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

# 5) request (max-time so curl doesn't hang on the frozen child)
curl -v --http1.0 --max-time 10 http://localhost:8080/f.py

# 6) cleanup() runs via trap: kills server and child, restores the file