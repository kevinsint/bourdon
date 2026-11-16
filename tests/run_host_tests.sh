#!/bin/sh
# The tests that run on this computer (no FM-1, no toolchain): tests/run_host_tests.sh
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/build/host"
cc -std=c99 -Wall -Wextra -Werror -I"$ROOT/app" "$ROOT/tests/font_test.c" -o "$ROOT/build/host/font_test"
"$ROOT/build/host/font_test"
