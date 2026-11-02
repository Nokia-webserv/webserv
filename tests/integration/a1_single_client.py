#!/usr/bin/env python3

import socket
import os
import subprocess
import sys
import time

HOST = "127.0.0.1"
PORT = 8080
STARTUP_TIMEOUT = 10
REQUEST_TIMEOUT = 3

REQUEST = (
    b"GET / HTTP/1.1\r\n"
    b"Host: localhost\r\n"
    b"\r\n"
)


def start_server(server_executable, config_path):
    print("[INFO] Starting server:", server_executable)
    print("[INFO] Using config:", config_path)

    if not os.path.isfile(config_path):
        print("[FAIL] Config file does not exist:", config_path)
        raise RuntimeError("Config file not found")

    try:
        server = subprocess.Popen([server_executable, config_path])
    except OSError as error:
        print("[FAIL] Could not start server:", error)
        raise

    print("[PASS] Server process started with config")
    return server


def connect_to_server(server):
    deadline = time.time() + STARTUP_TIMEOUT

    while time.time() < deadline:
        if server.poll() is not None:
            raise RuntimeError(
                "Server exited with code " + str(server.returncode)
            )

        try:
            client = socket.create_connection((HOST, PORT), timeout=1)
            print("[INFO] Connected to server")
            print("[PASS] Config parsed and server is listening")
            return client
        except OSError:
            time.sleep(0.2)

    raise RuntimeError("Server did not start")


def test_single_client(client, server):
    print("[INFO] Sending HTTP request")
    client.sendall(REQUEST)

    print("[INFO] Waiting for server to process request")
    time.sleep(1)

    if server.poll() is not None:
        raise AssertionError(
            "Server exited unexpectedly with code " + str(server.returncode)
        )

    print("[PASS] Server accepted connection and remained running")


def main():
    if len(sys.argv) != 3:
        print(
            "Usage: python3 a1_single_client.py "
            "<server_executable> <config-file>"
        )
        sys.exit(1)

    server_executable = sys.argv[1]
    config_path = sys.argv[2]

    server = None
    client = None

    try:
        server = start_server(server_executable, config_path)

        client = connect_to_server(server)
        client.settimeout(REQUEST_TIMEOUT)

        test_single_client(client, server)

        print("\n[PASS] A1 - Single client test passed")

    except Exception as error:
        print("\n[FAIL] A1 -", error)
        sys.exit(1)

    finally:
        if client:
            client.close()

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