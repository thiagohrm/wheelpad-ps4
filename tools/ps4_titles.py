#!/usr/bin/env python3
"""List installed PS4 title IDs over GoldHEN's FTP server.

    python tools/ps4_titles.py [host] [port]

Title names live in the system app.db, which needs SQLite access. This only
lists the IDs, which is what plugins.ini sections need.
"""

import os
import sys
from ftplib import FTP


def main():
    host = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("WHEELPAD_HOST", "")
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 2121
    if not host:
        sys.exit("no host: pass it as the first argument or set WHEELPAD_HOST")

    ftp = FTP()
    ftp.connect(host, port, timeout=10)
    ftp.login("anonymous", "x")
    ftp.set_pasv(True)

    entries = sorted(ftp.nlst("/user/app"))
    ftp.quit()

    ids = [entry.rstrip("/").split("/")[-1] for entry in entries]
    ids = [name for name in ids if name]
    for name in ids:
        print(name)
    print()
    print("%d installed title(s)" % len(ids))


if __name__ == "__main__":
    main()
