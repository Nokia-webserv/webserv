# Method tests

## All POST tests

```sh
bash tests/methods/test_post.sh
```

Runs all seven POST suites: raw-upload unit/component/curl integration, multipart
boundary/splitting/part parsing, and the complete multipart saving component.
Each suite runs once. A failure does not prevent the remaining suites from
running; the final command fails if any suite fails. Requires curl, Python 3,
a C++ compiler and local TCP sockets. This does not run GET or general CI suites,
and does not claim multipart network integration (that suite is component-only).
The combined runner is not added to CI.

## Complete multipart component

```sh
bash tests/methods/test_multipart.sh
```

36 cases exercise Router::route -> handlePost -> handleMultipart -> handlePart
-> saveFile, with real temporary files and exact byte comparisons. They check
multiple files, ordinary fields, binary/empty/1 MiB bodies, extensions, boundary
variants, closing delimiters, filename collisions, filesystem errors, short
writes and injected I/O failures. Responses and RouteDecision must agree;
errors must not be replaced with 201. The test checks descriptor cleanup and
that later parts are not processed after an error. Permission checks skip root.

The actual Router.cpp is included unchanged in a temporary translation unit.
The existing raw-upload open/write/close test wrappers normally perform real
I/O and inject failures only in the dedicated cases. A Listener test double
supplies configuration without sockets. Temporary files/binaries are removed
afterward. No production code, CI or Git state is changed by the runner.
This is component coverage, not HTTP parsing/network integration. The previously
excluded xfilename/X-Content-Disposition/X-Content-Type variants remain outside
the suite. Cleanup/rollback of earlier files after a later error is unspecified;
the tests allow those earlier files to remain or be removed. Partial write
errors may leave a prefix of the failed file.

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

26 component cases call the real handleMultipart and compare its extracted parts
byte-for-byte, including order, empty file content, NUL and CRLF. They also check
missing/truncated delimiters and 400 responses. In a temporary source copy,
the two handlePart calls are replaced with a test capture function that records
their exact part arguments; the splitting algorithm is unchanged. Debug output
is not required. A temporary header grants private-method access. No server,
handlePart processing, file saving, success-response checks, Git changes or CI
changes are involved. Preamble handling, transport
padding and validation of individual part headers are outside this small suite.
For malformed input after earlier complete parts, the suite requires 400 but
does not prescribe rollback of earlier parts. Ending checks include no final
CRLF, CRLF plus epilogue, immediate junk, a single junk byte, lone CR/LF,
CR followed by junk, a NUL byte and different boundary lengths. Output shows the
escaped body tail and separate totals for valid and malformed bodies. A passing
negative test alone does not prove valid multipart bodies are accepted.
The boundary-only runner stops before splitting and does not test endings.
Closing-line checks follow
[RFC 2046, section 5.1.1](https://www.rfc-editor.org/rfc/rfc2046.html#section-5.1.1).

## One multipart part before saving

```sh
python3 tests/methods/test_multipart_part.py
```

15 component checks call handlePart directly with one complete part. They check
the header/body split, file versus ordinary field, MIME/extension selection,
empty and binary bodies, 400 errors and filename text outside Content-Disposition.
The test checks the exact extension and body bytes passed to saveFile, including
the number of calls. It does not depend on debug output.
The temporary header grants private-method access; in a temporary source copy,
the saveFile call is replaced with a capture function. No handleMultipart loop, boundary parsing, file saving,
network, final success response, workflow or Git operations are involved.
This is not a full MIME-header validation
suite; xfilename/X-Content-Disposition/X-Content-Type name distinctions, filename
sanitization, empty-filename policy, duplicate headers, quoted parameter parsing
and storage behavior are outside its scope.

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

## Directory redirects

All three directory-redirect checks are mandatory parts of the main command.
The former SKIP_GET_REDIRECT_TESTS development flag no longer skips them.
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
HTTP parser, general integration and timeout suites, followed by GET, raw POST
and configured-redirect integration steps. GET directory redirects are enabled.
These run automatically on push and pull_request. The GET and POST runners each
build their own temporary server.

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

For all helper/component tests plus the GET HTTP suite together:

```sh
bash tests/methods/run_tests.sh
```

The combined runner is an optional diagnostic command. It is not needed when
you only want to check GET through the server. Directory redirects are included
in both the directory component and HTTP integration checks.
