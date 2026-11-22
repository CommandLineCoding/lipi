"""Fetch real phone photos with corner labels for the validation and test splits.

    python tools/fetch_mendeley.py --out /data/lipi-real --count 400

Source: "A New Image Dataset for Document Corner Localization" (Soheili,
Mansouri, Baniadam Dizaj, 2020), https://data.mendeley.com/datasets/x3nm4cxr83,
licensed CC BY 4.0. Please cite the paper when you use it. Images are
downloaded, checked against the published SHA-256, rotated upright using EXIF
and shrunk so that the long side is --size pixels. The originals are not kept.
The dataset is only ever used for validation and testing, never for training.
"""
import argparse
import csv
import hashlib
import io
import json
import os
import random
import sys
import urllib.request
from concurrent.futures import ThreadPoolExecutor

from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from lipi_eval.corpus import write_jsonl  # noqa: E402
from lipi_eval.quad import sort_quad  # noqa: E402

API = "https://data.mendeley.com/public-api/datasets/x3nm4cxr83"
LICENSE = "CC BY 4.0 (Soheili et al. 2020, doi:10.17632/x3nm4cxr83.3)"


HEADERS = {"User-Agent": "lipi-fetch/0.1 (+https://github.com/CommandLineCoding/lipi)"}


def fetch(url, sha256=None):
    with urllib.request.urlopen(urllib.request.Request(url, headers=HEADERS), timeout=120) as r:
        data = r.read()
    if sha256 and hashlib.sha256(data).hexdigest() != sha256:
        raise ValueError(f"checksum mismatch for {url}")
    return data


def to_display_space(points, orientation, raw_w, raw_h):
    """Map points from stored pixel space to the EXIF-corrected image."""
    out = []
    for x, y in points:
        if orientation == 3:
            out.append((raw_w - x, raw_h - y))
        elif orientation == 6:
            out.append((raw_h - y, x))
        elif orientation == 8:
            out.append((y, raw_w - x))
        else:
            out.append((x, y))
    return out


def inside(points, w, h, slack=2):
    return all(-slack <= x <= w + slack and -slack <= y <= h + slack for x, y in points)


def corners_in_display_space(points, orientation, raw_w, raw_h):
    """The CSV labels may use stored or EXIF-corrected coordinates; pick the one that fits."""
    if orientation in (3, 6, 8):
        mapped = to_display_space(points, orientation, raw_w, raw_h)
        disp_w, disp_h = (raw_h, raw_w) if orientation in (6, 8) else (raw_w, raw_h)
        if inside(points, raw_w, raw_h) and inside(mapped, disp_w, disp_h):
            if orientation == 3:
                return mapped, "stored"  # both fit; assume the stored space like the other cases
            if not inside(points, disp_w, disp_h):
                return mapped, "stored"
            return None, "ambiguous"
        if inside(mapped, disp_w, disp_h):
            return mapped, "stored"
        if inside(points, disp_w, disp_h):
            return list(points), "display"
        return None, "outside"
    return (list(points), "stored") if inside(points, raw_w, raw_h) else (None, "outside")


def process(entry, size, out_dir):
    name, jpg, csv_file = entry
    img_bytes = fetch(jpg["content_details"]["download_url"], jpg["content_details"].get("sha256_hash"))
    csv_text = fetch(csv_file["content_details"]["download_url"]).decode("utf-8", "replace")
    row = next(csv.reader(io.StringIO(csv_text)))
    pts = [(float(row[i]), float(row[i + 1])) for i in range(0, 8, 2)]
    img = Image.open(io.BytesIO(img_bytes))
    orientation = img.getexif().get(274, 1)
    raw_w, raw_h = img.size
    quad, how = corners_in_display_space(pts, orientation, raw_w, raw_h)
    if quad is None:
        return None, f"{name}: skipped ({how})"
    from PIL import ImageOps

    img = ImageOps.exif_transpose(img).convert("RGB")
    scale = size / max(img.size)
    small = img.resize((max(1, round(img.width * scale)), max(1, round(img.height * scale))), Image.LANCZOS)
    quad = sort_quad([(x * scale, y * scale) for x, y in quad])
    rel = f"images/real-{name}.jpg"
    small.save(os.path.join(out_dir, rel), quality=92)
    return {"id": f"real-{name}", "image": rel, "width": small.width, "height": small.height,
            "quad": [[round(float(x), 2), round(float(y), 2)] for x, y in quad],
            "orientation": None, "source": "real", "license": LICENSE, "kind": "photo"}, None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", required=True)
    ap.add_argument("--count", type=int, default=400)
    ap.add_argument("--size", type=int, default=512)
    ap.add_argument("--val", type=int, default=100, help="how many of the images go to the validation split")
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--jobs", type=int, default=8)
    args = ap.parse_args()

    with urllib.request.urlopen(urllib.request.Request(API, headers={**HEADERS, "Accept": "application/json"}), timeout=60) as r:
        meta = json.load(r)
    files = {f["filename"]: f for f in meta["files"]}
    names = sorted(n[:-4] for n in files if n.endswith(".jpg") and n[:-4] + ".csv" in files)
    random.Random(args.seed).shuffle(names)
    names = names[: args.count]
    os.makedirs(os.path.join(args.out, "images"), exist_ok=True)
    entries = [(n.strip("()"), files[n + ".jpg"], files[n + ".csv"]) for n in names]
    rows, skipped = [], []
    with ThreadPoolExecutor(args.jobs) as pool:
        for row, why in pool.map(lambda e: process(e, args.size, args.out), entries):
            (rows.append(row) if row else skipped.append(why))
    rows.sort(key=lambda r: r["id"])
    order = {n.strip("()"): i for i, n in enumerate(names)}
    rows.sort(key=lambda r: order[r["id"][5:]])
    val = [dict(r, split="val") for r in rows[: args.val]]
    test = [dict(r, split="test") for r in rows[args.val:]]
    write_jsonl(os.path.join(args.out, "val.jsonl"), val)
    write_jsonl(os.path.join(args.out, "test.jsonl"), test)
    print(f"{len(val)} val, {len(test)} test, {len(skipped)} skipped")
    for s in skipped[:10]:
        print("  ", s)


if __name__ == "__main__":
    main()
