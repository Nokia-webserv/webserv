"""Small test utilities shared by the GET checks; no production code is replaced."""

from collections import namedtuple
from contextlib import contextmanager
import hashlib
import os
from pathlib import Path
import socket
import subprocess
import time

Response = namedtuple("Response", "version status headers body")


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def exact_body(actual, expected):
    if actual == expected:
        return
    first = next((i for i, pair in enumerate(zip(actual, expected))
                  if pair[0] != pair[1]), min(len(actual), len(expected)))
    raise AssertionError(
        "Body mismatch: expected " + str(len(expected)) + " bytes, received "
        + str(len(actual)) + "; first difference at byte " + str(first)
        + "; expected SHA256=" + hashlib.sha256(expected).hexdigest()
        + "; received SHA256=" + hashlib.sha256(actual).hexdigest())


def check_response(response, status=200, body=None, mime=None, version="HTTP/1.1"):
    require(response.version == version,
            "Expected " + version + ", received " + response.version)
    require(response.status == status,
            "Expected HTTP " + str(status) + ", received " + str(response.status))
    require(response.headers.get("content-type"), "Missing Content-Type")
    require(int(response.headers["content-length"]) == len(response.body),
            "Content-Length does not match received bytes")
    if mime is not None:
        require(response.headers["content-type"] == mime,
                "Expected Content-Type " + mime + ", received "
                + response.headers["content-type"])
    if body is not None:
        exact_body(response.body, body)
    if status >= 400:
        require(response.body, "Empty error response")
    if status == 200:
        require("location" not in response.headers, "Unexpected redirect header")


class Reporter:
    def __init__(self):
        self.groups = {}
        self.current = None

    @staticmethod
    def note(text):
        print("    " + text, flush=True)

    def case(self, group, name, expected, test, skip=None):
        if group not in self.groups:
            self.groups[group] = [0, 0, 0]
        if self.current != group:
            self.current = group
            print("\n" + "=" * 72 + "\n" + group + "\n" + "=" * 72, flush=True)
        counts = self.groups[group]
        number = sum(counts) + 1
        identifier = group.split(" ", 1)[0] + "-" + str(number).zfill(2)
        print("\n[" + identifier + "] " + name, flush=True)
        self.note("Expected: " + expected)
        if skip:
            counts[2] += 1
            print("\033[33m[SKIP]\033[0m " + identifier + ": " + skip, flush=True)
            return
        try:
            test()
            counts[0] += 1
            print("\033[32m[PASS]\033[0m " + identifier + " " + name, flush=True)
        except Exception as error:
            counts[1] += 1
            print("\033[31m[FAIL]\033[0m " + identifier + ": "
                  + type(error).__name__ + ": " + str(error), flush=True)

    def summary(self):
        print("\n" + "=" * 72 + "\nRESULTS BY GROUP\n" + "=" * 72)
        print("{:<45} {:>6} {:>6} {:>6}".format("Group", "PASS", "FAIL", "SKIP"))
        totals = [0, 0, 0]
        for group, counts in self.groups.items():
            print("{:<45} {:>6} {:>6} {:>6}".format(group, *counts))
            totals = [total + count for total, count in zip(totals, counts)]
        print("\nPassed: {}, failed: {}, skipped: {}".format(*totals))
        return 1 if totals[1] else 0


