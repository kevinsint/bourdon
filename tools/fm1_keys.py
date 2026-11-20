#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kevin Sint
"""Print the number of each button and note key of the FM-1 as it is pressed (our firmware's serial port).
    python3 tools/fm1_keys.py [port]      Ctrl-C to stop
The firmware answers any character with its status line, which has buttons=XXXX keys=XXXXXXX in hex."""
import glob, os, re, sys, termios, time, tty


def pressed_now(status_line):
    """(buttons, keys) as lists of numbers, or None when the line is not a status line."""
    found = re.search(r"buttons=([0-9A-F]{4}) keys=([0-9A-F]{7})", status_line)
    if not found:
        return None
    return tuple([n for n in range(28) if int(bits, 16) >> n & 1] for bits in found.groups())


def main():
    ports = sys.argv[1:] or glob.glob("/dev/cu.usbmodem*")
    if len(ports) != 1:
        sys.exit(f"need one port, found: {ports or 'none'} (give the port as the argument)")
    port = os.open(ports[0], os.O_RDWR | os.O_NOCTTY)
    tty.setraw(port)
    mode = termios.tcgetattr(port)
    mode[6][termios.VMIN], mode[6][termios.VTIME] = 0, 2        # a read waits 0.2 s at most
    termios.tcsetattr(port, termios.TCSANOW, mode)
    print(f"{ports[0]}: press SEL, then LFO, then the OP1 key. Ctrl-C to stop.")
    shown, text = ([], []), ""
    while True:
        os.write(port, b"?")
        text += os.read(port, 4096).decode("ascii", "replace")
        *lines, text = text.split("\n")
        for line in lines:
            now = pressed_now(line)
            if now and now != shown:
                for name, before, after in zip(("button", "key"), shown, now):
                    for n in after:
                        if n not in before:
                            print(f"{name} {n}", flush=True)
                shown = now
        time.sleep(0.05)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
