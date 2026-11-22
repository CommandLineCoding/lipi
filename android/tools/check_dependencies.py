#!/usr/bin/env python3
"""Fail if the app depends on anything F-Droid would reject.

    cd android && ./gradlew :app:dependencies --configuration releaseRuntimeClasspath | python3 tools/check_dependencies.py

Reads Gradle's dependency tree from stdin. Proprietary SDKs (Google Play services, Firebase, ML Kit, analytics,
ads, billing) and anything under a (A)GPL licence must not be bundled; the project is MIT. The full list of
coordinates is printed so a reviewer can check the licences of anything new.
"""
import re
import sys

DENY_PREFIXES = (
    "com.google.android.gms", "com.google.firebase", "com.google.mlkit", "com.google.android.play",
    "com.android.billingclient", "com.google.android.ump", "com.google.android.datatransport", "io.fabric",
    "com.crashlytics", "com.facebook", "com.appsflyer", "com.adjust", "com.amplitude", "com.mixpanel",
    "io.sentry", "com.bugsnag", "com.microsoft.appcenter", "com.onesignal", "com.mapbox",
    "com.itextpdf", "com.artifex.mupdf", "com.ultralytics",
)
COORD = re.compile(r"([A-Za-z0-9_.\-]+):([A-Za-z0-9_.\-]+):([A-Za-z0-9_.\-]+)")


def main():
    seen = set()
    for line in sys.stdin:
        if not re.match(r"^[|+\\ -]*[+\\]--- ", line):
            continue
        m = COORD.search(line.split("--- ", 1)[1])
        if m:
            seen.add(m.groups())
    if not seen:
        sys.exit("no dependencies found in the input; was the right Gradle configuration used?")
    bad = sorted(c for c in seen if c[0].startswith(DENY_PREFIXES))
    for g, a, v in sorted(seen):
        print(f"{g}:{a}:{v}")
    print(f"\n{len(seen)} dependencies")
    if bad:
        print("FAIL: not allowed (proprietary or incompatible licence):", ", ".join(":".join(c) for c in bad))
        sys.exit(1)
    print("ok: no denied dependency")


if __name__ == "__main__":
    main()
