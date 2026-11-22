#!/usr/bin/env python3
"""Convert the simple stroked SVG icons exported from the design to Android vector drawables.

    python3 android/tools/svg_to_vector.py icon.svg android/app/src/main/res/drawable/ic_icon.xml

Handles what the design uses: paths with a stroke and round caps and joins. The colour is written as black so the
icon can be tinted in code.
"""
import re
import sys
import xml.etree.ElementTree as ET


def convert(svg_path):
    root = ET.parse(svg_path).getroot()
    ns = {"s": "http://www.w3.org/2000/svg"}
    w, h = root.get("width"), root.get("height")
    vb = (root.get("viewBox") or f"0 0 {w} {h}").split()
    out = ['<?xml version="1.0" encoding="utf-8"?>',
           '<vector xmlns:android="http://schemas.android.com/apk/res/android"',
           f'    android:width="{w}dp"', f'    android:height="{h}dp"',
           f'    android:viewportWidth="{vb[2]}"', f'    android:viewportHeight="{vb[3]}">']
    for p in root.iter("{%s}path" % ns["s"]):
        d = re.sub(r"\s+", " ", p.get("d", "")).strip()
        sw = p.get("stroke-width", "2")
        out += ["    <path", '        android:fillColor="#00000000"', f'        android:pathData="{d}"',
                '        android:strokeColor="#FF000000"', f'        android:strokeWidth="{sw}"',
                '        android:strokeLineCap="round"', '        android:strokeLineJoin="round" />']
    out.append("</vector>")
    return "\n".join(out) + "\n"


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    open(sys.argv[2], "w").write(convert(sys.argv[1]))
