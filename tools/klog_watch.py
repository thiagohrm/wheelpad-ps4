#!/usr/bin/env python3
"""Stream the console's klog to a file and show matches.

GoldHEN exposes klog on TCP 3232. The stream is continuous, so this writes
everything it receives to a file and prints only the lines matching the filter.
That keeps a full record for later without flooding the terminal.

Usage:
    python tools/klog_watch.py                       # 60 s, no filter
    python tools/klog_watch.py --seconds 300 --grep wheelpad
    python tools/klog_watch.py --out %TEMP%/klog.log --grep "wheelpad|usb"
"""

import argparse
import os
import re
import socket
import sys
import time
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("host", nargs="?", default=os.environ.get("WHEELPAD_HOST", ""))
    parser.add_argument("--port", type=int, default=3232)
    parser.add_argument("--seconds", type=float, default=60.0, help="0 means until interrupted")
    parser.add_argument("--grep", help="regex of lines to print")
    parser.add_argument("--out", default="build/klog.log")
    args = parser.parse_args()

    if not args.host:
        sys.exit("no host: pass it as the first argument or set WHEELPAD_HOST")

    pattern = re.compile(args.grep, re.IGNORECASE) if args.grep else None
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)

    sock = socket.create_connection((args.host, args.port), timeout=5)
    sock.settimeout(1.0)
    deadline = None if args.seconds == 0 else time.monotonic() + args.seconds

    lines = 0
    buffer = b""
    started = time.monotonic()
    print("klog: %s:%d -> %s" % (args.host, args.port, out))

    with out.open("ab") as handle:
        try:
            while deadline is None or time.monotonic() < deadline:
                try:
                    chunk = sock.recv(16384)
                except socket.timeout:
                    continue
                if not chunk:
                    break
                handle.write(chunk)
                handle.flush()
                buffer += chunk
                while b"\n" in buffer:
                    raw, buffer = buffer.split(b"\n", 1)
                    text = raw.decode("utf-8", "replace").rstrip("\r")
                    lines += 1
                    if pattern is None or pattern.search(text):
                        print(text)
        except KeyboardInterrupt:
            pass
        finally:
            sock.close()

    print("klog: %d lines in %.0fs" % (lines, time.monotonic() - started))
    return 0


if __name__ == "__main__":
    sys.exit(main())
