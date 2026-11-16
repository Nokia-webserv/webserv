#!/usr/bin/env python3
"""Raw uploads through the actual parser, router, filesystem and send buffer."""

from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import hashlib
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "get_integration"))
from get_test_support import (  # noqa: E402
    CurlClient, Reporter, exact_body, free_port, require, running_server,
)


class PostClient(CurlClient):
    def uploads(self, requests):
        command = ["curl", "-q", "--fail-early"]
        outputs = []
        for request in requests:
            if outputs:
                command.append("--next")
            self.counter += 1
            prefix = self.directory / ("post-" + str(self.counter))
            payload = prefix.with_suffix(".input")
            header = prefix.with_suffix(".headers")
            body = prefix.with_suffix(".body")
            payload.write_bytes(request["data"])
            outputs.append((header, body))
            version = request.get("version", "HTTP/1.1")
            self.note("Before: POST " + request["url"] + " " + version
                      + "; upload=" + str(len(request["data"])) + " bytes; type="
                      + repr(request.get("type")))
            command.extend([
                "--silent", "--show-error", "--noproxy", "*", "--path-as-is",
                "--connect-timeout", "2", "--max-time", "8",
                "--http1.0" if version == "HTTP/1.0" else "--http1.1",
                "--header", "Host: localhost", "--header", "Expect:",
                "--header", "Content-Type: " + request["type"]
                if request.get("type") is not None else "Content-Type:",
                "--data-binary", "@" + str(payload),
                "--dump-header", str(header), "--output", str(body),
                "--write-out", "%{num_connects}\n",
            ])
            if request.get("chunked"):
                command.extend(["--header", "Transfer-Encoding: chunked"])
            if request.get("rate"):
                command.extend(["--limit-rate", request["rate"]])
            if request.get("disposition"):
                command.extend(["--header", request["disposition"]])
            command.extend(["--url", self.base_url + request["url"]])
        completed = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, timeout=9 * len(requests))
        require(completed.returncode == 0,
                "curl exit " + str(completed.returncode) + ": " + completed.stderr.strip())
        responses = [self.read_response(header, body) for header, body in outputs]
        connections = [int(line) for line in completed.stdout.splitlines()]
        require(len(connections) == len(requests), "Missing curl connection metrics")
        for request, response in zip(requests, responses):
            expected = request.get("status", 201)
            require(response.status == expected,
                    "Expected HTTP " + str(expected) + ", received " + str(response.status))
            require(response.version == request.get("version", "HTTP/1.1"),
                    "Wrong HTTP response version")
            if expected >= 400:
                require(response.body, "Empty error response")
        return responses, connections

    def upload(self, url, data, content_type=None, **options):
        request = dict(url=url, data=data, type=content_type, **options)
        return self.uploads([request])[0][0]


class Fixtures:
    def __init__(self, root):
        self.root = root
        self.site = root / "site"
        self.site.mkdir()
        (self.site / "index.html").write_bytes(b"Raw POST integration readiness\n")
        self.stores = root / "uploads"
        self.stores.mkdir()
        self.permissions = []
        self.locations = [
            'location / { root "' + str(self.site) + '"; index index.html; methods GET; }',
            'location /stored/ { root "' + str(self.stores) + '"; methods GET; }',
        ]
        self.count = 0

    def location(self, mode="normal", methods="POST", relative=False, trailing=False, limit=None):
        self.count += 1
        key = "case-" + str(self.count)
        store = self.stores / (key + (" with spaces" if mode == "spaces" else ""))
        if mode == "file":
            store.write_bytes(b"Do not overwrite this file")
        elif mode == "loop":
            store.symlink_to(store.name)
        elif mode != "missing":
            store.mkdir()
        if mode == "denied":
            store.chmod(0o500)
            self.permissions.append(store)
        url = "/upload/" + key + "/"
        value = os.path.relpath(store, Path.cwd()) if relative else str(store)
        if trailing:
            value += "/"
        directive = '' if mode == "none" else ' upload_store "' + value + '";'
        if limit is not None:
            directive += ' client_max_body_size ' + str(limit) + ';'
        self.locations.append('location ' + url + ' { root "' + str(self.site)
                              + '"; methods ' + methods + ';' + directive + ' }')
        return SimpleNamespace(url=url, store=store)

    def config(self, port):
        config = self.root / "post.conf"
        config.write_text("server {\nlisten 127.0.0.1:" + str(port) + ";\n"
                          + "client_max_body_size 2m;\n"
                          + "\n".join(self.locations) + "\n}\n")
        return config

    def restore(self):
        for path in self.permissions:
            path.chmod(0o700)


