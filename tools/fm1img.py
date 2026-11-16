#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The two image formats the FM-1's boot code reads, made from the linker's output.
Taken from fm1-base tools/build.py (the update loader's wrapper and its checks) and felucca-bsp
examples/hwtest/build.sh (the app image); this file and lz4blk.py stay MPL-2.0.

  fm1img.py loader LOADER.bin SECTIONS.txt OTA.bin    (SECTIONS.txt: objdump -h of the loader)
  fm1img.py app TEXT.bin DATA.bin RAMTEXT.bin SYMBOLS.txt APP.bin    (SYMBOLS.txt: objdump -t of the app)
"""
import re
import struct
import sys
from pathlib import Path

import lz4blk

APP_XIP = 0x02000120                # app.bin offset 0 in the XIP map; the SPL jumps here
LOADER_LOAD = 0x01C0A800
LOADER_NAME = b"usb_hid_ota.bin"    # the file name the SPL looks for


def crc16(d, c=0):
    for b in d:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) if c & 0x8000 else c << 1
        c &= 0xFFFF
    return c


def ldr_head(dcrc, a, b, attr, name):
    body = struct.pack("<HIIBBH16s", dcrc, a, b, attr, 0, 0, name)
    return struct.pack("<H", crc16(body)) + body


def ldr_wrap(image):
    """ota.bin = outer JLFS head (32 B) + inner head (32 B, load address) +
    blocks [clen u32][dlen u32][LZ4 block], dlen 4096 except the last"""
    blocks = bytearray()
    for i in range(0, len(image), 4096):
        raw = image[i:i + 4096]
        c = lz4blk.compress(raw)
        if len(c) > 4096 or lz4blk.decompress(c) != raw:
            raise SystemExit(f"loader: block {i // 4096} does not compress below 4096 B")
        blocks += struct.pack("<II", len(c), len(raw)) + c
    inner = ldr_head(crc16(image), len(image), LOADER_LOAD, 0, LOADER_NAME)
    body = inner + bytes(blocks)
    return ldr_head(crc16(body), 0x20, len(body), 0x41, LOADER_NAME) + body


def loader(image_path, sections_path, ota_path):
    for ln in Path(sections_path).read_text().splitlines():     # the loader rewrites the flash: nothing may run from XIP
        p = ln.split()
        if len(p) > 4 and p[1].startswith(".") and int(p[3], 16) >= 0x02000000 and int(p[2], 16):
            raise SystemExit(f"loader: section {p[1]} at {p[3]} is outside RAM")
    image = Path(image_path).read_bytes()
    if not image or len(image) > 0x14000:
        raise SystemExit("loader: image empty or too big")
    ota = ldr_wrap(image)
    Path(ota_path).write_bytes(ota)
    print(f"loader: image {len(image)} B at {LOADER_LOAD:#x}, ota.bin {len(ota)} B")


def app(text_path, data_path, ramtext_path, symbols_path, app_path):
    syms = Path(symbols_path).read_text()

    def sym(name):
        return int(re.search(rf"^([0-9a-f]+) .*\s{name}$", syms, re.M).group(1), 16)
    if sym("_start") != APP_XIP:
        raise SystemExit(f"app: _start is at {sym('_start'):#x}, not {APP_XIP:#x}")
    img = bytearray(Path(text_path).read_bytes())
    # .data and the code that runs from RAM (the flash driver) follow .text; the app copies them to RAM at start
    for at, part in sorted([(sym("_rt_load"), Path(ramtext_path).read_bytes()), (sym("_data_load"), Path(data_path).read_bytes())]):
        if part:
            if at - APP_XIP < len(img):
                raise SystemExit("app: sections overlap")
            img += b"\xff" * (at - APP_XIP - len(img)) + part
    img += b"\xff" * (-len(img) % 4)
    Path(app_path).write_bytes(img)
    print(f"app: {len(img)} B")


if __name__ == "__main__":
    kinds = {"loader": (loader, 3), "app": (app, 5)}
    if len(sys.argv) < 2 or sys.argv[1] not in kinds or len(sys.argv) - 2 != kinds[sys.argv[1]][1]:
        raise SystemExit(__doc__)
    kinds[sys.argv[1]][0](*sys.argv[2:])
