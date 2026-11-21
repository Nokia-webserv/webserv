#!/usr/bin/env python3

import socket
import sys

from common import assert_response, read_response, start_server, stop_server, wait_for_server

HOST = "127.0.0.1"
PORT = 8080


def test_not_found(server):
    client = socket.create_connection((HOST, PORT), timeout=3)
    try:
        client.sendall(b"GET /no_such_path HTTP/1.1\r\nHost: localhost\r\n\r\n")
        response = read_response(client)
        assert_response(response, 404)
        print("[PASS] Server returned 404 for unknown path")
    finally:
        client.close()


def main():
    if len(sys.argv) != 3:
        print("Usage: python3 g1_not_found.py <server_executable> <config-file>")
        return 1

    server = None
    try:
        server = start_server(sys.argv[1:], "g1_not_found")
        wait_for_server(server)
        test_not_found(server)
        print("\n[PASS] G1 - 404 Not Found test passed")
        return 0
    except Exception as error:
        print("\n[FAIL] G1 -", error)
        return 1
    finally:
        stop_server(server)


if __name__ == "__main__":
    sys.exit(main())
