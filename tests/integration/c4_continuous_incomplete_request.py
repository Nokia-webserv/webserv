#!/usr/bin/env python3

import socket
import os
import subprocess
import sys
import time

HOST = "127.0.0.1"
PORT = 8080

STARTUP_TIMEOUT = 10
CLIENT_TIMEOUT = int(os.environ.get("TEST_TIMEOUT", "10"))
TEST_DURATION = CLIENT_TIMEOUT + 10
SEND_INTERVAL = 5

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


def test_continuous_incomplete_request(server):
    client = None

    try:
        print("[INFO] Connecting slow client")
        client = socket.create_connection((HOST, PORT), timeout=3)
        client.settimeout(2)

        start_time = time.time()
        part_index = 0

        print("[INFO] Sending incomplete request periodically")
        print("[INFO] Test duration:", TEST_DURATION, "seconds")

        while time.time() - start_time < TEST_DURATION:
            if server.poll() is not None:
                raise AssertionError(
                    "Server exited with code " + str(server.returncode)
                )

            part = REQUEST_PARTS[part_index % len(REQUEST_PARTS)]

            try:
                client.sendall(part)
            except OSError as error:
                raise AssertionError(
                    "Connection closed while client was sending: "
                    + str(error)
                )

            part_index += 1
            print("[INFO] Sent fragment", part_index)

            time.sleep(SEND_INTERVAL)

        if server.poll() is not None:
            raise AssertionError(
                "Server exited with code " + str(server.returncode)
            )

        print("[PASS] Server remained operational")
        print("[INFO] Client kept sending data without completing the request")
        print("[INFO] Timeout policy was not independently verified")

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
        server = start_server(sys.argv[1:])
        wait_for_server(server)

        test_continuous_incomplete_request(server)

        print("\n[PASS] C4 - Continuous incomplete request test passed")

    except Exception as error:
        print("\n[FAIL] C4 -", error)
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