class CurlClient:
    def __init__(self, port, directory, note=None):
        self.base_url = "http://127.0.0.1:" + str(port)
        self.directory = Path(directory)
        self.counter = 0
        self.note = note or (lambda text: None)

    def batch(self, targets, version="HTTP/1.1", rate=None, timeout=8):
        command = ["curl", "-q", "--fail-early"]
        outputs = []
        for target in targets:
            if outputs:
                command.append("--next")
            self.counter += 1
            header = self.directory / ("response-" + str(self.counter) + ".headers")
            body = self.directory / ("response-" + str(self.counter) + ".body")
            outputs.append((header, body))
            self.note("Request: GET " + target + " " + version)
            command.extend([
                "--silent", "--show-error", "--noproxy", "*", "--path-as-is",
                "--connect-timeout", "2", "--max-time", str(timeout),
                "--http1.0" if version == "HTTP/1.0" else "--http1.1",
                "--header", "Host: localhost", "--dump-header", str(header),
                "--output", str(body), "--write-out", "%{num_connects}\n"])
            if rate:
                command.extend(["--limit-rate", rate])
            command.extend(["--url", self.base_url + target])
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=(timeout + 1) * len(targets))
        if result.returncode:
            statuses = [header.read_bytes().split(b"\r\n", 1)[0].decode("ascii", "replace")
                        for header, _ in outputs if header.exists()]
            raise AssertionError("curl exit " + str(result.returncode) + ": "
                                 + result.stderr.strip() + "; status lines=" + repr(statuses))
        responses = [self.read_response(header, body) for header, body in outputs]
        connections = [int(line) for line in result.stdout.splitlines()]
        require(len(connections) == len(targets), "Missing curl connection metrics")
        return responses, connections

    def get(self, target, status=200, body=None, mime=None, version="HTTP/1.1",
            location=None, **options):
        responses, _ = self.batch([target], version=version, **options)
        response = responses[0]
        check_response(response, status, body, mime, version)
        if location is not None:
            require(response.headers.get("location") == location,
                    "Expected Location " + location + ", received "
                    + repr(response.headers.get("location")))
            self.note("Location: " + location)
        if body is not None:
            self.note("Body: exact byte comparison passed (" + str(len(body)) + " bytes)")
        return response

    def read_response(self, header_file, body_file):
        raw = header_file.read_bytes()
        require(raw.endswith(b"\r\n\r\n"), "Incomplete HTTP headers")
        lines = raw[:-4].split(b"\r\n")
        version, code, reason = lines[0].decode("ascii").split(" ", 2)
        require(reason, "Empty reason phrase")
        reasons = {200: "OK", 301: "Moved Permanently", 403: "Forbidden",
                   404: "Not Found", 405: "Method Not Allowed", 500: "Internal Server Error"}
        status = int(code)
        require(reason == reasons.get(status), "Unexpected HTTP reason phrase: " + reason)
        headers = {}
        for line in lines[1:]:
            key, value = line.split(b":", 1)
            key = key.decode("ascii").lower()
            require(key not in headers, "Duplicate response header: " + key)
            headers[key] = value.strip().decode("ascii")
        length = headers.get("content-length", "")
        require(length.isascii() and length.isdecimal(), "Invalid or missing Content-Length")
        body = body_file.read_bytes()
        require(int(length) == len(body), "Content-Length does not match downloaded bytes")
        self.note("Received: " + lines[0].decode("ascii") + "; Content-Type="
                  + headers.get("content-type", "<missing>") + "; Content-Length="
                  + length + "; downloaded=" + str(len(body)))
        return Response(version, status, headers, body)


def free_port():
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


@contextmanager
def running_server(executable, config, directory, port):
    with open(Path(directory) / "server.log", "w+") as log:
        server = subprocess.Popen([str(executable), str(config)], stdout=log,
                                  stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 5
            while True:
                require(server.poll() is None, "Server exited during startup")
                result = subprocess.run([
                    "curl", "-q", "--silent", "--noproxy", "*", "--http1.1",
                    "--connect-timeout", "0.2", "--max-time", "0.5",
                    "--output", os.devnull, "--write-out", "%{http_code}",
                    "http://127.0.0.1:" + str(port) + "/"], text=True,
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=1)
                if result.returncode == 0 and result.stdout == "200":
                    break
                require(time.monotonic() < deadline, "Server did not become ready")
                time.sleep(0.05)
            yield server
        except BaseException:
            log.flush()
            log.seek(0)
            print("Server log:\n" + log.read()[-6000:])
            raise
        finally:
            if server.poll() is None:
                server.terminate()
            try:
                server.wait(timeout=3)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait()
