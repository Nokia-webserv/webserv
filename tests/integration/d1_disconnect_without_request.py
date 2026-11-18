#!/usr/bin/env python3

import socket
import sys
import time
from common import start_server_simple, stop_server, wait_for_log_file, wait_for_server_simple

HOST = "127.0.0.1"
PORT = 8080

STARTUP_TIMEOUT = 10
OBSERVATION_TIME = 2


def assert_server_alive(server):
    if server.poll() is not None:
        raise AssertionError(
            "Server exited with code " + str(server.returncode)
        )


def test_disconnect_no_data(server):
    print("[INFO] Connecting client")
    client = socket.create_connection((HOST, PORT), timeout=3)
    print("[INFO] Closing connection without sending data")
    client.close()
    wait_for_log_file("disconnect")
    time.sleep(OBSERVATION_TIME)
    assert_server_alive(server)
    print("[PASS] Server remained operational after no-data disconnect")


def test_disconnect_during_headers(server):
    print("[INFO] Connecting client")
    client = socket.create_connection((HOST, PORT), timeout=3)
    print("[INFO] Sending partial headers, then disconnecting")
    client.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n")
    client.close()
    wait_for_log_file("disconnect")
    time.sleep(OBSERVATION_TIME)
    assert_server_alive(server)
    print("[PASS] Server remained operational after mid-headers disconnect")


def test_disconnect_during_body(server):
    client = None

    try:
        print("[INFO] Connecting client")
        client = socket.create_connection((HOST, PORT), timeout=3)
        print("[INFO] Sending POST headers with Content-Length: 100")
        client.sendall(
            b"POST /upload HTTP/1.1\r\n"
            b"Host: localhost\r\n"
            b"Content-Length: 100\r\n"
            b"\r\n"
        )
        time.sleep(0.5)
        print("[INFO] Sending 5 of 100 body bytes, then disconnecting")
        client.sendall(b"Hello")
        time.sleep(0.5)
        client.close()
        client = None
        wait_for_log_file("disconnect")
        time.sleep(OBSERVATION_TIME)
        assert_server_alive(server)
        print("[PASS] Server remained operational after mid-body disconnect")

    finally:
        if client:
            client.close()


def main():
    if len(sys.argv) != 3:
        print(
            "Usage: python3 d1_disconnect_without_request.py "
            "<server_executable> <config-file>"
        )
        sys.exit(1)

    server = None

    try:
        server = start_server_simple(sys.argv[1:])
        wait_for_server_simple(server)

        print("\n[INFO] Sub-case: disconnect with no data sent")
        test_disconnect_no_data(server)

        print("\n[INFO] Sub-case: disconnect during headers")
        test_disconnect_during_headers(server)

        print("\n[INFO] Sub-case: disconnect during body")
        test_disconnect_during_body(server)

        print("\n[PASS] D1 - Disconnect stability test passed")

    except Exception as error:
        print("\n[FAIL] D1 -", error)
        sys.exit(1)

    finally:
        stop_server(server)

        print("[INFO] Test finished")


if __name__ == "__main__":
    main()
