
#!/usr/bin/env bash
set -euo pipefail

SERVER="${1:-./webserv}"
CONFIG="${2:-tests/redirects/redirect_test.conf}"

HOST="127.0.0.1"
PORT=8080
BASE_URL="http://${HOST}:${PORT}"

TMP_DIR=$(mktemp -d)
SERVER_PID=""

cleanup() {
    if [[ -n "$SERVER_PID" ]]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -rf "$TMP_DIR"
}

trap cleanup EXIT

pass() {
    echo "[PASS] $1"
}

fail() {
    echo "[FAIL] $1"

    if [[ -f "$TMP_DIR/headers" ]]; then
        echo "[DEBUG] Response headers:"
        cat "$TMP_DIR/headers"
    fi

    if [[ -f "$TMP_DIR/pipeline" ]]; then
        echo "[DEBUG] Pipelined response:"
        head -c 1000 "$TMP_DIR/pipeline"
        echo
    fi

    if [[ -f "$TMP_DIR/webserv.log" ]]; then
        echo "[DEBUG] Webserv log:"
        cat "$TMP_DIR/webserv.log"
    fi

    exit 1
}

check_status_and_location() {
    local path="$1"
    local expected_status="$2"
    local expected_location="$3"

    local headers="$TMP_DIR/headers"
    local body="$TMP_DIR/body"

    curl -sS --max-time 5 \
        -D "$headers" \
        -o "$body" \
        "${BASE_URL}${path}" \
        || fail "Request failed: $path"

    # Validate HTTP status.
    tr -d '\r' < "$headers" |
        grep -q "^HTTP/1.1 ${expected_status} " \
        || fail "$path: incorrect HTTP status"

    # Validate exact Location header.
    tr -d '\r' < "$headers" |
        grep -Fxiq "Location: ${expected_location}" \
        || fail "$path: incorrect Location header"

    # Validate Content-Type.
    tr -d '\r' < "$headers" |
        grep -Fxiq "Content-Type: text/html; charset=utf-8" \
        || fail "$path: incorrect Content-Type"

    # Validate Content-Length against actual body size.
    local actual_length
    actual_length=$(wc -c < "$body" | tr -d '[:space:]')

    local declared_length
    declared_length=$(
        tr -d '\r' < "$headers" |
        awk 'tolower($1) == "content-length:" {print $2}'
    )

    [[ "$actual_length" == "$declared_length" ]] \
        || fail "$path: incorrect Content-Length"
}

echo "[INFO] Starting Webserv..."
"$SERVER" "$CONFIG" > "$TMP_DIR/webserv.log" 2>&1 &
SERVER_PID=$!

# Wait for Webserv to start listening.
ready=0

for ((i = 0; i < 50; i++)); do
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        fail "Webserv exited during startup"
    fi

    if (echo > "/dev/tcp/${HOST}/${PORT}") 2>/dev/null; then
        ready=1
        break
    fi

    sleep 0.1
done

[[ "$ready" -eq 1 ]] || fail "Webserv did not start listening"

echo "[INFO] Testing HTTP redirects"

# --------------------------------------------------
# TEST 1: 301 Permanent Redirect
# --------------------------------------------------

check_status_and_location "/old" "301" "/new"

cmp -s "$TMP_DIR/body" "www/redirects/301.html" \
    || fail "301: incorrect HTML response body"

pass "301 permanent redirect"

# --------------------------------------------------
# TEST 2: 302 Temporary Redirect
# --------------------------------------------------

check_status_and_location "/temporary" "302" "/new"

cmp -s "$TMP_DIR/body" "www/redirects/302.html" \
    || fail "302: incorrect HTML response body"

pass "302 temporary redirect"

# --------------------------------------------------
# TEST 3: 405 Method Restriction
# --------------------------------------------------

curl -sS --max-time 5 \
    -D "$TMP_DIR/headers" \
    -o "$TMP_DIR/body" \
    -X POST "${BASE_URL}/old" \
    || fail "POST /old request failed"

tr -d '\r' < "$TMP_DIR/headers" |
    grep -q '^HTTP/1.1 405 ' \
    || fail "POST /old: expected 405 Method Not Allowed"

tr -d '\r' < "$TMP_DIR/headers" |
    grep -Fxiq "Allow: GET" \
    || fail "POST /old: missing Allow: GET"

pass "405 method restriction"

# --------------------------------------------------
# TEST 4: Two Pipelined Redirect Requests
# --------------------------------------------------

# nc -w is supported by the Netcat versions used on
# macOS and typical Linux CI runners.
#
# The connection may remain open after both responses,
# so we use a timeout to avoid hanging indefinitely.

printf 'GET /old HTTP/1.1\r\nHost: localhost\r\n\r\nGET /temporary HTTP/1.1\r\nHost: localhost\r\n\r\n' |
    nc -w 5 "$HOST" "$PORT" > "$TMP_DIR/pipeline" \
    || true

# Extract response status lines using Bash 3.2-compatible code.
statuses=()

while IFS= read -r line; do
    statuses+=("$line")
done < <(
    tr -d '\r' < "$TMP_DIR/pipeline" |
    grep -a '^HTTP/1.1 '
)

[[ "${#statuses[@]}" -eq 2 ]] \
    || fail "Pipelining: expected exactly 2 responses, got ${#statuses[@]}"

[[ "${statuses[0]}" == "HTTP/1.1 301 Moved Permanently" ]] \
    || fail "Pipelining: first response should be 301"

[[ "${statuses[1]}" == "HTTP/1.1 302 Found" ]] \
    || fail "Pipelining: second response should be 302"

pass "Two pipelined redirects on one connection"

# --------------------------------------------------
# TEST 5: Prefix Location Matching
# --------------------------------------------------

check_status_and_location "/older" "301" "/new"

pass "Prefix location matching /older"

echo
echo "[RESULT] All 5 redirect integration tests PASSED"
