#!/usr/bin/env python3
"""Probe a Thrustmaster wheel: interfaces, report descriptor, read behaviour.

Read-only diagnostic. Tells us why the guided capture saw no reports.

Usage:
    python tools/hid_probe.py
    python tools/hid_probe.py --seconds 8
"""

import argparse
import sys
import time

try:
    import hid
except ImportError as exc:  # noqa: BLE001
    sys.exit(
        "cannot import hid: %s\n"
        "fix: python -m pip install --user hidapi" % exc
    )


def show_devices(vid):
    devices = list(hid.enumerate(vid, 0))
    print("devices with vid=0x%04X: %d" % (vid, len(devices)))
    for index, dev in enumerate(devices):
        print()
        print("  [%d] vid=0x%04X pid=0x%04X %r" % (
            index, dev["vendor_id"], dev["product_id"], dev.get("product_string")))
        print("      manufacturer=%r serial=%r" % (
            dev.get("manufacturer_string"), dev.get("serial_number")))
        print("      usage_page=0x%04X usage=0x%04X interface=%s release=0x%04X" % (
            dev.get("usage_page", 0),
            dev.get("usage", 0),
            dev.get("interface_number"),
            dev.get("release_number", 0),
        ))
        print("      path=%s" % dev["path"])
    return devices


def show_descriptor(dev_info):
    dev = hid.device()
    try:
        dev.open_path(dev_info["path"])
    except Exception as exc:  # noqa: BLE001
        print("      open failed: %s" % exc)
        dev.close()
        return
    try:
        if hasattr(dev, "get_report_descriptor"):
            desc = bytes(dev.get_report_descriptor())
            print("      report descriptor: %d bytes" % len(desc))
            for offset in range(0, len(desc), 16):
                print("        %04X  %s" % (
                    offset, desc[offset:offset + 16].hex(" ")))
        else:
            print("      report descriptor: unavailable in binding")
    except Exception as exc:  # noqa: BLE001
        print("      report descriptor failed: %s" % exc)
    finally:
        dev.close()


def try_feature_report(dev_info):
    dev = hid.device()
    try:
        dev.open_path(dev_info["path"])
    except Exception as exc:  # noqa: BLE001
        print("      feature open failed: %s" % exc)
        dev.close()
        return
    for report_id in (0x00, 0x01, 0x05, 0x42):
        try:
            data = bytes(dev.get_feature_report(report_id, 64))
            print("      feature id 0x%02X: %d bytes  %s" % (
                report_id, len(data), data.hex(" ")[:96]))
        except Exception as exc:  # noqa: BLE001
            print("      feature id 0x%02X: error %s" % (report_id, exc))
    dev.close()


def try_read(dev_info, seconds):
    dev = hid.device()
    try:
        dev.open_path(dev_info["path"])
    except Exception as exc:  # noqa: BLE001
        print("      read open failed: %s" % exc)
        dev.close()
        return
    frames = 0
    first = None
    start = time.monotonic()
    try:
        while time.monotonic() - start < seconds:
            data = dev.read(64, 100)
            if data:
                frames += 1
                if first is None:
                    first = bytes(data)
    finally:
        dev.close()
    print("      reads over %.1fs: %d frames" % (seconds, frames))
    if first is not None:
        print("      first frame: %s" % first.hex(" "))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--vid", type=lambda v: int(v, 0), default=0x044F)
    parser.add_argument("--seconds", type=float, default=5.0)
    args = parser.parse_args()

    devices = show_devices(args.vid)
    for dev_info in devices:
        print()
        print("  --- probe pid=0x%04X usage_page=0x%04X ---" % (
            dev_info["product_id"], dev_info.get("usage_page", 0)))
        show_descriptor(dev_info)
        try_feature_report(dev_info)
        print("      move the wheel now, testing reads...")
        try_read(dev_info, args.seconds)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
