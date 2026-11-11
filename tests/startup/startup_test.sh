#!/bin/bash

echo "=== STARTUP TEST ==="

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <server-executable> <config-file>"
    exit 1
fi

SERVER="$1"
CONFIG="$2"

echo "[INFO] Starting server: $SERVER"
echo "[INFO] Using config: $CONFIG"

"$SERVER" "$CONFIG" &
SERVER_PID=$!

cleanup()
{
    kill -TERM "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
}

trap cleanup EXIT INT TERM

sleep 1

if kill -0 "$SERVER_PID" 2>/dev/null; then
    echo "[PASS] Server process started with config"
    echo "STARTUP: PASS"
else
    echo "[FAIL] Config parsing or server startup failed"
    echo "STARTUP: FAIL"
    exit 1
fi