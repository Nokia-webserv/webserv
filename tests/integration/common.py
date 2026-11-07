#!/usr/bin/env python3

import os
import socket
import subprocess
import time
from pathlib import Path

HOST = "127.0.0.1"
PORT = 8080
STARTUP_TIMEOUT = 10
RESPONSE_TIMEOUT = 3
LOG_DIR = Path(__file__).resolve().parent / "logs"


def start_server(command, name):
    LOG_DIR.mkdir(exist_ok=True)
    log_path = LOG_DIR / (name + ".server.log")
    log_file = open(str(log_path), "w")
    server = subprocess.Popen(
        command,
        stdout=log_file,
        stderr=subprocess.STDOUT,
    )
    server._integration_log_file = log_file
    server._integration_log_path = log_path
    return server


def stop_server(server):
    if server is None:
        return
    if server.poll() is None:
        server.terminate()
        try:
            server.wait(timeout=3)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait()
    log_file = getattr(server, "_integration_log_file", None)
    if log_file is not None:
        log_file.flush()
        log_file.close()


def wait_for_server(server):
    deadline = time.monotonic() + STARTUP_TIMEOUT
    while time.monotonic() < deadline:
        if server.poll() is not None:
            raise RuntimeError("Server exited during startup")
        try:
            probe = socket.create_connection((HOST, PORT), timeout=1)
            probe.close()
            return
        except OSError:
            time.sleep(0.1)
    raise RuntimeError("Server did not start listening")


def wait_for_log(server, text, timeout=3):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        log_file = getattr(server, "_integration_log_file", None)
        if log_file is not None:
            log_file.flush()
        log_path = getattr(server, "_integration_log_path", None)
        if log_path is not None and log_path.exists():
            if text in log_path.read_text():
                return
        time.sleep(0.05)
    raise AssertionError("Server log did not contain: " + text)


def wait_for_log_file(text, timeout=3):
    log_path = os.environ.get("WEBSERV_TEST_LOG")
    if not log_path:
        raise AssertionError("WEBSERV_TEST_LOG is not set")
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        path = Path(log_path)
        if path.exists() and text in path.read_text():
            return
        time.sleep(0.05)
    raise AssertionError("Server log did not contain: " + text)


def log_count(text):
    log_path = os.environ.get("WEBSERV_TEST_LOG")
    if not log_path:
        raise AssertionError("WEBSERV_TEST_LOG is not set")
    path = Path(log_path)
    if not path.exists():
        return 0
    return sum(text in line for line in path.read_text().splitlines())


def wait_for_log_count(text, expected, timeout=3):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if log_count(text) >= expected:
            return
        time.sleep(0.05)
    raise AssertionError(
        "Expected at least " + str(expected) + " log entries for: " + text
    )


def read_response(client, timeout=RESPONSE_TIMEOUT):
    client.settimeout(timeout)
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = client.recv(4096)
        if not chunk:
            raise AssertionError("Connection closed before response headers")
        data += chunk
        if len(data) > 1024 * 1024:
            raise AssertionError("Response headers exceed test limit")

    header_data, body = data.split(b"\r\n\r\n", 1)
    lines = header_data.split(b"\r\n")
    parts = lines[0].split(b" ", 2)
    if len(parts) != 3:
        raise AssertionError("Malformed response status line: " + repr(lines[0]))
    headers = {}
    for line in lines[1:]:
        if b":" not in line:
            raise AssertionError("Malformed response header: " + repr(line))
        key, value = line.split(b":", 1)
        headers[key.decode("ascii").lower()] = value.strip().decode("ascii")

    if "content-length" not in headers:
        raise AssertionError("Response has no Content-Length")
    length = int(headers["content-length"])
    while len(body) < length:
        chunk = client.recv(4096)
        if not chunk:
            raise AssertionError("Connection closed before response body")
        body += chunk
    return int(parts[1]), headers, body[:length]


def assert_response(response, status, body=None):
    actual_status, headers, actual_body = response
    assert actual_status == status, (actual_status, headers, actual_body)
    assert int(headers["content-length"]) == len(actual_body)
    if body is not None:
        assert actual_body == body, (actual_body, body)


def read_until_closed(client, timeout=RESPONSE_TIMEOUT):
    client.settimeout(timeout)
    while True:
        data = client.recv(4096)
        if not data:
            return
