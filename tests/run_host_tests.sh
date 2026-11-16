#!/bin/sh
# The tests that run on this computer (no FM-1, no toolchain): tests/run_host_tests.sh
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/build/host"
for name in font_test params_test keys_test panel_test drone_test; do
    cc -std=c99 -Wall -Wextra -Werror -Wno-unused-function -I"$ROOT/app" "$ROOT/tests/$name.c" -o "$ROOT/build/host/$name" -lm
    "$ROOT/build/host/$name" | tail -1
done
