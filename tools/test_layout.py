#!/usr/bin/env python3
"""Validate the report layout in docs/REPORTS.md against the real captures.

This mirrors the parser in src/report.c. It proves the byte map, not the C code.
Run after every capture:

    python tools/test_layout.py
"""

import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
CAPTURE_DIR = REPO_ROOT / "docs" / "captures"

BTN_1 = 0x0001
BTN_2 = 0x0002
BTN_3 = 0x0004
BTN_4 = 0x0008
BTN_PADDLE_DOWN = 0x0010
BTN_PADDLE_UP = 0x0020
BTN_SE = 0x0040
BTN_ST = 0x0080
BTN_HOME = 0x0400

HAT_UP = 0
HAT_RIGHT = 2
HAT_DOWN = 4
HAT_LEFT = 6
HAT_NONE = 0x0F


def parse(frame_hex):
    data = bytes.fromhex(frame_hex.replace(" ", ""))
    if len(data) != 19:
        raise ValueError("expected 19 bytes, got %d" % len(data))
    low = data[0]
    high = data[1] & ~0xE0 & 0xFF
    buttons = 0
    for bit, flag in (
        (0x01, BTN_1),
        (0x02, BTN_2),
        (0x04, BTN_3),
        (0x08, BTN_4),
        (0x10, BTN_PADDLE_DOWN),
        (0x20, BTN_PADDLE_UP),
    ):
        if low & bit:
            buttons |= flag
    for bit, flag in ((0x01, BTN_SE), (0x02, BTN_ST), (0x10, BTN_HOME)):
        if high & bit:
            buttons |= flag
    return {
        "buttons": buttons,
        "hat": data[2] & 0x0F,
        "steering": data[3],
        "brake": data[4],
        "throttle": data[5],
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


def load_capture(mode):
    matches = sorted(CAPTURE_DIR.glob("*_%s.json" % mode))
    if not matches:
        return None
    return json.loads(matches[-1].read_text(encoding="utf-8"))


def step(capture, name):
    for item in capture["steps"]:
        if item["name"] == name:
            return parse(item["frame"])
    return None


def check_common(checker, capture, mode_label):
    print("%s (%s)" % (mode_label, capture["mode"]))
    base = parse(capture["baseline"])
    checker.check("baseline hat centred", base["hat"] == HAT_NONE, base)
    checker.check("baseline throttle released", base["throttle"] == 0xFF, base)
    checker.check("baseline brake released", base["brake"] == 0xFF, base)

    left = step(capture, "steer_left")
    right = step(capture, "steer_right")
    all_steering = [base["steering"]] + [
        parse(item["frame"])["steering"] for item in capture["steps"]
    ]
    checker.check(
        "steering spans the full 8-bit range",
        min(all_steering) <= 0x20 and max(all_steering) >= 0xE0,
        "min=%d max=%d" % (min(all_steering), max(all_steering)),
    )
    if left and right:
        checker.check(
            "steer_left sits below steer_right",
            left["steering"] < right["steering"],
            "left=%d right=%d" % (left["steering"], right["steering"]),
        )
        checker.check(
            "steer_left pedals untouched",
            left["brake"] == 0xFF and left["throttle"] == 0xFF,
        )

    for name, flag in (
        ("button_1", BTN_1),
        ("button_2", BTN_2),
        ("button_3", BTN_3),
        ("button_4", BTN_4),
        ("paddle_up", BTN_PADDLE_UP),
        ("paddle_down", BTN_PADDLE_DOWN),
        ("se", BTN_SE),
        ("st", BTN_ST),
        ("home", BTN_HOME),
    ):
        frame = step(capture, name)
        if frame:
            checker.check(
                "%s sets only its bit" % name,
                frame["buttons"] == flag,
                "got 0x%04X want 0x%04X" % (frame["buttons"], flag),
            )

    for name, want in (
        ("dpad_up", HAT_UP),
        ("dpad_right", HAT_RIGHT),
        ("dpad_down", HAT_DOWN),
        ("dpad_left", HAT_LEFT),
    ):
        frame = step(capture, name)
        if frame:
            checker.check(
                "%s hat %d" % (name, want),
                frame["hat"] == want,
                "got 0x%02X" % frame["hat"],
            )


def check_ps3_pedals(checker, capture):
    brake = step(capture, "brake")
    throttle = step(capture, "throttle")
    if brake:
        checker.check(
            "brake drives byte 4 only",
            brake["brake"] <= 0x10 and brake["throttle"] == 0xFF,
            "brake=%d throttle=%d" % (brake["brake"], brake["throttle"]),
        )
    if throttle:
        checker.check(
            "throttle drives byte 5 only",
            throttle["throttle"] <= 0x10 and throttle["brake"] == 0xFF,
            "throttle=%d brake=%d" % (throttle["throttle"], throttle["brake"]),
        )


def check_pc_combined(checker, capture):
    brake = step(capture, "brake")
    throttle = step(capture, "throttle")
    if brake and throttle:
        checker.check(
            "combined pedals share byte 5",
            brake["brake"] == 0xFF
            and throttle["brake"] == 0xFF
            and brake["throttle"] <= 0x10
            and throttle["throttle"] <= 0x10,
            "brake=(%d,%d) throttle=(%d,%d)"
            % (
                brake["brake"],
                brake["throttle"],
                throttle["brake"],
                throttle["throttle"],
            ),
        )


def main():
    checker = Checker()

    ps3 = load_capture("ps3_separate")
    if ps3:
        check_common(checker, ps3, "PS3 mode, separate pedals")
        check_ps3_pedals(checker, ps3)
    else:
        print("missing capture: ps3_separate")

    pc = load_capture("pc_separate")
    if pc:
        check_common(checker, pc, "PC mode, combined pedals (as captured)")
        check_pc_combined(checker, pc)
    else:
        print("missing capture: pc_separate")

    print()
    print("passed %d, failed %d" % (checker.passes, checker.failures))
    return 1 if checker.failures or not ps3 else 0


if __name__ == "__main__":
    raise SystemExit(main())
