#!/bin/bash

HOST="127.0.0.1"
PORT=8080

echo "=== LISTENING TEST ==="

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <server-executable> <config-file>"
    exit 1
fi

SERVER="$1"
CONFIG="$2"

echo "[INFO] Starting server: $SERVER"
echo "[INFO] Using config: $CONFIG"

# Start server
"$SERVER" "$CONFIG" &
SERVER_PID=$!

# Ensure server is stopped if the script is interrupted
trap 'kill -TERM "$SERVER_PID" 2>/dev/null; wait "$SERVER_PID" 2>/dev/null' EXIT

# Wait for listening socket
LISTENING=0

for i in $(seq 1 50); do
    if nc -z "$HOST" "$PORT" 2>/dev/null; then
        LISTENING=1
        break
    fi

    # Check whether server has already exited
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "LISTENING: FAIL (server exited during startup)"
        exit 1
    fi

    sleep 0.1
done

if [ "$LISTENING" -eq 1 ]; then
    echo "[PASS] Config parsed and server is listening"
    echo "LISTENING: PASS"
    exit 0
else
    echo "[FAIL] Config parsing or server startup failed"
    echo "LISTENING: FAIL (port $PORT not accepting connections)"
    exit 1
fi
