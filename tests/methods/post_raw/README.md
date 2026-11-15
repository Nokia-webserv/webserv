# Raw POST upload component tests

From the repository root:

```sh
bash tests/methods/test_post_raw.sh
```

Or use make -C tests/methods/post_raw. Requires Python 3 and a C++ compiler.
The runner finds the project root, creates temporary fixtures and a temporary
executable, and deletes them on exit. No listener socket or server main is run.
This is separate from CI's HTTP-parser/config/general integration tests.

## Build gate

The runner first checks the real Router.cpp and utils.cpp with C++98 and
-Wall -Wextra -Werror. It does not fix their declarations, TODO calls or syntax.
A production or component build error prints BLOCKED and returns exit code 2;
zero runtime cases have run in that situation. Failed runtime assertions return
1; a completed passing run returns 0. There is a 30-second runtime watchdog.

The current production draft has not reached the runtime gate yet. The test
translation units themselves have been syntax-checked; this is not a claim
that the upload scenarios have passed.

## Scope: 45 scenarios

| Group | Cases | What is checked |
| --- | ---: | --- |
| Raw body bytes | 10 | Empty, 1/999/1000/1001/4097/1048576-byte bodies, all byte values including NUL, UTF-8, boundary-looking text treated as raw data |
| MIME and headers | 11 | txt/json/png, JPEG and HTML aliases, media-type parameters/case, normalized header names, missing/unknown/generic type |
| Destination and request | 5 | Trailing slash, spaces, relative upload_store, generated name independent of request path/query, HTTP/1.0 response framing |
| Unique names/no overwrite | 5 | Existing file, sequence-number gap, existing directory, symlink collision and target protection, four successive uploads |
| Filesystem/routing errors | 5 | Empty store 403, missing store 404, file-as-store 404, unwritable store 403, forbidden POST 405 with Allow |
| Short writes | 3 | Real writes limited to 1, 7 or 1024 bytes; all bytes must eventually be written |
| Controlled I/O failures | 6 | Unexpected open failure, exhausted collisions, write -1, write zero, failure after a written prefix, close failure |

Each case reports its group, input size/type, expected HTTP status, actual
response status/body length, generated filename, saved byte count and I/O call
counts. PASS is green, FAIL red, SKIP yellow. Root explicitly skips the mode-0500
directory permission test because it bypasses ordinary permissions.

## Contract being tested

- A raw upload creates exactly one regular file in upload_store, using the
  sequential upload_N name from the draft, with a dot before a known extension.
- Unknown/missing types use no invented extension. MIME parameters are ignored
  for extension lookup and media-type case is normalized. Either conventional
  HTML/JPEG alias is accepted. These are explicit expectations for this upload
  design, not requirements about arbitrary filenames imposed by the subject.
- The generated filename is independent of the request path and query.
- The saved bytes equal the full request body, including embedded zero bytes.
- O_CREAT and O_EXCL prevent truncation/overwrite. Collisions retry another name.
- Success returns 201 Created with the request version and a correct response
  Content-Length. The acknowledgment body may be empty or nonempty; its wording,
  Content-Type and an optional Location header are not prescribed here.
- Errors return the appropriate ROUTE_ERROR decision and serialized response.
  Missing paths map to 404 and access denial to 403, matching the existing file
  helper policy. Injected write/close/I/O failures map to generic 500.
- The upload descriptor must be closed on success and failure. Files that existed
  before the upload, including a colliding symlink's target, remain unchanged.

## How the component is isolated

Tests call the public Router::route with real HttpRequest/HttpResponse and
configuration objects. A Listener test double only supplies server configs;
it never opens a socket. The real request setter is used, so header names are
normalized exactly as in production.

Router.cpp is included unchanged in a temporary translation unit. Only its
open/write/close call names are redirected to the test wrappers. Normally they
perform real filesystem operations. Fault cases deliberately return short writes
or errors; close-error injection still closes the real descriptor. This tests
the real handler's control flow, not a copied or replacement upload algorithm.
The production syntax check runs before wrappers can supply incidental includes.

Collision exhaustion is simulated without creating 100000 files. Test wrappers
stop unbounded loops after 200000 calls. Test teardown closes any leaked upload
descriptor after recording its failure, so one failure cannot poison later cases.

## Not included

Multipart and its dispatch, HTTP parsing/framing, chunk decoding, configured body
limits, sockets, server response enqueueing, connection closure, CGI and async
integration are outside this component suite. Only non-multipart bodies are used;
boundary-looking text is sent without multipart Content-Type. HTTP/1.0 checks
only response serialization, not TCP closure.

The suite does not currently enforce deletion of a partially saved file after
write failure. That policy needs a separate implementation decision. Permission
bits and acknowledgment text are not fixed by the tests. No production files,
workflow, Git index or branch history are modified by the runner.
