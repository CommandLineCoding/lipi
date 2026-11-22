"""Run lipi-cli detect over a corpus and write predictions for lipi_eval.score.

    python tools/predict_cli.py --cli ../build/cli/lipi-cli \
        --corpus data/real/test.jsonl --out preds.jsonl
"""
import argparse
import json
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from lipi_eval.corpus import read_jsonl, write_jsonl  # noqa: E402


def detect(cli, root, row):
    proc = subprocess.run([cli, "detect", os.path.join(root, row["image"]), "--json"], capture_output=True, text=True)
    pred = {"id": row["id"], "quad": None, "orientation": None, "ms": None}
    if proc.stdout.strip():
        out = json.loads(proc.stdout)
        pred["quad"], pred["ms"] = out["quad"], out["ms"]
    elif proc.returncode not in (0, 3):
        print(f"{row['id']}: lipi-cli exited with {proc.returncode}: {proc.stderr.strip()}", file=sys.stderr)
    return pred


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cli", required=True, help="path to the lipi-cli binary")
    ap.add_argument("--corpus", required=True, help="a split file such as data/real/test.jsonl")
    ap.add_argument("--out", required=True)
    ap.add_argument("--jobs", type=int, default=4)
    args = ap.parse_args()
    root = os.path.dirname(os.path.abspath(args.corpus))
    rows = read_jsonl(args.corpus)
    with ThreadPoolExecutor(args.jobs) as pool:
        preds = list(pool.map(lambda r: detect(args.cli, root, r), rows))
    write_jsonl(args.out, preds)
    print(f"wrote {len(preds)} predictions to {args.out}")


if __name__ == "__main__":
    main()
