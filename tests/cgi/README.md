# Mandatory CGI checks

From the repository root, run components and real-server checks together:

```sh
bash tests/cgi/run_tests.sh
```

Or run a single layer:

```sh
bash tests/cgi/run_tests.sh --component
bash tests/cgi/run_tests.sh --integration
```

The GitHub Actions workflow in `.github/workflows/ci.yml` runs the full command
in its **CGI component and integration tests** step. The workflow triggers on
pushes and pull requests. A failing CGI check fails that step.

Requires a C++98 compiler (`CXX` is supported), Python 3 and curl. Components
use Python as the one configured CGI interpreter; curl is used by integration
tests. No cookies, sessions or additional CGI interpreters are required.

Every case prints **Before**, **Expected**, **After** and a colored **PASS/FAIL**.
The summary groups results and lists failed case names and reasons. Any failure
makes the command exit nonzero.

The integration groups include six GET/POST checks for literal
`while True: pass`, a Python syntax error and an uncaught Python exception.
The scripts are embedded in `run_tests.py` and generated only in its temporary
test directory. They need no additional repository files. The loop checks an
HTTP timeout error, another client served during the loop, child cleanup and
a successful request afterwards. Use the default command or `--integration`;
`--component` does not run real-server cases. Sleeping loops are also tested.

## Coverage

Component checks exercise the CGI class, its request environment, pipe I/O,
script execution, response construction and resource cleanup.

| Group | Component checks |
| --- | --- |
| C1 Output | LF/CRLF, explicit/default status, binary/empty/large bodies, missing separators, unusable headers, invalid status range and inconsistent Content-Length |
| C2 Request environment | GET/POST/DELETE, exact raw query including empty/encoded values, body byte length, content metadata, custom headers, actual remote IP/port, interpreter arguments and valid envp pointers/terminator |
| C3 Setup and resources | Both pipe failures, fork failure, either nonblocking fcntl failure, cleanup of running/completed CGI, all remaining pipe ends, client pointer clearing, reaped/tracked child and repeated cleanup with a reused descriptor |
| C4 Stdin | Full/partial binary writes, exact offset after each write, final EOF, injected negative/zero write results, readiness and errno access |
| C5 Stdout | Full/partial reads, positive/zero/negative results, no premature response before EOF, unusable output, at most one read per handler call, readiness and errno access |
| C6 Real script execution | Empty through 1 MiB stdin, buffer boundaries, full stdout before consuming stdin, split output, relative file access, crashes, no/malformed output and chdir/dup2/execve failures |
| C7 Focused code checks | Directly discarded I/O and chdir results in CGI.cpp |

All component pipe I/O is driven by a small test dispatcher. It exercises the
real CGI class without starting the server or opening a TCP listener. The
execution cases use real fork/exec and the configured Python interpreter.

Integration groups additionally exercise the actual Router, parser and server
poll loop using curl and controlled raw sockets. They cover CGI selection,
GET/POST/DELETE, body limits, chunked input decoded before CGI, correct cwd,
errors, concurrent clients, a static request during slow CGI, timeout and
disconnect cleanup. Groups I2 and I4 exercise broken Python files and active
infinite CPU loops through GET and POST. Integration tests run the production
server timeout scheduling and poll loop.

## Error expectations

- Unusable output can be rejected or corrected. If accepted, the emitted HTTP
  response must have a valid status and Content-Length matching its body. A CGI
  script's incorrect length must not cause HTTP framing errors.
- A negative input write, or zero progress writing a nonempty buffer, must be
  handled explicitly. This harness expects the module to expose a server error
  through its existing error-code interface; it does not allow silent loss of
  input or waiting indefinitely for another event.
- A forced chdir failure has a same-named decoy script in the inherited cwd.
  Running that script is a failure, even if it produces a valid HTTP 200.
- The 2.5-second component deadline is a local test safeguard for immediate
  setup errors and short fixtures, not a timeout value prescribed by the subject.
- The real-server timeout build uses `WEBSERV_CLIENT_TIMEOUT_SECONDS=4`.
  A looping CGI must produce HTTP 500, 502 or 504 within the bounded test
  deadline while other clients still work. Empty connection close alone fails
  this check. The loop's child process must also be killed and reaped.

## How instrumentation works and its limits

The component build copies the entire include tree into its temporary directory
and grants a test peer access in the copied CGI.hpp. Only the temporary CGI
translation unit gets syscall wrappers. Selected cases inject syscall outcomes;
other calls perform real I/O. The parent wrappers consume a readiness permission
per call and record errno access after I/O.

The resource tests verify the module's reaped-or-tracked contract. The harness
then emulates pending-PID reaping to avoid leaving test children behind. This is
not proof that the production server's periodic reaper handles every OS error.
Each harness subprocess gets its own process group, cleaned up even on timeout.

The source checks detect directly discarded call results, including `(void)`
casts; they are focused checks, not a complete C++ control-flow analysis. Runtime
readiness/errno assertions cover exercised paths, not every possible execution.
The full-server single-poll architecture, long-running leak/stress checks and
other server-wide requirements still need their own checks and code review.
Ordinary disk files are exempt from poll in the supplied project subject; CGI
pipes are not.

This suite does not require full CGI/HTTP RFC compliance. It excludes cookies,
sessions, multiple CGI interpreters, GATEWAY_INTERFACE, HTTP/1.1 keep-alive,
implicit CGI redirects and exhaustive header validation. The invalid-output
cases above check basic accurate HTTP framing and error handling.

Builds, scripts, configs and responses are temporary. No production source,
Makefile, workflow or tracked website content is changed by the tests.
