
#!/usr/bin/env python3

import os
import socket
import subprocess
import sys
import tempfile
import time

SERVER = os.path.abspath(
    sys.argv[1] if len(sys.argv) > 1 else "./webserv"
)
PROJECT_ROOT = os.getcwd()
WWW = os.path.join(PROJECT_ROOT, "www")

SOCKET_TIMEOUT = 20
OTHER_CLIENT_TIMEOUT = 3
LARGE_FILE_SIZE = 2 * 1024 * 1024


def available_port():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind(("127.0.0.1", 0))
    port = sock.getsockname()[1]
    sock.close()
    return port


def wait_for_server(process, port):
    deadline = time.time() + 5

    while time.time() < deadline:
        if process.poll() is not None:
            raise RuntimeError("Webserv exited during startup")

        try:
            sock = socket.create_connection(
                ("127.0.0.1", port), 0.2
            )
            sock.close()
            return
        except socket.error:
            time.sleep(0.05)

    raise RuntimeError("Webserv did not start listening")


def get_request(path):
    return (
        "GET %s HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n"
    ) % path


def read_response(sock):
    response = b""
    header_end = -1

    # Read until complete HTTP headers arrive.
    while header_end == -1:
        chunk = sock.recv(65536)

        if not chunk:
            raise RuntimeError("Connection closed before headers completed")

        response += chunk
        header_end = response.find(b"\r\n\r\n")

    # Separate headers and body.
    headers = response[:header_end]
    body = response[header_end + 4:]

    status = headers.split(b"\r\n", 1)[0]

    if b" 200 " not in status:
        raise RuntimeError("Expected HTTP 200, got %s" % status)

    # Extract Content-Length.
    content_length = None

    for line in headers.split(b"\r\n")[1:]:
        if line.lower().startswith(b"content-length:"):
            content_length = int(line.split(b":", 1)[1].strip())
            break

    if content_length is None:
        raise RuntimeError("Missing Content-Length header")

    if content_length > LARGE_FILE_SIZE:
        raise RuntimeError("Response body exceeds expected maximum")

    # Read only until the complete body is received.
    while len(body) < content_length:
        try:
            chunk = sock.recv(65536)
        except socket.timeout:
            raise RuntimeError(
                "Timed out after receiving %d of %d body bytes" %
                (len(body), content_length)
            )

        if not chunk:
            raise RuntimeError(
                "Connection closed before complete response body"
            )

        body += chunk

    if len(body) != content_length:
        raise RuntimeError(
            "Body length mismatch: expected %d, got %d" %
            (content_length, len(body))
        )

    return headers, body


def main():
    port = available_port()
    process = None
    config_path = None
    large_path = None
    small_path = None
    client_a = None

    try:
        # Create a large file with predictable binary content.
        large_data = bytes(range(256)) * (
            LARGE_FILE_SIZE // 256
        )
        small_data = b"GET is working"

        with tempfile.NamedTemporaryFile(
            dir=WWW, prefix="stall_large_",
            suffix=".bin", delete=False
        ) as file:
            file.write(large_data)
            large_path = file.name

        with tempfile.NamedTemporaryFile(
            dir=WWW, prefix="stall_small_",
            suffix=".txt", delete=False
        ) as file:
            file.write(small_data)
            small_path = file.name

        large_url = "/" + os.path.basename(large_path)
        small_url = "/" + os.path.basename(small_path)

        config = """
server {
    listen 127.0.0.1:%d;
    server_name localhost;

    location / {
        root www;
        methods GET;
    }
}
""" % port

        with tempfile.NamedTemporaryFile(
            mode="w", suffix=".conf", delete=False
        ) as file:
            file.write(config)
            config_path = file.name

        process = subprocess.Popen(
            [SERVER, config_path],
            cwd=PROJECT_ROOT,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.STDOUT
        )

        wait_for_server(process, port)

        print("[INFO] Webserv started")

        # Client A: Request a large response, then stop reading.
        client_a = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client_a.setsockopt(
            socket.SOL_SOCKET, socket.SO_RCVBUF, 16384
        )
        client_a.settimeout(SOCKET_TIMEOUT)
        client_a.connect(("127.0.0.1", port))

        client_a.sendall(get_request(large_url).encode("ascii"))

        print("[INFO] Client A requested large file")
        print("[INFO] Client A temporarily stops reading")

        time.sleep(0.5)

        # Client B: Must still be served while A is stalled.
        client_b = socket.create_connection(
            ("127.0.0.1", port),
            OTHER_CLIENT_TIMEOUT
        )
        client_b.settimeout(OTHER_CLIENT_TIMEOUT)

        try:

            client_b.sendall(
                get_request(small_url).encode("ascii")
            )

            _, body_b = read_response(client_b)
        finally:
            client_b.close()

        if body_b != small_data:
            raise RuntimeError("Client B received incorrect body")

        print("[PASS] Client B served while A was stalled")

        # Client A resumes reading.
        print("[INFO] Client A resumes reading")

        _, body_a = read_response(client_a)

        if body_a != large_data:
            raise RuntimeError(
                "Client A response incomplete or corrupted: "
                "expected %d bytes, got %d" %
                (len(large_data), len(body_a))
            )

        print("[PASS] Client A received complete large response")
        print("[RESULT] STALLED WRITE TEST PASSED")

    finally:
        if client_a is not None:
            client_a.close()

        if process is not None:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()

        for path in (config_path, large_path, small_path):
            if path is not None and os.path.exists(path):
                os.unlink(path)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print("[FAIL] %s" % error)
        sys.exit(1)
