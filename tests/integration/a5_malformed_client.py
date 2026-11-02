#!/usr/bin/env python3

import socket
import subprocess
import sys
import time

HOST = "127.0.0.1"
PORT = 8080

STARTUP_TIMEOUT = 10
OBSERVATION_TIME = 2
CLIENT_TIMEOUT = 3

# Invalid HTTP version.
MALFORMED_REQUEST = (
    b"GET / HTTP/9.9\r\n"
    b"Host: localhost\r\n"
    b"\r\n"
)

VALID_REQUEST = (
    b"GET /valid HTTP/1.1\r\n"
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


def test_malformed_client(server):
    malformed_client = None
    valid_client = None

    try:
        # Client A: send malformed request.
        print("[INFO] Connecting malformed client")

        malformed_client = socket.create_connection(
            (HOST, PORT), timeout=CLIENT_TIMEOUT
        )

        print("[INFO] Sending malformed HTTP request")
        malformed_client.sendall(MALFORMED_REQUEST)

        time.sleep(OBSERVATION_TIME)

        if server.poll() is not None:
            raise AssertionError(
                "Server crashed after malformed request"
            )

        print("[PASS] Server remained operational")

        # Client B: send valid request.
        print("[INFO] Connecting valid client")

        valid_client = socket.create_connection(
            (HOST, PORT), timeout=CLIENT_TIMEOUT
        )

        print("[INFO] Sending valid HTTP request")
        valid_client.sendall(VALID_REQUEST)

        time.sleep(OBSERVATION_TIME)

        if server.poll() is not None:
            raise AssertionError(
                "Server crashed after valid request"
            )

        print("[PASS] Valid client was not prevented from connecting")
        print("[PASS] Server remained operational")

    finally:
        if valid_client:
            valid_client.close()

        if malformed_client:
            malformed_client.close()


def main():
    if len(sys.argv) != 3:
        print(
            "Usage: python3 a5_malformed_client.py "
            "<server_executable> <config-file>"
        )
        sys.exit(1)

    server = None

    try:
        server = start_server(sys.argv[1:])
        wait_for_server(server)

        test_malformed_client(server)

        print("\n[PASS] A5 - Malformed client isolation test passed")

    except Exception as error:
        print("\n[FAIL] A5 -", error)
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