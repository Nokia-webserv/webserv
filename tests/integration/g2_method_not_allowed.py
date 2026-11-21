#!/usr/bin/env python3

import socket
import sys

from common import assert_response, read_response, start_server, stop_server, wait_for_server

HOST = "127.0.0.1"
PORT = 8080


def test_method_not_allowed(server):
    client = socket.create_connection((HOST, PORT), timeout=3)
    try:
        client.sendall(b"DELETE / HTTP/1.1\r\nHost: cats.example\r\n\r\n")
        response = read_response(client)
        status, headers, _ = response
        assert_response(response, 405)
        allow = headers.get("allow", "")
        assert "GET" in allow, "Allow header missing GET: " + repr(allow)
        assert "DELETE" not in allow, "Allow header must not contain DELETE: " + repr(allow)
        print("[PASS] Server returned 405 with Allow: " + allow)
    finally:
        client.close()


def main():
    if len(sys.argv) != 3:
        print("Usage: python3 g2_method_not_allowed.py <server_executable> <config-file>")
        return 1

    server = None
    try:
        server = start_server(sys.argv[1:], "g2_method_not_allowed")
        wait_for_server(server)
        test_method_not_allowed(server)
        print("\n[PASS] G2 - 405 Method Not Allowed test passed")
        return 0
    except Exception as error:
        print("\n[FAIL] G2 -", error)
        return 1
    finally:
        stop_server(server)


if __name__ == "__main__":
    sys.exit(main())
