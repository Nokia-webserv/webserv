#!/bin/bash

PORT=8080
HOST="127.0.0.1"

echo "=== PORT CONFLICT TEST ==="

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <server-executable> <config-file>"
    exit 1
fi

SERVER="$1"
CONFIG="$2"

echo "[INFO] Starting server: $SERVER"
echo "[INFO] Using config: $CONFIG"

# Start the first server
"$SERVER" "$CONFIG" &
SERVER_PID=$!

cleanup()
{
    kill -TERM "$SERVER_PID" 2>/dev/null
    wait "$SERVER_PID" 2>/dev/null
}

trap cleanup EXIT

# Wait until the first server starts listening
LISTENING=0

for i in $(seq 1 50); do
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "FAIL: First server exited during startup"
        exit 1
    fi

    if nc -z "$HOST" "$PORT" 2>/dev/null; then
        LISTENING=1
        break
    fi

    sleep 0.1
done

if [ "$LISTENING" -eq 0 ]; then
    echo "FAIL: First server did not start listening"
    exit 1
fi

echo "[PASS] Config parsed and first server is listening on port $PORT"

# Try to start a second server on the same port
"$SERVER" "$CONFIG" > /tmp/webserv_port_conflict.log 2>&1
SECOND_EXIT_CODE=$?

if [ "$SECOND_EXIT_CODE" -ne 0 ]; then
    echo "[PASS] Second server failed to start on occupied port"
    exit 0
else
    echo "FAIL: Second server started on an occupied port"
    exit 1
fi