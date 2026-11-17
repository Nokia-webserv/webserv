#!/usr/bin/env python3
"""Separate TCP diagnostics: EOF and pipelined response order need raw sockets."""

from pathlib import Path
import socket
import sys
import tempfile

from get_test_support import (
    Reporter, Response, check_response, free_port, require, running_server,
)


class Reader:
    def __init__(self, connection):
        self.connection = connection
        self.pending = b""

    def read(self):
        data = self.connection.recv(65536)
        require(data, "Server closed before the complete response arrived")
        self.pending += data

    def response(self):
        while b"\r\n\r\n" not in self.pending:
            self.read()
        head, self.pending = self.pending.split(b"\r\n\r\n", 1)
        lines = head.split(b"\r\n")
        version, status, reason = lines[0].decode("ascii").split(" ", 2)
        require(reason, "Empty reason phrase")
        headers = {}
        for line in lines[1:]:
            name, value = line.split(b":", 1)
            name = name.decode("ascii").lower()
            require(name not in headers, "Duplicate header")
            headers[name] = value.strip().decode("ascii")
        length = int(headers["content-length"])
        require(length >= 0, "Negative Content-Length")
        while len(self.pending) < length:
            self.read()
        body, self.pending = self.pending[:length], self.pending[length:]
        return Response(version, int(status), headers, body)

    def expect_closed(self):
        require(not self.pending, "Server sent unexpected bytes after the final response")
        self.connection.settimeout(2)
        try:
            require(self.connection.recv(1) == b"", "Unexpected data after final response")
        except socket.timeout:
            raise AssertionError("No server TCP EOF within 2 seconds after complete response")


def main():
    if len(sys.argv) != 2:
        print("Usage: get_connection_test.py SERVER_EXECUTABLE")
        return 1
    report = Reporter()
    print("CONNECTION DIAGNOSTICS — raw TCP, separate from the curl GET suite")
    print("Checks HTTP/1.0 closure; HTTP/1.1 closure is outside this test scope.")
    with tempfile.TemporaryDirectory(prefix="webserv-get-connections-") as temp:
        root = Path(temp)
        bodies = {"a.txt": b"A" * 65536, "b.txt": b"SECOND", "empty.txt": b""}
        for name, body in bodies.items():
            (root / name).write_bytes(body)
        port = free_port()
        config = root / "get.conf"
        config.write_text('server {\nlisten 127.0.0.1:' + str(port)
                          + ';\nlocation / {\nroot "' + str(root)
                          + '";\nindex b.txt;\nmethods GET;\n}\n}\n')
        with running_server(Path(sys.argv[1]).resolve(), config, root, port):
            def request(name, version="HTTP/1.1"):
                return ("GET /" + name + " " + version
                        + "\r\nHost: localhost\r\n\r\n").encode("ascii")

            def pipeline(count):
                names = list(bodies)
                targets = [names[i % len(names)] for i in range(count)]
                with socket.create_connection(("127.0.0.1", port), timeout=4) as connection:
                    connection.settimeout(4)
                    connection.sendall(b"".join(request(name) for name in targets))
                    reader = Reader(connection)
                    for i, name in enumerate(targets, 1):
                        response = reader.response()
                        check_response(response, body=bodies[name])
                        report.note("Response " + str(i) + ": " + name + "; "
                                    + str(len(response.body)) + " correct bytes")
                    require(not reader.pending, "Extra bytes after final response")

            report.case("P PIPELINING", "Three requests without waiting for responses",
                        "Three complete responses in request order", lambda: pipeline(3))
            report.case("P PIPELINING", "Forty requests cross the 16-request event-loop batch",
                        "All 40 response bodies arrive once and in request order",
                        lambda: pipeline(40))

            def closing_http10():
                report.note("Request: GET /b.txt HTTP/1.0 (without keep-alive)")
                with socket.create_connection(("127.0.0.1", port), timeout=4) as connection:
                    connection.settimeout(4)
                    connection.sendall(request("b.txt", "HTTP/1.0"))
                    reader = Reader(connection)
                    response = reader.response()
                    check_response(response, body=bodies["b.txt"], version="HTTP/1.0")
                    report.note("Complete HTTP/1.0 response received; waiting for server EOF")
                    reader.expect_closed()

            report.case("C HTTP/1.0 CONNECTION CLOSE", "HTTP/1.0 closes by default",
                        "Full HTTP/1.0 response, then server EOF", closing_http10)
    return report.summary()


if __name__ == "__main__":
    sys.exit(main())
