
#!/usr/bin/env python3

import os
import re
import socket
import subprocess
import sys
import tempfile
import time

SERVER = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "./webserv")
PROJECT_ROOT = os.getcwd()
LOG_PATH = os.environ.get("WEBSERV_TEST_LOG")
SOCKET_TIMEOUT = 3

passed = 0
failed = 0


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
            sock = socket.create_connection(("127.0.0.1", port), 0.2)
            sock.close()
            return
        except socket.error:
            time.sleep(0.05)

    raise RuntimeError("Webserv did not start listening")


def create_config(port, directives, locations):
    return """
server {
    listen 127.0.0.1:%d;
    server_name body-test;
    %s
    %s
}
""" % (port, directives, locations)


def request(port, path, body=b"", content_length=None, chunked=False):
    headers = (
        "GET %s HTTP/1.1\r\n"
        "Host: body-test\r\n"
        "Connection: close\r\n"
    ) % path

    if chunked:
        headers += "Transfer-Encoding: chunked\r\n"
    else:
        if content_length is None:
            content_length = len(body)
        headers += "Content-Length: %d\r\n" % content_length

    payload = headers.encode("ascii") + b"\r\n" + body

    sock = socket.create_connection(("127.0.0.1", port), SOCKET_TIMEOUT)
    sock.settimeout(SOCKET_TIMEOUT)

    try:
        sock.sendall(payload)
        response = b""

        while b"\r\n" not in response:
            chunk = sock.recv(4096)
            if not chunk:
                break
            response += chunk

        first_line = response.split(b"\r\n", 1)[0]
        match = re.match(rb"HTTP/1\.[01] ([0-9]{3})", first_line)

        return int(match.group(1)) if match else None
    finally:
        sock.close()


def run_case(name, config_directives, locations,
             path, body=b"", content_length=None, chunked=False):
    global passed, failed

    port = available_port()
    config = create_config(port, config_directives, locations)
    config_path = None
    process = None

    try:
        with tempfile.NamedTemporaryFile(
            mode="w", suffix=".conf", delete=False
        ) as config_file:
            config_file.write(config)
            config_path = config_file.name

        with open(LOG_PATH or os.devnull, "ab") as log:
            process = subprocess.Popen(
                [SERVER, config_path],
                cwd=PROJECT_ROOT,
                stdout=log,
                stderr=subprocess.STDOUT
            )

            wait_for_server(process, port)

            status = request(
                port, path, body,
                content_length=content_length,
                chunked=chunked
            )

        if status == 413:
            print("[PASS] " + name)
            passed += 1
        else:
            print("[FAIL] %s: expected 413, got %s" % (name, status))
            failed += 1

    except Exception as error:
        print("[FAIL] %s: %s" % (name, error))
        failed += 1

    finally:
        if process is not None:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()

        if config_path is not None:
            os.unlink(config_path)


ROOT = """
location / {
    root www;
    methods GET;
}
"""

LOCATIONS = ROOT + """
location /small {
    root www;
    methods GET;
    client_max_body_size 5;
}

location /large {
    root www;
    methods GET;
    client_max_body_size 20;
}

location /inherit {
    root www;
    methods GET;
}
"""


print("============================================================")
print("          CLIENT MAX BODY SIZE TESTS")
print("============================================================")


# Test 1: Default 10 MiB when the directive is absent.
run_case(
    "Default limit: 10 MiB + 1",
    "", ROOT, "/",
    content_length=10485761
)


# Test 2: Explicit server-level limit.
run_case(
    "Server limit: 11 > 10",
    "client_max_body_size 10;", ROOT, "/",
    body=b"12345678901"
)


# Test 3: Location-specific smaller limit.
run_case(
    "Small location: 6 > 5",
    "client_max_body_size 10;", LOCATIONS, "/small",
    body=b"123456"
)


# Test 4: Location-specific larger limit.
run_case(
    "Large location: 21 > 20",
    "client_max_body_size 10;", LOCATIONS, "/large",
    body=b"123456789012345678901"
)


# Test 5: Location inherits server limit.
run_case(
    "Inherited limit: 11 > 10",
    "client_max_body_size 10;", LOCATIONS, "/inherit",
    body=b"12345678901"
)


# Test 6: Explicit zero limit.
run_case(
    "Zero limit: nonempty body",
    "client_max_body_size 0;", ROOT, "/",
    content_length=1
)


# Test 7: One chunk exceeding the configured limit.
run_case(
    "Chunked: 11 > 10",
    "client_max_body_size 10;", ROOT, "/",
    body=b"B\r\n12345678901\r\n0\r\n\r\n",
    chunked=True
)


# Test 8: Several chunks exceeding the cumulative limit.
run_case(
    "Chunked: cumulative size 11 > 10",
    "client_max_body_size 10;", ROOT, "/",
    body=b"6\r\n123456\r\n5\r\n12345\r\n0\r\n\r\n",
    chunked=True
)


print()
print("Passed: %d" % passed)
print("Failed: %d" % failed)

if failed:
    sys.exit(1)

print("[RESULT] ALL BODY SIZE TESTS PASSED")
sys.exit(0)
