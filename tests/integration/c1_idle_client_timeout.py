#!/usr/bin/env python3

import socket
import os
import subprocess
import sys
import time
from common import log_count, wait_for_log_count

HOST = "127.0.0.1"
PORT = 8080

STARTUP_TIMEOUT = 10
CLIENT_TIMEOUT = int(os.environ.get("TEST_TIMEOUT", "10"))
WAIT_TIME = CLIENT_TIMEOUT + 5


def start_server(command):
    print("[INFO] Starting server:", " ".join(command))
    print("[INFO] Using config:", command[-1])
    server = subprocess.Popen(command)
    print("[PASS] Server process started with config")
    return server


def wait_for_server(server):
    deadline = time.time() + STARTUP_TIMEOUT

    while time.time() < deadline:
        if server.poll() is not None:
            raise RuntimeError(
                "Server exited with code " + str(server.returncode)
            )

        try:
            probe = socket.create_connection((HOST, PORT), timeout=1)
            probe.close()
            print("[PASS] Config parsed and server is listening")
            return
        except OSError:
            time.sleep(0.2)

    raise RuntimeError("Server did not start")


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
        server = start_server(sys.argv[1:])
        wait_for_server(server)

        test_idle_client(server)

        print("\n[PASS] C1 - Idle client timeout test passed")

    except Exception as error:
        print("\n[FAIL] C1 -", error)
        sys.exit(1)

    finally:
        if server and server.poll() is None:
            print("[INFO] Stopping server")
            server.terminate()

            try:
                server.wait(timeout=3)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait()

        print("[INFO] Test finished")


if __name__ == "__main__":
    main()