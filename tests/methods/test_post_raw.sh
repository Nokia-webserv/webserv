#!/usr/bin/env bash
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
case "${1:---component}" in
    --component) exec bash "$SCRIPT_DIR/post_raw/test_post_raw.sh" ;;
    --unit) exec bash "$SCRIPT_DIR/post_raw_unit/test_post_raw_unit.sh" ;;
    --integration) exec bash "$SCRIPT_DIR/post_raw_integration/test_post_raw_integration.sh" ;;
    --all)
        failed=0
        for runner in post_raw_unit/test_post_raw_unit.sh post_raw/test_post_raw.sh \
            post_raw_integration/test_post_raw_integration.sh; do
            if bash "$SCRIPT_DIR/$runner"; then
                :
            else
                failed=1
            fi
        done
        exit "$failed"
        ;;
    *) echo "Usage: bash tests/methods/test_post_raw.sh [--unit|--component|--integration|--all]"; exit 2 ;;
esac
