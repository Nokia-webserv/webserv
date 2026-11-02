#!/usr/bin/env python3

import socket
import subprocess
import sys
import time

HOST = "127.0.0.1"
PORT = 8080

STARTUP_TIMEOUT = 10
OBSERVATION_TIME = 2

UNSUPPORTED_REQUEST = (
    b"PUT / HTTP/1.1\r\n"
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


def test_unsupported_method(server):
    unsupported_client = None
    valid_client = None

    try:
        print("[INFO] Connecting first client")
        unsupported_client = socket.create_connection(
            (HOST, PORT), timeout=3
        )

        print("[INFO] Sending unsupported PUT request")
        unsupported_client.sendall(UNSUPPORTED_REQUEST)

        time.sleep(OBSERVATION_TIME)

        if server.poll() is not None:
            raise AssertionError(
                "Server exited after unsupported method: "
                + str(server.returncode)
            )

        print("[PASS] Server survived unsupported method")

        print("[INFO] Connecting second client")
        valid_client = socket.create_connection(
            (HOST, PORT), timeout=3
        )

        print("[INFO] Sending valid GET request")
        valid_client.sendall(VALID_REQUEST)

        time.sleep(OBSERVATION_TIME)

        if server.poll() is not None:
            raise AssertionError(
                "Server exited after valid request: "
                + str(server.returncode)
            )

        print("[PASS] Server accepted another client")
        print("[PASS] Server remained operational")

    finally:
        if unsupported_client:
            unsupported_client.close()

        if valid_client:
            valid_client.close()


def main():
    if len(sys.argv) != 3:
        print(
            "Usage: python3 e2_unsupported_method.py "
            "<server_executable> <config-file>"
        )
        sys.exit(1)

    server = None

    try:
        server = start_server(sys.argv[1:])
        wait_for_server(server)

        test_unsupported_method(server)

        print("\n[PASS] E2 - Unsupported method stability test passed")

    except Exception as error:
        print("\n[FAIL] E2 -", error)
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