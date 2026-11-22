"""Measure PDF size per page at fixed JPEG quality, and per-stage latency, with lipi-cli.

    python tools/bench_pdf.py --cli ../build/cli/lipi-cli --model ../models/page-net/page-net \
        --corpus data/real-2000/val.jsonl --out reports/pdf-size.json

Every image is scanned into a one-page PDF for each setting; the report gives the mean and
median size in KB. Latency comes from `lipi-cli bench` on the first --bench-images images.
"""
import argparse
import json
import os
import statistics
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from lipi_eval.corpus import read_jsonl  # noqa: E402

SETTINGS = [("auto", 60), ("auto", 80), ("auto", 90), ("color", 80), ("gray", 80), ("bw", 80)]


def pdf_kb(cli, model, image, filter_name, quality, tmp):
    out = os.path.join(tmp, "page.pdf")
    cmd = [cli, "pdf", image, "-o", out, "--filter", filter_name, "--quality", str(quality)] + (["--model", model] if model else [])
    if subprocess.run(cmd, capture_output=True).returncode != 0:
        return None
    return os.path.getsize(out) / 1024.0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cli", required=True)
    ap.add_argument("--model", help="ncnn model prefix")
    ap.add_argument("--corpus", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--bench-images", type=int, default=5)
    args = ap.parse_args()
    root = os.path.dirname(os.path.abspath(args.corpus))
    rows = read_jsonl(args.corpus)
    report = {"images": len(rows), "image_size": [rows[0]["width"], rows[0]["height"]], "pdf_kb_per_page": {}, "latency_ms": {}}
    with tempfile.TemporaryDirectory() as tmp:
        for filter_name, quality in SETTINGS:
            sizes = [pdf_kb(args.cli, args.model, os.path.join(root, r["image"]), filter_name, quality, tmp) for r in rows]
            sizes = [s for s in sizes if s is not None]
            report["pdf_kb_per_page"][f"{filter_name}-q{quality}"] = {
                "pages": len(sizes), "mean": round(statistics.mean(sizes), 1), "median": round(statistics.median(sizes), 1)}
            print(filter_name, quality, report["pdf_kb_per_page"][f"{filter_name}-q{quality}"], flush=True)
    stages = {}
    for r in rows[: args.bench_images]:
        cmd = [args.cli, "bench", os.path.join(root, r["image"]), "--json", "--repeat", "20"] + (["--model", args.model] if args.model else [])
        out = subprocess.run(cmd, capture_output=True, text=True)
        if out.returncode != 0:
            continue
        for k, v in json.loads(out.stdout).items():
            if isinstance(v, dict):
                stages.setdefault(k, []).append(v["median_ms"])
    report["latency_ms"] = {k: round(statistics.median(v), 2) for k, v in stages.items()}
    with open(args.out, "w") as f:
        json.dump(report, f, indent=2, sort_keys=True)
        f.write("\n")
    print("latency (median over images, ms):", report["latency_ms"])


if __name__ == "__main__":
    main()
