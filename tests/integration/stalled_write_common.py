#!/usr/bin/env python3
"""Shared fixtures for the C6/C7 Webserv stalled-write integration tests."""

import contextlib
import os
import socket
import subprocess
import tempfile
import time
from pathlib import Path

HOST = "127.0.0.1"
SERVER_TIMEOUT = int(os.environ.get("TEST_TIMEOUT", "30"))  # Webserv's configured stalled-write timeout (seconds)
BIG_FILE_SIZE = 32 * 1024 * 1024


def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind((HOST, 0))
        return sock.getsockname()[1]


def prepare_fixture(directory, port):
    base = Path(directory)
    www = base / "www"
    www.mkdir()
    # Write using bounded chunks, rather than allocating a huge Python string.
    with (www / "large.bin").open("wb") as output:
        chunk = b"W" * (1024 * 1024)
        for _ in range(BIG_FILE_SIZE // len(chunk)):
            output.write(chunk)

    config = base / "webserv.conf"
    config.write_text(
        "server {\n"
        f"    listen {HOST}:{port};\n"
        "    server_name localhost;\n"
        "    location / {\n"
        "        root www;\n"
        "        methods GET;\n"
        "    }\n"
        "}\n"
    )
    return config


@contextlib.contextmanager
def running_server(server_path):
    server = str(Path(server_path).resolve())
    if not Path(server).is_file():
        raise RuntimeError(f"Server binary not found: {server}")

    with tempfile.TemporaryDirectory(prefix="webserv_stall_") as directory:
        port = free_port()
        config = prepare_fixture(directory, port)
        with (Path(directory) / "webserv.log").open("wb") as log:
            process = subprocess.Popen(
                [server, str(config)],
                cwd=directory,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline:
                    if process.poll() is not None:
                        raise RuntimeError(
                            "Webserv exited during startup; log:\n"
                            + (Path(directory) / "webserv.log").read_text(errors="replace")
                        )
                    try:
                        with socket.create_connection((HOST, port), timeout=0.2):
                            break
                    except OSError:
                        time.sleep(0.05)
                else:
                    raise RuntimeError("Webserv did not start listening within 5 seconds")
                yield port
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()


def request_without_reading(port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # A small receive buffer makes the server's outbound socket genuinely back up.
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
    sock.settimeout(3)
    sock.connect((HOST, port))
    request = (
        "GET /large.bin HTTP/1.1\r\n"
        f"Host: {HOST}\r\n"
        "Connection: keep-alive\r\n"
        "\r\n"
    )
    sock.sendall(request.encode("ascii"))
    return sock


def check_stalled_connection_closed(sock, max_drain_seconds=12):
    """Drain *after* the timeout. EOF/reset before the full body = PASS.

    If the server is still connected when the client starts reading again, it
    can resume writing the complete response; that is a FAIL.
    """
    deadline = time.monotonic() + max_drain_seconds
    received = 0
    headers = bytearray()
    expected_body = None
    body_received = 0
    while time.monotonic() < deadline:
        sock.settimeout(min(2, max(0.1, deadline - time.monotonic())))
        try:
            data = sock.recv(65536)
        except (ConnectionResetError, BrokenPipeError, ConnectionAbortedError):
            return received, "connection reset/aborted"
        except socket.timeout:
            continue
        if not data:
            return received, "EOF"
        received += len(data)
        if expected_body is None:
            headers.extend(data)
            delimiter = headers.find(b"\r\n\r\n")
            if delimiter >= 0:
                lines = bytes(headers[:delimiter]).split(b"\r\n")
                if b"200" not in lines[0]:
                    raise AssertionError(f"Expected HTTP 200, got {lines[0]!r}")
                lengths = [line for line in lines[1:] if line.lower().startswith(b"content-length:")]
                if not lengths:
                    raise AssertionError("Response missing Content-Length; cannot verify truncation")
                expected_body = int(lengths[0].split(b":", 1)[1].strip())
                body_received = len(headers) - (delimiter + 4)
                headers.clear()
        else:
            body_received += len(data)
        if expected_body is not None and body_received >= expected_body:
            raise AssertionError("Full response delivered: stalled outgoing write was not timed out")
    raise AssertionError(
        f"Connection did not close within {max_drain_seconds}s of resuming reads "
        f"(received {received} bytes)"
    )
