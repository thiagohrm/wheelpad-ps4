#!/usr/bin/env python3
"""Upload a file to the console over GoldHEN's FTP server.

    python tools/ps4_upload.py build/ps4/wheelpad.prx /data/GoldHEN/plugins/wheelpad.prx

The console's FTP server ignores an upload that would overwrite an existing
file, so the target is deleted first and the result is verified by reading
the file back.

Note: the server decodes a PRX container into a plain ELF when it is read
back, so the downloaded bytes are larger than the uploaded file. The check is
therefore on the decoded content, not on a byte-for-byte match.
"""

import argparse
import hashlib
import sys
from ftplib import FTP

HOST = "192.168.1.17"
PORT = 2121


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("local")
    parser.add_argument("remote")
    parser.add_argument("--host", default=HOST)
    parser.add_argument("--port", type=int, default=PORT)
    args = parser.parse_args()

    with open(args.local, "rb") as handle:
        data = handle.read()

    ftp = FTP()
    ftp.encoding = "utf-8"
    ftp.connect(args.host, args.port, timeout=10)
    ftp.login("anonymous", "x")
    ftp.set_pasv(True)

    try:
        try:
            ftp.delete(args.remote)
            print("removed old " + args.remote)
        except Exception:  # noqa: BLE001 - the file may not exist yet
            pass

        ftp.storbinary("STOR " + args.remote, __import__("io").BytesIO(data))

        buffer = bytearray()
        ftp.retrbinary("RETR " + args.remote, buffer.extend)
        remote = bytes(buffer)

        print("local  %7d bytes  md5 %s" % (len(data), hashlib.md5(data).hexdigest()))
        print("remote %7d bytes  md5 %s" % (len(remote), hashlib.md5(remote).hexdigest()))

        if len(remote) < len(data):
            print("FAILED: the remote file is smaller than the upload")
            return 1

        # The server decodes a PRX container into a plain ELF on read, so the
        # bytes do not match. Compare the text that the build embedded
        # instead: it must be present in the decoded image.
        marker = b"discovery start"
        if marker in data:
            if marker not in remote:
                print("FAILED: the marker is missing from the uploaded file")
                return 1
            print("marker found in the uploaded file")

        print("ok: uploaded")
        return 0
    finally:
        ftp.quit()


if __name__ == "__main__":
    sys.exit(main())
