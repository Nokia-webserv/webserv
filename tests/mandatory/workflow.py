#!/usr/bin/env python3
"""Mandatory Webserv workflow: build, configuration, methods, CGI, browser and load."""
import argparse
import base64
from contextlib import contextmanager
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
import time

ROOT = Path(__file__).resolve().parents[2]
TIMEOUT = 4


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def note(label, value):
    print("{:<10}{}".format(label + ":", value), flush=True)


class Report:
    def __init__(self):
        self.rows = []
        self.block = None

    def case(self, block, title, expected, function):
        if block != self.block:
            self.block = block
            print("\n" + "=" * 72 + "\n" + block + "\n" + "=" * 72, flush=True)
        print("", flush=True)
        note("Before", title)
        note("Expected", expected)
        try:
            function()
        except Exception as error:
            self.rows.append((block, "FAIL", title))
            print("\033[31m[FAIL]\033[0m " + str(error), flush=True)
        else:
            self.rows.append((block, "PASS", title))
            print("\033[32m[PASS]\033[0m " + title, flush=True)

    def summary(self):
        print("\n" + "=" * 72 + "\nMANDATORY WORKFLOW RESULTS\n" + "=" * 72)
        for block in dict.fromkeys(row[0] for row in self.rows):
            counts = [sum(b == block and s == status for b, s, _ in self.rows)
                      for status in ("PASS", "FAIL")]
            print("{}: PASS {}, FAIL {}".format(block, *counts))
        totals = [sum(s == status for _, s, _ in self.rows) for status in ("PASS", "FAIL")]
        print("\nTOTAL: PASS {}, FAIL {}".format(*totals), flush=True)
        for block, status, title in self.rows:
            if status != "PASS":
                print("  {} — {}: {}".format(status, block, title))
        return 1 if totals[1] else 0


def command(args, **options):
    result = subprocess.run(list(map(str, args)), capture_output=True, **options)
    require(result.returncode == 0, "Command failed: " + shlex.join(list(map(str, args))) + "\n"
            + (result.stdout + result.stderr).decode("utf8", "replace")[-2500:])
    return result


def free_ports(count):
    sockets = []
    try:
        for _ in range(count):
            connection = socket.socket()
            connection.bind(("127.0.0.1", 0))
            sockets.append(connection)
        return [connection.getsockname()[1] for connection in sockets]
    finally:
        for connection in sockets:
            connection.close()


def build_server(report, work):
    build = work / "build"
    build.mkdir()
    shutil.copyfile(ROOT / "Makefile", build / "Makefile")
    for name in ("source", "include"):
        shutil.copytree(ROOT / name, build / name, ignore=shutil.ignore_patterns(".*"))
    binary = build / "webserv"

    def compile_check():
        result = command(["make", "-j2", "SPECIAL_FLAGS=-DWEBSERV_CLIENT_TIMEOUT_SECONDS=" + str(TIMEOUT)],
                         cwd=build, timeout=90)
        (work / "build.log").write_bytes(result.stdout + result.stderr)
        require(binary.exists(), "Makefile did not create webserv")
        require(b"-std=c++98" in result.stdout, "Build did not show the required C++98 flag")
        note("After", "Real Makefile built webserv with C++98; temporary timeout=" + str(TIMEOUT) + "s")
    report.case("1. Code and build", "Compile using the real Makefile", "Successful C++98 build", compile_check)
    if not binary.exists():
        return None

    def relink_check():
        outputs = [binary, *sorted((build / "objects").glob("*.o"))]
        before = {path: path.stat().st_mtime_ns for path in outputs}
        command(["make"], cwd=build, timeout=15)
        require(all(path.stat().st_mtime_ns == stamp for path, stamp in before.items()),
                "A second make recompiled objects or relinked the executable")
        note("After", "Second make left executable and object timestamps unchanged")
    report.case("1. Code and build", "Run make a second time", "No recompilation or relinking", relink_check)
    return binary


def strip_cpp(text):
    pattern = r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    return re.sub(pattern, lambda m: re.sub(r"[^\n]", " ", m.group()), text, flags=re.S)


