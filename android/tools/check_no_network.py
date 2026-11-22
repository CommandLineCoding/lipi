#!/usr/bin/env python3
"""Fail if an APK asks for any network permission.

    python3 android/tools/check_no_network.py app/build/outputs/apk/debug/app-debug.apk

Lipi promises that it works offline and never asks for the network. This reads the permissions from the built
APK itself (with aapt2), so it also catches permissions added by a library's manifest.
"""
import glob
import os
import subprocess
import sys

FORBIDDEN = {
    "android.permission.INTERNET",
    "android.permission.ACCESS_NETWORK_STATE",
    "android.permission.ACCESS_WIFI_STATE",
    "android.permission.CHANGE_NETWORK_STATE",
    "android.permission.CHANGE_WIFI_STATE",
}


def find_aapt2():
    roots = [os.environ.get("ANDROID_HOME"), os.environ.get("ANDROID_SDK_ROOT"), os.path.expanduser("~/Android/Sdk")]
    for root in filter(None, roots):
        found = sorted(glob.glob(os.path.join(root, "build-tools", "*", "aapt2")))
        if found:
            return found[-1]
    sys.exit("aapt2 not found; set ANDROID_HOME")


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    apk = sys.argv[1]
    out = subprocess.run([find_aapt2(), "dump", "permissions", apk], capture_output=True, text=True, check=True).stdout
    perms = sorted({line.split("name='")[1].split("'")[0] for line in out.splitlines() if "name='" in line})
    print("permissions:", ", ".join(perms) or "none")
    bad = [p for p in perms if p in FORBIDDEN or p.endswith(".INTERNET")]
    if bad:
        print("FAIL: the app asks for network permissions:", ", ".join(bad))
        sys.exit(1)
    print("ok: no network permission")


if __name__ == "__main__":
    main()
