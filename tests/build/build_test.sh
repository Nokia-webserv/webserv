#!/bin/bash

set -e

echo "=== BUILD TEST ==="

SPECIAL_FLAGS="${1:-}"

make fclean
make SPECIAL_FLAGS="$SPECIAL_FLAGS"

echo "BUILD: PASS"