def function_body(text, name):
    cleaned = strip_cpp(text)
    found = re.search(re.escape(name) + r"\s*\([^;{}]*\)[^{;]*\{", cleaned)
    require(found is not None, "Cannot locate " + name + " for source review")
    depth = 1
    for index in range(found.end(), len(cleaned)):
        if cleaned[index] == "{":
            depth += 1
        elif cleaned[index] == "}":
            depth -= 1
            if depth == 0:
                return cleaned[found.end():index]
    raise AssertionError("Unmatched braces in " + name)


def code_checks(report, work, scripts):
    if not (ROOT / "source/CGI.cpp").exists():
        def missing():
            raise AssertionError("CGI is not present in this checkout; run against a checkout with CGI using --project-root")
        report.case("1. Code and build", "CGI implementation available", "Production CGI sources present", missing)
        return
    server = (ROOT / "source/Server.cpp").read_text()
    client = (ROOT / "source/ClientConnection.cpp").read_text()
    cgi = (ROOT / "source/CGI.cpp").read_text()
    component = []

    def invoke_component(mode, *arguments):
        if not component:
            from component_support import compile_component
            component.append(compile_component(ROOT, work))
        result = command([component[0], mode, scripts, *arguments], input=b"test body", timeout=10)
        actual = result.stdout.decode().strip().replace("\n", " | ")
        note("After", actual)
        return actual

    def multiplexing():
        main_loop = function_body(server, "Server::run")
        require(len(re.findall(r"\bpoll\s*\(", main_loop)) == 1, "Expected one poll call in the main loop")
        events = function_body(server, "Server::buildPollFds")
        require("POLLIN" in events and "POLLOUT" in events, "Read/write readiness not both registered")
        for name, operation in (("ClientConnection::readData", "recv"), ("ClientConnection::writeData", "send")):
            body = function_body(client, name)
            require(len(re.findall(r"\b" + operation + r"\s*\(", body)) == 1,
                    name + " does not contain exactly one socket operation")
            require("errno" not in body, name + " branches on errno")
        for name in ("CGI::provideInput", "CGI::readOutput"):
            require("errno" not in function_body(cgi, name), name + " branches on errno after pipe I/O")
        note("After", "Source checks: one main-loop poll, read/write events, one recv/send per handler, no errno in I/O handlers")
    report.case("1. Code and build", "Multiplexing and socket I/O source checks",
                "One server poll loop monitors reads and writes; I/O handlers avoid errno", multiplexing)

    for fault, result in ((5, "-1"), (6, "0")):
        def pipe_write_result(number=fault):
            actual = invoke_component("fault", str(number))
            require("error=500" in actual, "CGI input write failed/made no progress, but no server error was recorded")
        report.case("1. Code and build", "CGI input write returns " + result + " for a nonempty body",
                    "A server error is recorded; the request cannot remain stuck", pipe_write_result)

    def child_write_results():
        unchecked = list(re.finditer(r"(?m)^[ \t]*write\s*\(", strip_cpp(cgi)))
        require(not unchecked, "Unchecked CGI write() calls at source/CGI.cpp lines "
                + ", ".join(str(cgi.count("\n", 0, match.start()) + 1) for match in unchecked))
        note("After", "No standalone unchecked write calls found in CGI setup")
    report.case("1. Code and build", "CGI child error-report writes",
                "Error-report writes also check the returned byte count", child_write_results)

    def readiness():
        actual = invoke_component("poll")
        require("ungated=0" in actual, "CGI::runScript writes to the stdin pipe before poll readiness")
    report.case("1. Code and build", "CGI parent pipe readiness",
                "No parent pipe write before returning to the poll loop", readiness)
    def setup_cleanup():
        actual = invoke_component("fault", "4")
        require("error=500" in actual and "closed=1 cleared=1 orphan=0" in actual,
                "fcntl setup failure left an untracked child or uncleared descriptors/client state")
    report.case("1. Code and build", "CGI setup fails after fork",
                "500 recorded; descriptors closed; child reaped or queued for cleanup", setup_cleanup)

    from io_checks import run_io_checks
    run_io_checks(report, work, ROOT, Server, scripts)


