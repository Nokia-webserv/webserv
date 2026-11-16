#!/usr/bin/env python3
"""Integration tests for configured client_max_header_size (HTTP 431)."""

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
SOCKET_TIMEOUT = 10

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


def create_config(port, directives, locations, other_server=None):
    config = """
server {
    listen 127.0.0.1:%d;
    server_name header-test;
    %s
    %s
}
""" % (port, directives, locations)
    if other_server is not None:
        config += """
server {
    listen 127.0.0.1:%d;
    server_name other-test;
    client_max_header_size %d;
    location / {
        root www;
        methods GET;
    }
}
""" % (port, other_server)
    return config


def make_request(path="/", size=80, host="header-test"):
    # POST deliberately targets a GET-only location. If size validation
    # passes, Router returns 405 (handlers do not need to be implemented).
    prefix = ("POST %s HTTP/1.1\r\n"
              "Host: %s\r\n"
              "X-Pad: ") % (path, host)
    suffix = "\r\n\r\n"
    min_size = len((prefix + suffix).encode("ascii"))
    if size < min_size:
        raise ValueError("Requested header size %d is below minimum %d" %
                         (size, min_size))
    payload = (prefix + "x" * (size - min_size) + suffix).encode("ascii")
    assert len(payload) == size
    return payload


def request(port, payload, fragmented=False):
    sock = socket.create_connection(("127.0.0.1", port), SOCKET_TIMEOUT)
    sock.settimeout(SOCKET_TIMEOUT)
    try:
        try:
            if fragmented:
                # Split inside the header value (not on CRLF).
                split = min(len(payload) - 4, max(1, len(payload) // 2))
                sock.sendall(payload[:split])
                time.sleep(0.05)
                sock.sendall(payload[split:])
            else:
                sock.sendall(payload)
        except (BrokenPipeError, ConnectionResetError):
            # The server may reject a large header while it is being sent.
            pass

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


def run_case(name, directives, locations, path, header_size, expected,
             fragmented=False, host="header-test", other_server=None):
    global passed, failed
    port = available_port()
    config = create_config(port, directives, locations, other_server)
    process = None
    config_path = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", suffix=".conf",
                                         delete=False) as config_file:
            config_file.write(config)
            config_path = config_file.name

        with open(LOG_PATH or os.devnull, "ab") as log:
            process = subprocess.Popen([SERVER, config_path], cwd=PROJECT_ROOT,
                                       stdout=log, stderr=subprocess.STDOUT)
            wait_for_server(process, port)
            status = request(port, make_request(path, header_size, host), fragmented)

        if status == expected:
            print("[PASS] " + name)
            passed += 1
        else:
            print("[FAIL] %s: expected %d, got %s" %
                  (name, expected, status))
            failed += 1
    except Exception as error:
        print("[FAIL] %s: %s" % (name, error))
        failed += 1
    finally:
        if process is not None and process.poll() is None:
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
    client_max_header_size 80;
}
location /large {
    root www;
    methods GET;
    client_max_header_size 200;
}
location /inherit {
    root www;
    methods GET;
}
"""

print("============================================================")
print("          CLIENT MAX HEADER SIZE TESTS")
print("============================================================")

# 405 means the size checks passed and routing reached the method check.
run_case("Server: below limit", "client_max_header_size 80;", ROOT,
         "/", 79, 405)
run_case("Server: exact boundary", "client_max_header_size 80;", ROOT,
         "/", 80, 405)
run_case("Server: one byte over", "client_max_header_size 80;", ROOT,
         "/", 81, 431)
run_case("Location smaller: exceeded", "client_max_header_size 120;", LOCATIONS,
         "/small", 81, 431)
run_case("Location smaller: exact boundary", "client_max_header_size 120;", LOCATIONS,
         "/small", 80, 405)
run_case("Location larger: allowed", "client_max_header_size 120;", LOCATIONS,
         "/large", 150, 405)
run_case("Location larger: exceeded", "client_max_header_size 120;", LOCATIONS,
         "/large", 201, 431)
run_case("Inherited server limit: exceeded", "client_max_header_size 120;",
         LOCATIONS, "/inherit", 121, 431)
run_case("Fragmented header: exact boundary", "client_max_header_size 80;",
         ROOT, "/", 80, 405, fragmented=True)
run_case("Fragmented header: exceeded", "client_max_header_size 80;",
         ROOT, "/", 81, 431, fragmented=True)
run_case("Virtual hosts: larger server accepts", "client_max_header_size 80;",
         ROOT, "/", 150, 405, host="other-test", other_server=200)
run_case("Virtual hosts: smaller server rejects", "client_max_header_size 80;",
         ROOT, "/", 150, 431, host="header-test", other_server=200)
run_case("Default 10 MiB: one byte over", "", ROOT, "/", 10485761, 431)

print("\nPassed: %d" % passed)
print("Failed: %d" % failed)
if failed:
    sys.exit(1)
print("[RESULT] ALL HEADER SIZE TESTS PASSED")
sys.exit(0)
