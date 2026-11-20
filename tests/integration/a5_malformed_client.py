#!/usr/bin/env python3

import socket
import sys
from common import start_server_simple, stop_server, wait_for_log_file, wait_for_server_simple

HOST = "127.0.0.1"
PORT = 8080

STARTUP_TIMEOUT = 10
OBSERVATION_TIME = 2
CLIENT_TIMEOUT = 3

# Invalid HTTP version.
MALFORMED_REQUEST = (
    b"GET / HTTP/9.9\r\n"
    b"Host: localhost\r\n"
    b"\r\n"
)

VALID_REQUEST = (
    b"GET /valid HTTP/1.1\r\n"
    b"Host: localhost\r\n"
    b"\r\n"
)


def test_malformed_client(server):
    malformed_client = None
    valid_client = None

    try:
        # Client A: send malformed request.
        print("[INFO] Connecting malformed client")

        malformed_client = socket.create_connection(
            (HOST, PORT), timeout=CLIENT_TIMEOUT
        )

        print("[INFO] Sending malformed HTTP request")
        malformed_client.sendall(MALFORMED_REQUEST)
        wait_for_log_file("request status=ERROR HTTP_VERSION_NOT_SUPPORTED", OBSERVATION_TIME)

        if server.poll() is not None:
            raise AssertionError(
                "Server crashed after malformed request"
            )

        print("[PASS] Server remained operational")

        # Client B: send valid request.
        print("[INFO] Connecting valid client")

        valid_client = socket.create_connection(
            (HOST, PORT), timeout=CLIENT_TIMEOUT
        )

        print("[INFO] Sending valid HTTP request")
        valid_client.sendall(VALID_REQUEST)
        wait_for_log_file("request status=COMPLETE", OBSERVATION_TIME)

        if server.poll() is not None:
            raise AssertionError(
                "Server crashed after valid request"
            )

        print("[PASS] Valid client was not prevented from connecting")
        print("[PASS] Server remained operational")

    finally:
        if valid_client:
            valid_client.close()

        if malformed_client:
            malformed_client.close()


def main():
    if len(sys.argv) != 3:
        print(
            "Usage: python3 a5_malformed_client.py "
            "<server_executable> <config-file>"
        )
        sys.exit(1)

    server = None

    try:
        server = start_server_simple(sys.argv[1:])
        wait_for_server_simple(server)

        test_malformed_client(server)

        print("\n[PASS] A5 - Malformed client isolation test passed")

    except Exception as error:
        print("\n[FAIL] A5 -", error)
        sys.exit(1)

    finally:
        stop_server(server)

        print("[INFO] Test finished")


if __name__ == "__main__":
    main()