#!/usr/bin/env python3
"""42 Webserv mandatory CGI checks; one interpreter, no bonus features."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
TIMEOUT = 4


def require(condition, message):
    if not condition:
        raise AssertionError(message)


class Reporter:
    def __init__(self):
        self.results = []
        self.group = None

    def case(self, group, name, expected, check, skip=None):
        if group != self.group:
            self.group = group
            print("\n=== " + group + " ===", flush=True)
        print("\nBefore:   " + name + "\nExpected: " + expected, flush=True)
        if skip:
            print("\033[33m[SKIP]\033[0m " + skip, flush=True)
            self.results.append((group, "SKIP", name))
            return
        try:
            check()
        except Exception as error:
            print("\033[31m[FAIL]\033[0m " + str(error), flush=True)
            self.results.append((group, "FAIL", name))
        else:
            print("\033[32m[PASS]\033[0m " + name, flush=True)
            self.results.append((group, "PASS", name))

    def summary(self):
        print("\n=== RESULTS ===", flush=True)
        for group in dict.fromkeys(row[0] for row in self.results):
            counts = [sum(g == group and s == status for g, s, _ in self.results)
                      for status in ("PASS", "FAIL", "SKIP")]
            print("{}: passed {}, failed {}, skipped {}".format(group, *counts))
        counts = [sum(s == status for _, s, _ in self.results) for status in ("PASS", "FAIL", "SKIP")]
        print("TOTAL: passed {}, failed {}, skipped {}".format(*counts), flush=True)
        return bool(counts[1])


def parse_response(wire, expected=None, exact=None):
    require(b"\r\n\r\n" in wire, "Missing complete HTTP headers: " + repr(wire[:160]))
    head, body = wire.split(b"\r\n\r\n", 1)
    lines = head.split(b"\r\n")
    parts = lines[0].split(b" ", 2)
    require(len(parts) == 3 and parts[0] in (b"HTTP/1.0", b"HTTP/1.1"), "Invalid status line: " + repr(lines[0]))
    require(re.fullmatch(rb"[1-5][0-9][0-9]", parts[1]), "Invalid status code: " + repr(parts[1]))
    status = int(parts[1])
    if expected is not None:
        require(status in expected if isinstance(expected, tuple) else status == expected,
                "Expected HTTP " + str(expected) + ", received " + str(status))
    headers = {}
    for line in lines[1:]:
        require(b":" in line, "Malformed header: " + repr(line))
        key, value = line.split(b":", 1)
        require(re.fullmatch(rb"[!#$%&'*+.^_`|~0-9A-Za-z-]+", key), "Invalid header name: " + repr(key))
        require(not any(c < 32 and c != 9 or c == 127 for c in value), "Control byte in header")
        headers.setdefault(key.lower(), []).append(value.strip())
    lengths = headers.get(b"content-length", [])
    if lengths:
        require(len(lengths) == 1 and lengths[0].isdigit(), "Duplicate/invalid Content-Length: " + repr(lengths))
        require(int(lengths[0]) == len(body), "Content-Length " + repr(lengths[0]) + " but actual body bytes=" + str(len(body)))
    require(b"status" not in headers, "CGI Status leaked as an HTTP header")
    if exact is not None:
        require(body == exact, "Body mismatch: expected {} bytes, received {}".format(len(exact), len(body)))
    print("After:    {} | body bytes: {}".format(lines[0].decode("latin1"), len(body)), flush=True)
    return status, headers, body


def compile_binary(build, component=False):
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    command = compiler + ["-std=c++98", "-Wall", "-Wextra", "-Werror", "-I" + str(ROOT / "include")]
    executable = build / ("cgi_component" if component else "webserv")
    if component:
        header = (ROOT / "include/CGI.hpp").read_text()
        require(header.count("class CGI {") == 1, "Cannot grant CGI test-peer access")
        (build / "CGI.hpp").write_text(header.replace("class CGI {", "class CGI {\n    friend class CGITestPeer;", 1))
        wrapper = '''#include "CGI.hpp"
#include <fcntl.h>
#include <iostream>
#include <ctime>
#include <poll.h>
int cgiTestPipe(int*);
pid_t cgiTestFork();
int cgiTestFcntl(int, int, int);
ssize_t cgiTestWrite(int, const void*, size_t);
#define pipe cgiTestPipe
#define fork cgiTestFork
#define fcntl cgiTestFcntl
#define write cgiTestWrite
'''
        (build / "cgi_instrumented.cpp").write_text(wrapper + '#include ' + json.dumps(str(ROOT / "source/CGI.cpp")) + '\n')
        command.insert(len(compiler), "-I" + str(build))
        sources = [HERE / "component.cpp", build / "cgi_instrumented.cpp"]
        sources += [ROOT / "source" / (name + ".cpp") for name in (
            "Router", "HttpRequest", "HttpResponse", "ServerConfig", "LocationConfig", "utils")]
    else:
        command += ["-DWEBSERV_CLIENT_TIMEOUT_SECONDS=" + str(TIMEOUT)]
        sources = sorted((ROOT / "source").glob("*.cpp"))
    print("Building " + executable.name + " (C++98)...", flush=True)
    subprocess.run(command + list(map(str, sources)) + ["-o", str(executable)], check=True)
    return executable


def component_tests(report, build, fixtures):
    executable = compile_binary(build, component=True)

    def invoke(mode, data=b"", *arguments):
        run = subprocess.run([str(executable), mode, str(fixtures), *arguments], input=data,
                             capture_output=True, timeout=8)
        require(run.returncode == 0, "Component exited " + str(run.returncode) + ": " + run.stderr.decode("utf8", "replace"))
        return run.stdout

    def output(data, status=200, body=b"hello", invalid=False):
        meta, wire = invoke("output", data).split(b"\n", 1)
        if invalid:
            require(meta == b"0 0", "Malformed CGI output was accepted: " + repr(wire[:180]))
            print("After:    malformed CGI output rejected", flush=True)
        else:
            require(meta == b"1 1", "Valid CGI output rejected")
            parse_response(wire, status, body)

    basics = [
        ("CRLF headers; default status", b"Content-Type: text/plain\r\n\r\nhello", 200, b"hello"),
        ("LF headers", b"Content-Type: text/plain\n\nhello", 200, b"hello"),
        ("Explicit 201 status", b"Status: 201 Created\r\nContent-Type: text/plain\r\n\r\nhello", 201, b"hello"),
        ("Lowercase Status", b"status: 404 Not Found\nContent-Type: text/plain\n\nhello", 404, b"hello"),
        ("Empty body", b"Content-Type: text/plain\r\n\r\n", 200, b""),
        ("Body includes all byte values", b"Content-Type: application/octet-stream\n\n" + bytes(range(256)), 200, bytes(range(256))),
        ("Header-like body preserved", b"Content-Type: text/plain\n\nStatus: 500 Error\r\n\r\nbody", 200, b"Status: 500 Error\r\n\r\nbody"),
        ("Existing correct Content-Length", b"Content-Type: text/plain\nContent-Length: 5\n\nhello", 200, b"hello"),
        ("Custom header with colon in value", b"Content-Type: text/plain\nX-Link: https://example.test/a:b\n\nhello", 200, b"hello"),
        ("One MiB CGI output", b"Content-Type: application/octet-stream\n\n" + bytes(range(256)) * 4096, 200, bytes(range(256)) * 4096),
    ]
    for name, data, status, body in basics:
        report.case("C1 CGI output", name, "HTTP " + str(status) + ", exact body bytes and framing",
                    lambda d=data, s=status, b=body: output(d, s, b))
    for name, data in [
        ("No output", b""), ("Missing header/body separator", b"Content-Type: text/plain\nhello"),
        ("Header without colon", b"bad header\n\nhello"),
        ("Non-numeric status", b"Status: nope\n\n"),
    ]:
        report.case("C1 CGI output", name, "Reject malformed output", lambda d=data: output(d, invalid=True))
    raw = "x=one%26two&literal=%2520&plus=a+b"
    environment = dict(entry.decode().split("=", 1) for entry in invoke(
        "env", b"a\x00b\xff", "POST", "x=one&two&literal=%20&plus=a+b", raw).split(b"\0") if entry)
    expected = {"REQUEST_METHOD": "POST", "QUERY_STRING": raw,
                "SCRIPT_NAME": "/cgi/probe.py", "SERVER_PROTOCOL": "HTTP/1.1",
                "SERVER_NAME": "example.test", "SERVER_PORT": "8123",
                "CONTENT_LENGTH": "4", "CONTENT_TYPE": "application/octet-stream",
                "HTTP_X_TEST_TOKEN": "AbC-123"}
    for key, value in expected.items():
        def check_env(k=key, v=value):
            actual = environment.get(k)
            print("After:    " + k + "=" + repr(actual), flush=True)
            require(actual == v, "Expected " + repr(v) + ", received " + repr(actual))
        report.case("C2 Request environment", key, key + "=" + repr(value), check_env)
    for number, name in [(1, "First pipe failure"), (2, "Second pipe failure"), (3, "fork failure"), (4, "fcntl failure")]:
        def fault_check(n=number):
            result = invoke("fault", b"", str(n)).decode()
            print("After:    " + result.strip().replace("\n", " | "), flush=True)
            require("error=500" in result and "closed=1 cleared=1 orphan=0" in result,
                    "Setup failure left descriptors/client state/untracked child, or did not return 500")
        report.case("C3 Process setup", name, "500, descriptors closed and client CGI pointer cleared", fault_check)
    def readiness_check():
        result = invoke("poll", b"input").decode()
        print("After:    " + result.strip().replace("\n", " | "), flush=True)
        require("ungated=0" in result, "runScript writes to stdin pipe before returning to the poll loop")
    report.case("C3 Process setup", "No parent pipe write before poll readiness",
                "runScript performs zero writes before event-loop readiness", readiness_check)


class WireReader:
    def __init__(self, connection):
        self.connection = connection
        self.buffer = b""

    def response(self, expected=200, body=None):
        while b"\r\n\r\n" not in self.buffer:
            self.receive()
        head, _ = self.buffer.split(b"\r\n\r\n", 1)
        lengths = re.findall(rb"(?im)^content-length:[ \t]*([0-9]+)\r?$", head)
        if not lengths:
            if re.search(rb"(?im)^transfer-encoding:[ \t]*chunked\r?$", head):
                cursor = len(head) + 4
                chunks = []
                while True:
                    while b"\r\n" not in self.buffer[cursor:]:
                        self.receive()
                    end = self.buffer.index(b"\r\n", cursor)
                    size = int(self.buffer[cursor:end].split(b";", 1)[0], 16)
                    cursor = end + 2
                    if size == 0:
                        while True:
                            while b"\r\n" not in self.buffer[cursor:]:
                                self.receive()
                            end = self.buffer.index(b"\r\n", cursor)
                            empty = end == cursor
                            cursor = end + 2
                            if empty:
                                self.buffer = self.buffer[cursor:]
                                return parse_response(head + b"\r\n\r\n" + b"".join(chunks), expected, body)
                    while len(self.buffer) < cursor + size + 2:
                        self.receive()
                    require(self.buffer[cursor + size:cursor + size + 2] == b"\r\n", "Invalid chunk framing")
                    chunks.append(self.buffer[cursor:cursor + size])
                    cursor += size + 2
            while True:
                data = self.connection.recv(65536)
                if not data:
                    wire, self.buffer = self.buffer, b""
                    return parse_response(wire, expected, body)
                self.buffer += data
        require(len(lengths) == 1, "Duplicate length in raw response: " + repr(head[:200]))
        total = len(head) + 4 + int(lengths[0])
        while len(self.buffer) < total:
            self.receive()
        wire, self.buffer = self.buffer[:total], self.buffer[total:]
        return parse_response(wire, expected, body)

    def receive(self):
        data = self.connection.recv(65536)
        require(data, "Connection closed before a complete HTTP response")
        self.buffer += data


class TestServer:
    def __init__(self, executable, directory, fixtures):
        self.directory = directory
        self.fixtures = fixtures
        self.executable = executable
        self.process = None
        self.counter = 0
        self.lock = threading.Lock()

    def start(self):
        with socket.socket() as reserve:
            reserve.bind(("127.0.0.1", 0))
            self.port = reserve.getsockname()[1]
        document_root = self.directory / "document_root"
        document_root.mkdir(exist_ok=True)
        (document_root / "static.txt").write_bytes(b"static server still works\n")
        config = self.directory / "cgi.conf"
        config.write_text('''server {
    listen 127.0.0.1:PORT;
    server_name example.test;
    client_max_body_size 4m;
    location /cgi {
        root "SCRIPTS";
        methods GET POST DELETE;
        cgi_extension .py "PYTHON";
    }
    location /get-only {
        root "SCRIPTS";
        methods GET;
        cgi_extension .py "PYTHON";
    }
    location /limited {
        root "SCRIPTS";
        methods POST;
        client_max_body_size 4;
        cgi_extension .py "PYTHON";
    }
    location /bad-interpreter {
        root "SCRIPTS";
        methods GET;
        cgi_extension .py /nonexistent/webserv-cgi-interpreter;
    }
    location / {
        root "DOCUMENT_ROOT";
        methods GET;
    }
}
'''.replace("PORT", str(self.port)).replace("SCRIPTS", str(self.fixtures))
                          .replace("PYTHON", sys.executable).replace("DOCUMENT_ROOT", str(document_root)))
        self.log = open(self.directory / "server.log", "ab", buffering=0)
        self.process = subprocess.Popen([str(self.executable), str(config)], cwd=ROOT,
                                        stdout=self.log, stderr=self.log, start_new_session=True)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            require(self.process.poll() is None, "Server exited during startup; see " + str(self.directory / "server.log"))
            try:
                with socket.create_connection(("127.0.0.1", self.port), timeout=0.2):
                    return
            except OSError:
                time.sleep(0.03)
        raise AssertionError("Server did not start listening")

    def stop(self):
        if self.process is not None:
            # Kill only the session created by this test, including hung CGI children.
            try:
                os.killpg(self.process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            self.process.wait(timeout=5)
            self.log.close()
            self.process = None

    def connect(self):
        require(self.process.poll() is None, "Server crashed: exit " + str(self.process.returncode))
        connection = socket.create_connection(("127.0.0.1", self.port), timeout=3)
        connection.settimeout(TIMEOUT + 3)
        return connection

    def request_bytes(self, target="/cgi/probe.py", method="GET", body=b"", version="HTTP/1.0", headers=None):
        values = {"Host": "example.test:" + str(self.port), "Content-Length": str(len(body))}
        values.update(headers or {})
        return (method + " " + target + " " + version + "\r\n" + "".join(k + ": " + v + "\r\n" for k, v in values.items())
                + "\r\n").encode() + body

    def curl(self, target="/cgi/probe.py", method="GET", body=None, expected=200,
             exact=None, version="HTTP/1.0", headers=None, timeout=7):
        with self.lock:
            self.counter += 1
            output = self.directory / ("curl-" + str(self.counter))
        head_path, body_path = output.with_suffix(".head"), output.with_suffix(".body")
        command = ["curl", "--silent", "--show-error", "--noproxy", "*", "--path-as-is",
                   "--max-time", str(timeout), "--http1.0" if version == "HTTP/1.0" else "--http1.1",
                   "--request", method, "--header", "Host: example.test:" + str(self.port),
                   "--header", "Expect:", "--dump-header", str(head_path), "--output", str(body_path)]
        for key, value in (headers or {}).items():
            command += ["--header", key + ": " + value]
        if body is not None:
            input_path = output.with_suffix(".input")
            input_path.write_bytes(body)
            command += ["--data-binary", "@" + str(input_path)]
        command += ["http://127.0.0.1:" + str(self.port) + target]
        run = subprocess.run(command, capture_output=True, timeout=timeout + 2)
        require(run.returncode == 0, "curl exit " + str(run.returncode) + ": " + run.stderr.decode().strip())
        wire = head_path.read_bytes() + body_path.read_bytes()
        result = parse_response(wire, expected, exact)
        return result


def integration_tests(report, build, fixtures):
    executable = compile_binary(build)
    require(shutil.which("curl"), "curl is required")
    server = TestServer(executable, build, fixtures)
    try:
        server.start()
        for method in ("GET", "POST", "DELETE"):
            def check_method(value=method):
                _, _, body = server.curl(method=value, body=b"abc" if value != "GET" else None)
                data = json.loads(body)
                require(data["env"]["REQUEST_METHOD"] == value, "Wrong REQUEST_METHOD")
                require(data["length"] == (0 if value == "GET" else 3), "Wrong stdin body")
            report.case("I1 Subject: CGI requests", method + " reaches CGI", "Correct REQUEST_METHOD and stdin EOF", check_method)
        for size in (0, 1, 4095, 4096, 4097, 65536, 1048576):
            payload = (bytes(range(256)) * ((size + 255) // 256))[:size]
            report.case("I1 Subject: CGI requests", "Binary POST body, bytes=" + str(size), "Exact input bytes echoed without loss",
                        lambda data=payload: server.curl("/cgi/probe.py?mode=echo", "POST", data, exact=data,
                                                       headers={"Content-Type": "application/octet-stream"}))
        def query_check():
            query = "mode=env&value=a%26b%3Dc&literal=%2520&plus=a+b"
            _, _, body = server.curl("/cgi/probe.py?" + query)
            actual = json.loads(body)["env"]["QUERY_STRING"]
            require(actual == query, "QUERY_STRING changed: " + repr(actual))
        report.case("I1 Subject: CGI requests", "Encoded query arguments", "Original URL-encoded QUERY_STRING preserved", query_check)
        def headers_check():
            _, _, body = server.curl(headers={"X-Test-Token": "AbC123"})
            env = json.loads(body)["env"]
            require(env.get("HTTP_X_TEST_TOKEN") == "AbC123", "Request header missing from CGI environment")
        report.case("I1 Subject: CGI requests", "Custom request header", "HTTP_* meta-variable preserves its value", headers_check)
        report.case("I1 Subject: CGI requests", "Relative file access", "Script opens a file relative to its own directory",
                    lambda: server.curl("/cgi/probe.py?mode=cwd", exact=b"relative path works\n"))
        report.case("I1 Subject: CGI requests", "GET forbidden before CGI", "HTTP 405",
                    lambda: server.curl("/get-only/probe.py", "POST", b"abc", expected=405))
        report.case("I1 Subject: CGI requests", "Location body limit before CGI", "HTTP 413",
                    lambda: server.curl("/limited/probe.py", "POST", b"12345", expected=413))
        def chunked():
            payload = b"one\x00two\xffthree"
            head = ("POST /cgi/probe.py HTTP/1.1\r\nHost: example.test:" + str(server.port)
                    + "\r\nTransfer-Encoding: chunked\r\nContent-Type: application/octet-stream\r\n\r\n").encode()
            chunks = b"4\r\n" + payload[:4] + b"\r\n" + format(len(payload[4:]), "x").encode() + b"\r\n" + payload[4:] + b"\r\n0\r\n\r\n"
            with server.connect() as connection:
                connection.sendall(head + chunks)
                _, _, body = WireReader(connection).response()
            data = json.loads(body)
            require(data["length"] == len(payload) and data["sha256"] == hashlib.sha256(payload).hexdigest(), "Chunk framing leaked or stdin bytes lost")
            require(data["env"]["CONTENT_LENGTH"] == str(len(payload)), "CGI CONTENT_LENGTH does not describe decoded body")
        report.case("I1 Subject: CGI requests", "Chunked POST", "Dechunked binary stdin, correct length, EOF", chunked)
        cases = [
            ("CGI 201 response", "status", 201, b"created"),
            ("CGI redirect response", "redirect", 302, b"redirect"),
            ("Headers and body split over writes", "partial", 200, b"firstsecond\x00third"),
            ("One MiB CGI output without Content-Length", "large", 200, bytes(range(256)) * 4096),
            ("stderr stays out of HTTP response", "stderr", 200, b"stdout only"),
            ("Malformed output", "malformed", (500, 502), None),
            ("Empty CGI output", "empty", (500, 502), None),
            ("Script killed by signal", "crash", (500, 502), None),
        ]
        for name, mode, status, data in cases:
            report.case("I2 Subject: CGI responses", name, "HTTP " + str(status) + ", valid framing and complete body",
                        lambda m=mode, s=status, d=data: server.curl("/cgi/probe.py?mode=" + m, expected=s, exact=d))
        report.case("I2 Subject: CGI responses", "Interpreter does not exist", "HTTP 500 or 502; server remains alive",
                    lambda: server.curl("/bad-interpreter/probe.py", expected=(500, 502)))
        report.case("I2 Subject: CGI responses", "Script does not exist", "An HTTP error response; no hang",
                    lambda: server.curl("/cgi/nonexistent.py", expected=(404, 500, 502)))
        def duplex():
            payload = b"I" * 262144
            server.curl("/cgi/probe.py?mode=duplex", "POST", payload, exact=b"O" * 131072 + payload)
        report.case("I3 Subject: nonblocking I/O", "CGI writes a full pipe before consuming stdin",
                    "Both directions progress; no deadlock; exact bytes", duplex)
        def concurrent():
            with ThreadPoolExecutor(max_workers=8) as pool:
                futures = [pool.submit(server.curl, "/cgi/probe.py?mode=echo", "POST", ("body-" + str(i)).encode(),
                                       exact=("body-" + str(i)).encode()) for i in range(8)]
                for future in futures:
                    future.result()
        report.case("I3 Subject: nonblocking I/O", "Eight simultaneous CGI clients", "No response/body mixing", concurrent)
        def static_during_cgi():
            with server.connect() as connection:
                connection.sendall(server.request_bytes("/cgi/probe.py?mode=sleep&seconds=1.5"))
                time.sleep(0.15)
                started = time.monotonic()
                server.curl("/static.txt", exact=b"static server still works\n")
                require(time.monotonic() - started < 1.1, "Static response blocked behind CGI")
                WireReader(connection).response(body=b"finished")
        report.case("I3 Subject: nonblocking I/O", "Static request while CGI sleeps", "Static response arrives before slow CGI finishes", static_during_cgi)
        def timeout_check():
            # Fresh static clients verify availability without requiring keep-alive.
            with server.connect() as connection:
                connection.sendall(server.request_bytes("/cgi/probe.py?mode=hang"))
                started = time.monotonic()
                for _ in range(TIMEOUT + 2):
                    server.curl("/static.txt", exact=b"static server still works\n")
                    time.sleep(0.9)
                try:
                    WireReader(connection).response(expected=(500, 502, 504))
                except AssertionError as error:
                    if str(error) != "Connection closed before a complete HTTP response":
                        raise
                    print("After:    timed-out connection closed without HTTP response", flush=True)
                require(time.monotonic() - started < TIMEOUT + 5, "CGI was not stopped within the bounded test deadline")
        report.case("I4 Subject: bounded lifetime", "Hung CGI with another active client",
                    "Bounded error response or connection close; other client stays responsive", timeout_check)
        def cleanup():
            pid_file = fixtures / "child.pid"
            pid_file.unlink(missing_ok=True)
            connection = server.connect()
            try:
                connection.sendall(server.request_bytes("/cgi/probe.py?mode=pid"))
                deadline = time.monotonic() + 2
                while not pid_file.exists() and time.monotonic() < deadline:
                    time.sleep(0.02)
                require(pid_file.exists(), "CGI did not create PID marker")
                pid = int(pid_file.read_text())
            finally:
                connection.close()
            deadline = time.monotonic() + TIMEOUT + 3
            while time.monotonic() < deadline:
                try:
                    os.kill(pid, 0)
                except ProcessLookupError:
                    server.curl("/static.txt", exact=b"static server still works\n")
                    return
                time.sleep(0.1)
            raise AssertionError("Disconnected client's CGI process still exists: PID " + str(pid))
        report.case("I4 Subject: bounded lifetime", "Client disconnects while CGI is running",
                    "Child is killed/reaped within timeout; server stays responsive", cleanup)
        report.case("I4 Subject: bounded lifetime", "Static GET after all error/stress cases", "Server still serves static content",
                    lambda: server.curl("/static.txt", exact=b"static server still works\n"))
    finally:
        server.stop()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    layer = parser.add_mutually_exclusive_group()
    layer.add_argument("--component", action="store_true", help="Only CGI output/env/setup checks; no TCP listener")
    layer.add_argument("--integration", action="store_true", help="Only real-server curl/socket checks")
    args = parser.parse_args()
    report = Reporter()
    with tempfile.TemporaryDirectory(prefix="webserv-cgi-") as temporary:
        build = Path(temporary)
        fixtures = build / "scripts"
        fixtures.mkdir()
        shutil.copyfile(HERE / "probe.py", fixtures / "probe.py")
        (fixtures / "relative.txt").write_bytes(b"relative path works\n")
        if not args.integration:
            component_tests(report, build, fixtures)
        if not args.component:
            integration_tests(report, build, fixtures)
    return report.summary()


if __name__ == "__main__":
    sys.exit(main())
