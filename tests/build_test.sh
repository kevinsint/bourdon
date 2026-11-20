#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kevin Sint
# The build's results, checked: tests/build_test.sh
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
"$ROOT/scripts/build.sh"
sha() { shasum -a 256 "$1" | cut -d' ' -f1; }
fail() { echo "FAIL: $1" >&2; exit 1; }
# the update loader: byte for byte what fm1-base c8e0599 built from felucca-bsp 18eadad.
# A new felucca-bsp commit may change it: then check the new loader on an FM-1 and pin its hash here
[ "$(sha "$ROOT/build/loader/ota.bin")" = 127dd8e5f944a19f52a68ee04a9658edeb65031f1617a42fad4ee54998787f87 ] ||
    fail "ota.bin is not the pinned update loader"
grep -qa FELUCCA-LOADER-1 "$ROOT/build/loader/ota.bin" || fail "ota.bin has no loader marker"
# every package is the same size: head + app slot + loader + SDK files
[ "$(wc -c < "$ROOT/build/sint_fm1_dr1.fwsc" | tr -d ' ')" = 616003 ] || fail "package size"
# the identity the installer checks after the install, and the USB name
grep -qa FM-1_2000 "$ROOT/build/app/app.bin" || fail "app.bin has no identity"
grep -qa SINT_FM1_DR1 "$ROOT/build/app/app.bin" || fail "app.bin has no name"
echo "build_test: ok"
