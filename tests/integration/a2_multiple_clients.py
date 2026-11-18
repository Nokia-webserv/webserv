#!/usr/bin/env python3

import socket
import subprocess
import sys
import time
from common import wait_for_log_count

HOST = "127.0.0.1"
PORT = 8080

STARTUP_TIMEOUT = 10
OBSERVATION_TIME = 2

REQUEST = (
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


def test_clients(server, client_count):
    clients = []

    try:
        print("[INFO] Opening", client_count, "client connections")

        for i in range(client_count):
            client = socket.create_connection((HOST, PORT), timeout=3)
            clients.append(client)
            print("[INFO] Client", i + 1, "connected")

        print("[INFO] Sending requests from all clients")

        for i, client in enumerate(clients):
            client.sendall(REQUEST)
            print("[INFO] Client", i + 1, "sent request")

        print("[INFO] Waiting for server to process requests")
        wait_for_log_count("request status=COMPLETE", client_count, OBSERVATION_TIME)

        if server.poll() is not None:
            raise AssertionError(
                "Server exited unexpectedly with code " + str(server.returncode)
            )

        print("[PASS] Requests reached the parser")

    finally:
        for i, client in enumerate(clients):
            client.close()
            print("[INFO] Client", i + 1, "disconnected")


def main():
    if len(sys.argv) != 3:
        print(
            "Usage: python3 a2_multiple_clients.py "
            "<server_executable> <config-file>"
        )
        sys.exit(1)

    server = None

    try:
        server = start_server(sys.argv[1:])
        wait_for_server(server)

        print("\n[INFO] Sub-case: 5 concurrent clients")
        test_clients(server, 5)

        print("\n[INFO] Sub-case: 10 concurrent clients")
        test_clients(server, 10)

        print("\n[PASS] A2 - Multiple clients test passed")

    except Exception as error:
        print("\n[FAIL] A2 -", error)
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
