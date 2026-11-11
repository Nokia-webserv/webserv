#!/usr/bin/env python3

import socket
import sys
import time

from common import log_count, start_server, stop_server, wait_for_log_count, wait_for_server

HOST = "127.0.0.1"
PORT = 8080


def test_chunked_body(server):
    client = socket.create_connection((HOST, PORT), timeout=3)
    try:
        incomplete_before = log_count("request status=INCOMPLETE_BODY")
        complete_before = log_count("request status=COMPLETE")
        client.sendall(
            b"POST /chunked HTTP/1.1\r\n"
            b"Host: localhost\r\n"
            b"Transfer-Encoding: chunked\r\n"
            b"\r\n"
            b"5\r\nHello\r\n"
        )
        wait_for_log_count("request status=INCOMPLETE_BODY", incomplete_before + 1)
        time.sleep(0.2)

        client.sendall(b"6\r\n World\r\n0\r\n\r\n")
        wait_for_log_count("request status=COMPLETE", complete_before + 1)
    finally:
        client.close()


def main():
    if len(sys.argv) != 3:
        print("Usage: python3 f_chunked_body.py <server_executable> <config-file>")
        return 1

    server = None
    try:
        server = start_server(sys.argv[1:], "f_chunked_body")
        wait_for_server(server)
        test_chunked_body(server)
        if server.poll() is not None:
            raise AssertionError("Server exited while parsing chunked body")
        print("\n[PASS] F3 - Fragmented chunked body passed")
        return 0
    except Exception as error:
        print("\n[FAIL] F3 -", error)
        return 1
    finally:
        stop_server(server)


if __name__ == "__main__":
    sys.exit(main())