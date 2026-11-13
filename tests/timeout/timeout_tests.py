#!/usr/bin/env python3
"""Webserv timeout and HTTP regression tests (Python standard library only)."""

import argparse
import socket
import sys
import threading
import time

TIMEOUT_SECONDS = 30
MIN_TIMEOUT = 28
MAX_TIMEOUT = 35

VALID_REQUEST = b"DELETE / HTTP/1.1\r\nHost: localhost\r\n\r\n"
INCOMPLETE_REQUEST = b"GET / HTTP/1.1\r\nHost: localhost\r\n"


def connect(host, port, timeout=5):
    conn = socket.create_connection((host, port), timeout=timeout)
    conn.settimeout(timeout)
    return conn


def read_response(conn, initial=b""):
    """Read one complete Content-Length HTTP response; return (status, headers, body, remainder)."""
    data = initial
    while b"\r\n\r\n" not in data:
        chunk = conn.recv(65536)
        if not chunk:
            raise AssertionError("Connection closed before response headers")
        data += chunk
        if len(data) > 65536 and b"\r\n\r\n" not in data:
            raise AssertionError("Response headers too large")

    raw_headers, sep, payload = data.partition(b"\r\n\r\n")
    assert sep
    lines = raw_headers.split(b"\r\n")
    status = lines[0].decode("ascii", errors="replace")
    headers = {}
    for line in lines[1:]:
        if b":" not in line:
            raise AssertionError("Invalid response header: %r" % line)
        name, value = line.split(b":", 1)
        headers[name.strip().lower()] = value.strip()
    if b"content-length" not in headers:
        raise AssertionError("Missing Content-Length header")
    length = int(headers[b"content-length"])
    if length < 0:
        raise AssertionError("Negative Content-Length")
    while len(payload) < length:
        chunk = conn.recv(65536)
        if not chunk:
            raise AssertionError("Connection closed before full response body")
        payload += chunk
    return status, headers, payload[:length], payload[length:]


def test_incomplete_408(host, port):
    with connect(host, port, 5) as conn:
        conn.settimeout(42)
        conn.sendall(INCOMPLETE_REQUEST)
        started = time.monotonic()
        status, headers, body, extra = read_response(conn)
        elapsed = time.monotonic() - started
        assert MIN_TIMEOUT <= elapsed <= MAX_TIMEOUT, "408 arrived after %.1fs" % elapsed
        assert status == "HTTP/1.1 408 Request Timeout", "Unexpected status: " + status
        assert headers.get(b"connection", b"").lower() == b"close", "408 should close connection"
        assert body, "408 response body was empty"
        assert not extra, "Unexpected bytes after 408 response"
        # Verify that the server closes after sending the error page.
        assert conn.recv(1) == b"", "Server did not close after 408"
        return "HTTP 408 after %.1fs, full body received, connection closed" % elapsed


def test_idle_disconnect(host, port):
    with connect(host, port, 5) as conn:
        conn.settimeout(42)
        started = time.monotonic()
        data = conn.recv(1)
        elapsed = time.monotonic() - started
        assert data == b"", "Idle connection unexpectedly received data"
        assert MIN_TIMEOUT <= elapsed <= MAX_TIMEOUT, "Idle disconnect after %.1fs" % elapsed
        return "Idle connection closed after %.1fs without a response" % elapsed


def test_pipelined_errors(host, port):
    with connect(host, port, 5) as conn:
        conn.sendall(VALID_REQUEST + VALID_REQUEST)
        remainder = b""
        for number in (1, 2):
            status, headers, body, remainder = read_response(conn, remainder)
            assert status == "HTTP/1.1 405 Method Not Allowed", (
                "Response %d: unexpected status %s" % (number, status)
            )
            assert headers.get(b"allow", b"").upper() == b"GET", (
                "Response %d: expected Allow: GET" % number
            )
            assert body, "Response %d: empty body" % number
        assert not remainder, "Unexpected data after second response"
        return "Two complete 405 responses received in order on one connection"


def test_concurrent(host, port, clients):
    results = [None] * clients
    gate = threading.Event()

    def worker(index):
        try:
            gate.wait(timeout=5)
            with connect(host, port, 10) as conn:
                conn.settimeout(10)
                conn.sendall(VALID_REQUEST)
                status, headers, body, extra = read_response(conn)
                assert status == "HTTP/1.1 405 Method Not Allowed", status
                assert headers.get(b"allow", b"").upper() == b"GET"
                assert body, "Empty error page"
                assert not extra, "Unexpected trailing data"
            results[index] = None
        except Exception as exc:
            results[index] = "%s: %s" % (type(exc).__name__, exc)

    threads = [threading.Thread(target=worker, args=(i,)) for i in range(clients)]
    for thread in threads:
        thread.start()
    gate.set()
    for thread in threads:
        thread.join(timeout=20)
    failed = [(i, err) for i, err in enumerate(results) if err is not None]
    hanging = [i for i, thread in enumerate(threads) if thread.is_alive()]
    assert not hanging, "%d client threads did not finish: %s" % (len(hanging), hanging[:10])
    assert not failed, "%d/%d clients failed: %s" % (len(failed), clients, failed[:5])
    return "%d/%d concurrent clients received complete HTTP 405 responses" % (clients, clients)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--clients", type=int, default=50)
    args = parser.parse_args()
    if args.clients < 1:
        parser.error("--clients must be at least 1")

    tests = [
        ("Pipelined HTTP errors", lambda: test_pipelined_errors(args.host, args.port)),
        ("Concurrent HTTP responses", lambda: test_concurrent(args.host, args.port, args.clients)),
        ("Incomplete request -> 408", lambda: test_incomplete_408(args.host, args.port)),
        ("Idle connection -> close", lambda: test_idle_disconnect(args.host, args.port)),
    ]

    passed = 0
    for name, run in tests:
        print("[RUN ] %s" % name, flush=True)
        try:
            detail = run()
        except Exception as exc:
            print("[FAIL] %s: %s: %s" % (name, type(exc).__name__, exc), flush=True)
        else:
            passed += 1
            print("[PASS] %s: %s" % (name, detail), flush=True)

    print("\n[RESULT] %d/%d tests passed" % (passed, len(tests)))
    return 0 if passed == len(tests) else 1


if __name__ == "__main__":
    sys.exit(main())
