#!/usr/bin/env python3
"""Fail if a native library in the APK is not 16 KB page-aligned.

    python3 android/tools/check_alignment.py app/build/outputs/apk/debug/app-debug.apk

Android 15 and the Play Store need every ELF segment of a native library aligned to 16384 bytes. This unpacks the
libraries and reads the alignment of each loadable segment with readelf.
"""
import re
import subprocess
import sys
import tempfile
import zipfile

REQUIRED = 0x4000


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    failed = False
    with zipfile.ZipFile(sys.argv[1]) as z, tempfile.TemporaryDirectory() as tmp:
        libs = [n for n in z.namelist() if n.startswith("lib/") and n.endswith(".so")]
        if not libs:
            sys.exit("no native libraries found in the APK")
        for name in libs:
            path = z.extract(name, tmp)
            out = subprocess.run(["readelf", "-lW", path], capture_output=True, text=True, check=True).stdout
            aligns = [int(m.group(1), 16) for m in re.finditer(r"^\s*LOAD\s.*\s(0x[0-9a-fA-F]+)\s*$", out, re.M)]
            ok = bool(aligns) and all(a >= REQUIRED for a in aligns)
            print(("ok  " if ok else "FAIL"), name, [hex(a) for a in aligns])
            failed |= not ok
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
