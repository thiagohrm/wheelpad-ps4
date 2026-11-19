"""Summarise the raw frames the plugin logged.

    python tools/analyse_raw.py build/klog6.log

Prints, per byte, the distinct values seen in the logged 27 byte reports, so a
byte that never moves cannot be mistaken for a pedal or a paddle.
"""

import re
import sys
from collections import defaultdict

PATTERN = re.compile(r"\[wheelpad\] raw( other)?:\s*((?:[0-9a-f]{2}\s*){27})")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2

    text = open(sys.argv[1], encoding="utf-8", errors="replace").read()
    frames = [(bool(m.group(1)), bytes.fromhex(m.group(2))) for m in PATTERN.finditer(text)]
    print("%d frames" % len(frames))

    for kind, name in ((False, "state shape"), (True, "other shape")):
        selected = [f for f in frames if f[0] == kind]
        if not selected:
            continue
        print()
        print(name, "%d frames" % len(selected))
        for index in range(27):
            values = sorted({f[index] for _, f in selected})
            print("  byte %2d: %s" % (index, " ".join("%02x" % v for v in values)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
