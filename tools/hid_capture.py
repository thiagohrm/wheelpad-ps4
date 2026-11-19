#!/usr/bin/env python3
"""Guided HID capture for a Thrustmaster wheel.

Records one frame per input and writes the result to docs/captures/. The output
is the report layout needed to write the console plugin.

Install:
    python -m pip install --user hidapi
Do not install the "hid" package.

Usage:
    python tools/hid_capture.py
    python tools/hid_capture.py --mode pc_separate
"""

import argparse
import json
import sys
import time
from datetime import datetime
from pathlib import Path

try:
    import hid
except ImportError as exc:  # noqa: BLE001 - show the real reason
    sys.exit(
        "cannot import hid: %s\n"
        "fix: python -m pip install --user hidapi\n"
        "and: python -m pip uninstall -y hid" % exc
    )

THRUSTMASTER_VID = 0x044F
CAPTURE_SECONDS = 3.0
BASELINE_SECONDS = 3.0
REPO_ROOT = Path(__file__).resolve().parent.parent
CAPTURE_DIR = REPO_ROOT / "docs" / "captures"

MODES = [
    ("pc_separate", "PC position, pedals separate (red LED)"),
    ("pc_combined", "PC position, pedals combined (green LED)"),
    ("ps3_separate", "PS3 position, pedals separate (red LED)"),
    ("ps3_combined", "PS3 position, pedals combined (green LED)"),
]

STEPS = [
    ("steer_left", "turn the wheel fully LEFT", "axis"),
    ("steer_right", "turn the wheel fully RIGHT", "axis"),
    ("throttle", "press the THROTTLE pedal down fully", "axis"),
    ("brake", "press the BRAKE pedal down fully", "axis"),
    ("button_1", "press action button 1", "button"),
    ("button_2", "press action button 2", "button"),
    ("button_3", "press action button 3", "button"),
    ("button_4", "press action button 4", "button"),
    ("paddle_up", "pull the UP paddle", "button"),
    ("paddle_down", "pull the DOWN paddle", "button"),
    ("dpad_up", "press D-pad UP", "button"),
    ("dpad_down", "press D-pad DOWN", "button"),
    ("dpad_left", "press D-pad LEFT", "button"),
    ("dpad_right", "press D-pad RIGHT", "button"),
    ("se", "press the SE button", "button"),
    ("st", "press the ST button", "button"),
    ("home", "press the HOME button", "button"),
]


def ask(prompt):
    try:
        return input(prompt)
    except EOFError:
        return "q"


def pause(prompt="press Enter to continue"):
    try:
        input(prompt)
    except EOFError:
        raise SystemExit(0)


def choose_mode():
    print("Select the wheel configuration you are capturing:")
    for index, (_, label) in enumerate(MODES, start=1):
        print("  %d) %s" % (index, label))
    while True:
        answer = ask("number: ").strip().lower()
        if answer in ("q", "quit"):
            raise SystemExit(0)
        if answer.isdigit() and 1 <= int(answer) <= len(MODES):
            return MODES[int(answer) - 1]
        print("invalid choice")


def pick_device(vid, pid):
    devices = list(hid.enumerate(vid or 0, pid or 0))
    if not devices:
        return None
    if len(devices) == 1:
        return devices[0]
    print("several matching devices:")
    for index, dev in enumerate(devices, start=1):
        print(
            "  %d) vid=0x%04X pid=0x%04X %r usage_page=0x%04X"
            % (
                index,
                dev["vendor_id"],
                dev["product_id"],
                dev.get("product_string"),
                dev.get("usage_page", 0),
            )
        )
    while True:
        answer = ask("number: ").strip()
        if answer.isdigit() and 1 <= int(answer) <= len(devices):
            return devices[int(answer) - 1]
        print("invalid choice")


def open_device(dev_info):
    dev = hid.device()
    try:
        dev.open_path(dev_info["path"])
        return dev
    except Exception:  # noqa: BLE001 - retry by vid/pid/serial
        dev.close()
    dev = hid.device()
    dev.open(
        dev_info["vendor_id"],
        dev_info["product_id"],
        dev_info.get("serial_number") or None,
    )
    return dev


def read_baseline(dev, seconds):
    """Return the last frame seen in the window.

    This wheel reports only when something changes, so a silent idle window is
    normal. The caller asks the user to nudge the wheel first, then this takes
    the settled frame as the resting state.
    """
    deadline = time.monotonic() + seconds
    last = None
    frames = 0
    while time.monotonic() < deadline:
        data = dev.read(64, 50)
        if data:
            frames += 1
            last = bytes(data)
    return last, frames


