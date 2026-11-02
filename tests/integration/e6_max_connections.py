import os
import socket
import subprocess
import sys
import time

HOST = "127.0.0.1"
PORT = 8080

STARTUP_TIMEOUT = 10
ACCEPT_TIMEOUT = 10
CONNECT_TIMEOUT = 3
MAX_CONNECTIONS = 512


def get_process_socket_inodes(pid):
    inodes = set()
    fd_path = "/proc/" + str(pid) + "/fd"

    for fd in os.listdir(fd_path):
        try:
            target = os.readlink(os.path.join(fd_path, fd))

            if target.startswith("socket:[") and target.endswith("]"):
                inodes.add(target[8:-1])

        except (FileNotFoundError, PermissionError):
            continue

    return inodes


def get_server_connections(pid):
    connections = set()
    process_inodes = get_process_socket_inodes(pid)

    with open("/proc/net/tcp", "r") as file:
        next(file)

        for line in file:
            fields = line.split()

            local_address = fields[1]
            remote_address = fields[2]
            state = fields[3]
            inode = fields[9]

            if state != "01":  # TCP_ESTABLISHED
                continue

            if inode not in process_inodes:
                continue

            local_port = int(local_address.split(":")[1], 16)
            remote_port = int(remote_address.split(":")[1], 16)

            if local_port == PORT:
                connections.add((local_port, remote_port))

    return connections


def get_server_connection_count(pid):
    return len(get_server_connections(pid))


def server_has_client(pid, client):
    server_port = client.getpeername()[1]
    client_port = client.getsockname()[1]

    return (server_port, client_port) in get_server_connections(pid)


def wait_for_server(server):
    deadline = time.monotonic() + STARTUP_TIMEOUT

    while time.monotonic() < deadline:
        if server.poll() is not None:
            raise RuntimeError("Server exited during startup")

        try:
            probe = socket.create_connection(
                (HOST, PORT),
                timeout=1
            )
            probe.close()

            # The probe may have been accepted by the server. Wait until
            # its disconnect has been processed before starting the test.
            empty_deadline = time.monotonic() + ACCEPT_TIMEOUT

            while time.monotonic() < empty_deadline:
                if server.poll() is not None:
                    raise RuntimeError("Server exited after startup")

                if get_server_connection_count(server.pid) == 0:
                    print("[PASS] Config parsed and server is listening")
                    return

                time.sleep(0.1)

            raise RuntimeError(
                "Startup probe connection was not released"
            )

        except OSError:
            time.sleep(0.1)

    raise RuntimeError("Server did not start listening")


def wait_for_connection_count(server, expected):
    deadline = time.monotonic() + ACCEPT_TIMEOUT

    while time.monotonic() < deadline:
        if server.poll() is not None:
            raise RuntimeError("Server exited unexpectedly")

        if get_server_connection_count(server.pid) == expected:
            return True

        time.sleep(0.1)

    return get_server_connection_count(server.pid) == expected


def wait_for_client_state(server, client, expected):
    deadline = time.monotonic() + ACCEPT_TIMEOUT

    while time.monotonic() < deadline:
        if server.poll() is not None:
            raise RuntimeError("Server exited unexpectedly")

        if server_has_client(server.pid, client) == expected:
            return True

        time.sleep(0.1)

    return server_has_client(server.pid, client) == expected


def main():
    command = sys.argv[1:]

    if len(command) != 2:
        print("Usage: python3 e6_max_connections.py ./webserv <config-file>")
        return 1

    print("[INFO] MAX_CONNECTIONS:", MAX_CONNECTIONS)
    print("[INFO] Starting server:", " ".join(command))
    print("[INFO] Using config:", command[-1])

    server = subprocess.Popen(command)
    print("[PASS] Server process started with config")
    clients = []
    extra_client = None

    try:
        wait_for_server(server)
        print("[PASS] Config parsed and server is listening")

        print("[INFO] Opening", MAX_CONNECTIONS, "clients")

        for _ in range(MAX_CONNECTIONS):
            client = socket.create_connection(
                (HOST, PORT),
                timeout=CONNECT_TIMEOUT
            )
            clients.append(client)

        if not wait_for_connection_count(server, MAX_CONNECTIONS):
            count = get_server_connection_count(server.pid)
            raise RuntimeError(
                "Expected " + str(MAX_CONNECTIONS) +
                " accepted connections, found " + str(count)
            )

        print("[PASS] Server accepted all", MAX_CONNECTIONS, "clients")

        print("[INFO] Connecting client", MAX_CONNECTIONS + 1)

        try:
            extra_client = socket.create_connection(
                (HOST, PORT),
                timeout=CONNECT_TIMEOUT
            )
        except OSError as error:
            raise RuntimeError(
                "Additional client could not enter the listen queue: " +
                str(error)
            )

        print("[PASS] Additional TCP connection entered the queue")

        if not wait_for_client_state(server, extra_client, False):
            raise RuntimeError(
                "The additional client was accepted despite the limit"
            )

        print("[PASS] Additional client has not been accepted")

        print("[INFO] Disconnecting original client 1")
        clients[0].close()

        print("[INFO] Waiting for the queued client to be accepted")

        if not wait_for_client_state(server, extra_client, True):
            raise RuntimeError(
                "The queued client was not accepted after a slot was freed"
            )

        print("[PASS] Queued client was accepted")

        if not wait_for_connection_count(server, MAX_CONNECTIONS):
            count = get_server_connection_count(server.pid)
            raise RuntimeError(
                "Expected " + str(MAX_CONNECTIONS) +
                " accepted connections after replacement, found " +
                str(count)
            )

        print("[PASS] Server still has exactly", MAX_CONNECTIONS, "clients")

        if server.poll() is not None:
            raise RuntimeError("Server exited during the test")

        print("[PASS] E6 - Maximum connections test passed")
        return 0

    except Exception as error:
        print("[FAIL]", error)
        return 1

    finally:
        for client in clients:
            client.close()

        if extra_client is not None:
            extra_client.close()

        if server.poll() is None:
            server.terminate()

            try:
                server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait()


if __name__ == "__main__":
    sys.exit(main())