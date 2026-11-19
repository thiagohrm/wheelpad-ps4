#!/usr/bin/env python3
"""Analyse a console capture log.

    python tools/analyse_capture.py build/klog.log

Reads the change lines the capture plugin writes and reports, per byte, which
values it took. The byte map is in docs/REPORTS.md.
"""

import re
import sys
from collections import Counter

PATTERN = re.compile(r"b(\d+): 0x([0-9a-f]{2}) -> 0x([0-9a-f]{2})")

NAMES = {
    0: "buttons 1-6",
    1: "buttons 7-11",
    2: "hat",
    3: "steering",
    4: "brake",
    5: "throttle",
    6: "extra axis",
}


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2

    text = open(sys.argv[1], encoding="utf-8", errors="replace").read()

    events = []
    for match in PATTERN.finditer(text):
        events.append((int(match.group(1)), int(match.group(2), 16), int(match.group(3), 16)))

    print("%d change events" % len(events))
    if not events:
        return 1

    for byte in sorted(set(byte for _, byte, _ in events)):
        olds = sorted(set(old for _, b, old in events if b == byte))
        news = sorted(set(new for _, b, new in events if b == byte))
        name = NAMES.get(byte, "")
        print("byte %2d %-14s from %s" % (byte, name, " ".join("%02x" % v for v in olds[:8])))
        print("             to   %s" % (" ".join("%02x" % v for v in news[:14])))

    # Bytes that only ever hold two values are digital, the rest are analogue.
    print()
    print("digital bytes (two values only):")
    for byte in sorted(set(byte for _, byte, _ in events)):
        values = set(v for _, b, v in events if b == byte)
        old_values = set(v for _, b, v in events if b == byte)
        if len(values) <= 2 and byte > 6:
            print("  byte %2d: %s" % (byte, " ".join("%02x" % v for v in sorted(values))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
