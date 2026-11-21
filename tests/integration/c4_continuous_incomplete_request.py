#!/usr/bin/env python3

import socket
import os
import sys
import time
from common import start_server_simple, stop_server, wait_for_log_file, wait_for_server_simple

HOST = "127.0.0.1"
PORT = 8080

STARTUP_TIMEOUT = 10
CLIENT_TIMEOUT = int(os.environ.get("TEST_TIMEOUT", "10"))
TEST_DURATION = CLIENT_TIMEOUT + 10
SEND_INTERVAL = max(0.1, CLIENT_TIMEOUT / 2.0)

REQUEST_PARTS = [
    b"GET / HTTP/1.1\r\n",
    b"Host: localhost\r\n",
    b"X-Slow-Header: part1",
    b"part2",
    b"part3",
    b"part4",
    b"part5",
    b"part6",
]


def test_continuous_incomplete_request(server):
    client = None

    try:
        print("[INFO] Connecting slow client")
        client = socket.create_connection((HOST, PORT), timeout=3)
        client.settimeout(2)
        wait_for_log_file("request status=INCOMPLETE_HEAD")

        start_time = time.time()
        part_index = 0

        print("[INFO] Sending incomplete request while staying active")
        print("[INFO] Test duration:", TEST_DURATION, "seconds")

        while time.time() - start_time < TEST_DURATION:
            if server.poll() is not None:
                raise AssertionError(
                    "Server exited with code " + str(server.returncode)
                )

            part = REQUEST_PARTS[part_index % len(REQUEST_PARTS)]

            try:
                client.sendall(part)
            except OSError:
                raise AssertionError(
                    "Server closed an active incomplete request"
                )

            part_index += 1
            print("[INFO] Sent fragment", part_index)

            time.sleep(SEND_INTERVAL)

        if server.poll() is not None:
            raise AssertionError(
                "Server exited with code " + str(server.returncode)
            )

        wait_for_log_file("request status=INCOMPLETE_HEAD")
        print("[PASS] Active incomplete request remained open")
        print("[PASS] Server remained operational")

    finally:
        if client:
            client.close()
            print("[INFO] Client disconnected")


def main():
    if len(sys.argv) != 3:
        print(
            "Usage: python3 c4_continuous_incomplete_request.py "
            "<server_executable> <config-file>"
        )
        sys.exit(1)

    server = None

    try:
        server = start_server_simple(sys.argv[1:])
        wait_for_server_simple(server)

        test_continuous_incomplete_request(server)

        print("\n[PASS] C4 - Continuous incomplete request test passed")

    except Exception as error:
        print("\n[FAIL] C4 -", error)
        sys.exit(1)

    finally:
        stop_server(server)

        print("[INFO] Test finished")


if __name__ == "__main__":
    main()