def score_against(baseline, data):
    total = 0
    for offset in range(min(len(baseline), len(data))):
        total += abs(data[offset] - baseline[offset])
    return total


def capture_extreme(dev, baseline, seconds):
    deadline = time.monotonic() + seconds
    best = None
    best_score = 0
    frames = 0
    while time.monotonic() < deadline:
        data = dev.read(64, 20)
        if not data:
            continue
        frames += 1
        data = bytes(data)
        score = score_against(baseline, data)
        if score > best_score:
            best_score = score
            best = data
    return best, best_score, frames


def hex_bytes(data):
    return data.hex(" ")


def describe_changes(baseline, frame):
    changes = []
    for offset in range(min(len(baseline), len(frame))):
        base = baseline[offset]
        value = frame[offset]
        if base != value:
            delta = {
                "offset": offset,
                "base": base,
                "value": value,
                "xor": base ^ value,
            }
            changes.append(delta)
    return changes


def format_changes(changes, baseline, frame):
    if not changes:
        return "    no change detected"
    lines = []
    for item in changes:
        xor = item["xor"]
        note = ""
        if xor and xor & (xor - 1) == 0:
            note = "  bit %d" % (xor.bit_length() - 1)
        lines.append(
            "    offset %2d: 0x%02X -> 0x%02X  (xor 0x%02X)%s"
            % (item["offset"], item["base"], item["value"], xor, note)
        )
    lines.append("    full baseline: %s" % hex_bytes(baseline))
    lines.append("    full captured: %s" % hex_bytes(frame))
    return "\n".join(lines)


def run_steps(dev, baseline):
    results = []
    for index, (name, label, kind) in enumerate(STEPS, start=1):
        while True:
            print()
            print("[%d/%d] %s" % (index, len(STEPS), label))
            answer = ask("    Enter = record, s = skip, q = quit: ").strip().lower()
            if answer in ("q", "quit"):
                return results
            if answer == "s":
                break
            print("    recording %.1fs, do it now" % CAPTURE_SECONDS)
            frame, score, frames = capture_extreme(dev, baseline, CAPTURE_SECONDS)
            if frame is None or score == 0:
                print("    nothing changed. device idle or wrong mode.")
                continue
            changes = describe_changes(baseline, frame)
            print(format_changes(changes, baseline, frame))
            verdict = ask("    Enter = accept, r = redo, s = skip: ").strip().lower()
            if verdict == "r":
                continue
            if verdict == "s":
                break
            results.append(
                {
                    "index": index,
                    "name": name,
                    "label": label,
                    "kind": kind,
                    "frame": hex_bytes(frame),
                    "changes": changes,
                }
            )
            break
    return results


def build_summary(steps):
    touched = {}
    for step in steps:
        for item in step["changes"]:
            entry = touched.setdefault(
                item["offset"], {"axis": [], "button": [], "bits": set()}
            )
            entry[step["kind"]].append(step["name"])
            entry["bits"].add(item["xor"])
    lines = []
    for offset in sorted(touched):
        entry = touched[offset]
        bits = ", ".join("0x%02X" % bit for bit in sorted(entry["bits"]))
        lines.append(
            "  offset %2d  axis=[%s]  button=[%s]  xor=[%s]"
            % (
                offset,
                ", ".join(entry["axis"]),
                ", ".join(entry["button"]),
                bits,
            )
        )
    return lines, touched


def guess_layout(touched):
    hints = []
    axis_offsets = sorted(offset for offset, entry in touched.items() if entry["axis"])
    button_offsets = sorted(
        offset for offset, entry in touched.items() if entry["button"]
    )
    for offset in axis_offsets:
        hints.append("offset %d carries an axis" % offset)
    for first, second in zip(axis_offsets, axis_offsets[1:]):
        if second == first + 1:
            hints.append(
                "offsets %d and %d are adjacent: likely one 16-bit little-endian axis"
                % (first, second)
            )
    for offset in button_offsets:
        hints.append("offset %d carries button bits" % offset)
    if not hints:
        hints.append("no usable changes recorded")
    return hints


