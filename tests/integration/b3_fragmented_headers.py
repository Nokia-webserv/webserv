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

FRAGMENTS = [
    b"GET / HTTP/1.1\r\n",
    b"Host: localhost\r\n",
    b"Connection: keep-alive\r\n\r\n",
]


def test_fragmented_headers(server):
    client = None

    try:
        print("[INFO] Connecting client")
        client = socket.create_connection((HOST, PORT), timeout=3)

        for index, fragment in enumerate(FRAGMENTS, start=1):
            client.sendall(fragment)
            print(
                f"[INFO] Fragment {index}/{len(FRAGMENTS)} sent: "
                f"{fragment!r}"
            )

            if index < len(FRAGMENTS):
                time.sleep(FRAGMENT_DELAY)

        print("[INFO] All header fragments sent")

        wait_for_log_file("request status=COMPLETE", OBSERVATION_TIME)

        if server.poll() is not None:
            raise AssertionError(
                "Server exited with code " + str(server.returncode)
            )

        print("[PASS] Fragmented headers reached COMPLETE")

    finally:
        if client:
            client.close()
            print("[INFO] Client disconnected")


def main():
    if len(sys.argv) != 3:
        print(
            "Usage: python3 b3_fragmented_headers.py "
            "<server_executable> <config-file>"
        )
        sys.exit(1)

    server = None

    try:
        server = start_server_simple(sys.argv[1:])
        wait_for_server_simple(server)

        test_fragmented_headers(server)

        print("\n[PASS] B3 - Fragmented headers test passed")

    except Exception as error:
        print("\n[FAIL] B3 -", error)
        sys.exit(1)

    finally:
        stop_server(server)

        print("[INFO] Test finished")


if __name__ == "__main__":
    main()