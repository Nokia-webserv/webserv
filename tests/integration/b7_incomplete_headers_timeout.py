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
OBSERVATION_TIME = CLIENT_TIMEOUT + 5

REQUEST_PART = (
    b"GET / HTTP/1.1\r\n"
    b"Host: localhost\r\n"
)


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


def test_incomplete_headers(server):
    client = None

    try:
        print("[INFO] Connecting slow client")
        client = socket.create_connection((HOST, PORT), timeout=3)
        client.settimeout(2)

        print("[INFO] Sending incomplete request headers")
        client.sendall(REQUEST_PART)

        print(
            "[INFO] Waiting",
            OBSERVATION_TIME,
            "seconds for server timeout"
        )
        time.sleep(OBSERVATION_TIME)

        if server.poll() is not None:
            raise AssertionError(
                "Server exited with code " + str(server.returncode)
            )

        try:
            data = client.recv(1)

            if data == b"":
                print("[PASS] Server closed the connection")
            else:
                print("[FAIL] Server sent unexpected data:", data)
                raise AssertionError("Unexpected data received")

        except socket.timeout:
            raise AssertionError(
                "Connection is still open after "
                + str(OBSERVATION_TIME)
                + " seconds"
            )

    finally:
        if client:
            client.close()
            print("[INFO] Client disconnected")


def main():
    if len(sys.argv) != 3:
        print(
            "Usage: python3 b7_incomplete_headers_timeout.py "
            "<server_executable> <config-file>"
        )
        sys.exit(1)

    server = None

    try:
        server = start_server(sys.argv[1:])
        wait_for_server(server)

        test_incomplete_headers(server)

        print("\n[PASS] B7 - Incomplete headers stability test passed")

    except Exception as error:
        print("\n[FAIL] B7 -", error)
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