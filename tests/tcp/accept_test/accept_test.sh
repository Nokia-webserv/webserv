#!/bin/bash

SERVER="./webserv"
CLIENT_SOURCE="./tests/tcp/accept_test/accept_test.cpp"
CLIENT="./tests/tcp/accept_test/accept_test"
HOST="127.0.0.1"
PORT=8080

LOG="/tmp/webserv_accept_test.log"
TRACE="/tmp/webserv_accept_trace.log"

SERVER_PID=""

cleanup()
{
    if [ -n "$SERVER_PID" ]; then
        kill -TERM -- "-$SERVER_PID" 2>/dev/null
        wait "$SERVER_PID" 2>/dev/null
    fi
}

trap cleanup EXIT

echo "=== ACCEPT TEST ==="

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <server-executable> <config-file>"
    exit 1
fi

SERVER="$1"
CONFIG="$2"

# 1. Check server executable
if [ ! -x "$SERVER" ]; then
    echo "FAIL: Server executable not found"
    exit 1
fi

# 2. Compile test client
echo "[1/3] Compiling test client..."

if ! c++ -Wall -Wextra -Werror -std=c++98 \
    "$CLIENT_SOURCE" -o "$CLIENT"; then
    echo "FAIL: Client compilation"
    exit 1
fi

# 3. Start server under strace
echo "[2/3] Starting server..."
echo "[INFO] Using config: $CONFIG"

setsid strace -f -e trace=accept,accept4 -o "$TRACE" "$SERVER" "$CONFIG" \
    > "$LOG" 2>&1 &

SERVER_PID=$!

# 4. Run test client
echo "[3/3] Running client test..."

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