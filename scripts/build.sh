#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kevin Sint
# Build the firmware and pack it with the update loader: build/sint_fm1_dr1.fwsc.
#   scripts/build.sh
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PY="$ROOT/.venv/bin/python"
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}" AC79_SDK="${AC79_SDK:-$HOME/.jieli/ac79-sdk}"
# JieLi's toolchain is Linux x86-64 only, so the compile step runs in a linux/amd64 container
docker build -q --platform linux/amd64 -t fm1-toolchain "$ROOT/docker" >/dev/null
docker run --rm --platform linux/amd64 -v "$ROOT:/work" -v "$(cd "$JIELI_TOOLCHAIN" && pwd -P):/opt/jieli:ro" \
    fm1-toolchain sh /work/scripts/compile.sh
L="$ROOT/build/loader"
A="$ROOT/build/app"
"$PY" "$ROOT/tools/fm1img.py" loader "$L/loader.bin" "$L/sections.txt" "$L/ota.bin"
"$PY" "$ROOT/tools/fm1img.py" app "$A/text.bin" "$A/data.bin" "$A/ramtext.bin" "$A/symbols.txt" "$A/app.bin"
# the package's identity is the app's (app/app_id.h): the installer compares the two after the install
ID="$(sed -n 's/^#define APP_PACKAGE_ID "\(.*\)"$/\1/p' "$ROOT/app/app_id.h")"
"$PY" "$ROOT/bsp/tools/fm1pkg.py" "$A/app.bin" "$L/ota.bin" "$ROOT/build/sint_fm1_dr1.fwsc" --product "$ID"