def write_output(mode_id, mode_label, dev_info, baseline, steps):
    CAPTURE_DIR.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    base_name = "%s_%s" % (stamp, mode_id)
    payload = {
        "captured_at": stamp,
        "mode": mode_id,
        "mode_label": mode_label,
        "device": {
            "vendor_id": dev_info["vendor_id"],
            "product_id": dev_info["product_id"],
            "product": dev_info.get("product_string"),
            "usage_page": dev_info.get("usage_page", 0),
            "usage": dev_info.get("usage", 0),
        },
        "report_length": len(baseline),
        "baseline": hex_bytes(baseline),
        "steps": steps,
    }
    json_path = CAPTURE_DIR / (base_name + ".json")
    json_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")

    summary, touched = build_summary(steps)
    hints = guess_layout(touched)
    md_path = CAPTURE_DIR / (base_name + ".md")
    markdown = [
        "# Capture %s" % mode_id,
        "",
        mode_label,
        "",
        "- device: vid=0x%04X pid=0x%04X %r"
        % (
            dev_info["vendor_id"],
            dev_info["product_id"],
            dev_info.get("product_string"),
        ),
        "- report length: %d bytes" % len(baseline),
        "- baseline: `%s`" % hex_bytes(baseline),
        "",
        "## Inputs",
        "",
    ]
    for step in steps:
        markdown.append("### %s" % step["name"])
        markdown.append("")
        markdown.append(step["label"])
        markdown.append("")
        markdown.append("```")
        for item in step["changes"]:
            xor = item["xor"]
            note = ""
            if xor and xor & (xor - 1) == 0:
                note = "  bit %d" % (xor.bit_length() - 1)
            markdown.append(
                "offset %2d: 0x%02X -> 0x%02X  xor 0x%02X%s"
                % (item["offset"], item["base"], item["value"], xor, note)
            )
        markdown.append("captured: %s" % step["frame"])
        markdown.append("```")
        markdown.append("")
    markdown.append("## Offset summary")
    markdown.append("")
    markdown.append("```")
    markdown.extend(summary)
    markdown.append("```")
    markdown.append("")
    markdown.append("## Hints")
    markdown.append("")
    for hint in hints:
        markdown.append("- %s" % hint)
    markdown.append("")
    md_path.write_text("\n".join(markdown), encoding="utf-8")
    return json_path, md_path, summary, hints


def main():
    parser = argparse.ArgumentParser(
        description="Guided HID capture for a Thrustmaster wheel."
    )
    parser.add_argument("--mode", choices=[mode for mode, _ in MODES])
    parser.add_argument("--vid", type=lambda value: int(value, 0), default=THRUSTMASTER_VID)
    parser.add_argument("--pid", type=lambda value: int(value, 0), default=0)
    args = parser.parse_args()

    mode_id, mode_label = (
        (args.mode, dict(MODES)[args.mode]) if args.mode else choose_mode()
    )

    dev_info = pick_device(args.vid, args.pid)
    if dev_info is None:
        print()
        print("no Thrustmaster device found (vid=0x%04X)." % args.vid)
        print("check the USB cable and the PS3/PC selector switch.")
        return 1

    print()
    print("device: vid=0x%04X pid=0x%04X %r"
          % (dev_info["vendor_id"], dev_info["product_id"], dev_info.get("product_string")))
    try:
        dev = open_device(dev_info)
        dev.set_nonblocking(0)
    except Exception as exc:  # noqa: BLE001
        print("cannot open device for reading: %s" % exc)
        print("in PS3 mode Windows may not attach a HID driver. capture that mode on the console instead.")
        return 1

    try:
        print()
        print("Baseline: wheel centered and untouched.")
        print("This wheel only reports when something changes.")
        print("So nudge the wheel slightly, let it settle, and keep hands off.")
        pause("press Enter, then nudge slightly within 3 seconds")
        baseline, frames = read_baseline(dev, BASELINE_SECONDS)
        if baseline is None:
            print()
            print("no reports at all. try, in this order:")
            print("  1) press any wheel button, the wheel may be asleep")
            print("  2) confirm the switch matches the mode you picked")
            print("  3) run joy.cpl and see if Windows sees the wheel move")
            return 1
        print(
            "baseline (%d bytes, %d frames): %s"
            % (len(baseline), frames, hex_bytes(baseline))
        )

        steps = run_steps(dev, baseline)
    finally:
        dev.close()

    if not steps:
        print("nothing captured")
        return 1

    json_path, md_path, summary, hints = write_output(
        mode_id, mode_label, dev_info, baseline, steps
    )
    print()
    print("offset summary:")
    for line in summary:
        print(line)
    print()
    print("hints:")
    for hint in hints:
        print("  - %s" % hint)
    print()
    print("wrote %s" % json_path)
    print("wrote %s" % md_path)
    print()
    print("next: run it again for the other switch and pedal combinations.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
