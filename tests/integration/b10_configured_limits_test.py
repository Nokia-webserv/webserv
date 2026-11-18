#!/usr/bin/env python3
"""GH-104 / #124: end-to-end configured HTTP body and header limits.

Usage (matches tests/integration/run_tests.sh):
  python3 tests/integration/g1_configured_limits_test.py ./webserv tests/integration/basic.conf

Creates an isolated server/config/fixtures. The supplied config argument is
accepted for runner compatibility but deliberately not used.

NOTE: Uses GET requests with Content-Length bodies to test framing without
requiring an upload/POST handler. If Webserv explicitly forbids GET bodies,
change the successful request cases to POST against a working POST endpoint.
"""
import contextlib
import os
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

HOST = "127.0.0.1"
SERVER_LIMIT = 1024
LOCATION_LIMIT = 4096
HEADER_LIMIT = 1024


def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((HOST, 0))
        return s.getsockname()[1]


@contextlib.contextmanager
def server_fixture(executable):
    executable = str(Path(executable).resolve())
    if not Path(executable).is_file():
        raise RuntimeError("Server executable missing: " + executable)
    with tempfile.TemporaryDirectory(prefix="webserv_limits_") as tmp:
        base = Path(tmp)
        www = base / "www"
        www.mkdir()

        # Support location-prefix stripping
        (www / "ok.txt").write_text("LIMITS_OK", encoding="ascii")

        # Support location-prefix preservation
        for location in ("large", "small"):
            path = www / location
            path.mkdir()
            (path / "ok.txt").write_text("LIMITS_OK", encoding="ascii")
        port = free_port()
        config = base / "limits.conf"
        config.write_text(
            "server {\n"
            f"    listen {HOST}:{port};\n"
            "    server_name localhost;\n"
            f"    client_max_body_size {SERVER_LIMIT};\n"
            f"    client_max_header_size {HEADER_LIMIT};\n"
            "    location /large {\n"
            "        root www;\n"
            "        methods GET;\n"
            f"        client_max_body_size {LOCATION_LIMIT};\n"
            "    }\n"
            "    location /small {\n"
            "        root www;\n"
            "        methods GET;\n"
            "    }\n"
            "}\n", encoding="ascii"
        )
        logpath = base / "server.log"
        with logpath.open("wb") as log:
            proc = subprocess.Popen([executable, str(config)], cwd=str(base), stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline:
                    if proc.poll() is not None:
                        raise RuntimeError("Webserv exited at startup:\n" + logpath.read_text(errors="replace"))
                    try:
                        with socket.create_connection((HOST, port), timeout=0.2):
                            break
                    except OSError:
                        time.sleep(0.05)
                else:
                    raise RuntimeError("Webserv did not listen; log:\n" + logpath.read_text(errors="replace"))
                yield port
            finally:
                if proc.poll() is None:
                    proc.terminate()
                    try:
                        proc.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        proc.kill()
                        proc.wait()


def request(port, path, body=b"", extra_header=""):
    raw = (
        f"GET {path} HTTP/1.1\r\n"
        f"Host: localhost\r\n"
        f"Content-Length: {len(body)}\r\n"
        "Connection: close\r\n"
        f"{extra_header}"
        "\r\n"
    ).encode("ascii") + body
    with socket.create_connection((HOST, port), timeout=4) as sock:
        sock.settimeout(4)
        sock.sendall(raw)
        response = bytearray()
        until = time.monotonic() + 5
        while time.monotonic() < until:
            try:
                data = sock.recv(8192)
            except socket.timeout:
                break
            if not data:
                break
            response.extend(data)
            # For this test, all successful fixtures have very small bodies,
            # and errors need only a status line.
            if b"\r\n\r\n" in response:
                break
    if b"\r\n" not in response:
        raise AssertionError(f"Missing HTTP response for {path}; got {bytes(response[:80])!r}")
    status_line = bytes(response).split(b"\r\n", 1)[0]
    try:
        status = int(status_line.split()[1])
    except (IndexError, ValueError) as exc:
        raise AssertionError(f"Invalid HTTP response: {status_line!r}") from exc
    return status


def expect(label, actual, expected):
    if actual != expected:
        raise AssertionError(f"{label}: expected HTTP {expected}, received HTTP {actual}")
    print(f"[PASS] {label}: HTTP {actual}", flush=True)


def main(executable):
    with server_fixture(executable) as port:
        print("[INFO] Started isolated Webserv with body limits 1024/4096 and header limit 1024", flush=True)
        expect("Baseline GET", request(port, "/small/ok.txt"), 200)
        expect("Inherited server limit, exact boundary", request(port, "/small/ok.txt", b"a" * 1024), 200)
        expect("Inherited server limit, +1", request(port, "/small/ok.txt", b"a" * 1025), 413)
        expect("Location override permits body larger than server limit", request(port, "/large/ok.txt", b"b" * 2048), 200)
        expect("Location override, exact boundary", request(port, "/large/ok.txt", b"b" * 4096), 200)
        expect("Location override, +1", request(port, "/large/ok.txt", b"b" * 4097), 413)
        expect("Oversized header", request(port, "/small/ok.txt", extra_header="X-Large: " + "z" * 2048 + "\r\n"), 431)


if __name__ == "__main__":
    if len(sys.argv) not in (2, 3):
        print(f"Usage: python3 {sys.argv[0]} ./webserv [config-file]", file=sys.stderr)
        sys.exit(2)
    try:
        main(sys.argv[1])
    except (AssertionError, RuntimeError, OSError) as exc:
        print(f"[FAIL] GH-104/#124: {exc}", file=sys.stderr)
        sys.exit(1)
