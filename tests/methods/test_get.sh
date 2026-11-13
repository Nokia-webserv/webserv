#!/usr/bin/env bash
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# The public GET check: build the real server and test HTTP requests/responses.
exec bash "$SCRIPT_DIR/get_integration/test_get_integration.sh"
