# Mandatory workflow

Run the required Webserv functionality in one command from the repository root:

```sh
bash tests/mandatory/run_workflow.sh
```

All test fixtures and helpers are inside this folder. No other test directory
is required. To keep the test branch separate and check another checkout:

```sh
bash tests/mandatory/run_workflow.sh --project-root /path/to/webserv-checkout
```

The checkout must contain the implementations being tested. If its CGI or method
handlers are absent, those checks fail; tests do not substitute implementations.
The CGI component harness supports both the explicit-pointer API and the newer
ClientConnection-based API and uses a consistent shadow include tree.

The workflow builds the real Makefile in a temporary directory, launches the
real server, and prints aligned `Before:`, `Expected:` and `After:` lines with
colored PASS/FAIL results. It continues after failed checks so all blocks are
visible, then prints a summary. No commits, pushes, production changes or CI
changes are performed. All test sites and uploaded files are temporary.

To retain the temporary binaries, configuration, curl responses, Siege logs and
leak report for inspection, append `--keep-artifacts`. The directory is printed
at the end; it is outside the repository.

## Blocks

1. **Code and build:** C++98 compilation, second make without relinking,
   poll/socket source checks and runtime traces, forced socket/pipe failures,
   per-cycle I/O counts, errno access, pipe readiness and process cleanup.
2. **Configuration:** multiple ports and hostnames, custom 404 page, body-size
   limits below/at/above the boundary, directory mapping, default index files and
   per-route method permissions.
3. **Basic HTTP methods:** static GET, binary and multipart uploads followed by
   download/deletion, absent-file errors, forbidden directory deletion and an
   unknown method without a server crash.
4. **CGI:** one Python interpreter, GET/POST, binary stdin, preserved arguments,
   relative file access, script syntax errors, infinite scripts and server health.
5. **Browser and static website:** Chrome/Chromium actually renders HTML, CSS,
   JavaScript and an image; checks error/listing/redirect pages and the second
   website; curl prints real request and response headers.
6. **Port handling:** separate websites on separate ports, duplicate listen
   directives, two processes competing for a port, and rejection of partial
   startup when a configuration includes both free and occupied ports.
7. **Siege and stress:** repeated benchmark traffic on an empty page, availability
   above 99.5%, sampled resident memory, a live leak scan where available, and
   checks for remaining established connections and server health.

Cookies, sessions and multiple CGI interpreters are excluded. Hostname selection
is tested because it is part of this workflow's configuration scenarios; it does
not add any new implementation requirements to the code.
Different virtual-host blocks may share one listener. The duplicate-listen
rejection check concerns repeating the same directive within one server block;
the competing-process checks concern a separate executable binding an occupied
port.

## Requirements and duration

- `make`, a C++98 compiler (`CXX` is supported), Python 3 and curl.
- Siege: `brew install siege` on macOS, or the platform package manager.
- Chrome/Chromium for browser rendering. The macOS Chrome installation is
  detected automatically; otherwise set `MANDATORY_BROWSER` to its executable.
- `leaks` on macOS and `lsof`. If a required diagnostic is unavailable, its
  automatic check fails with an explanation. Linux users can run Valgrind using
  the manual checklist below; absence of a diagnostic is never counted as PASS.

The default load is **3 consecutive 10-second rounds with 25 clients**, using
`siege -b` against a local temporary server. The process stays running between
rounds. Siege's configuration disables cookies/HTML parsing and requests
persistent connections to avoid client ephemeral-port exhaustion during the
benchmark. No system networking settings are changed. A server may close those
connections normally; this is not a separate keep-alive conformance test.

For a longer observation window:

```sh
bash tests/mandatory/run_workflow.sh --stress-seconds 60 --stress-rounds 10 --clients 25
```

Compilation, browser startup, diagnostics and the default workload usually take
around a minute or two. The temporary build reduces the request timeout to four
seconds to keep infinite-script tests bounded. It does not change the project's
production timeout.

## Manual browser demo

```sh
bash tests/mandatory/run_workflow.sh --serve-demo
```

After the automated checks, the server remains running and prints exact browser
URLs and the temporary configuration path. Open the main site and the second
port; use DevTools -> Network to inspect request/response headers. On the main
site, try `/missing`, `/listing/`, `/old` and `/route/`. Stop the demo with Ctrl-C;
the server and its temporary files are then cleaned up.

