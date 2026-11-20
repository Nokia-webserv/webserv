#!/usr/bin/env python3

import socket
import sys
import time
from common import start_server_simple, stop_server, wait_for_log_file, wait_for_server_simple

HOST = "127.0.0.1"
PORT = 8080

STARTUP_TIMEOUT = 10
BYTE_DELAY = 0.01
OBSERVATION_TIME = 2

REQUEST = (
    b"GET / HTTP/1.1\r\n"
    b"Host: localhost\r\n"
    b"\r\n"
)


def test_one_byte_at_a_time(server):
    client = None

    try:
        print("[INFO] Connecting client")
        client = socket.create_connection((HOST, PORT), timeout=3)

        print("[INFO] Sending request one byte at a time")

        for byte in REQUEST:
            client.sendall(bytes([byte]))
            time.sleep(BYTE_DELAY)

        print("[INFO] All", len(REQUEST), "bytes sent")

        wait_for_log_file("request status=COMPLETE", OBSERVATION_TIME)

        if server.poll() is not None:
            raise AssertionError(
                "Server exited with code " + str(server.returncode)
            )

        print("[PASS] One-byte request reached COMPLETE")

    finally:
        if client:
            client.close()
            print("[INFO] Client disconnected")


def main():
    if len(sys.argv) != 3:
        print(
            "Usage: python3 b2_one_byte_at_a_time.py "
            "<server_executable> <config-file>"
        )
        sys.exit(1)

    server = None

    try:
        server = start_server_simple(sys.argv[1:])
        wait_for_server_simple(server)

        test_one_byte_at_a_time(server)

        print("\n[PASS] B2 - One byte at a time test passed")

    except Exception as error:
        print("\n[FAIL] B2 -", error)
        sys.exit(1)

    finally:
        stop_server(server)

        print("[INFO] Test finished")


if __name__ == "__main__":
    main()