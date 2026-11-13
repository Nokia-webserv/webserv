# GET file component tests

From the repository root:

```sh
bash tests/methods/get_file/test_get_file.sh
```

Or run `make -C tests/methods/get_file`. The script also works when invoked
by absolute path from another directory. It compiles the current source files
with the project's C++98 warning flags. Build failures stop the run and are
reported as failures, not passing tests.

The tests call the real public `Router::route()` and check the resulting
`HttpResponse::serialize()`. A test-only `Listener` supplies configuration
without opening sockets. No server, HTTP parser, poll loop or network is used.
Fixtures and the executable are created in the system temporary directory and
removed when the script exits; no generated binary is left in the repository.

Coverage: text and UTF-8, empty and binary files, reads crossing a buffer
boundary, nested paths, root/trailing-slash combinations, MIME types,
HTTP versions and a separate query string. Missing files and invalid path
components check the 404 routing decision; an unreadable file checks 403.
The permission case is explicitly skipped when run as root.

This suite checks error routing decisions; the directory component suite also
checks generated error responses after the GH71 integration. Directories,
autoindex and URI normalization (#107) are outside this suite. These tests do
not simulate disk read failures or verify descriptor cleanup on exceptions.
