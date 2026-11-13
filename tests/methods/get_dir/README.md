# GET directory component tests

From the repository root:

```sh
bash tests/methods/get_dir/test_get_dir.sh
```

Or run `make -C tests/methods/get_dir`. The script finds the project root
regardless of the caller's working directory. It uses the project's C++98
warning flags and stops on compilation errors.

While handleRedirect is not integrated, explicitly exclude its two cases with:

```sh
SKIP_GET_REDIRECT_TESTS=1 bash tests/methods/get_dir/test_get_dir.sh
```

They are reported as SKIP. The default command still checks redirects.

The real Router, index-file handling, directory listing, error responses and
serialization are tested through `Router::route()`. A test-only Listener supplies
configuration without opening sockets. No server or poll loop is started.
Fixtures and the binary are temporary and removed on exit.

The 16 cases cover configured/missing/empty/nested indexes, index priority,
autoindex on/off, listing links and parent entries, empty directories, permission
errors, trailing-slash redirects and query preservation. Permission cases are
explicitly skipped when running as root. Listing order is not assumed.

URI normalization (#107), HTML escaping and URL encoding (#115), configured
`return` redirects, CGI and network delivery are outside this component suite.
The tests do not inject readdir/closedir failures.
