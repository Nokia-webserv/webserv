#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

cd "$PROJECT_ROOT" || exit 1

SERVER="./webserv"
CLIENT_SOURCE="./tests/tcp/accept_test/accept_test.cpp"
CLIENT="./tests/tcp/accept_test/accept_test"
HOST="127.0.0.1"
PORT=8080

LOG="/tmp/webserv_accept_test.log"
TRACE="/tmp/webserv_accept_trace.log"
SPECIAL_FLAGS="${3:-}"

SERVER_PID=""

cleanup()
{
    if [ -n "$SERVER_PID" ]; then
        kill -TERM -- "-$SERVER_PID" 2>/dev/null
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -f "$CLIENT" "$LOG" "$TRACE"
    make fclean >/dev/null 2>&1 || true
}

trap cleanup EXIT

echo "=== ACCEPT TEST ==="

if [ "$#" -gt 3 ]; then
    echo "Usage: $0 [server-executable] [config-file] [special-flags]"
    exit 1
fi

SERVER="${1:-./webserv}"
CONFIG="${2:-tests/integration/basic.conf}"
SPECIAL_FLAGS="${3:-}"

# 1. Rebuild the server because previous test suites clean their artifacts.
echo "[1/4] Rebuilding server..."

if ! make fclean || ! make SPECIAL_FLAGS="$SPECIAL_FLAGS"; then
    echo "FAIL: Server build"
    exit 1
fi

# 1. Check server executable
if [ ! -x "$SERVER" ]; then
    echo "FAIL: Server executable not found"
    exit 1
fi

# 2. Compile test client
echo "[2/4] Compiling test client..."

if ! c++ -Wall -Wextra -Werror -std=c++98 \
    "$CLIENT_SOURCE" -o "$CLIENT"; then
    echo "FAIL: Client compilation"
    exit 1
fi

# 3. Start server under strace
echo "[3/4] Starting server..."
echo "[INFO] Using config: $CONFIG"

setsid strace -f -e trace=accept,accept4 -o "$TRACE" "$SERVER" "$CONFIG" \
    > "$LOG" 2>&1 &

SERVER_PID=$!

# 4. Run test client
echo "[4/4] Running client test..."

if ! "$CLIENT"; then
    echo "FAIL: Client connections"
    exit 1
fi

sleep 1

ACCEPTED=$(grep -E 'accept4?\(' "$TRACE" | grep -v '= -1' | wc -l)

echo "Successful accept calls: $ACCEPTED"

if [ "$ACCEPTED" -ge 5 ]; then
    echo "[PASS] Config parsed and server accepted connections"
    echo "ACCEPT: PASS"
    exit 0
else
    echo "ACCEPT: FAIL"
    exit 1
fi