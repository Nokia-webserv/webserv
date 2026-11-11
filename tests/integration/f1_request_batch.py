#!/usr/bin/env python3

import socket
import sys

from common import log_count, start_server, stop_server, wait_for_log_count, wait_for_server

HOST = "127.0.0.1"
PORT = 8080
REQUEST_COUNT = 40


def make_request(index):
    return (
        f"GET /request/{index} HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n"
    ).encode()


def test_request_batch(server):
    client = socket.create_connection((HOST, PORT), timeout=3)
    try:
        completed_before = log_count("request status=COMPLETE")
        client.sendall(b"".join(make_request(index) for index in range(1, REQUEST_COUNT + 1)))
        wait_for_log_count(
            "request status=COMPLETE",
            completed_before + REQUEST_COUNT,
        )
    finally:
        client.close()


def main():
    if len(sys.argv) != 3:
        print("Usage: python3 f_request_batch.py <server_executable> <config-file>")
        return 1

    server = None
    try:
        server = start_server(sys.argv[1:], "f_request_batch")
        wait_for_server(server)
        test_request_batch(server)
        if server.poll() is not None:
            raise AssertionError("Server exited while processing one connection")
        print("\n[PASS] F1 - 40 requests on one connection passed")
        return 0
    except Exception as error:
        print("\n[FAIL] F1 -", error)
        return 1
    finally:
        stop_server(server)


if __name__ == "__main__":
    sys.exit(main())