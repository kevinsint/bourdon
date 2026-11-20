# DR1

DR1 is a firmware for the M-VAVE FM-1 that makes it a drone instrument. It replaces the firmware of the FM-1.

It has three voices. Each voice has a style that you choose, a free frequency (no notes, no keyboard), a few knobs
that you play, and a link to the voice before it. A mixer and a chain of effects (delay, reverb, compressor) come
after the voices.

DR1 is not a product of M-VAVE, and M-VAVE does not support it.

## State

Version V0.27. This is early work:

- The six styles and the ranges of the knobs are a first estimate.
- The firmware does not save anything: all the values go back to their start at power-on.
- It is tested on one FM-1 only, with the Puya P25Q80 flash.

## Before you install

**You install this at your own risk.** An install that goes wrong can leave an FM-1 that does not start. There is a
rescue tool (see "Back to the stock firmware"), but there is no warranty of any kind.

- DR1 replaces the firmware that is on the FM-1. Your FM-1 is not an FM synthesizer while DR1 is on it.
- DR1 can replace itself, or go back to another firmware, only on an FM-1 with one of these two flash parts: Puya
  P25Q80 (the first units) or UC25HQ80 (newer units). On a different part, DR1 refuses the update and only the
  rescue tool can replace it.
- What we tested: an install on an FM-1 that ran a firmware of the Felucca family, and an install of DR1 over DR1.
  An install on an FM-1 with M-VAVE's stock firmware uses the same tool, but we did not test it.

## Install

You need a Mac or a Linux computer, a USB cable, Git and Python 3.12. (The installer's MIDI library has ready
packages for Python 3.12 and older only.)

1. Get this repository with its submodule. The installer is in the submodule.

   ```sh
   git clone --recurse-submodules https://github.com/kevinsint/dr1.git
   cd dr1
   ```

2. Make a Python environment with the installer's two libraries.

   ```sh
   python3.12 -m venv .venv
   .venv/bin/pip install -r requirements.txt
   ```

3. Download `sint_fm1_dr1.fwsc` from the [latest release](https://github.com/kevinsint/dr1/releases/latest) into
   this folder. Compare its SHA-256 with the one in the release notes.

   ```sh
   shasum -a 256 sint_fm1_dr1.fwsc
   ```

4. Connect the FM-1 to the computer with the USB cable and turn it on. Close every program that uses MIDI.

5. Read what is on the FM-1 now, and write it down. You need it if you want to go back.

   ```sh
   .venv/bin/python bsp/tools/fm1_install.py --info
   ```

6. Install. The tool shows what it will do and asks you to confirm. Do not disconnect the FM-1 before it restarts.

   ```sh
   .venv/bin/python bsp/tools/fm1_install.py sint_fm1_dr1.fwsc
   ```

After the restart the screen shows `OP1 FLOOR` and a frequency. The three voices are off: press the `OP1` key.

### Install a newer DR1 over DR1

DR1 has its own USB name, so tell the installer where to look:

```sh
.venv/bin/python bsp/tools/fm1_install.py sint_fm1_dr1.fwsc --port SINT_FM1
```

If an install was cut off, run the same command again: it continues.

## Back to the stock firmware

1. Download the official FM-1 firmware V15 (`FM-1.fwsc`) from M-VAVE (m-vave.com, Download, PC Firmware).
2. Install it with the same tool:

   ```sh
   .venv/bin/python bsp/tools/fm1_install.py FM-1.fwsc --port SINT_FM1
   ```

If the FM-1 does not start any more, use the rescue tool of the base: `bsp/tools/fm1_rescue.py`. Its instructions
are at the top of that file.

## How to play

The full list of the controls is in [pannel-description.md](pannel-description.md). The short form:

| Control | Use |
|---|---|
| `OP1`, `OP2`, `OP3` keys | A short press turns the voice on or off. Hold 1 second to select it. Hold it and turn a knob: the voice is selected and the knob is for it. |
| `SELECT` | The voice's frequency. A slow turn is very fine; a fast turn goes far. |
| `OCT+` | The frequency times two. With `OCT-` held: divided by two. |
| `ALGORITHM` | The voice's style: FLOOR, REED, GLASS, GROWL, TIDE, PULSE. With `OCT-` held: the kind of its link, FOLLOW or THROUGH. |
| `KNOB1` to `KNOB4` | TONE, MOVE, GRIT, LINK. With `OCT-` held, `KNOB2` is SPEED. |
| `GLO` | The next page: play, mixer, end of the chain, then play again. |
| `MASTER` | The output level. |
| `SEL` | Held: the version and the processor's load. |

**The links.** Each voice listens to the voice before it: OP2 to OP1, OP3 to OP2, OP1 to OP3. LINK sets how much.
With FOLLOW, the other voice's loudness opens this voice's filter. With THROUGH, the other voice's sound goes through
this voice's filter.

**The mixer.** `SELECT` chooses a channel: none, OP1, OP2 or OP3. With no channel, `KNOB1` to `KNOB3` are the three
levels. With a channel, the knobs are its LEVEL, PAN, SEND 1 (to the delay) and SEND 2 (to the reverb), and `OCT+`
changes its sends from PRE to POST.

**The end of the chain.** `SELECT` chooses the delay, the reverb or the compressor. The four right knobs are its
values. The delay is silent until you turn its LVL up.

## Build from source

You need Docker, Git, `curl` and Python 3.12. The compile step runs in a Linux x86-64 container, because the chip
maker's toolchain is for that system only.

```sh
git clone --recurse-submodules https://github.com/kevinsint/dr1.git
cd dr1
scripts/setup.sh            # one time: downloads JieLi's toolchain and AC79 SDK, checks them by hash, makes .venv
scripts/build.sh            # makes build/sint_fm1_dr1.fwsc
```

`scripts/setup.sh` puts the toolchain and the SDK in `~/.jieli`. They are not in this repository.

The tests:

```sh
tests/run_host_tests.sh     # on your computer: no FM-1, no toolchain
tests/build_test.sh         # builds the package and checks it
```

## Licence and credits

DR1 is under the Mozilla Public License 2.0: see [LICENSE](LICENSE).

DR1 is made by Kevin Sint: [kevinsint.live/apps/fm1-drone](https://kevinsint.live/apps/fm1-drone/).

- DR1 is built on [felucca-bsp](https://github.com/hugelton/felucca-bsp) by Leo Kuroshita, Hügelton Instruments
  (MPL-2.0). It is the `bsp` submodule, used unchanged: the chip and board code, the update loader, the package
  builder and the installer.
- `tools/fm1img.py` and `tools/lz4blk.py` come from that family of projects (MPL-2.0; their authors are named in the
  files). `app/app.ld` is felucca-bsp's memory map with more RAM.
- A built package holds three files of JieLi's AC79 SDK, which is under the Apache License 2.0. Its text is in
  `bsp/LICENSES/Apache-2.0.txt` and goes with every release.
