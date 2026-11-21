# getFileFormat unit tests

From the repository root:

```sh
bash tests/methods/test_post_raw.sh --unit
```

24 cases check known extensions, JPEG/HTML aliases, case normalization,
parameters (including quoted values), leading/trailing spaces and tabs, empty
values, unknown types and nonmatching prefixes. Each case prints Before, After,
Expected and a colored PASS/FAIL. The input must remain unchanged.

The runner extracts the current pure `Router::getFileFormat` definition from
Router.cpp, changing only its qualified name for a standalone test. It links the
actual utils.cpp, so getMime, trimStr and strTolower are not reimplemented or
mocked. This isolates the string helper from unrelated private Router methods.
The component and integration suites additionally compile the real Router class
and exercise the method through the public route entry point.

The temporary C++98 executable and generated header are deleted on exit. No
production files or Git state are changed. This is not a MIME syntax validator;
the current helper's role is choosing a filename extension.