def saved_files(store, before, payloads, suffixes, report):
    after = set(store.iterdir())
    require(before <= after, "An existing upload directory entry disappeared")
    new = after - before
    require(len(new) == len(payloads),
            "Expected " + str(len(payloads)) + " new file(s), found " + str(len(new)))
    contents = []
    for path in sorted(new):
        require(path.is_file() and not path.is_symlink(), "Upload is not a regular file")
        require(any(re.fullmatch(r"upload_[0-9]+" + re.escape(suffix), path.name)
                    for suffix in suffixes), "Unexpected upload filename: " + path.name)
        data = path.read_bytes()
        contents.append(data)
        report.note("Saved: " + path.name + "; bytes=" + str(len(data))
                    + "; SHA256=" + hashlib.sha256(data).hexdigest())
    if len(payloads) == 1:
        exact_body(contents[0], payloads[0])
    else:
        require(Counter(contents) == Counter(payloads), "Saved uploads lost, mixed or duplicated bytes")
    report.note("Saved bytes match the submitted payload(s) exactly")
    return sorted(new)


def main():
    if len(sys.argv) != 2:
        print("Usage: post_raw_integration_test.py SERVER_EXECUTABLE")
        return 2
    report = Reporter()
    tests = []
    print("RAW POST INTEGRATION — real server, curl, exact saved-file checks", flush=True)
    with tempfile.TemporaryDirectory(prefix="webserv-post-http-") as temp:
        root = Path(temp)
        fixtures = Fixtures(root)
        port = free_port()
        client = PostClient(port, root, report.note)

        def register(group, name, test, expected="HTTP/1.1 201 Created; exact saved bytes", skip=None):
            tests.append((group, name, expected, test, skip))

        def upload_case(group, name, data, media=None, suffixes=("",), **options):
            location = fixtures.location(**options.pop("location", {}))
            target = options.pop("target", "")

            def check():
                before = set(location.store.iterdir())
                client.upload(location.url + target, data, media, **options)
                saved_files(location.store, before, [data], suffixes, report)
            register(group, name, check)
            return location

        pattern = bytes(range(256))
        group = "B RAW BODY DELIVERY"
        for size in (0, 1, 999, 1000, 1001, 1999, 2000, 2001, 1048576):
            upload_case(group, str(size) + " bytes", (pattern * (size // 256 + 1))[:size])
        upload_case(group, "UTF-8 and NUL bytes", "Привет 😊".encode("utf-8") + b"\x00\xff")

        group = "M MIME AND HEADERS"
        for name, media, suffixes in (
            ("Text", "text/plain", (".txt",)),
            ("JSON", "application/json", (".json",)),
            ("Binary PNG", "image/png", (".png",)),
            ("JPEG alias", "image/jpeg", (".jpg", ".jpeg")),
            ("Mixed case and parameters", "Text/Plain; charset=utf-8", (".txt",)),
            ("Whitespace before parameters", "text/plain  ; charset=utf-8", (".txt",)),
            ("Unknown type", "application/x-custom", ("",)),
            ("Form URL-encoded stays raw", "application/x-www-form-urlencoded", ("",)),
        ):
            upload_case(group, name, pattern + b"a=1&b=2", media, suffixes)

        group = "P CONFIGURATION AND DESTINATION"
        upload_case(group, "Spaces in upload_store", b"spaced store", location={"mode": "spaces"})
        upload_case(group, "Trailing slash in upload_store", b"trailing slash", location={"trailing": True})
        upload_case(group, "Relative upload_store", b"relative store", location={"relative": True})
        upload_case(group, "Request path and query do not choose filename", b"safe destination",
                    "text/plain", (".txt",), target="nested/untrusted.exe?name=evil.php")
        upload_case(group, "Content-Disposition cannot choose filename", b"safe generated name",
                    "text/plain", (".txt",),
                    disposition='Content-Disposition: attachment; filename="../../escape.php"')

        group = "N COLLISIONS AND NO OVERWRITES"
        for kind in ("file", "directory", "symlink"):
            location = fixtures.location()
            original = b"Existing bytes must stay unchanged"
            collision = location.store / "upload_0.txt"
            target = root / ("symlink-target-" + str(fixtures.count))
            if kind == "file":
                collision.write_bytes(original)
            elif kind == "directory":
                collision.mkdir()
            else:
                target.write_bytes(original)
                collision.symlink_to(target)

            def collision_check(location=location, kind=kind, collision=collision, target=target):
                before = set(location.store.iterdir())
                payload = b"New upload after collision"
                client.upload(location.url, payload, "text/plain")
                saved_files(location.store, before, [payload], (".txt",), report)
                require((location.store / "upload_1.txt").is_file(), "Did not retry next filename")
                if kind == "file":
                    exact_body(collision.read_bytes(), original)
                elif kind == "directory":
                    require(collision.is_dir(), "Existing directory was replaced")
                else:
                    require(collision.is_symlink(), "Existing symlink was replaced")
                    exact_body(target.read_bytes(), original)
            register(group, "Existing " + kind + " is preserved", collision_check)

        group = "E ERROR RESPONSES"
        for mode, status, name in (
            ("none", 403, "No upload_store"),
            ("missing", 404, "Missing upload directory"),
            ("file", 404, "upload_store is a regular file"),
            ("denied", 403, "Unwritable upload directory"),
            ("loop", 500, "Filesystem error from symlink loop"),
        ):
            location = fixtures.location(mode=mode)

            def error_check(location=location, status=status, mode=mode):
                client.upload(location.url, b"Must not be saved", status=status)
                if mode in ("none", "denied"):
                    require(not list(location.store.iterdir()), "Failed request created an upload")
                elif mode == "missing":
                    require(not location.store.exists(), "Handler created missing upload_store")
                elif mode == "file":
                    exact_body(location.store.read_bytes(), b"Do not overwrite this file")
            register(group, name, error_check, "HTTP/1.1 " + str(status) + "; error response",
                     "root bypasses directory permissions" if mode == "denied" and os.geteuid() == 0 else None)

        forbidden = fixtures.location(methods="GET")

        def forbidden_check():
            response = client.upload(forbidden.url, b"Forbidden", status=405)
            require(response.headers.get("allow") == "GET", "405 must advertise Allow: GET")
            require(not list(forbidden.store.iterdir()), "Forbidden POST created a file")
        register(group, "POST forbidden by configured methods", forbidden_check,
                 "HTTP/1.1 405 Method Not Allowed; Allow: GET; no upload")

        group = "S CONFIGURED BODY LIMITS"
        upload_case(group, "Exactly the location limit is accepted", b"x" * 64,
                    location={"limit": 64})
        limited = fixtures.location(limit=64)

        def limit_check():
            client.upload(limited.url, b"x" * 65, status=413)
            require(not list(limited.store.iterdir()), "Oversized request created an upload")
        register(group, "One byte over the location limit is rejected", limit_check,
                 "HTTP/1.1 413 Payload Too Large; no uploaded file")

        group = "C FULL SERVER DELIVERY"
        sequential = fixtures.location()

        def sequential_check():
            before = set(sequential.store.iterdir())
            bodies = [b"first", b"\x00second\xff", b""]
            _, connections = client.uploads([
                dict(url=sequential.url, data=data, type="text/plain") for data in bodies])
            report.note("New connections per POST: " + repr(connections))
            require(connections == [1, 0, 0], "Sequential POSTs did not reuse one connection")
            saved_files(sequential.store, before, bodies, (".txt",), report)
        register(group, "Three POSTs on one HTTP/1.1 connection", sequential_check)

        recovery = fixtures.location()
        failed = fixtures.location(mode="none")

        def recovery_check():
            before = set(recovery.store.iterdir())
            bodies = [b"before error", b"after error"]
            _, connections = client.uploads([
                dict(url=recovery.url, data=bodies[0], type=None),
                dict(url=failed.url, data=b"not saved", type=None, status=403),
                dict(url=recovery.url, data=bodies[1], type=None),
            ])
            report.note("New connections per POST: " + repr(connections))
            require(connections == [1, 0, 0], "POST error prevented reuse of the connection")
            saved_files(recovery.store, before, bodies, ("",), report)
            require(not list(failed.store.iterdir()), "Failed POST created a file")
        register(group, "Successful POST after 403 on the same connection", recovery_check,
                 "201, 403, 201 in order; one TCP connection; two correct files")

        parallel = fixtures.location()
        worker_dirs = [root / ("curl-worker-" + str(i)) for i in range(4)]
        for directory in worker_dirs:
            directory.mkdir()

        def parallel_check():
            before = set(parallel.store.iterdir())
            bodies = [bytes([i]) * (1024 + i) for i in range(4)]

            def worker(i):
                PostClient(port, worker_dirs[i]).upload(parallel.url, bodies[i], "text/plain")
            with ThreadPoolExecutor(max_workers=4) as pool:
                list(pool.map(worker, range(4)))
            saved_files(parallel.store, before, bodies, (".txt",), report)
        register(group, "Four concurrent clients get distinct saved files", parallel_check)

        upload_case(group, "Chunked raw body decoded before saving", pattern * 7,
                    "application/octet-stream", chunked=True)
        upload_case(group, "Slow upload spans multiple receive events", pattern * 6,
                    "application/octet-stream", rate="512")
        roundtrip = fixtures.location()

        def roundtrip_check():
            before = set(roundtrip.store.iterdir())
            data = pattern * 4 + b"\x00round trip"
            client.upload(roundtrip.url, data, "image/png")
            paths = saved_files(roundtrip.store, before, [data], (".png",), report)
            client.get("/stored/" + roundtrip.store.name + "/" + paths[0].name,
                       body=data, mime="image/png")
        register(group, "POST then GET the saved binary file", roundtrip_check,
                 "POST 201, then GET 200 with exactly the uploaded bytes")

        version = fixtures.location()

        def version_check():
            before = set(version.store.iterdir())
            data = b"HTTP/1.0 upload"
            client.upload(version.url, data, "text/plain", version="HTTP/1.0")
            saved_files(version.store, before, [data], (".txt",), report)
        register("V HTTP VERSION", "HTTP/1.0 POST response framing", version_check,
                 "HTTP/1.0 201 Created; exact saved bytes (TCP closure is not checked)")

        try:
            with running_server(Path(sys.argv[1]).resolve(), fixtures.config(port), root, port) as server:
                for group, name, expected, test, skip in tests:
                    def checked(test=test):
                        test()
                        require(server.poll() is None, "Server exited during the test")
                        require(list(fixtures.site.iterdir()) == [fixtures.site / "index.html"],
                                "Upload wrote into document root instead of upload_store")
                        exact_body((fixtures.site / "index.html").read_bytes(),
                                   b"Raw POST integration readiness\n")
                    report.case(group, name, expected, checked, skip)
        finally:
            fixtures.restore()
        return report.summary()


if __name__ == "__main__":
    sys.exit(main())
