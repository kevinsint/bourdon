#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kevin Sint
# Compile the update loader and the app. scripts/build.sh runs this in the toolchain's container:
# the repository at /work, JieLi's toolchain at /opt/jieli.
set -e
cd /work
TC=/opt/jieli
CC="$TC/pi32v2/bin/clang -target pi32v2"
CFLAGS="-Os -ffunction-sections -fno-builtin -Wall -Wno-unused-function -Ibsp/chip -Ibsp/boards/fm1"
L=build/loader
A=build/app
mkdir -p $L $A

# the update loader: felucca-bsp's, unchanged. It runs from RAM, so it is one image (.text)
$CC -c bsp/loader/crt0_ldr.S -o $L/crt0_ldr.o
$CC $CFLAGS -c bsp/loader/loader.c -o $L/loader.o
$TC/pi32v2/bin/ld --gc-sections -e _start -T bsp/loader/loader.ld $L/crt0_ldr.o $L/loader.o -o $L/loader.elf
$TC/common/bin/objcopy -O binary -j .text $L/loader.elf $L/loader.bin
$TC/common/bin/objdump -h $L/loader.elf > $L/sections.txt

# the app: felucca-bsp's start-up and vectors; the memory map is app/app.ld, felucca-bsp's with more of the RAM
$CC -c bsp/chip/crt0.S -o $A/crt0.o
$CC -c bsp/chip/fm1_vec.S -o $A/vec.o
$CC -c bsp/chip/fm1_isr.S -o $A/isr.o
$CC $CFLAGS -Werror -Iapp -c app/main.c -o $A/main.o
$TC/pi32v2/bin/ld -T app/app.ld $A/crt0.o $A/vec.o $A/isr.o $A/main.o -o $A/app.elf
$TC/common/bin/objcopy -O binary -j .text $A/app.elf $A/text.bin
$TC/common/bin/objcopy -O binary -j .data $A/app.elf $A/data.bin
$TC/common/bin/objcopy -O binary -j .ram_text $A/app.elf $A/ramtext.bin
$TC/common/bin/objdump -t $A/app.elf > $A/symbols.txt
