
import socket
import subprocess
import sys
import time
from common import log_count, wait_for_log_count

HOST = "127.0.0.1"
PORT = 8080

REQUEST = (
    b"GET / HTTP/1.1\r\n"
    b"Host: localhost\r\n"
    b"\r\n"
)

STARTUP_TIMEOUT = 10
CLIENT_COUNT = 50
OBSERVATION_TIME = 2


def wait_for_server():
    deadline = time.time() + STARTUP_TIMEOUT

    while time.time() < deadline:
        if server.poll() is not None:
            raise RuntimeError("Server exited during startup")

        try:
            probe = socket.create_connection((HOST, PORT), timeout=1)
            probe.close()
            print("[PASS] Config parsed and server is listening")
            return
        except OSError:
            time.sleep(0.1)

    raise RuntimeError("Server did not start listening")


def main():
    global server

    command = sys.argv[1:]
    if len(command) != 2:
        print("Usage: python3 e5_repeated_connect_disconnect.py ./webserv <config-file>")
        return 1

    print("[INFO] Starting server:", " ".join(command))
    print("[INFO] Using config:", command[-1])
    server = subprocess.Popen(command)
    print("[PASS] Server process started with config")

    try:
        wait_for_server()
        print("[PASS] Config parsed and server is listening")
        completed_before = log_count("request status=COMPLETE")
        disconnected_before = log_count("disconnect ")

        print("[INFO] Performing", CLIENT_COUNT, "connect/disconnect cycles")

        for i in range(CLIENT_COUNT):
            client = socket.create_connection((HOST, PORT), timeout=2)
            client.sendall(REQUEST)
            wait_for_log_count(
                "request status=COMPLETE", completed_before + i + 1, OBSERVATION_TIME
            )
            client.close()
            wait_for_log_count(
                "disconnect ", disconnected_before + i + 1, OBSERVATION_TIME
            )

            if server.poll() is not None:
                raise RuntimeError(
                    "Server exited during cycle " + str(i + 1)
                )

        print("[PASS] All clients connected, parsed requests, and disconnected")

        if server.poll() is not None:
            raise RuntimeError("Server exited after disconnects")

        print("[PASS] Server remained operational")
        print("[PASS] E5 - Repeated connect/disconnect test passed")
        return 0

    except Exception as error:
        print("[FAIL]", error)
        return 1

    finally:
        if server.poll() is None:
            server.terminate()
            try:
                server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait()


if __name__ == "__main__":
    sys.exit(main())