class Server:
    def __init__(self, binary, config, work, ports, environment=None):
        self.binary, self.config, self.work, self.ports = binary, config, work, ports
        self.environment = environment
        self.process = None
        self.counter = 0

    def start(self):
        self.log = open(self.work / "server.log", "ab", buffering=0)
        self.process = subprocess.Popen([str(self.binary), str(self.config)], cwd=ROOT,
                                        stdout=self.log, stderr=self.log, start_new_session=True, env=self.environment)
        deadline = time.monotonic() + 5
        for port in self.ports:
            while True:
                require(self.process.poll() is None, "Server startup failed; see " + str(self.work / "server.log"))
                try:
                    with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                        break
                except OSError:
                    require(time.monotonic() < deadline, "Listener did not become ready: " + str(port))
                    time.sleep(0.03)

    def stop(self):
        if self.process:
            try:
                os.killpg(self.process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            self.process.wait(timeout=5)
            self.log.close()
            self.process = None

    def request(self, target="/", method="GET", data=None, status=200, body=None,
                port=None, host="alpha.test", headers=None, timeout=7, show_headers=False):
        require(self.process.poll() is None, "Webserv exited unexpectedly: " + str(self.process.returncode))
        port = port or self.ports[0]
        self.counter += 1
        stem = self.work / ("response-" + str(self.counter))
        head_file, body_file = stem.with_suffix(".head"), stem.with_suffix(".body")
        args = ["curl", "-q", "--silent", "--show-error", "--noproxy", "*", "--path-as-is", "--http1.0",
                "--max-time", str(timeout), "--resolve", host + ":" + str(port) + ":127.0.0.1",
                "--request", method, "--header", "Expect:", "--dump-header", head_file, "--output", body_file]
        for key, value in (headers or {}).items():
            args += ["--header", key + ": " + value]
        if show_headers:
            args += ["--verbose"]
        if data is not None:
            input_file = stem.with_suffix(".input")
            input_file.write_bytes(data)
            args += ["--data-binary", "@" + str(input_file)]
        run = command(args + ["http://" + host + ":" + str(port) + target], timeout=timeout + 2)
        if show_headers:
            for line in run.stderr.decode("utf8", "replace").splitlines():
                if line.startswith("> "):
                    note("Request", line[2:])
                elif line.startswith("< "):
                    note("Response", line[2:])
        head, actual = head_file.read_bytes(), body_file.read_bytes()
        line = head.split(b"\r\n", 1)[0]
        parts = line.split(b" ", 2)
        require(len(parts) == 3 and parts[0] in (b"HTTP/1.0", b"HTTP/1.1"), "Invalid status line " + repr(line))
        code = int(parts[1])
        note("After", line.decode("latin1") + "; body bytes=" + str(len(actual)))
        require(code in status if isinstance(status, tuple) else code == status,
                "Expected HTTP " + str(status) + ", received " + str(code))
        fields = {}
        for raw in head.split(b"\r\n")[1:]:
            if raw:
                key, value = raw.split(b":", 1)
                fields[key.lower()] = value.strip()
        if b"content-length" in fields:
            require(int(fields[b"content-length"]) == len(actual), "Response Content-Length differs from body size")
        if body is not None:
            require(actual == body, "Expected body " + repr(body[:100]) + ", received " + repr(actual[:100]))
        if code >= 400:
            require(actual, "HTTP error response has no visible body")
        return fields, actual


def make_fixtures(work):
    site = work / "site"
    for name in ("alpha", "beta", "routes", "listing", "closed", "files", "scripts", "alpha/errors"):
        (site / name).mkdir(parents=True, exist_ok=True)
    alpha = site / "alpha"
    (alpha / "empty.html").write_bytes(b"")
    (alpha / "hello.txt").write_bytes(b"hello from static site\n")
    (alpha / "style.css").write_text("body { color: rgb(12, 34, 56); }")
    (alpha / "script.js").write_text("window.addEventListener('load', () => { document.body.dataset.js='ok'; document.body.dataset.css=getComputedStyle(document.body).color; document.body.dataset.image=String(document.querySelector('img').naturalWidth); });")
    (alpha / "pixel.png").write_bytes(base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAusB9Y9Z7ZsAAAAASUVORK5CYII="))
    (alpha / "index.html").write_text('<!doctype html><html><head><title>Mandatory workflow</title><link rel="stylesheet" href="/style.css"><script src="/script.js" defer></script></head><body><h1>ALPHA website</h1><img src="/pixel.png" alt="pixel"><a href="/listing/">Files</a></body></html>')
    (alpha / "errors/404.html").write_bytes(b"<h1>Custom missing page</h1>\n")
    (site / "beta/index.html").write_bytes(b"BETA website\n")
    (site / "routes/home.html").write_bytes(b"Configured directory index\n")
    (site / "listing/a.txt").write_bytes(b"listing entry\n")
    shutil.copyfile(Path(__file__).resolve().parent / "probe.py", site / "scripts/probe.py")
    (site / "scripts/relative.txt").write_bytes(b"relative path works\n")
    (site / "scripts/broken.py").write_text("this is invalid Python syntax !\n")
    ports = free_ports(2)
    config = work / "mandatory.conf"
    config.write_text(f'''server {{
listen 127.0.0.1:{ports[0]};
server_name alpha.test;
client_max_body_size 2m;
error_page 404 /errors/404.html;
location / {{ root "{alpha}"; index index.html; methods GET; }}
location /route {{ root "{site / 'routes'}"; index home.html; methods GET; }}
location /listing {{ root "{site / 'listing'}"; autoindex on; methods GET; }}
location /closed {{ root "{site / 'closed'}"; autoindex off; methods GET; }}
location /files {{ root "{site / 'files'}"; upload_store "{site / 'files'}"; methods GET POST DELETE; }}
location /readonly {{ root "{site / 'files'}"; methods GET; }}
location /limited {{ root "{site / 'files'}"; upload_store "{site / 'files'}"; methods POST; client_max_body_size 4; }}
location /cgi {{ root "{site / 'scripts'}"; methods GET POST; cgi_extension .py "{sys.executable}"; }}
location /old {{ return 301 /hello.txt; }}
}}
server {{
listen 127.0.0.1:{ports[1]};
server_name beta.test;
location / {{ root "{site / 'beta'}"; index index.html; methods GET; }}
}}
server {{
listen 127.0.0.1:{ports[0]};
server_name beta.test;
location / {{ root "{site / 'beta'}"; index index.html; methods GET; }}
}}
''')
    return site, config, ports


def configuration_checks(report, server, site):
    block = "2. Configuration"
    report.case(block, "Different websites on two ports", "Port A serves ALPHA; port B serves BETA",
                lambda: (server.request(body=(site / "alpha/index.html").read_bytes()),
                         server.request(port=server.ports[1], host="beta.test", body=b"BETA website\n")))
    report.case(block, "Different hostnames on one listener", "curl --resolve selects the matching website",
                lambda: (server.request(host="alpha.test", body=(site / "alpha/index.html").read_bytes()),
                         server.request(host="beta.test", body=b"BETA website\n")))
    report.case(block, "Configured 404 error page", "HTTP 404 and the configured page body",
                lambda: server.request("/missing", status=404, body=b"<h1>Custom missing page</h1>\n"))
    report.case(block, "Default error page without an override", "HTTP 404 and a visible default error body",
                lambda: server.request("/missing", host="beta.test", status=404))
    for data, status in ((b"123", 201), (b"1234", 201), (b"12345", 413)):
        report.case(block, "POST body bytes=" + str(len(data)) + " with a 4-byte limit", "HTTP " + str(status),
                    lambda d=data, s=status: server.request("/limited/", "POST", d, status=s,
                                                         headers={"Content-Type": "text/plain"}))
    report.case(block, "Route points to another directory", "The configured route's file is returned",
                lambda: server.request("/route/home.html", body=b"Configured directory index\n"))
    report.case(block, "Directory default file", "The configured home.html is used",
                lambda: server.request("/route/", body=b"Configured directory index\n"))
    def permissions():
        target = site / "files/protected.txt"
        target.write_bytes(b"keep me")
        fields, _ = server.request("/readonly/protected.txt", "DELETE", status=405)
        require(b"GET" in fields.get(b"allow", b""), "405 response is missing the allowed method")
        require(target.exists(), "Disallowed DELETE still removed the file")
        server.request("/files/protected.txt", "DELETE", status=204, body=b"")
        require(not target.exists(), "Allowed DELETE did not remove the file")
    report.case(block, "Per-route method permissions", "DELETE rejected with 405 or allowed with 204", permissions)


def basic_checks(report, server, site):
    block = "3. Basic HTTP methods"
    report.case(block, "GET a static text file", "HTTP 200 and exact body",
                lambda: server.request("/hello.txt", body=b"hello from static site\n"))
    def raw_roundtrip():
        before = set((site / "files").iterdir())
        payload = bytes(range(256)) * 17
        server.request("/files/", "POST", payload, status=201, headers={"Content-Type": "application/octet-stream"})
        created = set((site / "files").iterdir()) - before
        require(len(created) == 1, "Upload did not create exactly one file")
        file = created.pop()
        require(file.read_bytes() == payload, "Saved upload bytes differ")
        server.request("/files/" + file.name, body=payload)
        server.request("/files/" + file.name, "DELETE", status=204, body=b"")
        require(not file.exists(), "DELETE did not remove the upload")
        server.request("/files/" + file.name, status=404)
    report.case(block, "Binary upload -> GET -> DELETE -> GET", "201 -> 200 exact bytes -> 204 -> 404", raw_roundtrip)
    def multipart():
        before = set((site / "files").iterdir())
        data = (b'--boundary\r\nContent-Disposition: form-data; name="file"; filename="a.txt"\r\n'
                b'Content-Type: text/plain\r\n\r\nfrom browser form\r\n--boundary--\r\n')
        server.request("/files/", "POST", data, status=201, headers={"Content-Type": "multipart/form-data; boundary=boundary"})
        created = set((site / "files").iterdir()) - before
        require(len(created) == 1, "Multipart upload did not create one file")
        file = created.pop()
        server.request("/files/" + file.name, body=b"from browser form")
        server.request("/files/" + file.name, "DELETE", status=204, body=b"")
    report.case(block, "Multipart form upload and download", "File uploaded and retrieved with exact bytes", multipart)
    report.case(block, "DELETE an absent file", "HTTP 404",
                lambda: server.request("/files/absent.txt", "DELETE", status=404))
    report.case(block, "DELETE a directory", "HTTP 403; directory remains intact",
                lambda: server.request("/files/", "DELETE", status=403))
    report.case(block, "Unknown HTTP method", "An error response and the server stays alive",
                lambda: (server.request("/", "UNKNOWN", status=(400, 405, 501)), server.request("/hello.txt", body=b"hello from static site\n")))


def cgi_checks(report, server):
    block = "4. CGI"
    def get():
        _, body = server.request("/cgi/probe.py")
        data = json.loads(body)
        require(data["env"]["REQUEST_METHOD"] == "GET" and data["length"] == 0, "GET method or stdin EOF was not passed correctly")
    report.case(block, "GET executes Python CGI", "HTTP 200; correct method and empty stdin", get)
    report.case(block, "POST executes Python CGI", "Exact binary stdin is returned",
                lambda: server.request("/cgi/probe.py?mode=echo", "POST", b"one\x00two\xff", body=b"one\x00two\xff",
                                       headers={"Content-Type": "application/octet-stream"}))
    def query():
        query = "mode=env&value=a%26b%3Dc&literal=%2520"
        _, body = server.request("/cgi/probe.py?" + query)
        actual = json.loads(body)["env"]["QUERY_STRING"]
        note("Expected", query)
        note("After", actual)
        require(actual == query, "CGI arguments were changed before the script received them")
    report.case(block, "Encoded CGI query arguments", "Original encoded arguments preserved", query)
    report.case(block, "CGI relative file access", "Script opens relative.txt in its own directory",
                lambda: server.request("/cgi/probe.py?mode=cwd", body=b"relative path works\n"))
    report.case(block, "Script syntax error", "Visible HTTP server error, no crash",
                lambda: server.request("/cgi/broken.py", status=(500, 502)))
    report.case(block, "Script loops indefinitely", "Visible HTTP server error within the timeout",
                lambda: server.request("/cgi/probe.py?mode=hang", status=(500, 502, 504), timeout=TIMEOUT + 4))
    report.case(block, "Static request after CGI failures", "Server remains available",
                lambda: server.request("/hello.txt", body=b"hello from static site\n"))


def browser_checks(report, server, site, work):
    block = "5. Browser and static website"
    chrome = os.environ.get("MANDATORY_BROWSER") or shutil.which("google-chrome") or shutil.which("chromium")
    if not chrome:
        path = Path("/Applications/Google Chrome.app/Contents/MacOS/Google Chrome")
        chrome = str(path) if path.exists() else None
    def dom(target, expected=None, marker=None, port=None):
        if not chrome:
            raise AssertionError("Chrome/Chromium is required for this automatic browser check; set MANDATORY_BROWSER")
        profile = work / ("chrome-" + str(time.monotonic_ns()))
        args = [chrome, "--headless=new", "--disable-gpu", "--no-first-run", "--no-default-browser-check",
                "--disable-background-networking", "--no-proxy-server", "--user-data-dir=" + str(profile),
                "--dump-dom", "--timeout=7000", "--virtual-time-budget=1500",
                "http://127.0.0.1:" + str(port or server.ports[0]) + target]
        result = command(args, timeout=15)
        html = result.stdout.decode("utf8", "replace")
        if expected:
            require(expected in html, "Browser DOM is missing " + repr(expected) + ": " + html[:250])
        if marker:
            require(marker in html, "Browser did not complete script/style/image loading: " + html[:500])
        note("After", "Chrome rendered " + target + ("; marker=" + marker if marker else ""))
        return html
    def website():
        html = dom("/", "ALPHA website", 'data-js="ok"')
        require('data-css="rgb(12, 34, 56)"' in html and 'data-image="1"' in html,
                "Browser did not load the stylesheet or image correctly")
    report.case(block, "Static HTML, CSS, JavaScript and image in Chrome", "Page renders; linked resources load and script executes", website)
    report.case(block, "Wrong URL in Chrome", "The custom missing page is rendered",
                lambda: dom("/missing-browser", "Custom missing page"))
    report.case(block, "Directory listing in Chrome", "Listing contains a.txt",
                lambda: dom("/listing/", "a.txt"))
    report.case(block, "Redirect in Chrome", "Browser follows /old to /hello.txt",
                lambda: dom("/old", "hello from static site"))
    report.case(block, "Second port in Chrome", "The second listener renders the BETA website",
                lambda: dom("/", "BETA website", port=server.ports[1]))
    report.case(block, "Inspect request and response headers", "GET request and HTTP response headers visible in the terminal",
                lambda: server.request("/hello.txt", body=b"hello from static site\n", show_headers=True))


def port_checks(report, server, work, site):
    block = "6. Port handling"
    report.case(block, "Different ports keep their own content", "Port A=ALPHA, port B=BETA",
                lambda: (server.request(body=(site / "alpha/index.html").read_bytes()),
                         server.request(port=server.ports[1], host="beta.test", body=b"BETA website\n")))
    def reject(text, name):
        config = work / (name + ".conf")
        config.write_text(text)
        process = subprocess.Popen([str(server.binary), str(config)], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            out, err = process.communicate(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            process.communicate()
            raise AssertionError("Invalid/conflicting configuration started a running server")
        require(process.returncode > 0, "Configuration was not rejected cleanly")
        note("After", (out + err).decode("utf8", "replace").strip()[-300:])
    port = free_ports(1)[0]
    report.case(block, "Duplicate listen directive within one server", "Configuration rejected",
                lambda: reject(f"server {{ listen 127.0.0.1:{port}; listen 127.0.0.1:{port}; }}", "duplicate-listen"))
    report.case(block, "Second process requests an occupied port", "Second process fails; original server still works",
                lambda: (reject(f"server {{ listen 127.0.0.1:{server.ports[0]}; }}", "occupied-port"), server.request("/hello.txt", body=b"hello from static site\n")))
    other = free_ports(1)[0]
    report.case(block, "Configuration mixes free and occupied ports", "The second process rejects the whole startup",
                lambda: reject(f"server {{ listen 127.0.0.1:{other}; listen 127.0.0.1:{server.ports[0]}; }}", "partial-startup"))


def rss_kib(pid):
    result = command(["ps", "-o", "rss=", "-p", str(pid)], timeout=3)
    return int(result.stdout.strip())


def stress_checks(report, server, work, args):
    block = "7. Siege and stress"
    siege = shutil.which("siege")
    samples = []
    stats = []
    rc = work / "siege.conf"
    rc.write_text("verbose = false\nlogging = false\nparser = false\ncookies = false\n"
                  "connection = keep-alive\nprotocol = HTTP/1.1\ntimeout = 5\nfailures = 1000\n")
    def benchmark():
        if not siege:
            raise AssertionError("Siege is required; install it before running this check")
        server.request("/empty.html", body=b"")
        original_pid = server.process.pid
        for number in range(args.stress_rounds):
            call = [siege, "-R", str(rc), "-b", "-j", "--no-parser", "-c" + str(args.clients),
                    "-t" + str(args.stress_seconds) + "S", "http://127.0.0.1:" + str(server.ports[0]) + "/empty.html"]
            process = subprocess.Popen(call, cwd=work, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                deadline = time.monotonic() + args.stress_seconds + 15
                while process.poll() is None:
                    require(time.monotonic() < deadline, "Siege did not complete within its deadline")
                    require(server.process.poll() is None, "Webserv crashed under Siege")
                    samples.append(rss_kib(server.process.pid))
                    time.sleep(0.5)
                out, err = process.communicate(timeout=3)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.communicate()
            (work / ("siege-" + str(number + 1) + ".log")).write_bytes(out + err)
            require(process.returncode == 0, "Siege failed: " + err.decode("utf8", "replace")[-1200:])
            start = out.find(b"{")
            require(start >= 0, "Siege did not print JSON statistics: " + repr(out[-500:]))
            result, _ = json.JSONDecoder().raw_decode(out[start:].decode())
            stats.append(result)
            availability = float(result["availability"])
            successes = int(result["successful_transactions"])
            failures = int(result["failed_transactions"])
            note("After", "Round {}: availability={:.2f}%; successful={}; failed={}; server PID={}".format(
                number + 1, availability, successes, failures, original_pid))
            require(availability > 99.5, "Availability must be above 99.5%")
            require(successes > 0 and successes / (successes + failures) > 0.995,
                    "Fewer than 99.5% of transactions succeeded")
            require(server.process.pid == original_pid and server.process.poll() is None, "Server restarted or exited")
            server.request("/empty.html", body=b"")
    report.case(block, "Siege benchmark on an empty page",
                "{} rounds x {}s, {} clients, >99.5% availability; same server stays responsive".format(
                    args.stress_rounds, args.stress_seconds, args.clients), benchmark)

    def memory_growth():
        if not samples or not stats:
            raise AssertionError("No completed Siege run to assess memory growth")
        warm = samples[len(samples) // 3:]
        window = min(5, len(warm))
        early = sorted(warm[:window])[window // 2]
        late = sorted(warm[-window:])[window // 2]
        growth = late - early
        note("After", "RSS samples={} min={} KiB max={} KiB; post-warmup change={} KiB".format(
            len(samples), min(samples), max(samples), growth))
        require(growth <= 16 * 1024, "RSS grew by more than 16 MiB after warmup; inspect the retained allocations")
    report.case(block, "Memory trend during repeated traffic",
                "No >16 MiB RSS rise after warmup in the sampled window", memory_growth)

    def leaks():
        tool = shutil.which("leaks")
        if not tool:
            raise AssertionError("The automatic live leak scan needs macOS leaks; see the manual Linux checklist")
        result = subprocess.run([tool, "--quiet", str(server.process.pid)], capture_output=True, timeout=25)
        text = (result.stdout + result.stderr).decode("utf8", "replace")
        (work / "leaks.log").write_text(text)
        matched = re.search(r"(\d+) leaks? for (\d+) total leaked bytes", text)
        if not matched:
            raise AssertionError("Leak inspection unavailable: " + text.strip()[-400:])
        note("After", matched.group())
        require(int(matched[1]) == 0 and int(matched[2]) == 0, "Leak detector found unreachable heap allocations")
    report.case(block, "Live heap leak scan after workload", "Zero reported leaked blocks/bytes", leaks)

    def connections():
        tool = shutil.which("lsof")
        if not tool:
            raise AssertionError("lsof is required for the automatic remaining-connection check")
        time.sleep(1)
        result = subprocess.run([tool, "-a", "-p", str(server.process.pid), "-iTCP", "-sTCP:ESTABLISHED", "-Ff"],
                                capture_output=True, timeout=5)
        require(result.returncode in (0, 1), "lsof failed: " + result.stderr.decode("utf8", "replace"))
        count = sum(line.startswith(b"f") for line in result.stdout.splitlines())
        note("After", "Remaining established server sockets: " + str(count))
        require(count == 0, "Client sockets remained established after the clients exited")
        server.request("/hello.txt", body=b"hello from static site\n")
    report.case(block, "No stuck connections after load", "Clients have disconnected; server still answers", connections)


@contextmanager
def workspace(keep):
    work = Path(tempfile.mkdtemp(prefix="webserv-mandatory-"))
    try:
        yield work
    finally:
        if keep:
            print("Artifacts retained: " + str(work), flush=True)
        else:
            shutil.rmtree(work)


def main():
    global ROOT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, help="Checkout to test (default: repository containing this suite)")
    parser.add_argument("--stress-seconds", type=int, default=10, help="Seconds per Siege round (default: 10)")
    parser.add_argument("--stress-rounds", type=int, default=3, help="Consecutive Siege rounds (default: 3)")
    parser.add_argument("--clients", type=int, default=25, help="Concurrent Siege clients (default: 25)")
    parser.add_argument("--serve-demo", action="store_true", help="Keep fixture sites available for manual browser review after tests")
    parser.add_argument("--keep-artifacts", action="store_true", help="Retain temporary configs, binaries, responses and diagnostics")
    args = parser.parse_args()
    if args.project_root:
        ROOT = args.project_root.resolve()
    if not (ROOT / "Makefile").is_file() or not (ROOT / "source").is_dir():
        parser.error("Project root must contain Makefile and source/")
    if min(args.stress_seconds, args.stress_rounds, args.clients) < 1:
        parser.error("Stress parameters must be positive")
    report = Report()
    original = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT)
    with workspace(args.keep_artifacts) as work:
        print("Mandatory workflow; temporary files: " + str(work), flush=True)
        binary = build_server(report, work)
        if not binary:
            return report.summary()
        site, config, ports = make_fixtures(work)
        code_checks(report, work, site / "scripts")
        server = Server(binary, config, work, ports)
        try:
            server.start()
            configuration_checks(report, server, site)
            basic_checks(report, server, site)
            cgi_checks(report, server)
            browser_checks(report, server, site, work)
            port_checks(report, server, work, site)
            stress_checks(report, server, work, args)
            status = report.summary()
            if args.serve_demo:
                print("\nBrowser demo: http://127.0.0.1:" + str(ports[0]) + "/")
                print("Second site: http://127.0.0.1:" + str(ports[1]) + "/")
                print("Config: " + str(config) + "\nCtrl-C stops the demo.", flush=True)
                try:
                    while True:
                        time.sleep(1)
                except KeyboardInterrupt:
                    pass
        finally:
            server.stop()
            require(subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT) == original, "Git HEAD changed")
    return status


if __name__ == "__main__":
    sys.exit(main())
