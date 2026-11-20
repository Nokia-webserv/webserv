#!/usr/bin/env python3

import socket
import sys
from common import start_server_simple, stop_server, wait_for_log_file, wait_for_server_simple

HOST = "127.0.0.1"
PORT = 8080

STARTUP_TIMEOUT = 10
OBSERVATION_TIME = 2

MAX_BODY_SIZE = 10 * 1024 * 1024
BODY_SIZE = MAX_BODY_SIZE + 1

BODY = b"A" * BODY_SIZE

HEADERS = (
    b"POST /upload HTTP/1.1\r\n"
    b"Host: localhost\r\n"
    + f"Content-Length: {BODY_SIZE}\r\n".encode()
    + b"\r\n"
)

REQUEST = HEADERS + BODY


def test_request_over_limit(server):
    client = None

    try:
        print("[INFO] Connecting client")
        client = socket.create_connection((HOST, PORT), timeout=3)

        print("[INFO] Configured body limit:", MAX_BODY_SIZE, "bytes")
        print("[INFO] Actual body size:", BODY_SIZE, "bytes")
        print("[INFO] Sending oversized request")

        try:
            client.sendall(REQUEST)
        except OSError:
            print("[INFO] Server closed the oversized request")

        print("[INFO] Request sent successfully")

        wait_for_log_file("request status=ERROR CONTENT_TOO_LARGE", OBSERVATION_TIME)

        if server.poll() is not None:
            raise AssertionError(
                "Server exited with code " + str(server.returncode)
            )

        print("[PASS] Oversized request reached the parser error path")

    finally:
        if client:
            client.close()
            print("[INFO] Client disconnected")


def main():
    if len(sys.argv) != 3:
        print(
            "Usage: python3 b6_request_over_limit.py "
            "<server_executable> <config-file>"
        )
        sys.exit(1)

    server = None

    try:
        server = start_server_simple(sys.argv[1:])
        wait_for_server_simple(server)

        test_request_over_limit(server)

        print("\n[PASS] B6 - Request over limit test passed")

    except Exception as error:
        print("\n[FAIL] B6 -", error)
        sys.exit(1)

    finally:
        stop_server(server)

        print("[INFO] Test finished")


if __name__ == "__main__":
    main()