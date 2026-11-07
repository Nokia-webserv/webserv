#!/usr/bin/env python3

import socket
import sys

from common import log_count, start_server, stop_server, wait_for_log_count, wait_for_server

HOST = "127.0.0.1"
PORT = 8080


def test_half_close(server):
    client = socket.create_connection((HOST, PORT), timeout=3)
    try:
        incomplete_before = log_count("request status=INCOMPLETE_HEAD")
        disconnect_before = log_count("disconnect ")
        client.sendall(b"GET /incomplete HTTP/1.1\r\n")
        wait_for_log_count("request status=INCOMPLETE_HEAD", incomplete_before + 1)

        client.shutdown(socket.SHUT_WR)
        wait_for_log_count("disconnect ", disconnect_before + 1)

        client.settimeout(3)
        if client.recv(1) != b"":
            raise AssertionError("Server sent data after incomplete half-close")
    finally:
        client.close()


def main():
    if len(sys.argv) != 3:
        print("Usage: python3 f_half_close.py <server_executable> <config-file>")
        return 1

    server = None
    try:
        server = start_server(sys.argv[1:], "f_half_close")
        wait_for_server(server)
        test_half_close(server)
        if server.poll() is not None:
            raise AssertionError("Server exited while handling half-close")
        print("\n[PASS] F2 - Half-close with incomplete request passed")
        return 0
    except Exception as error:
        print("\n[FAIL] F2 -", error)
        return 1
    finally:
        stop_server(server)


if __name__ == "__main__":
    sys.exit(main())