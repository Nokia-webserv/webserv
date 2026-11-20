#!/usr/bin/env python3

import socket
import sys
from common import start_server_simple, stop_server, wait_for_log_count, wait_for_server_simple

HOST = "127.0.0.1"
PORT = 8080

CLIENT_COUNT = 5
REQUESTS_PER_CLIENT = 5

STARTUP_TIMEOUT = 10
OBSERVATION_TIME = 2

def make_request(client_id, request_id):
    return (
        f"GET /client-{client_id}/request-{request_id} HTTP/1.1\r\n"
        f"Host: localhost\r\n"
        f"\r\n"
    ).encode()


def test_repeated_requests(server):
    clients = []

    try:
        print("[INFO] Connecting", CLIENT_COUNT, "clients")

        for i in range(CLIENT_COUNT):
            client = socket.create_connection((HOST, PORT), timeout=3)
            clients.append(client)
            print("[INFO] Client", i + 1, "connected")

        print("[INFO] Sending repeated requests")

        for request_id in range(1, REQUESTS_PER_CLIENT + 1):
            for client_id, client in enumerate(clients, start=1):
                request = make_request(client_id, request_id)
                client.sendall(request)

                print(
                    f"[INFO] Client {client_id}: "
                    f"request {request_id} sent"
                )

        print("[INFO] All requests sent")
        wait_for_log_count(
            "request status=COMPLETE",
            CLIENT_COUNT * REQUESTS_PER_CLIENT,
            OBSERVATION_TIME,
        )

        if server.poll() is not None:
            raise AssertionError(
                "Server exited with code " + str(server.returncode)
            )

        print(
            f"[PASS] {CLIENT_COUNT * REQUESTS_PER_CLIENT} requests "
            "sent across multiple connections"
        )
        print("[PASS] Server remained operational")

    finally:
        for client in clients:
            client.close()

        print("[INFO] All clients disconnected")


def main():
    if len(sys.argv) != 3:
        print(
            "Usage: python3 a6_repeated_requests.py "
            "<server_executable> <config-file>"
        )
        sys.exit(1)

    server = None

    try:
        server = start_server_simple(sys.argv[1:])
        wait_for_server_simple(server)

        test_repeated_requests(server)

        print("\n[PASS] A6 - Repeated requests test passed")

    except Exception as error:
        print("\n[FAIL] A6 -", error)
        sys.exit(1)

    finally:
        stop_server(server)

        print("[INFO] Test finished")


if __name__ == "__main__":
    main()