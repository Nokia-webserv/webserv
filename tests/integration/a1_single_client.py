#!/usr/bin/env python3

import socket
import sys

from common import start_server, stop_server, wait_for_log, wait_for_server

HOST = "127.0.0.1"
PORT = 8080
REQUEST_TIMEOUT = 3

REQUEST = (
    b"GET / HTTP/1.1\r\n"
    b"Host: localhost\r\n"
    b"\r\n"
)


def test_single_client(client, server):
    print("[INFO] Sending HTTP request")
    client.sendall(REQUEST)

    wait_for_log(server, "request status=COMPLETE")
    print("[PASS] Server parser completed the request")


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
        server = start_server([server_executable, config_path], "a1_single_client")
        wait_for_server(server)
        client = socket.create_connection((HOST, PORT), timeout=REQUEST_TIMEOUT)
        test_single_client(client, server)

        print("\n[PASS] A1 - Single client test passed")

    except Exception as error:
        print("\n[FAIL] A1 -", error)
        sys.exit(1)

    finally:
        if client:
            client.close()

        stop_server(server)

        print("[INFO] Test finished")


if __name__ == "__main__":
    main()