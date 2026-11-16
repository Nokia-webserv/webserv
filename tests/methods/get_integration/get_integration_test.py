#!/usr/bin/env python3
"""Grouped GET acceptance checks. All HTTP requests in this suite use curl."""

from collections import Counter
from concurrent.futures import ThreadPoolExecutor
from html.parser import HTMLParser
import os
from pathlib import Path
import sys
import tempfile
import time
from urllib.parse import urljoin

from get_test_support import (
    CurlClient, Reporter, check_response, free_port, require, running_server,
)


class ListingLinks(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.links = []
        self.href = None
        self.text = []

    def handle_starttag(self, tag, attrs):
        if tag == "a":
            self.href = dict(attrs).get("href")
            self.text = []

    def handle_data(self, data):
        if self.href is not None:
            self.text.append(data)

    def handle_endtag(self, tag):
        if tag == "a" and self.href is not None:
            self.links.append((self.href, "".join(self.text)))
            self.href = None


def links_in(body):
    parser = ListingLinks()
    parser.feed(body.decode("utf-8"))
    parser.close()
    return parser.links


def make_fixtures(root, port):
    for directory in (
        "files", "files/nested", "files/override", "files/my dir", "override",
        "root with spaces", "listing", "listing/sub & dir", "indexed",
        "indexed/nested", "indexed/index-dir", "empty", "denied-dir",
    ):
        (root / directory).mkdir()
    data = {
        "files/plain.txt": b"Hello from GET!\n",
        "files/nested/child.txt": b"Nested file",
        "files/override/chosen.txt": b"WRONG: less specific location",
        "override/chosen.txt": b"Correct: longest location",
        "root with spaces/plain.txt": b"Root contains spaces",
        "files/my dir/child.txt": b"Spaced directory",
        "indexed/index.html": "<p>Index: привет</p>\n".encode("utf-8"),
        "indexed/empty.html": b"",
        "indexed/nested/home.html": b"Nested configured index",
        "indexed/denied.html": b"Unreadable index",
        "files/denied.txt": b"Unreadable file",
        "denied-dir/entry.txt": b"Not readable through this directory",
        "listing/sub & dir/child.txt": b"Nested listing file",
    }
    sizes = (0, 1, 999, 1000, 1001, 1999, 2000, 2001, 1048576)
    pattern = bytes(range(256))
    for size in sizes:
        data["files/block-" + str(size) + ".txt"] = (pattern * (size // 256 + 1))[:size]
    mime_cases = (
        ("page.html", "text/html", b"<h1>HTML</h1>"),
        ("style.css", "text/css", b"body {color: red;}"),
        ("app.js", "text/javascript", b"console.log('GET');"),
        ("data.json", "application/json", b'{"ok":true}'),
        ("image.PNG", "image/png", pattern * 9),
        ("drawing.svg", "image/svg+xml", b"<svg></svg>"),
        ("font.woff2", "font/woff2", b"\x00font\xff"),
        ("video.mp4", "video/mp4", b"\x00video\xff"),
        ("unknown.custom", "application/octet-stream", pattern),
        ("noextension", "application/octet-stream", b"No extension"),
    )
    for name, _, body in mime_cases:
        data["files/" + name] = body
    names = (
        "my photo.txt", '<img src=x onerror=alert(1)>&"\'.txt',
        "literal%20.txt", "фото.txt", "😊.txt", "plus+query?#.txt", ".hidden",
    )
    for name in names:
        data["listing/" + name] = ("File: " + name).encode("utf-8")
    for i in range(6):
        data["files/parallel-" + str(i) + ".bin"] = bytes([i + 1]) * (65536 + i)
    for relative, body in data.items():
        (root / relative).write_bytes(body)
    os.mkfifo(root / "files/special.fifo", 0o600)
    os.mkfifo(root / "indexed/index-fifo", 0o600)
    # A deterministic stat() ELOOP exercises the generic 500 branch without mocks.
    (root / "files/loop").symlink_to("loop")
    locations = [
        ("/", "indexed", "index index.html; methods GET;"),
        ("/files", "files", "methods GET;"),
        ("/files/override", "override", "methods GET;"),
        ("/trailing/", "files/", "methods GET;"),
        ("/spaced", "root with spaces", "methods GET;"),
        ("/listing", "listing", "autoindex on; methods GET;"),
        ("/fallback", "listing", "index missing.html; autoindex on; methods GET;"),
        ("/indexed", "indexed", "index index.html; autoindex on; methods GET;"),
        ("/zeroindex", "indexed", "index empty.html; methods GET;"),
        ("/nestedindex", "indexed", "index nested/home.html; methods GET;"),
        ("/empty", "empty", "autoindex on; methods GET;"),
        ("/closed", "empty", "autoindex off; methods GET;"),
        ("/missing-index-off", "empty", "index missing.html; autoindex off; methods GET;"),
        ("/index-dir", "indexed", "index index-dir; autoindex on; methods GET;"),
        ("/index-fifo", "indexed", "index index-fifo; autoindex on; methods GET;"),
        ("/denied-index", "indexed", "index denied.html; autoindex on; methods GET;"),
        ("/denied-dir", "denied-dir", "autoindex on; methods GET;"),
        ("/restricted", "files", "methods DELETE;"),
    ]
    config = root / "get.conf"
    config.write_text("server {\nlisten 127.0.0.1:" + str(port) + ";\n" + "".join(
        'location ' + url + ' {\nroot "' + str(root / folder)
        + ("/" if folder.endswith("/") else "") + '";\n' + directives + '\n}\n'
        for url, folder, directives in locations) + "}\n")
    return config, data, sizes, mime_cases, names


def main():
    if len(sys.argv) != 2:
        print("Usage: get_integration_test.py SERVER_EXECUTABLE")
        return 1
    executable = Path(sys.argv[1]).resolve()
    report = Reporter()
    print("GET ACCEPTANCE TESTS — real server, real curl, exact response checks")
    print("No helper functions are called directly. IDs identify failing scenarios.")
    with tempfile.TemporaryDirectory(prefix="webserv-get-http-") as temp:
        root = Path(temp)
        port = free_port()
        config, data, sizes, mime_cases, names = make_fixtures(root, port)
        client = CurlClient(port, root, report.note)
        permissions = (root / "files/denied.txt", root / "indexed/denied.html", root / "denied-dir")
        try:
            if os.geteuid() != 0:
                for path in permissions:
                    path.chmod(0)
            with running_server(executable, config, root, port) as server:
                def case(group, name, expected, test, skip=None):
                    def checked():
                        test()
                        require(server.poll() is None, "Server exited after request")
                    report.case(group, name, expected, checked, skip)

                group = "F FILE BYTES AND READ BOUNDARIES"
                for size in sizes:
                    relative = "files/block-" + str(size) + ".txt"
                    case(group, str(size) + "-byte file",
                         "HTTP/1.1 200; Content-Length=" + str(size) + "; exact binary bytes",
                         lambda relative=relative: client.get(
                             "/" + relative, body=data[relative], mime="text/plain"))

                group = "M MIME TYPES"
                for name, mime, body in mime_cases:
                    case(group, name, "HTTP/1.1 200; Content-Type=" + mime + "; exact file bytes",
                         lambda name=name, mime=mime, body=body:
                             client.get("/files/" + name, body=body, mime=mime))

                group = "P PATH AND LOCATION MAPPING"
                for name, url, relative in (
                    ("Nested request path", "/files/nested/child.txt", "files/nested/child.txt"),
                    ("Longest location wins", "/files/override/chosen.txt", "override/chosen.txt"),
                    ("Root and location with trailing slash", "/trailing/plain.txt", "files/plain.txt"),
                    ("Configured root contains spaces", "/spaced/plain.txt", "root with spaces/plain.txt"),
                    ("Query is excluded from disk path",
                     "/files/plain.txt?filename=%2Fother%2Ffile&x=%2520", "files/plain.txt"),
                ):
                    case(group, name, "HTTP/1.1 200; exact bytes from " + relative,
                         lambda url=url, relative=relative: client.get(url, body=data[relative]))

                group = "D DIRECTORY INDEX AND AUTOINDEX"
                for name, url, relative in (
                    ("Root configured index", "/", "indexed/index.html"),
                    ("Index has priority over listing", "/indexed/", "indexed/index.html"),
                    ("Empty index is a valid file", "/zeroindex/", "indexed/empty.html"),
                    ("Nested configured index", "/nestedindex/", "indexed/nested/home.html"),
                ):
                    case(group, name, "HTTP/1.1 200; configured index bytes",
                         lambda url=url, relative=relative:
                             client.get(url, body=data[relative], mime="text/html"))

                def check_listing(url):
                    links = links_in(client.get(url, mime="text/html").body)
                    expected_labels = ["../", "sub & dir"] + list(names)
                    require(Counter(label for _, label in links) == Counter(expected_labels),
                            "Missing, extra, duplicate or incorrectly escaped listing labels: " + repr(links))
                    require(("../", "../") in links, "Missing parent link")
                    require(all(label not in (".", "..") for _, label in links), "Dot entry leaked")
                    report.note("Listing: every expected entry appears once; labels are readable HTML text")
                    return links

                case(group, "Missing index falls back to listing",
                     "HTTP/1.1 200; expected files, subdirectory and parent link",
                     lambda: check_listing("/fallback/"))

                def empty_listing():
                    links = links_in(client.get("/empty/", mime="text/html").body)
                    require(links == [("../", "../")], "Unexpected empty-directory entries: " + repr(links))
                    report.note("Listing: only the parent link")

                case(group, "Empty directory", "HTTP/1.1 200; only ../ in listing", empty_listing)
                case(group, "Autoindex off", "HTTP/1.1 403; nonempty error body",
                     lambda: client.get("/closed/", status=403))
                case(group, "Missing index and autoindex off", "HTTP/1.1 403",
                     lambda: client.get("/missing-index-off/", status=403))

                group = "L LISTING LINKS AS A CLIENT"
                case(group, "Listing contains precisely the expected entries",
                     "HTTP/1.1 200; one link per entry; no interpreted HTML filename",
                     lambda: check_listing("/listing/"))

                def follow_file(name):
                    links = links_in(client.get("/listing/", mime="text/html").body)
                    matches = [href for href, label in links if label == name]
                    require(len(matches) == 1, "Expected exactly one visible filename: " + name)
                    target = urljoin("/listing/", matches[0])
                    report.note("Follow listing link: " + repr(name) + " -> " + target)
                    client.get(target, body=data["listing/" + name])

                for name in names:
                    case(group, "Follow " + repr(name),
                         "HTML label matches filename; linked GET returns exactly that file",
                         lambda name=name: follow_file(name))

                group = "E ERRORS AND UNSUPPORTED FILES"
                for name, url, status in (
                    ("Missing file", "/files/missing.txt", 404),
                    ("Regular file used as directory", "/files/plain.txt/child", 404),
                    ("FIFO must not be opened/read", "/files/special.fifo", 403),
                    ("Configured index is a directory", "/index-dir/", 403),
                    ("Configured index is a FIFO", "/index-fifo/", 403),
                    ("Filesystem stat failure: symlink loop", "/files/loop", 500),
                ):
                    case(group, name, "HTTP/1.1 " + str(status) + "; framed error body; server stays running",
                         lambda url=url, status=status: client.get(url, status=status))
                permission_skip = "root bypasses mode-000 permission restrictions" if os.geteuid() == 0 else None
                for name, url in (
                    ("Unreadable file", "/files/denied.txt"),
                    ("Unreadable directory", "/denied-dir/"),
                    ("Unreadable index must not fall back to listing", "/denied-index/"),
                ):
                    case(group, name, "HTTP/1.1 403; no content disclosure",
                         lambda url=url: client.get(url, status=403), skip=permission_skip)

                def forbidden_method():
                    response = client.get("/restricted/plain.txt", status=405)
                    require(response.headers.get("allow") == "DELETE",
                            "Expected Allow: DELETE, received " + repr(response.headers.get("allow")))
                    report.note("Allow: DELETE")
                case(group, "GET forbidden by location methods", "HTTP/1.1 405; Allow: DELETE",
                     forbidden_method)

                group = "C CONNECTION REUSE AND DELIVERY"
                def sequence(with_errors):
                    targets = [("/files/plain.txt", 200, data["files/plain.txt"]),
                               ("/files/image.PNG", 200, data["files/image.PNG"]),
                               ("/files/block-0.txt", 200, b"")]
                    if with_errors:
                        targets[1:1] = [("/files/missing.txt", 404, None), ("/files/loop", 500, None)]
                    responses, connects = client.batch([url for url, _, _ in targets])
                    for response, (_, status, body) in zip(responses, targets):
                        check_response(response, status=status, body=body)
                    require(connects == [1] + [0] * (len(targets) - 1),
                            "Connection was not reused: " + repr(connects))
                    report.note("New TCP connections per request: " + repr(connects) + "; all bodies matched")

                case(group, "Three different files on one connection",
                     "Three correct responses; curl new connections [1, 0, 0]", lambda: sequence(False))
                case(group, "Recovery after 404 and 500 on the same connection",
                     "200 -> 404 -> 500 -> 200 -> 200; no stale response data or reconnect",
                     lambda: sequence(True))

                def concurrent_downloads():
                    def download(i):
                        directory = root / ("worker-" + str(i))
                        directory.mkdir()
                        worker = CurlClient(port, directory)
                        relative = "files/parallel-" + str(i) + ".bin"
                        worker.get("/" + relative, body=data[relative], mime="application/octet-stream")
                        return str(i) + ": " + str(len(data[relative])) + " correct bytes"
                    with ThreadPoolExecutor(max_workers=6) as workers:
                        results = list(workers.map(download, range(6)))
                    for result in results:
                        report.note("Client " + result)
                case(group, "Six simultaneous clients receive distinct files",
                     "All six clients receive their own exact bytes without mixing responses",
                     concurrent_downloads)

                def slow_download():
                    started = time.monotonic()
                    client.get("/files/block-1048576.txt", body=data["files/block-1048576.txt"],
                               rate="256k", timeout=12)
                    report.note("Rate-limited curl (256 KiB/s); elapsed {:.2f}s".format(
                        time.monotonic() - started))
                case(group, "One MiB download to a slow reader",
                     "Complete, uncorrupted body with curl limited to 256 KiB/s", slow_download)

                group = "V HTTP VERSION"
                case(group, "HTTP/1.0 response version and body",
                     "HTTP/1.0 200; exact file bytes (server-side EOF is a separate diagnostic)",
                     lambda: client.get("/files/plain.txt", body=data["files/plain.txt"], version="HTTP/1.0"))

                group = "R DIRECTORY REDIRECTS"
                redirect_skip = ("handleRedirect is not integrated; SKIP_GET_REDIRECT_TESTS=1"
                                 if os.environ.get("SKIP_GET_REDIRECT_TESTS") == "1" else None)
                for name, url, location in (
                    ("Append trailing slash", "/indexed", "/indexed/"),
                    ("Preserve query", "/indexed?sort=name", "/indexed/?sort=name"),
                    ("Preserve path encoding", "/files/my%20dir", "/files/my%20dir/"),
                ):
                    def redirect(url=url, location=location):
                        client.get(url, status=301, location=location)
                        client.get(location)
                    case(group, name, "HTTP/1.1 301; Location: " + location + "; target GET succeeds",
                         redirect, skip=redirect_skip)
        except Exception as error:
            report.case("S SETUP", "Fixture/server setup", "Runnable server and fixtures",
                        lambda error=error: (_ for _ in ()).throw(error))
        finally:
            for path in permissions:
                path.chmod(0o700 if path.is_dir() else 0o600)
    print("\nScope: GET via curl. Connection-close rules and pipelining have a separate diagnostic.")
    print("Known exclusions: configured redirects, CGI, root containment (#107), injected read failures.")
    return report.summary()


if __name__ == "__main__":
    sys.exit(main())
