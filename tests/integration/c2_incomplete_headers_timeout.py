
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

# Must match the actual Webserv inactivity timeout.
CLIENT_TIMEOUT = int(os.environ.get("TEST_TIMEOUT", "30"))
WAIT_TIME = CLIENT_TIMEOUT + 5

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
    deadline = time.monotonic() + STARTUP_TIMEOUT

    while time.monotonic() < deadline:
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


def receive_until_closed(client):
    response = b""

    while True:
        chunk = client.recv(4096)

        if not chunk:
            return response

        response += chunk


def test_incomplete_headers(server):
    client = None

    try:
        print("[INFO] Connecting client")
        client = socket.create_connection((HOST, PORT), timeout=3)

        timeout_count = log_count("timeout")

        print("[INFO] Sending incomplete HTTP headers")

        start = time.monotonic()
        client.sendall(REQUEST_PART)

        # The server must not respond immediately.
        client.settimeout(2)

        try:
            data = client.recv(1)

            if data == b"":
                raise AssertionError(
                    "Server closed connection before timeout"
                )

            raise AssertionError(
                "Server responded before timeout: " + repr(data)
            )

        except socket.timeout:
            print("[PASS] Incomplete request remains open")

        print("[INFO] Waiting for HTTP 408 Request Timeout")

        # Allow enough time for the complete response and EOF.
        client.settimeout(WAIT_TIME + 2)

        response = receive_until_closed(client)
        elapsed = time.monotonic() - start

        if server.poll() is not None:
            raise AssertionError(
                "Server exited with code " + str(server.returncode)
            )

        expected_status = b"HTTP/1.1 408 Request Timeout\r\n"

        if not response.startswith(expected_status):
            raise AssertionError(
                "Expected HTTP 408, received: "
                + repr(response[:200])
            )

        headers = response.split(b"\r\n\r\n", 1)[0].lower()

        if b"connection: close\r\n" not in headers + b"\r\n":
            raise AssertionError(
                "Missing Connection: close header"
            )

        if elapsed < CLIENT_TIMEOUT - 1:
            raise AssertionError(
                "Server timed out too early: "
                + str(round(elapsed, 2))
                + " seconds"
            )

        if elapsed > WAIT_TIME:
            raise AssertionError(
                "Server timed out too late: "
                + str(round(elapsed, 2))
                + " seconds"
            )

        print(
            "[PASS] HTTP 408 received after",
            round(elapsed, 2),
            "seconds"
        )
        print("[PASS] Server closed connection after response")

        # Preserve the original timeout logging assertion.
        remaining = max(
            0.1,
            WAIT_TIME - (time.monotonic() - start)
        )

        wait_for_log_count(
            "timeout",
            timeout_count + 1,
            remaining
        )

        print("[PASS] Server recorded the timeout event")
        print("[PASS] Server remains running")

    except socket.timeout:
        raise AssertionError(
            "Server did not respond and close within timeout period"
        )

    finally:
        if client:
            client.close()
            print("[INFO] Client disconnected")


def main():
    if len(sys.argv) != 3:
        print(
            "Usage: python3 c2_incomplete_headers_timeout.py "
            "<server_executable> <config-file>"
        )
        sys.exit(1)

    server = None

    try:
        server = start_server(sys.argv[1:])
        wait_for_server(server)

        test_incomplete_headers(server)

        print("\n[PASS] C2 - Incomplete headers timeout test passed")

    except Exception as error:
        print("\n[FAIL] C2 -", error)
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
