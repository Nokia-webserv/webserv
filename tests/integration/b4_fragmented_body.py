#!/usr/bin/env python3

import socket
import sys
import time
from common import start_server_simple, stop_server, wait_for_log_file, wait_for_server_simple

HOST = "127.0.0.1"
PORT = 8080

STARTUP_TIMEOUT = 10
FRAGMENT_DELAY = 0.5
OBSERVATION_TIME = 2

BODY_FRAGMENTS = [
    b"Hello ",
    b"from ",
    b"fragmented body!",
]

BODY = b"".join(BODY_FRAGMENTS)

HEADERS = (
    b"POST /upload HTTP/1.1\r\n"
    b"Host: localhost\r\n"
    + f"Content-Length: {len(BODY)}\r\n".encode()
    + b"\r\n"
)


def test_fragmented_body(server):
    client = None

    try:
        print("[INFO] Connecting client")
        client = socket.create_connection((HOST, PORT), timeout=3)

        print("[INFO] Sending request headers")
        client.sendall(HEADERS)

        time.sleep(FRAGMENT_DELAY)

        for index, fragment in enumerate(BODY_FRAGMENTS, start=1):
            client.sendall(fragment)

            print(
                f"[INFO] Body fragment {index}/{len(BODY_FRAGMENTS)} "
                f"sent: {fragment!r}"
            )

            if index < len(BODY_FRAGMENTS):
                time.sleep(FRAGMENT_DELAY)

        print("[INFO] Complete body sent:", len(BODY), "bytes")

        wait_for_log_file("request status=COMPLETE", OBSERVATION_TIME)

        if server.poll() is not None:
            raise AssertionError(
                "Server exited with code " + str(server.returncode)
            )

        print("[PASS] Fragmented body reached COMPLETE")

    finally:
        if client:
            client.close()
            print("[INFO] Client disconnected")


def main():
    if len(sys.argv) != 3:
        print(
            "Usage: python3 b4_fragmented_body.py "
            "<server_executable> <config-file>"
        )
        sys.exit(1)

    server = None

    try:
        server = start_server_simple(sys.argv[1:])
        wait_for_server_simple(server)

        test_fragmented_body(server)

        print("\n[PASS] B4 - Fragmented body test passed")

    except Exception as error:
        print("\n[FAIL] B4 -", error)
        sys.exit(1)

    finally:
        stop_server(server)

        print("[INFO] Test finished")


if __name__ == "__main__":
    main()