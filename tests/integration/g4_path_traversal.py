#!/usr/bin/env python3

import socket
import sys

from common import assert_response, read_response, start_server, stop_server, wait_for_server

HOST = "127.0.0.1"
PORT = 8080


def test_path_traversal(server):
    # /../escape attempts to go above root — parser must reject with 400
    client = socket.create_connection((HOST, PORT), timeout=3)
    try:
        client.sendall(b"GET /../escape HTTP/1.1\r\nHost: localhost\r\n\r\n")
        response = read_response(client)
        assert_response(response, 400)
        print("[PASS] Server returned 400 for path traversal above root")
    finally:
        client.close()

    # /images/../index.html is valid — normalizes to /index.html
    client = socket.create_connection((HOST, PORT), timeout=3)
    try:
        client.sendall(b"GET /images/../index.html HTTP/1.1\r\nHost: localhost\r\n\r\n")
        response = read_response(client)
        status, _, _ = response
        assert status != 400, "Normalizable path must not return 400, got " + str(status)
        print("[PASS] Normalizable path /images/../index.html accepted (status %d)" % status)
    finally:
        client.close()


def main():
    if len(sys.argv) != 3:
        print("Usage: python3 g4_path_traversal.py <server_executable> <config-file>")
        return 1

    server = None
    try:
        server = start_server(sys.argv[1:], "g4_path_traversal")
        wait_for_server(server)
        test_path_traversal(server)
        print("\n[PASS] G4 - Path traversal handling test passed")
        return 0
    except Exception as error:
        print("\n[FAIL] G4 -", error)
        return 1
    finally:
        stop_server(server)


if __name__ == "__main__":
    sys.exit(main())
