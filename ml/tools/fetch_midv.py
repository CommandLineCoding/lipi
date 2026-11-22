"""Turn the MIDV frames in mapo80/DocCornerDataset into a corpus split.

    python tools/fetch_midv.py --out data/midv --count 2400 \
        --against data/real/val.jsonl data/real/test.jsonl data/real-train/train.jsonl

Only the rows whose filename starts with midv500_ or midv2019_ are used, because they are the only part of that
dataset whose upstream terms we could confirm: the MIDV authors license both under CC BY-SA 2.5
(ftp://smartengines.com/midv-500/license.txt). See models/datasets/lipi-corpus.md for what was excluded and why.
The images are not redistributed; this tool only builds a local training split.

Steps: read the corner labels of every shard, take a seeded sample (the frames of one video are near duplicates, so
a sample is enough), download the shards that hold the sample one at a time, shrink each image so its long side is
--size pixels, and drop any image that looks like one of the photos in the --against corpora (a perceptual hash, so
the held-out real photos stay honest).
"""
import argparse
import io
import json
import os
import random
import sys
import urllib.request

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from lipi_eval.corpus import read_jsonl, write_jsonl  # noqa: E402
from lipi_eval.quad import polygon_area, sort_quad  # noqa: E402

REPO = "mapo80/DocCornerDataset"
PREFIXES = ("midv500_", "midv2019_")
LICENSE = "CC BY-SA 2.5 (Arlazarov et al. 2019, MIDV-500 and MIDV-2019)"
CORNERS = ["corner_tl_x", "corner_tl_y", "corner_tr_x", "corner_tr_y", "corner_br_x", "corner_br_y", "corner_bl_x", "corner_bl_y"]
HEADERS = {"User-Agent": "lipi-fetch/0.1 (+https://github.com/CommandLineCoding/lipi)"}


def dhash(img):
    """64-bit difference hash: robust to resizing and recompression."""
    small = np.asarray(img.convert("L").resize((9, 8), Image.LANCZOS), dtype=np.int16)
    bits = (small[:, 1:] > small[:, :-1]).flatten()
    return int("".join("1" if b else "0" for b in bits), 2)


def hamming(a, b):
    return bin(a ^ b).count("1")


def usable_quad(corners):
    """The page must be fully in view and take up a sensible part of the frame (corners are 0-1 fractions)."""
    q = np.asarray(corners, dtype=float).reshape(4, 2)
    if not np.all(np.isfinite(q)) or q.min() < -0.002 or q.max() > 1.002:
        return False
    return abs(polygon_area(sort_quad(q))) >= 0.02


def reference_hashes(jsonl_paths):
    hashes = []
    for path in jsonl_paths:
        root = os.path.dirname(os.path.abspath(path))
        for row in read_jsonl(path):
            with Image.open(os.path.join(root, row["image"])) as im:
                hashes.append(dhash(im))
    return hashes


def list_shards():
    shards = []
    for split in ("train", "val", "test"):
        req = urllib.request.Request(f"https://huggingface.co/api/datasets/{REPO}/tree/main/{split}", headers=HEADERS)
        shards += sorted(i["path"] for i in json.load(urllib.request.urlopen(req, timeout=60)) if i["path"].endswith(".parquet"))
    return shards


def main():
    import fsspec
    import pyarrow.parquet as pq

    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", required=True)
    ap.add_argument("--count", type=int, default=2400)
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--size", type=int, default=512)
    ap.add_argument("--against", nargs="*", default=[], help="corpus .jsonl files whose images must not appear here")
    ap.add_argument("--max-hamming", type=int, default=8, help="dHash distance (of 64 bits) counted as the same image")
    args = ap.parse_args()

    fs = fsspec.filesystem("https", headers=HEADERS)
    url = lambda path: f"https://huggingface.co/datasets/{REPO}/resolve/main/{path}"  # noqa: E731
    refs = reference_hashes(args.against)
    print(f"{len(refs)} reference images to keep clear of", flush=True)

    candidates, dropped = [], {"negative": 0, "other source": 0, "corners outside or tiny": 0}
    for shard in list_shards():
        with fs.open(url(shard), "rb") as fh:
            t = pq.read_table(fh, columns=["filename", "is_negative"] + CORNERS).to_pydict()
        for i, name in enumerate(t["filename"]):
            if not name.startswith(PREFIXES):
                dropped["other source"] += 1
            elif t["is_negative"][i]:
                dropped["negative"] += 1
            elif not usable_quad([t[c][i] for c in CORNERS]):
                dropped["corners outside or tiny"] += 1
            else:
                candidates.append((shard, i, name, [t[c][i] for c in CORNERS]))
    print(f"{len(candidates)} usable MIDV rows; dropped {dropped}", flush=True)

    random.Random(args.seed).shuffle(candidates)
    chosen = candidates[: args.count]
    by_shard = {}
    for c in chosen:
        by_shard.setdefault(c[0], []).append(c)

    os.makedirs(os.path.join(args.out, "images"), exist_ok=True)
    rows, overlap = [], []
    for n, (shard, items) in enumerate(sorted(by_shard.items()), 1):
        with fs.open(url(shard), "rb") as fh:
            images = pq.read_table(fh, columns=["image"]).column("image").to_pylist()
        for _, i, name, corners in items:
            img = Image.open(io.BytesIO(images[i]["bytes"])).convert("RGB")
            h = dhash(img)
            if any(hamming(h, r) <= args.max_hamming for r in refs):
                overlap.append(name)
                continue
            w0, h0 = img.size
            scale = args.size / max(w0, h0)
            small = img.resize((max(1, round(w0 * scale)), max(1, round(h0 * scale))), Image.LANCZOS)
            quad = sort_quad(np.asarray(corners, dtype=float).reshape(4, 2) * np.array([small.width, small.height]))
            stem = os.path.splitext(name)[0].replace("_", "-")
            rel = f"images/{stem}.jpg"
            small.save(os.path.join(args.out, rel), quality=92)
            rows.append({"id": stem, "image": rel, "width": small.width, "height": small.height,
                         "quad": [[round(float(x), 2), round(float(y), 2)] for x, y in quad], "orientation": None,
                         "source": "midv", "license": LICENSE, "split": "train", "kind": "id_card", "origin": name})
        print(f"shard {n}/{len(by_shard)} done, {len(rows)} kept", flush=True)
    rows.sort(key=lambda r: r["id"])
    write_jsonl(os.path.join(args.out, "train.jsonl"), rows)
    with open(os.path.join(args.out, "report.json"), "w") as f:
        json.dump({"usable_rows": len(candidates), "dropped": dropped, "sampled": len(chosen), "kept": len(rows),
                   "overlap_with_held_out": overlap, "seed": args.seed, "max_hamming": args.max_hamming,
                   "reference_images": len(refs)}, f, indent=2)
    print(f"kept {len(rows)}, dropped {len(overlap)} that matched the reference photos")


if __name__ == "__main__":
    main()
