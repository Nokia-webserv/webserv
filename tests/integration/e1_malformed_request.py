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


def assert_server_alive(server):
    if server.poll() is not None:
        raise AssertionError(
            "Server exited with code " + str(server.returncode)
        )


def send_bad_then_good(bad_request, error_log_token, label):
    bad_client = None
    good_client = None

    try:
        print("[INFO] Connecting client for", label)
        bad_client = socket.create_connection((HOST, PORT), timeout=3)
        print("[INFO] Sending", label)
        bad_client.sendall(bad_request)
        wait_for_log_file("request status=ERROR " + error_log_token, OBSERVATION_TIME)
        print("[PASS] Server logged error for", label)

        print("[INFO] Connecting follow-up client")
        good_client = socket.create_connection((HOST, PORT), timeout=3)
        good_client.sendall(VALID_REQUEST)
        wait_for_log_file("request status=COMPLETE", OBSERVATION_TIME)
        print("[PASS] Server accepted follow-up request after", label)

    finally:
        if bad_client:
            bad_client.close()
        if good_client:
            good_client.close()


def test_bad_http_version(server):
    send_bad_then_good(
        b"GET / HTTP/9.9\r\nHost: localhost\r\n\r\n",
        "HTTP_VERSION_NOT_SUPPORTED",
        "bad HTTP version"
    )
    assert_server_alive(server)
    print("[PASS] Server remained operational after bad HTTP version")


def test_unsupported_method(server):
    send_bad_then_good(
        b"PUT / HTTP/1.1\r\nHost: localhost\r\n\r\n",
        "NOT_IMPLEMENTED",
        "unsupported method PUT"
    )
    assert_server_alive(server)
    print("[PASS] Server remained operational after unsupported method")


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

        print("\n[INFO] Sub-case: bad HTTP version")
        test_bad_http_version(server)

        print("\n[INFO] Sub-case: unsupported method")
        test_unsupported_method(server)

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
