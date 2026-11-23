#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kevin Sint
# The screens of one released version, for the website's page: build/web_screens/<tag>/*.png, 480 x 480.
#   scripts/web_screens.sh v0.27
# They are drawn by that tag's own code (tests/view_test.c), not by the working tree: the page shows what the
# release shows. The site's script (kevinsint/site scripts/bourdon-update.sh) copies them from that directory.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TAG="$1"
[ -n "$TAG" ] || { echo "usage: scripts/web_screens.sh <tag>" >&2; exit 2; }
git -C "$ROOT" rev-parse -q --verify "refs/tags/$TAG^{commit}" >/dev/null || { echo "FAIL: no tag $TAG" >&2; exit 1; }
OUT="$ROOT/build/web_screens/$TAG"
WORK="$(mktemp -d)"
trap 'git -C "$ROOT" worktree remove --force "$WORK/tree" 2>/dev/null; rm -rf "$WORK"' EXIT
git -C "$ROOT" worktree add -q --detach "$WORK/tree" "refs/tags/$TAG"
mkdir -p "$WORK/ppm"
cc -std=c99 -Wno-unused-function -I"$WORK/tree/app" "$WORK/tree/tests/view_test.c" -o "$WORK/view_test" -lm
"$WORK/view_test" "$WORK/ppm" | tail -1
rm -rf "$OUT"
mkdir -p "$OUT"
# each .ppm (240 x 240) as a .png, every pixel two times each way
python3 - "$WORK/ppm" "$OUT" <<'PY'
import glob, os, struct, sys, zlib
SCALE = 2
def chunk(kind, body):
    return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))
names = sorted(glob.glob(os.path.join(sys.argv[1], "*.ppm")))
if not names:
    sys.exit("FAIL: view_test wrote no screen")
for name in names:
    magic, width, height, top, pixels = open(name, "rb").read().split(None, 4)
    width, height = int(width), int(height)
    if magic != b"P6" or top != b"255" or len(pixels) != width * height * 3:
        sys.exit("FAIL: %s is not a plain 8-bit .ppm" % name)
    rows = b""
    for y in range(height):
        row = b"".join(pixels[(y * width + x) * 3:(y * width + x) * 3 + 3] * SCALE for x in range(width))
        rows += (b"\0" + row) * SCALE
    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width * SCALE, height * SCALE, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))
    open(os.path.join(sys.argv[2], os.path.basename(name)[:-4] + ".png"), "wb").write(png)
print("%d screens in %s" % (len(names), sys.argv[2]))
PY
