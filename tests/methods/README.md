# Method tests

## Multipart boundary only

```sh
python3 tests/methods/test_multipart_boundary.py
```

12 component cases check only boundary extraction and its 400 errors. They print
Before, Expected, After and colored PASS/FAIL. The temporary build observes the
local boundary variable and returns before the next multipart stage. Production
files, body parsing, uploads and CI are unchanged. The test uses the comment
`// find multipart parts` (or its original Russian wording) as the stopping point.
Parameter-name case and whitespace expectations follow
[RFC 9110, section 5.6.6](https://www.rfc-editor.org/rfc/rfc9110.html#section-5.6.6).

## Multipart parts only

```sh
python3 tests/methods/test_multipart_parts.py
```

19 component cases call the real handleMultipart and compare its emitted parts
byte-for-byte, including order, empty file content, NUL and CRLF. They also check
missing/truncated delimiters and 400 responses. The test captures each existing
`std::cout << part << std::endl` output separately; it does not replace the
splitting algorithm. The temporary header grants test access to the private
method. Router.cpp is compiled unchanged. No server, handlePart, file saving,
success response, Git changes or CI changes are involved. Keep the debug output
until handlePart provides another observation point. Preamble handling, transport
padding and validation of individual part headers are outside this small suite.
For malformed input after earlier complete parts, the suite requires 400 but
does not prescribe rollback of earlier parts. Closing-line checks follow
[RFC 2046, section 5.1.1](https://www.rfc-editor.org/rfc/rfc2046.html#section-5.1.1).

## Raw POST upload

To check raw POST through the actual server using curl:

```sh
bash tests/methods/test_post_raw.sh --integration
```

For all raw POST checks (unit, component and HTTP integration):

```sh
bash tests/methods/test_post_raw.sh --all
```

The suites cover only raw uploads; multipart is unfinished and is not tested.
See [the POST integration guide](post_raw_integration/README.md) for coverage,
output and separate commands. CI runs the raw POST integration command on push
and pull_request. Unit and component checks remain separate local commands.

## Test GET through the server

From the repository root, run:

```sh
bash tests/methods/test_get.sh
```

This is the main command for reviewing the completed GET implementation.
It builds the current real server, creates a temporary configuration and files,
starts a listener on a free local port, sends GET requests with curl and checks
the HTTP responses received by curl. The script also works by absolute path from
another directory. It requires curl, Python 3, a C++ compiler and local TCP sockets.

The command checks GET through the complete chain:

```text
curl -> HTTP parser -> router -> GET -> response -> send buffer -> curl
```

It runs only the GET HTTP integration suite. It does not invoke individual helper
or component tests, general parser/config tests, or the other CI suites.

## Groups and readable results

The 58 acceptance scenarios are grouped by observable GET behavior:

| Group | Cases | Checks |
| --- | ---: | --- |
| F File bytes and read boundaries | 9 | 0, 1, 999, 1000, 1001, 1999, 2000, 2001 and 1048576 bytes; exact binary comparison |
| M MIME types | 10 | HTML, CSS, JavaScript, JSON, uppercase PNG, SVG, WOFF2, MP4, unknown and missing extensions |
| P Path/location mapping | 5 | Nested paths, longest location, trailing slashes, root with spaces, query separation |
| D Index/autoindex | 8 | Index priority, empty/nested index, missing index, empty listing and autoindex off |
| L Listing links | 8 | Complete label set; follow filenames with spaces, HTML markup, percent, Cyrillic, emoji, query/fragment symbols and dotfile |
| E Errors/access | 10 | 404, ENOTDIR, FIFO, non-regular indexes, permissions, 405/Allow and generic filesystem 500 |
| C Reuse/delivery | 4 | Correct bodies on reused connection, recovery after 404/500, six simultaneous distinct downloads and rate-limited 1 MiB download |
| V HTTP version | 1 | HTTP/1.0 response version and body; closing is tested separately |
| R Directory redirects | 3 | Trailing slash, query and encoding; verify Location and then request target |

Each case prints its stable ID, name, expected result, actual HTTP version/status,
Content-Type, Content-Length and downloaded size. Exact file comparisons are
explicit. On mismatch, diagnostics include the first differing byte and SHA256
of expected/actual bodies. The final table shows PASS/FAIL/SKIP for every group.
PASS is green, FAIL red and SKIP yellow. Any failing case makes the command fail.

The FIFO cases ensure GET does not open a pipe for a blocking read. A self-looping
symlink makes stat fail with ELOOP to exercise this implementation's generic 500
mapping without changing server code. This is not a general symlink/security test.
Permission tests are explicitly skipped for root. No directory ordering or exact
HTML layout is assumed. Every HTTP request in the main suite is made by curl.

## While handleRedirect is unfinished

```sh
SKIP_GET_REDIRECT_TESTS=1 bash tests/methods/test_get.sh
```

This runs the 55 other scenarios and reports the three redirect cases as SKIP.
Once handleRedirect is integrated, use the main command without the flag.
There are no automatic expected-failure allowances for redirects.

## Separate connection diagnostics

```sh
bash tests/methods/test_get_connections.sh
```

This additional diagnostic sends raw TCP requests, since it needs to observe
server EOF and send pipelined requests without waiting for responses. It uses
the same real server and grouped output; it does not replace the curl GET suite.

Two cases check three and forty pipelined responses, comparing every body and
its order; forty requests cross the event loop's 16-request batch boundary.
One case checks HTTP/1.0 default close after the complete response. It currently
exposes the known server bug and returns FAIL, not SKIP or expected PASS.
HTTP/1.1 connection-close behavior is outside this diagnostic's scope.
The curl-only HTTP/1.0 case never claims to verify server EOF.

## Explicit limits

This is functional coverage of the current static GET implementation, not an
exhaustive protocol/security audit. Configured return redirects, CGI, URI
normalization/root containment (#107), custom error-page mapping, arbitrary
symlink policy, injected read/readdir/closedir failures, allocation failures and
resource exhaustion remain outside this suite. The checks create real files;
no production helpers or filesystem calls are mocked.

## Existing GitHub CI

The .github/workflows/ci.yml runs the existing build, startup, config, TCP accept,
HTTP parser and general integration suites, followed by GET and raw POST
integration steps.
The GET step invokes tests/methods/test_get.sh once with SKIP_GET_REDIRECT_TESTS=1.
It runs automatically on push and pull_request. Remove the flag once directory
redirects are integrated. The runner builds its own temporary server.

Helper/component suites and the separate connection diagnostic are not invoked
by this new step. The diagnostic still exposes the unfinished HTTP/1.0 closure.
Existing CI suites remain in place; they are not called again by the GET runner.

## Optional diagnostics: individual functions and components

These are useful for locating a failure. They are separate from the main GET check.

```sh
# Unit tests for the string helpers
bash tests/methods/url_encode_path/test_url_encode_path.sh
bash tests/methods/escape_html/test_escape_html.sh

# Component tests for listings, files and directories
bash tests/methods/generate_files_list/test_generate_files_list.sh
bash tests/methods/get_file/test_get_file.sh
bash tests/methods/get_dir/test_get_dir.sh
```

While redirects are unfinished, prefix the directory component command with
SKIP_GET_REDIRECT_TESTS=1 as well.

For all helper/component tests plus the GET HTTP suite together:

```sh
bash tests/methods/run_tests.sh
```

The combined runner is an optional diagnostic command. It is not needed when
you only want to check GET through the server. The same temporary redirect flag
can be applied to it during development.
