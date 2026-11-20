# Mandatory CGI tests — 42 Webserv

From the repository root:

```sh
bash tests/cgi/run_tests.sh
```

Or run one layer:

```sh
bash tests/cgi/run_tests.sh --component
bash tests/cgi/run_tests.sh --integration
```

Requires a C++98 compiler (`CXX` is supported), Python 3 and curl. Python is the
only CGI interpreter required by this suite. Each case prints Before, Expected,
After and a colored PASS/FAIL. Any failed check makes the command exit nonzero;
results are grouped at the end.

## Mandatory scope

The scope follows the mandatory requirements and CGI remarks in the project
subject. The subject requires at least one CGI, makes the full request and its
arguments available to it, specifies chunked input/EOF output and the correct
working directory, and requires nonblocking I/O, accurate responses, bounded
requests, disconnect handling and availability under stress.

| Subject requirement | Checks |
| --- | --- |
| CGI selected by file extension | Actual `.py` handler configured and executed through the real server |
| Request and arguments available to CGI | GET/POST/DELETE, method/path/protocol/host/port metadata, Content-Type, byte length, a custom request header and unchanged encoded query |
| Request body and EOF | Empty/binary bodies, sizes around the 4096-byte read buffer, 64 KiB and 1 MiB, chunked input decoded before stdin, correct length and EOF |
| Complete CGI response | LF/CRLF headers, default/explicit status, empty/binary/large bodies, headers/body split over several writes, output without Content-Length |
| Correct working directory | Script opens a fixture using a relative path |
| Nonblocking I/O through poll | Initial parent pipe write checked for readiness; simultaneous large stdin/stdout, eight concurrent clients, static response while CGI sleeps |
| Accurate errors and server robustness | Basic unusable output, crash, missing script/interpreter, disallowed method, configured body limit, pipe/fork/fcntl setup failures and cleanup |
| Bounded requests and client disconnects | Hung CGI finishes with an error or connection close; disconnected client's child is cleaned up; static requests still work |

The main HTTP scenarios use HTTP/1.0 as the subject's suggested reference. The
explicit chunked-input test uses HTTP/1.1 because the subject requires that input
framing. Responses may use HTTP/1.0 or HTTP/1.1. Framing may use Content-Length,
EOF, or chunking; adding a length header is not required when a valid
connection-close response is used.

This suite does **not** test cookies/sessions, multiple CGI types, HTTP/1.1
keep-alive/pipelining, inherited-socket hardening, HTTP_PROXY filtering, the
GATEWAY_INTERFACE variable, implicit CGI redirects, repeated response headers or
exhaustive malformed-header handling. They are outside this minimal mandatory
assessment. Basic unusable-output/error checks remain so crashes and invalid
responses do not get mistaken for working CGI.

For interpreting encoded query arguments, see
[CGI QUERY_STRING](https://www.rfc-editor.org/rfc/rfc3875.html#section-4.1.7).
This clarifies how to preserve request arguments; the suite does not require
implementing the complete CGI/HTTP RFCs.

## Test implementation

Component groups check output, request environment and process setup. They
compile the actual CGI/Router/request/response implementation. A temporary copy
of CGI.hpp grants a test peer access to output/environment; no production file
is changed. Only CGI.cpp gets syscall wrappers: ordinary operations call the
real functions, while selected cases inject pipe/fork/fcntl failures. The
initial-write check detects parent writes inside runScript before returning to
the poll loop; it does not prove readiness for every I/O call in the project.
The harness cleans up any child left untracked by production error handling.

Integration groups build the real `source/*.cpp`, including main, configuration
parser and poll loop. The server uses a temporary loopback port and scripts
copied from probe.py. Normal requests use real curl. Raw sockets control chunked
input, disconnects and requests held open while CGI runs. Script bodies are
checked byte-for-byte, not merely by HTTP status or curl exit code.

The test build sets `WEBSERV_CLIENT_TIMEOUT_SECONDS=4` to keep lifetime checks
short. Error tests accept reasonable equivalent server-error responses; a hung
CGI may instead cause a bounded connection close. Missing scripts/interpreters
must produce an error without hanging. Timing checks assume reasonable local/CI
load. The suite is not a proof of absence of all leaks or resource exhaustion.

All builds, configs, response files and script fixtures are temporary. Test
server process groups are stopped on exit. No source, Makefile, workflow,
tracked website or database is changed. GitHub Actions is not modified.
