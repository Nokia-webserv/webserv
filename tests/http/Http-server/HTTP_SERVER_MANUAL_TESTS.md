# HTTP Server Manual Tests

## Test 1 - Single complete GET

```bash
python3 - <<'PY'
import socket

with socket.create_connection(("127.0.0.1", 8080)) as s:
    s.sendall(b"GET /hello HTTP/1.1\r\nHost: localhost\r\n\r\n")
    print("Request sent successfully")
PY
```

## Test 2 - Partial HTTP headers

```bash
python3 - <<'PY'
import socket
import time

with socket.create_connection(("127.0.0.1", 8080)) as s:
    s.sendall(b"GET /partial HTTP/1.1\r\n")
    print("Part 1 sent")
    time.sleep(2)

    s.sendall(b"Host: localhost\r\n\r\n")
    print("Part 2 sent")
    time.sleep(2)
PY
```

## Test 3 - Five GET requests on the same connection

```bash
python3 - <<'PY'
import socket
import time

with socket.create_connection(("127.0.0.1", 8080)) as s:
    requests = b"".join(
        f"GET /request/{i} HTTP/1.1\r\nHost: localhost\r\n\r\n".encode()
        for i in range(1, 6)
    )

    s.sendall(requests)
    print("Sent 5 requests on the same connection")
    time.sleep(2)
PY
```

## Test 4 - Forty requests / 16-request limit

```bash
python3 - <<'PY'
import socket
import time

with socket.create_connection(("127.0.0.1", 8080)) as s:
    requests = b"".join(
        f"GET /request/{i} HTTP/1.1\r\nHost: localhost\r\n\r\n".encode()
        for i in range(1, 41)
    )

    s.sendall(requests)
    print("Sent 40 requests on one connection")
    time.sleep(3)
PY
```

## Test 5 - Two simultaneous clients (slow + fast)

```bash
python3 - <<'PY'
import socket
import time

host = "127.0.0.1"
port = 8080

client1 = socket.create_connection((host, port))
client2 = socket.create_connection((host, port))

try:
    client1.sendall(b"GET /slow HTTP/1.1\r\n")
    print("Client 1: sent incomplete request")

    time.sleep(1)

    client2.sendall(
        b"GET /fast HTTP/1.1\r\n"
        b"Host: localhost\r\n\r\n"
    )
    print("Client 2: sent complete request")

    time.sleep(1)

    client1.sendall(b"Host: localhost\r\n\r\n")
    print("Client 1: completed request")

    time.sleep(1)
finally:
    client1.close()
    client2.close()
PY
```

## Test 6 - Invalid HTTP method

```bash
python3 - <<'PY'
import socket

with socket.create_connection(("127.0.0.1", 8080)) as s:
    s.sendall(
        b"PATCH /invalid HTTP/1.1\r\n"
        b"Host: localhost\r\n"
        b"\r\n"
    )

    print("Invalid request sent")

    s.settimeout(3)

    try:
        data = s.recv(1024)
        if data == b"":
            print("Server closed connection")
        else:
            print("Received:", data)
    except socket.timeout:
        print("Server did not close connection within 3 seconds")
PY
```

## Test 7 - EOF with an incomplete request

```bash
python3 - <<'PY'
import socket
import time

with socket.create_connection(("127.0.0.1", 8080)) as s:
    s.sendall(b"GET /incomplete HTTP/1.1\r\n")
    print("Sent incomplete request")

    time.sleep(1)

    s.shutdown(socket.SHUT_WR)
    print("Sent EOF (shutdown write side)")

    s.settimeout(3)

    try:
        data = s.recv(1024)
        if data == b"":
            print("Server closed connection")
        else:
            print("Received:", data)
    except socket.timeout:
        print("Connection still open after 3 seconds")
PY
```

## Test 8 - Inactivity timeout (not fully verified)

```bash
python3 - <<'PY'
import socket
import time

with socket.create_connection(("127.0.0.1", 8080)) as s:
    s.sendall(b"GET /timeout HTTP/1.1\r\n")
    print("Incomplete request sent")

    start = time.monotonic()
    s.settimeout(40)

    try:
        data = s.recv(1024)
        elapsed = time.monotonic() - start

        if data == b"":
            print(f"Server closed after {elapsed:.1f} seconds")
        else:
            print(f"Unexpected data: {data!r}")
    except socket.timeout:
        print("FAIL: Connection still open after 40 seconds")
PY
```

## Test 9 - Partial POST body (Content-Length)

