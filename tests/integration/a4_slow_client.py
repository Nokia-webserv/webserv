#!/usr/bin/env python3

import socket
import subprocess
import sys
import time

HOST = "127.0.0.1"
PORT = 8080

STARTUP_TIMEOUT = 10
SLOW_CLIENT_WAIT = 2
FAST_CLIENT_TIMEOUT = 3

SLOW_REQUEST_PART = (
    b"GET /slow HTTP/1.1\r\n"
    b"Host: localhost\r\n"
)

FAST_REQUEST = (
    b"GET /fast HTTP/1.1\r\n"
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


def test_slow_client(server):
    slow_client = None
    fast_client = None

    try:
        # Client A: connect and send an incomplete request.
        print("[INFO] Connecting slow client")
        slow_client = socket.create_connection((HOST, PORT), timeout=3)

        print("[INFO] Slow client sends incomplete request")
        slow_client.sendall(SLOW_REQUEST_PART)

        print("[INFO] Slow client remains connected")
        time.sleep(SLOW_CLIENT_WAIT)

        if server.poll() is not None:
            raise AssertionError("Server exited while handling slow client")

        # Client B: connect and send a complete request.
        print("[INFO] Connecting fast client")
        start = time.monotonic()

        fast_client = socket.create_connection(
            (HOST, PORT),
            timeout=FAST_CLIENT_TIMEOUT
        )

        fast_client.settimeout(FAST_CLIENT_TIMEOUT)
        fast_client.sendall(FAST_REQUEST)

        elapsed = time.monotonic() - start

        print("[INFO] Fast client connected and sent request")
        print("[INFO] Elapsed time:", round(elapsed, 3), "seconds")

        if elapsed > FAST_CLIENT_TIMEOUT:
            raise AssertionError("Fast client was blocked")

        if server.poll() is not None:
            raise AssertionError("Server exited unexpectedly")

        print("[PASS] Fast client was not blocked by slow client")
        print("[PASS] Both connections remained usable")

    finally:
        if fast_client:
            fast_client.close()

        if slow_client:
            slow_client.close()


def main():
    if len(sys.argv) != 3:
        print(
            "Usage: python3 a4_slow_client.py "
            "<server_executable> <config-file>"
        )
        sys.exit(1)

    server = None

    try:
        server = start_server(sys.argv[1:])
        wait_for_server(server)

        test_slow_client(server)

        print("\n[PASS] A4 - Slow client isolation test passed")

    except Exception as error:
        print("\n[FAIL] A4 -", error)
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