"""Deterministic CGI fixture. Copied to a temporary document root by the suite."""
import hashlib
import json
import os
from pathlib import Path
import signal
import sys
import time
from urllib.parse import parse_qs

parameters = parse_qs(os.environ.get("QUERY_STRING", ""), keep_blank_values=True)
mode = parameters.get("mode", ["env"])[0]

def output(data):
    sys.stdout.buffer.write(data)
    sys.stdout.buffer.flush()

if mode == "sleep":
    time.sleep(float(parameters.get("seconds", ["1"])[0]))
    output(b"Content-Type: text/plain\r\n\r\nfinished")
elif mode == "hang":
    while True:
        time.sleep(1)
elif mode == "pid":
    Path("child.pid").write_text(str(os.getpid()))
    while True:
        time.sleep(1)
elif mode == "crash":
    os.kill(os.getpid(), signal.SIGKILL)
elif mode == "empty":
    pass
elif mode == "malformed":
    output(b"this is not a CGI header\r\n\r\nbody")
elif mode == "partial":
    output(b"Content-Ty")
    time.sleep(0.1)
    output(b"pe: text/plain\r\nX-Test: split\r\n\r\nfirst")
    time.sleep(0.1)
    output(b"second\x00third")
elif mode == "large":
    output(b"Content-Type: application/octet-stream\r\n\r\n")
    output(bytes(range(256)) * 4096)
elif mode == "duplex":
    output(b"Content-Type: application/octet-stream\r\n\r\n" + b"O" * 131072)
    data = sys.stdin.buffer.read()
    output(data)
elif mode == "status":
    output(b"Status: 201 Created\r\nContent-Type: text/plain\r\n\r\ncreated")
elif mode == "redirect":
    output(b"Status: 302 Found\r\nLocation: /static.txt\r\nContent-Type: text/plain\r\n\r\nredirect")
elif mode == "stderr":
    print("CGI diagnostic must not become HTTP body", file=sys.stderr, flush=True)
    output(b"Content-Type: text/plain\r\n\r\nstdout only")
elif mode == "cwd":
    output(b"Content-Type: text/plain\r\n\r\n" + Path("relative.txt").read_bytes())
else:
    data = sys.stdin.buffer.read()  # Must receive EOF, including empty/chunked bodies.
    if mode == "echo":
        output(b"Content-Type: application/octet-stream\r\n\r\n" + data)
    else:
        result = {"env": dict(os.environ), "length": len(data),
                  "sha256": hashlib.sha256(data).hexdigest(), "cwd": os.getcwd()}
        output(b"Content-Type: application/json\r\n\r\n" + json.dumps(result, sort_keys=True).encode())
