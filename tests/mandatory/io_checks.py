"""Test-only syscall tracing for readiness, byte counts, errno and disconnects."""
import json
import os
from pathlib import Path
import re
import shlex
import socket
import subprocess
import sys
import time

PRELUDE = '''#include "io_audit.hpp"
#define poll(f,n,t) ioAuditPoll(f,n,t,__FILE__,__LINE__)
#define recv(f,b,n,g) ioAuditRecv(f,b,n,g,__FILE__,__LINE__)
#define send(f,b,n,g) ioAuditSend(f,b,n,g,__FILE__,__LINE__)
#define read(f,b,n) ioAuditRead(f,b,n,__FILE__,__LINE__)
#define write(f,b,n) ioAuditWrite(f,b,n,__FILE__,__LINE__)
#define close ioAuditClose
#define open ioAuditOpen
#define stat(p,s) ioAuditStat(p,s)
#define accept ioAuditAccept
#define opendir ioAuditOpendir
#define readdir ioAuditReaddir
#define closedir ioAuditClosedir
#undef errno
#define errno ioAuditErrno(__FILE__,__LINE__)
'''


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def run_io_checks(report, work, root, server_class, scripts):
    block = "1. Code and build"
    build = work / "io-audit"
    build.mkdir()
    here = Path(__file__).resolve().parent
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    flags = ["-std=c++98", "-Wall", "-Wextra", "-Werror", "-I" + str(root / "include"), "-I" + str(here)]
    binary = build / "webserv-audited"
    all_logs = []

    def compile_audit():
        sources = []
        # Every production translation unit is compiled with the wrappers. This
        # also catches errno accesses in callees outside the immediate handler.
        for path in sorted((root / "source").glob("*.cpp")):
            if path.name.startswith("."):
                continue
            source = path.read_text().replace("std::remove(", "ioAuditRemove(")
            target = build / path.name
            target.write_text(PRELUDE + '\n#line 1 ' + json.dumps(str(path)) + '\n' + source)
            sources.append(str(target))
        command = compiler + flags + ["-DWEBSERV_CLIENT_TIMEOUT_SECONDS=4", str(here / "io_audit.cpp"), *sources, "-o", str(binary)]
        result = subprocess.run(command, capture_output=True, timeout=90)
        (build / "build.log").write_bytes(result.stdout + result.stderr)
        check(result.returncode == 0, "I/O audit build failed:\n" + result.stderr.decode("utf8", "replace")[-2500:])
        print("After:    Audited server built from all production translation units", flush=True)
    report.case(block, "Build runtime I/O tracer", "Real server code with test-only syscall wrappers; production sources untouched", compile_audit)
    if not binary.exists():
        return

    def start(name, fault=None):
        folder = build / name
        folder.mkdir()
        data = folder / "files"
        data.mkdir()
        (data / "body.txt").write_bytes(b"0123456789abcdef" * 128)
        with socket.socket() as reserve:
            reserve.bind(("127.0.0.1", 0))
            port = reserve.getsockname()[1]
        config = folder / "server.conf"
        config.write_text(f'''server {{
listen 127.0.0.1:{port};
server_name alpha.test;
location / {{ root "{data}"; methods GET; }}
location /files {{ root "{data}"; upload_store "{data}"; methods GET POST DELETE; }}
location /cgi {{ root "{scripts}"; methods GET POST; cgi_extension .py "{sys.executable}"; }}
}}''')
        log = folder / "io.log"
        env = dict(os.environ, MANDATORY_IO_LOG=str(log), MANDATORY_IO_MAX_READ="11", MANDATORY_IO_MAX_WRITE="7")
        if fault:
            env.update(MANDATORY_IO_FAULT=fault, MANDATORY_IO_ARM=str(folder / "arm"))
        server = server_class(binary, config, folder, [port], environment=env)
        server.start()
        all_logs.append(log)
        return server, folder, data, log

    def entries(log):
        return log.read_text().splitlines() if log.exists() else []

    def readiness(lines):
        bad = [line for line in lines if line.startswith("VIOLATION|READY|")]
        check(not bad, "I/O before readiness for its fd/direction:\n" + "\n".join(bad[:10]))

    def methods():
        server, folder, data, log = start("methods")
        try:
            payload = b"binary\x00\xff" * 64
            server.request("/body.txt", body=(data / "body.txt").read_bytes())
            server.request("/files/", "POST", payload, status=201, headers={"Content-Type": "application/octet-stream"})
            files = [path for path in data.iterdir() if path.name != "body.txt"]
            check(len(files) == 1 and files[0].read_bytes() == payload, "Partial reads corrupted POST body")
            server.request("/files/" + files[0].name, body=payload)
            server.request("/files/" + files[0].name, "DELETE", status=204, body=b"")
            server.request("/files/" + files[0].name, status=404)
        finally:
            server.stop()
        lines = entries(log)
        readiness(lines)
        check(any(line.startswith("IO|recv|") for line in lines) and any(line.startswith("IO|send|") for line in lines),
              "Socket I/O was not observed")
        regular = [line for line in lines if line.startswith("IO|") and "|R|" in line]
        check(any(line.startswith("IO|read|") for line in regular) and any(line.startswith("IO|write|") for line in regular),
              "Regular disk file reads/writes were not observed")
        print("After:    GET/POST/DELETE: socket readiness valid; fragmented recv/partial send preserve bytes; regular file I/O exempt", flush=True)
    report.case(block, "Trace GET/POST/DELETE with fragmented I/O",
                "Every socket operation follows readiness; ordinary file reads/writes need no poll", methods)

    def cgi():
        server, folder, data, log = start("cgi")
        try:
            payload = b"pipe payload" * 8
            server.request("/cgi/probe.py?mode=echo", "POST", payload, body=payload)
        finally:
            server.stop()
        readiness(entries(log))
        print("After:    CGI pipe I/O follows poll readiness for each fd/direction", flush=True)
    report.case(block, "Trace CGI stdin/stdout readiness", "Every parent pipe read/write follows poll readiness for that pipe", cgi)

    def pipe_error():
        server, folder, data, log = start("pipe-read-error", "read:-1")
        try:
            (folder / "arm").write_text("0")
            server.request("/cgi/probe.py?mode=echo", "POST", b"pipe input", status=500)
            check(any(line.startswith("INJECT|read|") for line in entries(log)), "Pipe read failure was not injected")
            server.request("/body.txt", body=(data / "body.txt").read_bytes())
        finally:
            server.stop()
    report.case(block, "CGI stdout read returns -1",
                "Server returns 500, cleans up CGI and continues serving clients", pipe_error)

    for operation in ("recv", "send"):
        for value in ("-1", "0"):
            def disconnect(op=operation, result=value):
                server, folder, data, log = start(op + result.replace("-", "negative"), op + ":" + result)
                try:
                    connection = socket.create_connection(("127.0.0.1", server.ports[0]), timeout=3)
                    with connection:
                        connection.settimeout(3)
                        # Arm only this source port: startup probes cannot
                        # accidentally consume the injected syscall failure.
                        (folder / "arm").write_text(str(connection.getsockname()[1]))
                        connection.sendall(b"GET /body.txt HTTP/1.0\r\nHost: alpha.test\r\n\r\n")
                        try:
                            received = connection.recv(1)
                            check(received == b"", "Failed socket still delivered response data")
                        except ConnectionResetError:
                            pass
                    deadline = time.monotonic() + 2
                    while True:
                        lines = entries(log)
                        injected = next((line.split("|")[2] for line in lines if line.startswith("INJECT|")), None)
                        closed = injected is not None and "CLOSE|" + injected + "|0" in lines
                        if closed or time.monotonic() >= deadline:
                            break
                        time.sleep(0.02)
                    check(injected is not None, "Failure injection did not reach the test socket")
                    check(closed, "Socket was not closed after " + op + " returned " + result)
                    check(not any(line.startswith("VIOLATION|ERRNO|") for line in lines), "errno was inspected after injected socket failure")
                    server.request("/body.txt", body=(data / "body.txt").read_bytes())
                    print("After:    " + op + "=" + result + ": failed client closed; no errno access; next client served", flush=True)
                finally:
                    server.stop()
            report.case(block, operation + " returns " + value,
                        "Client socket closed without consulting errno; server still serves another client", disconnect)

    def counts():
        lines = [line for log in all_logs for line in entries(log)]
        bad = [line for line in lines if line.startswith("VIOLATION|COUNT|")]
        check(not bad, "Repeated I/O on one fd/direction in the same poll cycle:\n" + "\n".join(bad[:10]))
        watches = [line for line in lines if line.startswith("POLL|")]
        check(watches and any(int(line.split("|")[2]) > 0 for line in watches), "No simultaneous read/write monitoring observed")
        locations = {line.rsplit("|", 1)[1] for line in watches}
        check(len(locations) == 1 and "Server.cpp:" in next(iter(locations)), "Parent used multiple poll call sites: " + str(locations))
        print("After:    One parent poll call site; simultaneous read/write monitoring; at most one read and write per fd/cycle", flush=True)
    report.case(block, "Poll call site and per-cycle I/O counts",
                "One dispatcher; read/write watched together; no repeated same-direction socket/pipe call per cycle", counts)

    def accepts():
        lines = [line for log in all_logs for line in entries(log)]
        check(any(line.startswith("ACCEPT|") for line in lines), "No listener accepts were traced")
        bad = [line for line in lines if line.startswith("VIOLATION|READY|accept|")]
        check(not bad, "Listener accept without POLLIN:\n" + "\n".join(bad[:10]))
        print("After:    Listener accept calls follow POLLIN; draining the accept queue remains allowed", flush=True)
    report.case(block, "Listener readiness before accept", "Listener is included in the poll loop; accepts follow POLLIN", accepts)

    def errnos():
        lines = [line for log in all_logs for line in entries(log)]
        bad = [line for line in lines if line.startswith("VIOLATION|ERRNO|")]
        check(not bad, "errno access following read/recv/write/send:\n" + "\n".join(bad[:10]))
        print("After:    No errno access after I/O in the traced methods, CGI and forced socket failures", flush=True)
    report.case(block, "Trace errno access after I/O", "No errno inspection after read/recv/write/send in exercised paths", errnos)

    def eof():
        events = [line.split("|") for log in all_logs for line in entries(log) if line.startswith("IO|read|") and "|P|" in line]
        values = [int(event[3]) for event in events]
        check(any(value > 0 for value in values) and 0 in values and -1 in values,
              "Pipe read tracing did not exercise positive data, EOF and failure separately")
        print("After:    CGI pipe reads observed with positive bytes, EOF=0 and failure=-1", flush=True)
    report.case(block, "CGI read data, EOF and failure are distinct",
                "Positive reads preserve bytes, zero completes output, negative read becomes an error", eof)
