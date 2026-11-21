#!/usr/bin/env python3

import socket
import sys

from common import assert_response, read_response, start_server, stop_server, wait_for_server

HOST = "127.0.0.1"
PORT = 8080


def test_vhost_selection(server):
    # DELETE is disallowed on both vhosts; the Allow header reveals which vhost was selected.
    # cats.example: methods GET      -> Allow: GET
    # dogs.example: methods GET POST -> Allow: GET, POST

    client = socket.create_connection((HOST, PORT), timeout=3)
    try:
        client.sendall(b"DELETE / HTTP/1.1\r\nHost: cats.example\r\n\r\n")
        response = read_response(client)
        assert_response(response, 405)
        allow = response[1].get("allow", "")
        assert "POST" not in allow, "cats.example Allow must not contain POST: " + repr(allow)
        print("[PASS] cats.example selected: Allow: " + allow)
    finally:
        client.close()

    client = socket.create_connection((HOST, PORT), timeout=3)
    try:
        client.sendall(b"DELETE / HTTP/1.1\r\nHost: dogs.example\r\n\r\n")
        response = read_response(client)
        assert_response(response, 405)
        allow = response[1].get("allow", "")
        assert "POST" in allow, "dogs.example Allow must contain POST: " + repr(allow)
        print("[PASS] dogs.example selected: Allow: " + allow)
    finally:
        client.close()


def main():
    if len(sys.argv) != 3:
        print("Usage: python3 g3_vhost_selection.py <server_executable> <config-file>")
        return 1

    server = None
    try:
        server = start_server(sys.argv[1:], "g3_vhost_selection")
        wait_for_server(server)
        test_vhost_selection(server)
        print("\n[PASS] G3 - Virtual host selection test passed")
        return 0
    except Exception as error:
        print("\n[FAIL] G3 -", error)
        return 1
    finally:
        stop_server(server)


if __name__ == "__main__":
    sys.exit(main())
