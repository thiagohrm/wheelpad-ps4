#!/usr/bin/env python3
"""Decode a HID report descriptor.

    python tools/hid_descriptor.py build/rd.txt

The descriptor tells the report size, the report id and which fields the
device sends, which is what the console plugin needs to know before it can
read the wheel.
"""

import sys

NAMES = {
    0x00: "MAIN",
    0x01: "GLOBAL",
    0x02: "LOCAL",
    0x04: "MAIN",
    0x05: "USAGE_PAGE",
    0x06: "DATA",
    0x07: "LOGICAL_MIN",
    0x08: "LOGICAL_MAX",
    0x09: "USAGE",
    0x0A: "COLLECTION",
    0x0B: "FEATURE",
    0x0C: "PHYSICAL",
    0x0D: "UNIT_EXP",
    0x0E: "UNIT",
    0x0F: "STRING",
    0x10: "DELIMITER",
    0x11: "DESIGNATOR",
    0x12: "DESIGNATOR_SET",
    0x15: "LOGICAL_MIN",
    0x25: "LOGICAL_MAX",
    0x26: "UNIT_EXP",
    0x27: "UNIT",
    0x28: "STRING_MAX",
    0x29: "PHYSICAL_MIN",
    0x2A: "PHYSICAL_MAX",
    0x2B: "UNIT_EXP",
    0x2C: "UNIT",
    0x75: "REPORT_SIZE",
    0x81: "INPUT",
    0x85: "REPORT_ID",
    0x89: "REPORT_COUNT",
    0x91: "OUTPUT",
    0x95: "REPORT_COUNT",
    0xB1: "FEATURE",
    0xC0: "END_COLLECTION",
    0xC2: "MAIN",
}

PAGES = {
    0x01: "Generic Desktop",
    0x02: "Simulation",
    0x03: "Keyboard",
    0x04: "Consumer",
    0x05: "Game Pad",
    0x09: "Button",
}


def decode(raw):
    index = 0
    size = 0
    count = 0
    reports = []
    while index < len(raw):
        item = raw[index]
        if item == 0:
            break

        # A short item is one byte: the low two bits hold the data size and
        # the next four bits hold the type. A long item starts with 0xfe.
        if item == 0xFE:
            data_size = raw[index + 1]
            type_size = raw[index + 2]
            tag = int.from_bytes(raw[index + 3:index + 3 + type_size], "big")
            data = raw[index + 3 + type_size:index + 3 + type_size + data_size]
            index += 3 + type_size + data_size
        else:
            data_size = item & 0x03
            if data_size == 3:
                data_size = 4
            tag = (item & 0x3C) >> 2
            data = raw[index + 1:index + 1 + data_size]
            index += 1 + data_size

        if len(data) < 1 and tag not in (0x0A, 0xC0):
            break

        name = NAMES.get(tag, hex(tag))
        value = data[0] if data else 0

        if tag == 0x75:
            size = value
            print("%-14s %d" % (name, value))
        elif tag == 0x95:
            count = value
            print("%-14s %d" % (name, value))
        elif tag == 0x05:
            print("%-14s 0x%02x (%s)" % (name, value, PAGES.get(value, "")))
        elif tag == 0x09:
            print("%-14s 0x%02x" % (name, value))
        elif tag in (0x81, 0x91, 0xB1):
            total = size * count
            reports.append((name, total))
            print("%-14s flags=0x%02x  %d bit(s)" % (name, value, total))
        else:
            print("%-14s %s" % (name, " ".join("%02x" % b for b in data)))

    return reports


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2

    text = open(sys.argv[1]).read().strip().replace(" ", "")
    raw = bytes.fromhex(text)
    print("descriptor: %d bytes" % len(raw))
    reports = decode(raw)
    if reports:
        print()
        print("report sizes in bits:", ", ".join("%s=%d" % r for r in reports))
        print("report sizes in bytes:", ", ".join("%s=%d" % (r[0], r[1] // 8) for r in reports))
    return 0


if __name__ == "__main__":
    sys.exit(main())
