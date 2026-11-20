# Directory listing component tests

From the repository root:

    bash tests/methods/generate_files_list/test_generate_files_list.sh

Or use make -C tests/methods/generate_files_list.
Requires Python 3 and a C++ compiler.

The runner compiles the current generateFilesList, escapeHtml and urlEncodePath
method definitions from source/Router.cpp together in an isolated component.
Only the class qualifier changes. There are no replacement escaping algorithms
or fake filesystem calls. The complete Router/server is not linked, so these
tests do not check production headers, routing, HTTP responses or socket delivery.

Tests create real files and directories in a temporary folder. Both fixtures
and the executable are removed on exit. Each case prints Before, After, Expected
and PASS/FAIL. Expected links and labels are fixed test data, never calculated
using the implementation under test.

The 22 cases cover empty root/nested directories, parent links, URL prefixes,
spaces, HTML-sensitive characters, quotes, literal percent sequences, plus,
query/fragment characters, entity-looking names, HTML injection text, UTF-8,
backslash, hidden files, subdirectories and multiple entries. Full HTML entries
are compared without assuming readdir order; missing, duplicate or extra entries,
including dot entries, fail the test.

Error cases check missing directories, a regular file passed as a directory,
a file used as an intermediate path component and permission denial.
The permission test is skipped explicitly when running as root.
Injected readdir/closedir failures and allocation failures are not covered.

The requestPath input is a decoded directory URL ending in a slash; serveDir
handles the missing-slash redirect before calling this component.
Subdirectory links currently have no appended slash; following them relies
on that redirect. These tests do not require sorting, metadata or a specific
visual style beyond the current HTML structure.
