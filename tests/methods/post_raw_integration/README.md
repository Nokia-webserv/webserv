# Raw POST through the real server

From the repository root:

```sh
bash tests/methods/test_post_raw.sh --integration
```

This is the main command for checking that raw upload is connected to the server:

```text
curl POST -> parser -> router -> handlePost -> upload_store
                                      -> response -> send buffer -> curl
```

The script builds all current source/*.cpp files, including the real main,
without test doubles, production patches or special test macros. It starts that
server with a temporary configuration on a free 127.0.0.1 port. All HTTP requests
are sent by curl. Python prepares files and checks the responses and disk bytes.
Requires a C++ compiler, Python 3, curl and permission to use local TCP sockets.

## 41 HTTP scenarios

| Group | Cases | Checks |
| --- | ---: | --- |
| B Raw body delivery | 10 | 0, 1, 999, 1000, 1001, 1999, 2000, 2001 and 1048576 bytes under a configured 2 MiB server limit; NUL and UTF-8 |
| M MIME and headers | 8 | Text, JSON, PNG, JPEG aliases, case/parameter/space normalization, unknown type, URL-encoded body stored as raw bytes |
| P Configuration/destination | 5 | Spaces, trailing slash, relative upload_store, request path/query and Content-Disposition do not choose the generated filename |
| N Collisions | 3 | Existing regular file, directory and symlink remain unchanged; another filename is used |
| E Error responses | 6 | No store 403, missing store 404, file-as-store 404, unwritable store 403, symlink loop 500, forbidden POST 405/Allow |
| S Configured body limits | 2 | A 64-byte location limit accepts 64 bytes and rejects 65 with 413 without creating an upload |
| C Full server delivery | 6 | Three POSTs on one connection, POST after 403 on that connection, four concurrent clients, chunked input, slow upload, POST followed by GET of saved file |
| V HTTP version | 1 | HTTP/1.0 status/version, response length and saved bytes |

Every successful upload must create exactly the expected number of regular files
with the correct extension and exact request-body bytes. Multiple-upload checks
compare complete payloads, not just lengths. Existing collision targets and the
document-root readiness file must remain unchanged. File checks print their name,
byte count and SHA256. Responses must have the right version, status, reason,
CRLF framing and Content-Length matching the bytes received by curl. The suite
does not prescribe the acknowledgment text or require echoing the uploaded file.

The sequential cases use one curl process with --next and require connection
metrics [1, 0, 0]. This checks actual connection reuse, not three new connections.
The round-trip case reads the uploaded file using the real GET handler as well.
The slow upload spans multiple receive events; injected short writes and write
failures remain in the component suite rather than being faked in this server.
Concurrency, chunked and slow-upload payloads stay below 2000 bytes so these
checks isolate delivery behavior; the raw-body group separately tests larger
payloads against the configured server limit.

Output is grouped with stable IDs, expected results, Before/Received/Saved data,
green PASS, red FAIL and a final totals table. Root skips the permission case
explicitly because it bypasses ordinary directory permissions. Failed checks
return nonzero. Fixtures, server logs, curl payloads and executables are temporary;
the server is terminated and files are removed on exit. The runner can be invoked
by absolute path from another working directory.

## Other layers

```sh
# Only getFileFormat, linked with the real MIME/normalization helpers: 24 cases
bash tests/methods/test_post_raw.sh --unit

# Router component, with controlled short-write/I/O failures: 45 cases
bash tests/methods/test_post_raw.sh --component

# All three layers, continuing to the next layer if one fails
bash tests/methods/test_post_raw.sh --all
```

Calling test_post_raw.sh without an option still runs the original component
suite. The unit/component checks intentionally overlap some MIME/body assertions
to help locate failures; only integration proves the real network path works.
The GitHub workflow runs the raw POST integration command on push and
pull_request, after the existing GET integration step. Unit/component commands
are not added to CI. Body-limit failures are not skipped or allowed to pass.

## Limits

Multipart dispatch/parsing, CGI, malformed HTTP,
Expect: 100-continue, disk exhaustion, arbitrary filesystem races and cleanup of
partial files after write failure are not covered here. HTTP/1.0 checks framing,
not TCP closure; HTTP/1.1 connection-close policy is also not checked. Generated
filename extensions are this project's design, not a subject requirement that
arbitrary uploads have particular filenames. Passing these tests does not claim
the entire POST feature or server integration is complete for other body types.

## Configured body limits

GH-104 / PR #124 replaces the temporary 2000-byte parser limit with configured
limits and enforces the selected server/location limit before dispatching POST.
The suite requires 2001 bytes and 1 MiB to fit the configured 2 MiB server limit,
and rejects 65 bytes under a 64-byte location limit with 413. These checks run
through the real parser and router without overriding limits in production code.
