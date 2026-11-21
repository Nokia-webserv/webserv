#!/usr/bin/env python3

import socket
import os
import sys
from common import log_count, start_server_simple, stop_server, wait_for_log_count, wait_for_server_simple

HOST = "127.0.0.1"
PORT = 8080

STARTUP_TIMEOUT = 10
CLIENT_TIMEOUT = int(os.environ.get("TEST_TIMEOUT", "10"))
WAIT_TIME = CLIENT_TIMEOUT + 5


def test_idle_client(server):
    client = None

    try:
        print("[INFO] Connecting idle client")
        client = socket.create_connection((HOST, PORT), timeout=3)
        client.settimeout(2)
        timeout_count = log_count("timeout")

        print("[INFO] Sending no data")
        print("[INFO] Waiting", WAIT_TIME, "seconds")
        wait_for_log_count("timeout", timeout_count + 1, WAIT_TIME)

        if server.poll() is not None:
            raise AssertionError(
                "Server exited with code " + str(server.returncode)
            )

        try:
            data = client.recv(1)

            if data == b"":
                print("[PASS] Server timed out and closed the idle connection")
            else:
                raise AssertionError(
                    "Unexpected data received: " + repr(data)
                )

        except socket.timeout:
            raise AssertionError(
                "Connection is still open after the timeout period"
            )

    finally:
        if client:
            client.close()
            print("[INFO] Client disconnected")


def main():
    if len(sys.argv) != 3:
        print(
            "Usage: python3 c1_idle_client_timeout.py "
            "<server_executable> <config-file>"
        )
        sys.exit(1)

    server = None

    try:
        server = start_server_simple(sys.argv[1:])
        wait_for_server_simple(server)

        test_idle_client(server)

        print("\n[PASS] C1 - Idle client timeout test passed")

    except Exception as error:
        print("\n[FAIL] C1 -", error)
        sys.exit(1)

    finally:
        stop_server(server)

        print("[INFO] Test finished")


if __name__ == "__main__":
    main()