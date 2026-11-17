# Isolated URL path encoding tests

From the repository root:

```sh
bash tests/methods/url_encode_path/test_url_encode_path.sh
```

Or use `make -C tests/methods/url_encode_path`. Both commands use C++98 and
the project's warning flags. The script also works by absolute path from
another directory. It requires Python 3 and a C++ compiler.

The runner extracts the current `Router::urlEncodePath` definition from
`source/Router.cpp` and compiles that exact body under a standalone test name.
Only the qualified name is changed. The encoding implementation is not copied
into the tests. Generated code and the binary are temporary and removed on exit.
Missing or malformed definitions cause a failing test run.

This deliberately tests only the pure encoding function. It does not validate
the Router.hpp declaration, production includes, autoindex HTML, redirects or
the complete project build. This allows testing while those pieces are unfinished.

The contract is raw path bytes in, percent-encoded path out: preserve ASCII
letters/digits, `-._~` and path separators `/`; encode other bytes as uppercase
`%HH`. These expectations use [RFC 3986 sections 2.1 and 2.3](https://www.rfc-editor.org/rfc/rfc3986.html#section-2.1).
The conservative encoding of other punctuation is a choice of this helper.

The 22 cases include spaces, literal plus/percent, query/fragment characters,
HTML-sensitive characters, controls, high bytes and UTF-8. A literal `%20` in
the input becomes `%2520`: input is raw text, not an already encoded URL.
Encoding does not normalize `.` or `..`; URI validation is separate.
