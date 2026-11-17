#!/usr/bin/env python3
"""C7: Incoming client bytes must NOT refresh a stalled outgoing write timeout.

Usage: python3 tests/integration/c7_stalled_write_incoming_activity_test.py ./webserv
Expected runtime: roughly 36-48 seconds.
"""

import socket
import sys
import time

from stalled_write_common import (
    BIG_FILE_SIZE,
    SERVER_TIMEOUT,
    check_stalled_connection_closed,
    request_without_reading,
    running_server,
)


def main(server_path):
    with running_server(server_path) as port:
        print("[INFO] Webserv started", flush=True)
        with request_without_reading(port) as client:
            print(f"[INFO] Requested {BIG_FILE_SIZE // (1024 * 1024)} MiB; not reading response", flush=True)
            # Feed one incomplete next request, one byte at a time. Do not send
            # a terminating CRLFCRLF: this must not generate another response.
            # Keep total input below the configured max-header-size limit.
            keep_sending = b"GET /next HTTP/1.1\r\nHost: localhost\r\nX-Padding: " + b"x" * 200
            started = time.monotonic()
            sent = 0
            while time.monotonic() - started < SERVER_TIMEOUT + 6:
                if sent < len(keep_sending):
                    try:
                        client.sendall(keep_sending[sent:sent + 1])
                        sent += 1
                    except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
                        print("[INFO] Server closed while client was sending", flush=True)
                        break
                    except socket.timeout:
                        # An outgoing stall might block the server's read side;
                        # TCP flow control can then block client writes too.
                        print("[INFO] Client send timed out", flush=True)
                time.sleep(1)
                
            elapsed = time.monotonic() - started
            print(f"[DEBUG] Connection closed or send loop ended after {elapsed:.2f}s")

            tolerance = max(2, SERVER_TIMEOUT * 0.1)

            if elapsed < SERVER_TIMEOUT - tolerance:
                raise AssertionError(
                    f"Connection closed too early after {elapsed:.2f}s "
                    f"(expected approximately {SERVER_TIMEOUT}s)"
                )
            received, reason = check_stalled_connection_closed(client)
            print(f"[PASS] Incoming activity did not postpone stalled-write timeout ({reason}, {received} buffered bytes)")


if __name__ == "__main__":

    if len(sys.argv) != 2:
        print(f"Usage: python3 {sys.argv[0]} ./webserv", file=sys.stderr)
        sys.exit(2)
    try:
        main(sys.argv[1])
    except (AssertionError, OSError, RuntimeError) as exc:
        print(f"[FAIL] C7: {exc}", file=sys.stderr)
        sys.exit(1)
