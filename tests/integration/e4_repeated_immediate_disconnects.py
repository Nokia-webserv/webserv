
import socket
import subprocess
import sys
import time

HOST = "127.0.0.1"
PORT = 8080

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
        print("Usage: python3 e4_repeated_immediate_disconnects.py ./webserv <config-file>")
        return 1

    print("[INFO] Starting server:", " ".join(command))
    print("[INFO] Using config:", command[-1])
    server = subprocess.Popen(command)
    print("[PASS] Server process started with config")

    try:
        wait_for_server()
        print("[PASS] Config parsed and server is listening")

        print("[INFO] Performing", CLIENT_COUNT, "connect/disconnect cycles")

        for i in range(CLIENT_COUNT):
            client = socket.create_connection((HOST, PORT), timeout=2)
            client.close()

            if server.poll() is not None:
                raise RuntimeError(
                    "Server exited during cycle " + str(i + 1)
                )

        print("[PASS] All clients connected and disconnected")

        time.sleep(OBSERVATION_TIME)

        if server.poll() is not None:
            raise RuntimeError("Server exited after disconnects")

        print("[PASS] Server remained operational")
        print("[PASS] E4 - Repeated immediate disconnects test passed")
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
