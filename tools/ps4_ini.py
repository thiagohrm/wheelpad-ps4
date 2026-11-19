#!/usr/bin/env python3
"""Read or edit the console's plugins.ini over FTP.

    python tools/ps4_ini.py show
    python tools/ps4_ini.py remove /data/GoldHEN/plugins/wheelpad.prx
    python tools/ps4_ini.py add CUSA01830 /data/GoldHEN/plugins/wheelpad.prx

Keeps the UTF-8 BOM and the existing line endings. A timestamped backup of the
previous content is written next to the file on first change.
"""

import io
import sys
import time
from ftplib import FTP

HOST = "192.168.1.17"
PORT = 2121
INI = "/data/GoldHEN/plugins.ini"


def connect():
    ftp = FTP()
    ftp.encoding = "utf-8"
    ftp.connect(HOST, PORT, timeout=10)
    ftp.login("anonymous", "x")
    ftp.set_pasv(True)
    return ftp


def read_text(ftp):
    buffer = io.BytesIO()
    ftp.retrbinary("RETR " + INI, buffer.write)
    return buffer.getvalue().decode("utf-8-sig")


def write_text(ftp, text, backup_suffix=""):
    if backup_suffix:
        ftp.storbinary(
            "STOR %s.%s" % (INI, backup_suffix),
            io.BytesIO(("\ufeff" + text).encode("utf-8")),
        )
    ftp.storbinary("STOR " + INI, io.BytesIO(("\ufeff" + text).encode("utf-8")))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2

    command = sys.argv[1]
    ftp = connect()
    try:
        text = read_text(ftp)
        if command == "show":
            print(text)
            return 0

        if command == "remove":
            if len(sys.argv) != 3:
                print("usage: remove <plugin path>")
                return 2
            plugin = sys.argv[2]
            kept = [line for line in text.splitlines() if line.strip() != plugin]
            if len(kept) == len(text.splitlines()):
                print("not listed: " + plugin)
                return 0
            write_text(ftp, "\n".join(kept) + "\n", time.strftime("%Y%m%d-%H%M%S"))
            print("removed " + plugin)
            return 0

        if command == "add":
            if len(sys.argv) != 4:
                print("usage: add <title id or default> <plugin path>")
                return 2
            section = sys.argv[2]
            plugin = sys.argv[3]
            lines = text.splitlines()
            header = "[%s]" % section
            try:
                index = [line.strip() for line in lines].index(header)
            except ValueError:
                index = None

            # Look only inside the target section: the same plugin can be
            # registered for more than one title.
            listed = False
            if index is not None:
                for line in lines[index + 1:]:
                    if line.strip().startswith("["):
                        break
                    if line.strip() == plugin:
                        listed = True
                        break

            if listed:
                print("already listed under %s" % header)
                return 0

            if index is not None:
                lines.insert(index + 1, plugin)
            else:
                lines += ["", header, plugin]
            write_text(ftp, "\n".join(lines) + "\n", time.strftime("%Y%m%d-%H%M%S"))
            print("added %s under %s" % (plugin, header))
            return 0
    finally:
        ftp.quit()

    print("unknown command: " + command)
    return 2


if __name__ == "__main__":
    sys.exit(main())
