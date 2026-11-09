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

MALFORMED_REQUEST = (
    b"GET / HTTP/9.9\r\n"
    b"Host: localhost\r\n"
    b"\r\n"
)

VALID_REQUEST = (
    b"GET / HTTP/1.1\r\n"
    b"Host: localhost\r\n"
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


def test_malformed_request(server):
    malformed_client = None
    valid_client = None

    try:
        print("[INFO] Connecting malformed client")
        malformed_client = socket.create_connection(
            (HOST, PORT), timeout=3
        )

        print("[INFO] Sending malformed HTTP request")
        malformed_client.sendall(MALFORMED_REQUEST)
        wait_for_log_file("request status=ERROR HTTP_VERSION_NOT_SUPPORTED", OBSERVATION_TIME)

        if server.poll() is not None:
            raise AssertionError(
                "Server exited after malformed request: "
                + str(server.returncode)
            )

        print("[PASS] Server survived malformed request")

        print("[INFO] Connecting second client")
        valid_client = socket.create_connection(
            (HOST, PORT), timeout=3
        )

        print("[INFO] Sending valid HTTP request")
        valid_client.sendall(VALID_REQUEST)
        wait_for_log_file("request status=COMPLETE", OBSERVATION_TIME)

        if server.poll() is not None:
            raise AssertionError(
                "Server exited after valid request: "
                + str(server.returncode)
            )

        print("[PASS] Server accepted another client")
        print("[PASS] Server remained operational")

    finally:
        if malformed_client:
            malformed_client.close()

        if valid_client:
            valid_client.close()


def main():
    if len(sys.argv) != 3:
        print(
            "Usage: python3 e1_malformed_request.py "
            "<server_executable> <config-file>"
        )
        sys.exit(1)

    server = None

    try:
        server = start_server(sys.argv[1:])
        wait_for_server(server)

        test_malformed_request(server)

        print("\n[PASS] E1 - Malformed request stability test passed")

    except Exception as error:
        print("\n[FAIL] E1 -", error)
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