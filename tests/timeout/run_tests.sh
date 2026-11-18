#!/usr/bin/env bash
# Run the timeout regression suite against a fresh Webserv process.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BINARY="${1:-$ROOT/webserv}"
CONFIG="${2:-$ROOT/test1.conf}"
HOST="${WEBSERV_TEST_HOST:-127.0.0.1}"
PORT="${WEBSERV_TEST_PORT:-8080}"
CLIENTS="${WEBSERV_TEST_CLIENTS:-50}"
LOG="$(mktemp "${TMPDIR:-/tmp}/webserv-timeout.XXXXXX")"
SERVER_PID=""

cleanup() {
    if [[ -n "$SERVER_PID" ]]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -f "$LOG"
}
trap cleanup EXIT

if [[ ! -x "$BINARY" ]]; then
    echo "[ERROR] Executable not found: $BINARY" >&2
    exit 1
fi
if [[ ! -f "$CONFIG" ]]; then
    echo "[ERROR] Config not found: $CONFIG" >&2
    exit 1
fi

# Avoid accidentally testing a server that was already running.
if python3 - "$HOST" "$PORT" <<'PY'
import socket, sys
s = socket.socket()
s.settimeout(0.5)
try:
    sys.exit(0 if s.connect_ex((sys.argv[1], int(sys.argv[2]))) == 0 else 1)
finally:
    s.close()
PY
then
    echo "[ERROR] Port $PORT already has a listener. Stop the existing Webserv first." >&2
    exit 1
fi

echo "[INFO] Starting Webserv with $CONFIG"
"$BINARY" "$CONFIG" >"$LOG" 2>&1 &
SERVER_PID=$!

# Wait for startup; print log if the process exits or never begins listening.
ready=false
for _ in {1..50}; do
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "[ERROR] Webserv exited during startup" >&2
        cat "$LOG" >&2
        exit 1
    fi
    if python3 - "$HOST" "$PORT" <<'PY'
import socket, sys
s = socket.socket()
s.settimeout(0.2)
try:
    sys.exit(0 if s.connect_ex((sys.argv[1], int(sys.argv[2]))) == 0 else 1)
finally:
    s.close()
PY
    then
        ready=true
        break
    fi
    sleep 0.1
done
if [[ "$ready" != true ]]; then
    echo "[ERROR] Webserv did not listen on $HOST:$PORT" >&2
    cat "$LOG" >&2
    exit 1
fi

set +e
python3 "$ROOT/tests/timeout/timeout_tests.py" --host "$HOST" --port "$PORT" --clients "$CLIENTS"
status=$?
set -e
if [[ "$status" -ne 0 ]]; then
    echo "[INFO] Server output:" >&2
    cat "$LOG" >&2
fi
exit "$status"
