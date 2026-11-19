#!/usr/bin/env python3
"""Dump raw HID reports from a Thrustmaster wheel.

Purpose: learn the wheel's report layout before writing any console code.
Run it on a PC with the wheel plugged in, once with the selector on PC and
once with the selector on PS3.

Install:
    pip install hidapi
Do not install "hid". That is a different package and needs a DLL it does not
ship. The hidapi package installs the import module named "hid".

Usage:
    python tools/hid_dump.py --list
    python tools/hid_dump.py --dump
    python tools/hid_dump.py --dump --all --seconds 30
"""

import argparse
import sys
import time

try:
    import hid
except ImportError as exc:  # noqa: BLE001 - show the real reason
    sys.exit(
        "cannot import hid: %s\n"
        "fix: python -m pip install --user hidapi\n"
        "and: python -m pip uninstall -y hid" % exc
    )

THRUSTMASTER_VID = 0x044F


def enumerate_devices(vid, pid):
    return list(hid.enumerate(vid or 0, pid or 0))


def show_list(devices):
    if not devices:
        print("no devices found (check VID filter and USB mode switch)")
        return
    for dev in devices:
        print(
            "vid=0x%04X pid=0x%04X  %r  %r  serial=%r"
            % (
                dev["vendor_id"],
                dev["product_id"],
                dev.get("product_string"),
                dev.get("manufacturer_string"),
                dev.get("serial_number"),
            )
        )
        print("    path=%s" % dev["path"])
        print(
            "    usage_page=0x%04X usage=0x%04X interface=%s"
            % (
                dev.get("usage_page", 0),
                dev.get("usage", 0),
                dev.get("interface_number"),
            )
        )


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


def show_descriptor(dev_info):
    try:
        dev = open_device(dev_info)
    except Exception as exc:  # noqa: BLE001
        print("could not open device: %s" % exc)
        return
    try:
        if not hasattr(dev, "get_report_descriptor"):
            print("report descriptor unavailable in this binding")
            return
        desc = bytes(dev.get_report_descriptor())
        print("report descriptor (%d bytes):" % len(desc))
        for offset in range(0, len(desc), 16):
            chunk = desc[offset : offset + 16]
            print("  %04X  %s" % (offset, " ".join("%02X" % b for b in chunk)))
    except Exception as exc:  # noqa: BLE001 - backend may not support it
        print("report descriptor unavailable: %s" % exc)
    finally:
        dev.close()


def dump(devices, seconds, show_all):
    if not devices:
        print("no devices found (check VID filter and USB mode switch)")
        return
    for dev_info in devices:
        print()
        print(
            "=== dumping vid=0x%04X pid=0x%04X %r ==="
            % (
                dev_info["vendor_id"],
                dev_info["product_id"],
                dev_info.get("product_string"),
            )
        )
        show_descriptor(dev_info)

        try:
            dev = open_device(dev_info)
            dev.set_nonblocking(0)
        except Exception as exc:  # noqa: BLE001
            print("could not open for reading: %s" % exc)
            continue

        deadline = time.monotonic() + seconds if seconds else None
        previous = None
        frames = 0
        start = time.monotonic()
        try:
            while True:
                if deadline is not None and time.monotonic() >= deadline:
                    break
                data = dev.read(64, 200)
                if not data:
                    continue
                frames += 1
                current = bytes(data)
                if not show_all and current == previous:
                    continue
                previous = current
                elapsed = (time.monotonic() - start) * 1000.0
                print(
                    "t=%8.1fms n=%5d len=%2d  %s"
                    % (elapsed, frames, len(current), current.hex(" "))
                )
        except KeyboardInterrupt:
            print("\nstopped by user")
        finally:
            dev.close()
        print("total frames read: %d" % frames)


def main():
    parser = argparse.ArgumentParser(
        description="Dump raw HID reports from a Thrustmaster wheel."
    )
    parser.add_argument("--list", action="store_true", help="list matching devices")
    parser.add_argument("--dump", action="store_true", help="dump input reports")
    parser.add_argument(
        "--vid",
        type=lambda value: int(value, 0),
        default=THRUSTMASTER_VID,
        help="vendor id filter, 0 disables (default 0x044F)",
    )
    parser.add_argument(
        "--pid",
        type=lambda value: int(value, 0),
        default=0,
        help="product id filter, 0 disables (default 0)",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="print every frame, not only changed frames",
    )
    parser.add_argument(
        "--seconds",
        type=float,
        default=0,
        help="stop after N seconds, 0 runs until Ctrl+C",
    )
    args = parser.parse_args()

    if not args.list and not args.dump:
        parser.print_help()
        return

    devices = enumerate_devices(args.vid, args.pid)
    if args.list:
        show_list(devices)
    if args.dump:
        dump(devices, args.seconds, args.all)


if __name__ == "__main__":
    main()
