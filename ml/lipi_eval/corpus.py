"""Corpus annotation format.

One JSON object per line:

    {"id": "syn-000001", "image": "images/syn-000001.jpg", "width": 512,
     "height": 384, "quad": [[x, y], [x, y], [x, y], [x, y]],
     "orientation": 0, "source": "synthetic", "license": "generated",
     "split": "test"}

`quad` is the page outline in image pixels, ordered top-left, top-right,
bottom-right, bottom-left as seen on the image. `orientation` is the number of
90 degree clockwise rotations that make the page upright once it has been
flattened with that quad (0 means it already is), or null when it was not
labelled (real photos).
"""
import json

REQUIRED = ("id", "image", "width", "height", "quad", "orientation", "source", "license", "split")


def read_jsonl(path):
    with open(path, encoding="utf-8") as f:
        return [json.loads(line) for line in f if line.strip()]


def write_jsonl(path, rows):
    with open(path, "w", encoding="utf-8") as f:
        for row in rows:
            f.write(json.dumps(row, ensure_ascii=False, separators=(",", ":")) + "\n")


def validate(row):
    missing = [k for k in REQUIRED if k not in row]
    if missing:
        raise ValueError(f"{row.get('id', '?')}: missing {missing}")
    if len(row["quad"]) != 4 or any(len(p) != 2 for p in row["quad"]):
        raise ValueError(f"{row['id']}: quad must be four [x, y] points")
    if row["orientation"] not in (None, 0, 1, 2, 3):
        raise ValueError(f"{row['id']}: orientation must be 0-3, or null when unknown")
    return row
