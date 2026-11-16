#!/bin/sh
# Build felucca-bsp's hwtest and pack it with the update loader: build/hwtest.fwsc.
# felucca-bsp has the loader's source but no build of it; fm1-base's build of its template (no radios) makes it,
# third_party/fm1-base/template/build/loader/ota.bin, the same for every app on the same felucca-bsp commit.
#   scripts/build_hwtest.sh
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PY="$ROOT/.venv/bin/python"
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}" AC79_SDK="${AC79_SDK:-$HOME/.jieli/ac79-sdk}"
# Pillow's raqm loads fribidi when it runs, and Homebrew's lib folder is not on macOS's library search path
if [ -d /opt/homebrew/lib ]; then export DYLD_FALLBACK_LIBRARY_PATH=/opt/homebrew/lib; fi
(cd "$ROOT/third_party/fm1-base" && "$PY" tools/build.py template --no-ble)
# hwtest's build.sh runs the toolchain itself, so it runs where the toolchain does: a linux/amd64 container
docker build -q --platform linux/amd64 -t fm1-toolchain "$ROOT/docker" >/dev/null
docker run --rm --platform linux/amd64 -v "$ROOT/bsp:/work" -v "$(cd "$JIELI_TOOLCHAIN" && pwd -P):/opt/jieli:ro" \
    -e JIELI_TOOLCHAIN=/opt/jieli fm1-toolchain sh /work/examples/hwtest/build.sh
mkdir -p "$ROOT/build"
"$PY" "$ROOT/bsp/tools/fm1pkg.py" "$ROOT/bsp/examples/hwtest/build/hwtest.bin" \
    "$ROOT/third_party/fm1-base/template/build/loader/ota.bin" "$ROOT/build/hwtest.fwsc" --product FM-1_1200