```bash
python3 - <<'PY'
import socket
import time

with socket.create_connection(("127.0.0.1", 8080)) as s:
    s.sendall(
        b"POST /upload HTTP/1.1\r\n"
        b"Host: localhost\r\n"
        b"Content-Length: 11\r\n"
        b"Content-Type: text/plain\r\n"
        b"\r\n"
        b"Hello"
    )

    print("Sent headers + first 5 body bytes")
    time.sleep(2)

    s.sendall(b" World")
    print("Sent remaining 6 body bytes")
    time.sleep(1)
PY
```

## Test 10 - Chunked body in two parts

```bash
python3 - <<'PY'
import socket
import time

with socket.create_connection(("127.0.0.1", 8080)) as s:
    s.sendall(
        b"POST /chunked HTTP/1.1\r\n"
        b"Host: localhost\r\n"
        b"Transfer-Encoding: chunked\r\n"
        b"\r\n"
        b"5\r\nHello\r\n"
    )
    print("Sent first chunk")
    time.sleep(2)

    s.sendall(
        b"6\r\n World\r\n"
        b"0\r\n\r\n"
    )
    print("Sent remaining chunk and terminator")
    time.sleep(1)
PY
```

## Test 11 - POST + GET pipelining

```bash
python3 - <<'PY'
import socket
import time

with socket.create_connection(("127.0.0.1", 8080)) as s:
    requests = (
        b"POST /upload HTTP/1.1\r\n"
        b"Host: localhost\r\n"
        b"Content-Length: 11\r\n"
        b"\r\n"
        b"Hello World"
        b"GET /after-post HTTP/1.1\r\n"
        b"Host: localhost\r\n"
        b"\r\n"
    )

    s.sendall(requests)
    print("Sent POST + GET on the same connection")
    time.sleep(2)
PY
```

## Test 12 - Mixed HTTP requests in one buffer

```bash
python3 - <<'PY'
import socket
import time

requests = (
    b"GET /first HTTP/1.1\r\n"
    b"Host: localhost\r\n\r\n"

    b"POST /upload HTTP/1.1\r\n"
    b"Host: localhost\r\n"
    b"Content-Length: 11\r\n\r\n"
    b"Hello World"

    b"GET /middle HTTP/1.1\r\n"
    b"Host: localhost\r\n\r\n"

    b"POST /chunked HTTP/1.1\r\n"
    b"Host: localhost\r\n"
    b"Transfer-Encoding: chunked\r\n\r\n"
    b"5\r\nHello\r\n"
    b"6\r\n World\r\n"
    b"0\r\n\r\n"

    b"DELETE /last HTTP/1.1\r\n"
    b"Host: localhost\r\n\r\n"
)

with socket.create_connection(("127.0.0.1", 8080)) as s:
    s.sendall(requests)
    print(f"Sent {len(requests)} bytes in one sendall()")
    time.sleep(2)
PY
```

## Test 13 - Two complete requests + incomplete last request

```bash
python3 - <<'PY'
import socket
import time

with socket.create_connection(("127.0.0.1", 8080)) as s:
    s.sendall(
        b"GET /first HTTP/1.1\r\nHost: localhost\r\n\r\n"
        b"POST /second HTTP/1.1\r\nHost: localhost\r\n"
        b"Content-Length: 5\r\n\r\nHello"
        b"GET /third HTTP/1.1\r\n"
    )

    print("Sent 2 complete requests + 1 partial request")
    time.sleep(2)

    s.sendall(b"Host: localhost\r\n\r\n")
    print("Completed third request")
    time.sleep(1)
PY
```

## Test 14 - Partial POST body followed by pipelined GET

```bash
python3 - <<'PY'
import socket
import time

with socket.create_connection(("127.0.0.1", 8080)) as s:
    s.sendall(
        b"POST /upload HTTP/1.1\r\n"
        b"Host: localhost\r\n"
        b"Content-Length: 11\r\n\r\n"
        b"Hello"
    )

    print("Sent POST headers + partial body")
    time.sleep(2)

    s.sendall(
        b" World"
        b"GET /after-body HTTP/1.1\r\n"
        b"Host: localhost\r\n\r\n"
    )

    print("Sent remaining POST body + complete GET")
    time.sleep(1)
PY
```

## Test 15 - Partial chunked POST body followed by pipelined GET

```bash
python3 - <<'PY'
import socket
import time

with socket.create_connection(("127.0.0.1", 8080)) as s:
    s.sendall(
        b"POST /chunked HTTP/1.1\r\n"
        b"Host: localhost\r\n"
        b"Transfer-Encoding: chunked\r\n\r\n"
        b"5\r\nHello\r\n"
    )

    print("Sent first chunk")
    time.sleep(2)

    s.sendall(
        b"6\r\n World\r\n"
        b"0\r\n\r\n"
        b"GET /after-chunked HTTP/1.1\r\n"
        b"Host: localhost\r\n\r\n"
    )

    print("Sent final chunk + pipelined GET")
    time.sleep(1)
PY
```
