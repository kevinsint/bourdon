#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kevin Filteau
# Build felucca-bsp's hwtest (the hardware test) and pack it with our update loader: build/hwtest.fwsc.
#   scripts/build_hwtest.sh
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}" AC79_SDK="${AC79_SDK:-$HOME/.jieli/ac79-sdk}"
"$ROOT/scripts/build.sh"                        # the update loader: build/loader/ota.bin
# hwtest's build.sh runs the toolchain itself, so it runs where the toolchain does
docker run --rm --platform linux/amd64 -v "$ROOT/bsp:/work" -v "$(cd "$JIELI_TOOLCHAIN" && pwd -P):/opt/jieli:ro" \
    -e JIELI_TOOLCHAIN=/opt/jieli fm1-toolchain sh /work/examples/hwtest/build.sh
"$ROOT/.venv/bin/python" "$ROOT/bsp/tools/fm1pkg.py" "$ROOT/bsp/examples/hwtest/build/hwtest.bin" \
    "$ROOT/build/loader/ota.bin" "$ROOT/build/hwtest.fwsc" --product FM-1_1200
