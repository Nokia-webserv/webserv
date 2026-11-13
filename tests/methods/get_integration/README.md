# GET acceptance checks

Start with [../README.md](../README.md) for the main command, group descriptions,
expected output and optional helper tests.

```sh
bash tests/methods/test_get.sh
```

The main suite has 58 cases, organized into file bytes, MIME, path mapping,
directories, listing links, filesystem errors, connection reuse/delivery,
HTTP version and redirects. While redirect handling is unfinished:

```sh
SKIP_GET_REDIRECT_TESTS=1 bash tests/methods/test_get.sh
```

This runs 55 cases and explicitly skips the three redirects. Permission cases
are skipped when run as root. Each test prints an ID, expectation and actual
response metadata. A table summarizes each group. Failed bodies include size,
first differing byte and SHA256 diagnostics.

All acceptance requests use curl. Python prepares fixtures, manages the real
server and verifies response headers/bodies. Temporary executables, data,
configuration and logs are cleaned up. Requires curl, Python 3.7+ and a C++ compiler.

Separate raw TCP connection diagnostics:

```sh
bash tests/methods/test_get_connections.sh
```

Two pipelining cases check actual response order and bytes, including forty
requests across the server's batch limit. One HTTP/1.0 EOF/close check exposes
the known connection-policy bug. HTTP/1.1 closure is outside this diagnostic's
scope. The HTTP/1.0 failure remains a real FAIL. This optional
command is separate from the curl acceptance suite and individual helper tests.

The shell runner compiles the actual production sources. get_test_support.py
contains only reporting, curl assertions and temporary-server management; no
production algorithm is reimplemented there. The tests do not cover every
protocol/security feature; the main README lists the boundaries explicitly.
