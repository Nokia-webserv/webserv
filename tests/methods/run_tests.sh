#!/usr/bin/env bash
set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
failed=0

# Only GET-specific suites. General CI build/startup/config/parser/TCP tests
# already have their own runners and must not be called again here.
for suite in url_encode_path escape_html generate_files_list get_file get_dir get_integration; do
    echo
    echo "=== $suite ==="
    if ! bash "$SCRIPT_DIR/$suite/test_$suite.sh"; then
        failed=$((failed + 1))
    fi
done

if [ "$failed" -ne 0 ]; then
    printf '\033[31m[FAIL]\033[0m %s GET suites failed\n' "$failed"
    exit 1
fi
printf '\033[32m[PASS]\033[0m All GET suites passed (see explicit SKIP messages above)\n'