## Automatic I/O checks

The normal server is built unchanged. A separate diagnostic build compiles all
production translation units with test-only wrappers around explicit
poll/read/recv/write/send/close calls and errno access. It records the call site,
fd type, byte counts, readiness direction and operation count per poll cycle.
Production sources are not rewritten. The wrappers preserve syscall errno.

Automatic scenarios cover:

- GET, POST and DELETE with socket reads capped at 11 bytes and writes at 7 bytes.
  The exact transferred/saved bytes must still match.
- No socket/parent-pipe I/O before readiness for that exact fd and direction.
  A pipe HUP permits reading EOF. Closing a descriptor clears its readiness so
  a newly created descriptor with the same number cannot reuse stale readiness.
- Listener accept calls follow POLLIN from the same dispatcher. Accepting queued
  clients until the nonblocking listener is drained is allowed.
- At most one read and one write per descriptor per poll iteration; one parent
  poll call site; actual simultaneous read/write monitoring.
- Forced recv/send results of 0 and -1. The affected client must be closed and
  a subsequent client must receive a complete response. Failure injection is
  armed for the target client's source port, not an arbitrary startup probe.
- CGI pipe reads with positive data, EOF and forced failure; CGI stdin writes
  with negative/zero-progress results for a nonempty body.
- No observed errno inspection following read/recv/write/send, including in
  callees. Calls such as open/stat/accept/readdir are tracked separately so their
  legitimate errno handling is not mistaken for errno handling after I/O.
- Actual regular disk-file reads/writes are identified and exempted from poll
  readiness, as permitted by the subject. Their I/O return values still matter.
- CGI child error-report writes with ignored results and parent writes before
  the CGI pipe has been polled. External Python internals are not instrumented.

Tracing validates the exercised execution paths. It is not a formal proof for
all possible inputs, dependencies, compiler behavior or external script code.
The manual source checklist complements these tests.

## Reading results

- **PASS:** the stated automatic check succeeded.
- **FAIL:** a behavioral or build problem was observed. The command exits **1**.
- Exit **0** means all automatic checks passed. Manual work is listed below and
  is not included in the automatic result. There are no manual-status rows in
  the terminal output.

The zero-progress write injection uses a nonempty body. An empty input should
instead close stdin so CGI receives EOF. Error-report writes inside the CGI child
are checked for ignored return values even though those paths are uncommon.

A script error must be visible as an HTTP error response; simply disconnecting a
looping script's client does not pass this workflow's CGI error check. For normal
responses, equivalent reason phrases are accepted; numeric status codes, framing
and relevant body bytes are checked. An existing correct Content-Length is
checked when present; connection-close framing is also accepted by curl.

The memory trend check allows up to 16 MiB growth after warmup in its sampled
window. This is a coarse regression threshold, not proof of leak freedom.
The separate live heap scan and longer observation serve different purposes;
even a long finite test cannot prove indefinite stability. The automatic Chrome
checks do not claim that a human inspected the Network panel.

## Manual checklist — outside the automatic results

- Explain the HTTP request/response flow, listener/client sockets and how poll
  watches reading and writing together in the main loop.
- Walk from `Server::run` through each readiness branch to every socket/pipe
  operation. Readiness must belong to the actual fd and direction; receiving a
  client request does not make a newly created CGI pipe ready for writing.
- Inspect all I/O callers and helper functions: positive counts, short transfers,
  EOF, zero progress, failures, disconnects and process cleanup. Check new I/O
  locations added during refactoring, not only the currently exercised paths.
- Check that read/recv/write/send errors never change server behavior by testing
  errno. Do not confuse legitimate errno checks after open/stat/accept/readdir
  with checks after those four I/O functions. Ordinary disk files do not require
  poll readiness under the subject's explicit exception.
- Use `--serve-demo` and open both printed sites in the team's actual browser.
  Inspect request/response headers in DevTools -> Network, load assets, visit a
  missing URL, list a directory and follow a redirect.
- Run the longer Siege command above without restarting the server. Observe
  availability, connection cleanup and whether memory stabilizes over time.
- Run native leak tooling again after additional workloads. On Linux, use
  Valgrind; on macOS, inspect the retained leaks report. A finite live scan does
  not prove all allocations are released on every exit/error path.
- Confirm the actual reference browser, supported environment and project code
  can be explained by the team. These are human checks, not executable tests.
