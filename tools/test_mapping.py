#!/usr/bin/env python3
"""Check the mapping in docs/MAPPING.md against the real captures.

Mirrors src mapping: wheel report -> pad buttons, sticks, triggers.
Run after tools/test_layout.py:

    python tools/test_mapping.py
"""

import json
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
CAPTURE_DIR = REPO_ROOT / "docs" / "captures"

L3 = 0x0002
R3 = 0x0004
OPTIONS = 0x0008
UP = 0x0010
RIGHT = 0x0020
DOWN = 0x0040
LEFT = 0x0080
L2 = 0x0100
R2 = 0x0200
L1 = 0x0400
R1 = 0x0800
TRIANGLE = 0x1000
CIRCLE = 0x2000
CROSS = 0x4000
SQUARE = 0x8000
TOUCH_PAD = 0x100000

TRIGGER_THRESHOLD = 32

# Wheel button bits, from src/report.h
WHEEL_1 = 0x0001
WHEEL_2 = 0x0002
WHEEL_3 = 0x0004
WHEEL_4 = 0x0008
WHEEL_PADDLE_DOWN = 0x0010
WHEEL_PADDLE_UP = 0x0020
WHEEL_SE = 0x0040
WHEEL_ST = 0x0080
WHEEL_HOME = 0x0400

BUTTON_MAP = (
    (WHEEL_1, CROSS),
    (WHEEL_2, CIRCLE),
    (WHEEL_3, SQUARE),
    (WHEEL_4, TRIANGLE),
    (WHEEL_PADDLE_UP, R1),
    (WHEEL_PADDLE_DOWN, L1),
    (WHEEL_SE, OPTIONS),
    (WHEEL_ST, TOUCH_PAD),
    (WHEEL_HOME, L3),
)

HAT_MAP = {0: UP, 2: RIGHT, 4: DOWN, 6: LEFT}


def parse(frame_hex):
    data = bytes.fromhex(frame_hex.replace(" ", ""))
    low, high = data[0], data[1] & ~0xE0 & 0xFF
    buttons = 0
    for bit, flag in (
        (0x01, WHEEL_1),
        (0x02, WHEEL_2),
        (0x04, WHEEL_3),
        (0x08, WHEEL_4),
        (0x10, WHEEL_PADDLE_DOWN),
        (0x20, WHEEL_PADDLE_UP),
    ):
        if low & bit:
            buttons |= flag
    for bit, flag in ((0x01, WHEEL_SE), (0x02, WHEEL_ST), (0x10, WHEEL_HOME)):
        if high & bit:
            buttons |= flag
    return {
        "buttons": buttons,
        "hat": data[2] & 0x0F,
        "steering": data[3],
        "brake": data[4],
        "throttle": data[5],
    }


def to_pad(report):
    """Wheel report -> pad state, per docs/MAPPING.md."""
    buttons = 0
    for wheel_bit, pad_bit in BUTTON_MAP:
        if report["buttons"] & wheel_bit:
            buttons |= pad_bit
    buttons |= HAT_MAP.get(report["hat"], 0)

    l2 = 255 - report["brake"]
    r2 = 255 - report["throttle"]
    if l2 >= TRIGGER_THRESHOLD:
        buttons |= L2
    if r2 >= TRIGGER_THRESHOLD:
        buttons |= R2

    return {
        "buttons": buttons,
        "left_x": report["steering"],
        "left_y": 128,
        "l2": l2,
        "r2": r2,
    }


class Checker:
    def __init__(self):
        self.failures = 0
        self.passes = 0

    def check(self, label, condition, detail=""):
        if condition:
            self.passes += 1
            print("  ok    %s" % label)
        else:
            self.failures += 1
            print("  FAIL  %s %s" % (label, detail))


def load(mode):
    matches = sorted(CAPTURE_DIR.glob("*_%s.json" % mode))
    if not matches:
        return None
    return json.loads(matches[-1].read_text(encoding="utf-8"))


def step(capture, name):
    for item in capture["steps"]:
        if item["name"] == name:
            return to_pad(parse(item["frame"]))
    return None


def main():
    capture = load("ps3_separate")
    if capture is None:
        print("missing capture: ps3_separate")
        return 1

    checker = Checker()
    print("mapping checks (ps3_separate)")

    base = to_pad(parse(capture["baseline"]))
    checker.check(
        "resting state has no buttons",
        base["buttons"] == 0,
        "buttons=0x%08X" % base["buttons"],
    )
    checker.check(
        "resting state has triggers released",
        base["l2"] == 0 and base["r2"] == 0,
        "l2=%d r2=%d" % (base["l2"], base["r2"]),
    )

    left = step(capture, "steer_left")
    right = step(capture, "steer_right")
    checker.check("steer_left low", left["left_x"] <= 0x20, left["left_x"])
    checker.check("steer_right high", right["left_x"] >= 0xE0, right["left_x"])

    throttle = step(capture, "throttle")
    checker.check(
        "throttle drives R2, not L2",
        throttle["r2"] >= 200
        and throttle["l2"] == 0
        and throttle["buttons"] & R2
        and not throttle["buttons"] & L2,
        "l2=%d r2=%d buttons=0x%08X"
        % (throttle["l2"], throttle["r2"], throttle["buttons"]),
    )

    brake = step(capture, "brake")
    checker.check(
        "brake drives L2, not R2",
        brake["l2"] >= 200
        and brake["r2"] == 0
        and brake["buttons"] & L2
        and not brake["buttons"] & R2,
        "l2=%d r2=%d buttons=0x%08X"
        % (brake["l2"], brake["r2"], brake["buttons"]),
    )

    for name, want in (
        ("button_1", CROSS),
        ("button_2", CIRCLE),
        ("button_3", SQUARE),
        ("button_4", TRIANGLE),
        ("paddle_up", R1),
        ("paddle_down", L1),
        ("se", OPTIONS),
        ("st", TOUCH_PAD),
        ("home", L3),
    ):
        pad = step(capture, name)
        checker.check(
            "%s maps to one button only" % name,
            pad["buttons"] == want,
            "got 0x%08X want 0x%08X" % (pad["buttons"], want),
        )

    for name, want in (
        ("dpad_up", UP),
        ("dpad_right", RIGHT),
        ("dpad_down", DOWN),
        ("dpad_left", LEFT),
    ):
        pad = step(capture, name)
        checker.check(
            "%s maps to %s" % (name, hex(want)),
            pad["buttons"] == want,
            "got 0x%08X" % pad["buttons"],
        )

    print()
    print("pad preview")
    print("  %-14s %-10s %-4s %-4s %-4s" % ("input", "buttons", "l2", "r2", "x"))
    for item in capture["steps"]:
        pad = to_pad(parse(item["frame"]))
        print(
            "  %-14s 0x%08X %-4d %-4d %-4d"
            % (item["name"], pad["buttons"], pad["l2"], pad["r2"], pad["left_x"])
        )

    print()
    print("passed %d, failed %d" % (checker.passes, checker.failures))
    return 1 if checker.failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
