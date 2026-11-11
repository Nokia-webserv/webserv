#!/usr/bin/env python3

import socket
import os
import subprocess
import sys
import time

HOST = "127.0.0.1"
PORT = 8080

STARTUP_TIMEOUT = 10

# Must match the Webserv inactivity timeout.
CLIENT_TIMEOUT = int(os.environ.get("TEST_TIMEOUT", "30"))
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
        print("[INFO] Connecting slow client")
        client = socket.create_connection((HOST, PORT), timeout=3)
        client.settimeout(2)

        print("[INFO] Sending incomplete request headers")
        client.sendall(REQUEST_PART)

        # Incomplete headers must not cause an immediate response.
        try:
            data = client.recv(1)

            if data == b"":
                raise AssertionError(
                    "Server closed the connection before timeout"
                )

            raise AssertionError(
                "Server responded before timeout: " + repr(data)
            )

        except socket.timeout:
            print("[PASS] No immediate response for incomplete headers")

        print(
            "[INFO] Waiting for HTTP 408 (timeout:",
            CLIENT_TIMEOUT,
            "seconds)"
        )

        start = time.monotonic()

        # Allow enough time for the configured inactivity timeout.
        client.settimeout(OBSERVATION_TIME + 2)

        response = receive_until_closed(client)

        elapsed_after_wait = time.monotonic() - start

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

        if b"Connection: close\r\n" not in response:
            raise AssertionError(
                "Missing Connection: close header"
            )

        # The first receive already waited approximately 2 seconds.
        total_elapsed = elapsed_after_wait + 2

        if total_elapsed < CLIENT_TIMEOUT - 1:
            raise AssertionError(
                "Server timed out too early: "
                + str(round(total_elapsed, 2))
                + " seconds"
            )

        if total_elapsed > OBSERVATION_TIME:
            raise AssertionError(
                "Server timed out too late: "
                + str(round(total_elapsed, 2))
                + " seconds"
            )

        print(
            "[PASS] HTTP 408 received after",
            round(total_elapsed, 2),
            "seconds"
        )
        print("[PASS] Connection closed after response")
        print("[PASS] Server remains running")

    except socket.timeout:
        raise AssertionError(
            "Server did not respond and close within the timeout window"
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

        print("\n[PASS] B7 - Incomplete headers timeout test passed")

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
