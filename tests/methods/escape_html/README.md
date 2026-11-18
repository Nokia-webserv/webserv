# Isolated HTML escaping tests

From the repository root:

```sh
bash tests/methods/escape_html/test_escape_html.sh
```

Or use `make -C tests/methods/escape_html`.
Requires Python 3 and a C++ compiler. The script works from any directory.

Each case prints PASS/FAIL, Before, After and Expected. Quotes delimit displayed
strings; control and non-ASCII bytes are shown as escapes for readable output.

The 20 cases check the helper's five replacements: `&` becomes `&amp;`,
`<` becomes `&lt;`, `>` becomes `&gt;`, `"` becomes `&quot;`, and `'` becomes
`&#39;`. Other bytes stay unchanged. Cases include repeated symbols, filenames,
HTML tags, quotes in attributes, entity-looking text, URL punctuation, UTF-8,
embedded zero bytes, control characters and long input. Input must remain unchanged.

Input is raw text: an existing literal `&amp;` must become `&amp;amp;`.
HTML escaping does not perform URL encoding or remove control bytes.

The runner extracts the current `Router::escapeHtml` definition from
`source/Router.cpp` and changes only its qualified name for isolated compilation.
It uses C++98 with `-Wall -Wextra -Werror`. Compilation errors stop the run.
Generated code and the executable are temporary and removed on exit.

These tests cover only the helper, not its Router.hpp declaration, production
includes, use in directory listings, or the complete project build.
