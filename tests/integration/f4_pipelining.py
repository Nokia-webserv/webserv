#!/usr/bin/env python3

import socket
import sys

from common import log_count, start_server, stop_server, wait_for_log_count, wait_for_server

HOST = "127.0.0.1"
PORT = 8080


def wait_for_requests(client, payload, complete_count, incomplete_status=None):
    complete_before = log_count("request status=COMPLETE")
    incomplete_before = (
        log_count("request status=" + incomplete_status)
        if incomplete_status
        else 0
    )
    client.sendall(payload)
    if incomplete_status:
        wait_for_log_count(
            "request status=" + incomplete_status,
            incomplete_before + 1,
        )
    wait_for_log_count("request status=COMPLETE", complete_before + complete_count)


def test_pipelining(server):
    client = socket.create_connection((HOST, PORT), timeout=3)
    try:
        wait_for_requests(
            client,
            b"POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 11\r\n\r\n"
            b"Hello World"
            b"GET /after-post HTTP/1.1\r\nHost: localhost\r\n\r\n",
            2,
        )
    finally:
        client.close()

    client = socket.create_connection((HOST, PORT), timeout=3)
    try:
        wait_for_requests(
            client,
            b"GET /first HTTP/1.1\r\nHost: localhost\r\n\r\n"
            b"POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 11\r\n\r\n"
            b"Hello World"
            b"GET /middle HTTP/1.1\r\nHost: localhost\r\n\r\n"
            b"POST /chunked HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n"
            b"5\r\nHello\r\n6\r\n World\r\n0\r\n\r\n"
            b"DELETE /last HTTP/1.1\r\nHost: localhost\r\n\r\n",
            5,
        )
    finally:
        client.close()

    client = socket.create_connection((HOST, PORT), timeout=3)
    try:
        wait_for_requests(
            client,
            b"GET /first HTTP/1.1\r\nHost: localhost\r\n\r\n"
            b"POST /second HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\n"
            b"Hello"
            b"GET /third HTTP/1.1\r\n",
            2,
            "INCOMPLETE_HEAD",
        )
        complete_before = log_count("request status=COMPLETE")
        client.sendall(b"Host: localhost\r\n\r\n")
        wait_for_log_count("request status=COMPLETE", complete_before + 1)
    finally:
        client.close()

    client = socket.create_connection((HOST, PORT), timeout=3)
    try:
        wait_for_requests(
            client,
            b"POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 11\r\n\r\n"
            b"Hello",
            0,
            "INCOMPLETE_BODY",
        )
        complete_before = log_count("request status=COMPLETE")
        client.sendall(
            b" WorldGET /after-body HTTP/1.1\r\nHost: localhost\r\n\r\n"
        )
        wait_for_log_count("request status=COMPLETE", complete_before + 2)
    finally:
        client.close()

    client = socket.create_connection((HOST, PORT), timeout=3)
    try:
        wait_for_requests(
            client,
            b"POST /chunked HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n"
            b"5\r\nHello\r\n",
            0,
            "INCOMPLETE_BODY",
        )
        complete_before = log_count("request status=COMPLETE")
        client.sendall(
            b"6\r\n World\r\n0\r\n\r\n"
            b"GET /after-chunked HTTP/1.1\r\nHost: localhost\r\n\r\n"
        )
        wait_for_log_count("request status=COMPLETE", complete_before + 2)
    finally:
        client.close()


def main():
    if len(sys.argv) != 3:
        print("Usage: python3 f_pipelining.py <server_executable> <config-file>")
        return 1

    server = None
    try:
        server = start_server(sys.argv[1:], "f_pipelining")
        wait_for_server(server)
        test_pipelining(server)
        if server.poll() is not None:
            raise AssertionError("Server exited while processing pipelined requests")
        print("\n[PASS] F4 - Pipelining and mixed-buffer requests passed")
        return 0
    except Exception as error:
        print("\n[FAIL] F4 -", error)
        return 1
    finally:
        stop_server(server)


if __name__ == "__main__":
    sys.exit(main())