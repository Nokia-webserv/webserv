#!/usr/bin/env python3

import socket
import subprocess
import sys
import time
from common import wait_for_log_file

HOST = "127.0.0.1"
PORT = 8080

STARTUP_TIMEOUT = 10
OBSERVATION_TIME = 2

BODY = b"Hello"

HEADERS = (
    b"POST /upload HTTP/1.1\r\n"
    b"Host: localhost\r\n"
    b"Content-Length: 100\r\n"
    b"\r\n"
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


def test_disconnect_during_body(server):
    client = None

    try:
        print("[INFO] Connecting client")
        client = socket.create_connection((HOST, PORT), timeout=3)

        print("[INFO] Sending request headers")
        client.sendall(HEADERS)

        time.sleep(0.5)

        print("[INFO] Sending partial body: 5 of 100 bytes")
        client.sendall(BODY)

        time.sleep(0.5)

        print("[INFO] Disconnecting before completing the body")
        client.close()
        client = None
        wait_for_log_file("disconnect")

        print("[INFO] Waiting for server to process disconnect")
        time.sleep(OBSERVATION_TIME)

        if server.poll() is not None:
            raise AssertionError(
                "Server exited with code " + str(server.returncode)
            )

        print("[PASS] Server remained operational")
        print("[INFO] Incomplete request body was abandoned by the client")

    finally:
        if client:
            client.close()


def main():
    if len(sys.argv) != 3:
        print(
            "Usage: python3 d3_disconnect_during_body.py "
            "<server_executable> <config-file>"
        )
        sys.exit(1)

    server = None

    try:
        server = start_server(sys.argv[1:])
        wait_for_server(server)

        test_disconnect_during_body(server)

        print("\n[PASS] D3 - Disconnect during body test passed")

    except Exception as error:
        print("\n[FAIL] D3 -", error)
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