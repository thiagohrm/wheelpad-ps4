#!/usr/bin/env python3
"""Send wheel reports from the laptop to the PS4 plugin over UDP.

The wheel reports only when something changes, so this keeps the last report
and resends it at a fixed rate. The plugin then always has a current state and
can drop the wheel cleanly when packets stop.

Install:
    python -m pip install --user hidapi

Usage:
    python tools/wheel_bridge.py --host 192.168.1.17
    python tools/wheel_bridge.py --host 192.168.1.17 --replay docs/captures/20261118-190530_ps3_separate.json
"""

import argparse
import json
import socket
import struct
import sys
import time
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
KNOWN_PIDS = (0xB655, 0xB658)
REPORT_LEN = 19
MAGIC = b"VLNT"
DEFAULT_PORT = 9107
SEND_INTERVAL = 0.008  # 125 Hz, twice the console pad poll rate
IDLE_NOTICE = 2.0


def find_device():
    for info in hid.enumerate():
        if info["vendor_id"] != THRUSTMASTER_VID:
            continue
        if info["product_id"] in KNOWN_PIDS:
            return info
    for info in hid.enumerate():
        if info["vendor_id"] == THRUSTMASTER_VID:
            return info
    return None


def open_device(info):
    dev = hid.device()
    try:
        dev.open_path(info["path"])
        return dev
    except Exception:  # noqa: BLE001 - retry by vid/pid
        dev.close()
    dev = hid.device()
    dev.open(info["vendor_id"], info["product_id"], info.get("serial_number") or None)
    return dev


def describe(frame):
    return "steer=%3d brake=%3d throttle=%3d buttons=0x%02X%02X hat=0x%02X" % (
        frame[3],
        frame[4],
        frame[5],
        frame[1],
        frame[0],
        frame[2] & 0x0F,
    )


def packet(seq, frame):
    return MAGIC + struct.pack("<HBB", seq & 0xFFFF, REPORT_LEN, 0) + frame


def run_hardware(args, sock):
    info = find_device()
    if info is None:
        sys.exit("no Thrustmaster device found; check the USB cable and the mode switch")
    print(
        "device: vid=0x%04X pid=0x%04X %r"
        % (info["vendor_id"], info["product_id"], info.get("product_string"))
    )
    dev = open_device(info)
    dev.set_nonblocking(1)

    seq = 0
    last = None
    last_send = 0.0
    last_notice = time.monotonic()
    frames = 0
    started = time.monotonic()

    while True:
        data = dev.read(64)
        if data:
            frame = bytes(data)[:REPORT_LEN]
            if len(frame) == REPORT_LEN:
                last = frame
                frames += 1

        now = time.monotonic()
        if last is not None and now - last_send >= SEND_INTERVAL:
            sock.sendto(packet(seq, last), (args.host, args.port))
            seq += 1
            last_send = now

        if now - last_notice >= IDLE_NOTICE:
            elapsed = now - started
            state = describe(last) if last else "waiting for first report"
            print("%5.1fs  %6.1f Hz  %s" % (elapsed, frames / elapsed, state))
            last_notice = now


def run_replay(args, sock):
    path = Path(args.replay)
    capture = json.loads(path.read_text(encoding="utf-8"))
    frames = [bytes.fromhex(capture["baseline"].replace(" ", ""))]
    for item in capture["steps"]:
        frames.append(bytes.fromhex(item["frame"].replace(" ", "")))

    print("replaying %d frames from %s" % (len(frames), path.name))
    seq = 0
    for frame in frames:
        if len(frame) != REPORT_LEN:
            continue
        for _ in range(int(args.hold / SEND_INTERVAL)):
            sock.sendto(packet(seq, frame), (args.host, args.port))
            seq += 1
            time.sleep(SEND_INTERVAL)
        print("  %s" % describe(frame))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="192.168.1.17", help="PS4 address")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--replay", help="send the frames of a capture file instead of hardware")
    parser.add_argument("--hold", type=float, default=1.5, help="seconds per replayed frame")
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        if args.replay:
            run_replay(args, sock)
        else:
            run_hardware(args, sock)
    except KeyboardInterrupt:
        print()
        print("stopped")
    finally:
        sock.close()


if __name__ == "__main__":
    main()
