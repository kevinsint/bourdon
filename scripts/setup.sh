#!/bin/sh
# One-time setup: JieLi's toolchain and AC79 SDK (both checked by hash) and the Python venv.
#   scripts/setup.sh
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
JL="$HOME/.jieli"
TC_NAME=jieli-linux-toolchains-20260730.1
TC_SHA=a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd
# fm1-base's get_toolchain.sh pipes JieLi's download into tar unchecked; this pins the file JieLi served on 2026-11-15
if [ ! -d "$JL/$TC_NAME" ]; then
    mkdir -p "$JL/download"
    curl -fL --retry 3 -o "$JL/download/$TC_NAME.tar.xz" "https://jl-update.oss-cn-shenzhen.aliyuncs.com/$TC_NAME.tar.xz"
    echo "$TC_SHA  $JL/download/$TC_NAME.tar.xz" | shasum -a 256 -c - >/dev/null ||
        { echo "setup: the toolchain is not the file this repository pins" >&2; exit 1; }
    tar -xJf "$JL/download/$TC_NAME.tar.xz" -C "$JL"
fi
ln -sfn "$TC_NAME" "$JL/toolchain"
# JieLi took the SDK off GitHub; Gitee has the same revision, and get_sdk.sh checks its files by hash
SDK_URL=https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git "$ROOT/bsp/tools/get_sdk.sh" "$JL/ac79-sdk"
# Python 3.12: python-rtmidi 1.5.8 (the installer's MIDI) has prebuilt wheels only up to 3.12
PY312="$(command -v python3.12 || uv python find 3.12 2>/dev/null)" ||
    { echo "setup: needs Python 3.12 (uv python install 3.12)" >&2; exit 1; }
"$ROOT/.venv/bin/python" -c 'import sys; assert sys.version_info[:2] == (3, 12)' 2>/dev/null ||
    "$PY312" -m venv --clear "$ROOT/.venv"
"$ROOT/.venv/bin/pip" install -q --disable-pip-version-check -r "$ROOT/requirements.txt"
# fm1-base draws its fonts with Pillow's raqm, which loads fribidi when it runs (macOS: brew install fribidi)
if [ -d /opt/homebrew/lib ]; then export DYLD_FALLBACK_LIBRARY_PATH=/opt/homebrew/lib; fi
"$ROOT/.venv/bin/python" -c "from PIL import features; assert features.check('raqm')" 2>/dev/null ||
    { echo "setup: Pillow has no raqm: install fribidi" >&2; exit 1; }
echo "setup: done"
