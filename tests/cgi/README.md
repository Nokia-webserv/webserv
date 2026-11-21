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

Requires a C++98 compiler (`CXX` is supported), Python 3 and curl. Components
use Python as the one configured CGI interpreter; curl is used by integration
tests. No cookies, sessions or additional CGI interpreters are required.

Every case prints **Before**, **Expected**, **After** and a colored **PASS/FAIL**.
The summary lists failed case names and reasons. Any failure makes the command
exit nonzero. Known failures remain failures: there are no expected-failure
exceptions or flags that skip error-handling checks.

## Coverage and requirements

The project subject requires nonblocking, polled pipe I/O, unchanged request
arguments, decoded chunked stdin with EOF, stdout completion at EOF, the correct
working directory, accurate responses and bounded requests. The
[mandatory evaluation checklist](https://42evals.slddl.ovh/Cursus/Webserv/)
also asks for checked I/O results, no errno-based handling after I/O, CGI GET/POST,
and visible errors for failing or looping scripts.

| Group | Component checks |
| --- | --- |
| C1 Output | LF/CRLF, explicit/default status, binary/empty/large bodies, missing separators, unusable headers, invalid status range and inconsistent Content-Length |
| C2 Request environment | GET/POST/DELETE, exact raw query including empty/encoded values, body byte length, content metadata, custom headers, actual remote IP/port, interpreter arguments and valid envp pointers/terminator |
| C3 Setup and resources | Both pipe failures, fork failure, either nonblocking fcntl failure, cleanup of running/completed CGI, all remaining pipe ends, client pointer clearing, reaped/tracked child and repeated cleanup with a reused descriptor |
| C4 Stdin | Full/partial binary writes, exact offset after each write, final EOF, injected negative/zero write results, readiness and errno access |
| C5 Stdout | Full/partial reads, positive/zero/negative results, no premature response before EOF, unusable output, at most one read per handler call, readiness and errno access |
| C6 Real script execution | Empty through 1 MiB stdin, buffer boundaries, full stdout before consuming stdin, split output, relative file access, crashes, no/malformed output, chdir/dup2/execve failures and failed/zero/partial child error-report writes |
| C7 Focused code checks | Traced child error-report write readiness; directly discarded I/O and chdir results in CGI.cpp |

All component pipe I/O is driven by a small test dispatcher. It exercises the
real CGI class without starting the server or opening a TCP listener. The
execution cases use real fork/exec and the configured Python interpreter.

Integration groups additionally exercise the actual Router, parser and server
poll loop using curl and controlled raw sockets. They cover CGI selection,
GET/POST/DELETE, body limits, chunked input decoded before CGI, correct cwd,
errors, concurrent clients, a static request during slow CGI, timeout and
disconnect cleanup. These checks are necessary because the CGI class alone
does not own server timeout scheduling or the main poll loop.

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
  deadline while other clients still work. The checklist requires a visible
  error; this suite checks for an HTTP error visible to the requesting client.
  Empty connection close alone is a failure. The checklist does not prescribe
  one exact error code or timeout duration.

## Current findings

On CGI commit `ffb14bd`, the expanded checks identify these remaining areas
(components: 69 passed / 14 failed; integration: 31 passed / 1 failed):

1. `provideInput()` does not expose an error when write returns -1 or zero for
   nonempty input.
2. `buildResponse()` trusts an invalid/mismatched Content-Length and accepts
   numeric statuses outside the HTTP range. Reject unusable output or normalize
   framing before sending it.
3. Both dup2 failure paths leave an output pipe write end open while the child
   waits, so the parent cannot reach EOF and report the setup error promptly.
4. The chdir result is ignored, allowing a same-named script in the old working
   directory to execute.
5. Child error-report writes do not first obtain POLLOUT readiness, and both
   write return values are discarded.
6. A looping CGI causes an empty connection close instead of an HTTP error.
   Check coordination between generic client timeout and CGI timeout; the CGI
   timeout loop must also continue past clients that have no CGI.

Several failed cases exercise the same underlying problem. A failed-case count
is not a count of independent bugs. Normal requests, partial transfers, large
bodies and the tested descriptor/child cleanup paths pass.

The existing `Server::handleCGIevents()` -> `Router::handleError()` path already
builds configured/default error pages and response headers. Parent-side CGI
failures can report through `decision.errorCode` and use that path. Errors in
the forked child still need to reach the parent; changing child memory alone
does not update the parent. GET/POST's private `Router::fillResponse()` helper
only assigns fields: it does not validate statuses or calculate Content-Length,
so it is not a drop-in fix for CGI output validation.

## How instrumentation works and its limits

The component build copies the entire include tree into its temporary directory
and grants a test peer access in the copied CGI.hpp. Only the temporary CGI
translation unit gets syscall wrappers. Selected cases inject syscall outcomes;
other calls perform real I/O. The parent wrappers consume a readiness permission
per call and record errno access after I/O. Child error-report writes are traced
to a temporary regular file so their readiness can be checked from the parent.

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
