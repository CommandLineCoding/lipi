"""Score page-detector predictions against a corpus.

    python -m lipi_eval.score --corpus corpus/test.jsonl --pred preds.jsonl

Predictions are JSON lines: {"id": ..., "quad": [[x, y] * 4] or null,
"orientation": 0-3 or null, "ms": float or null}. A missing or null quad counts
as a failure with IoU 0. The report is JSON with sorted keys and rounded
numbers so that CI can diff it. When the corpus rows carry a "kind" (document, receipt, id_card,
notes, whiteboard) the report also has a "kinds" section with the same numbers per kind.
"""
import argparse
import json
import sys

import numpy as np

from .corpus import read_jsonl
from .quad import corner_error, quad_iou


def _stats(values):
    if not values:
        return {"mean": None, "median": None, "p95": None}
    a = np.asarray(values, dtype=float)
    return {
        "mean": round(float(a.mean()), 5),
        "median": round(float(np.median(a)), 5),
        "p95": round(float(np.percentile(a, 95)), 5),
    }


def score(corpus, preds):
    by_id = {p["id"]: p for p in preds}
    groups = {}
    for row in corpus:
        g = groups.setdefault(row["source"], {"err": [], "iou": [], "ms": [], "orient": [], "n": 0, "fail": 0})
        g["n"] += 1
        p = by_id.get(row["id"])
        if p is None or p.get("quad") is None:
            g["fail"] += 1
            g["iou"].append(0.0)
            continue
        g["err"].append(corner_error(p["quad"], row["quad"], row["width"], row["height"]))
        g["iou"].append(quad_iou(p["quad"], row["quad"]))
        if p.get("ms") is not None:
            g["ms"].append(float(p["ms"]))
        if p.get("orientation") is not None and row["orientation"] is not None:
            g["orient"].append(1.0 if p["orientation"] == row["orientation"] else 0.0)
    kinds = {}
    for row in corpus:
        if row.get("kind"):
            p = by_id.get(row["id"])
            g = kinds.setdefault(f"{row['source']}/{row['kind']}", {"err": [], "iou": [], "ms": [], "orient": [], "n": 0, "fail": 0})
            g["n"] += 1
            if p is None or p.get("quad") is None:
                g["fail"] += 1
                g["iou"].append(0.0)
                continue
            g["err"].append(corner_error(p["quad"], row["quad"], row["width"], row["height"]))
            g["iou"].append(quad_iou(p["quad"], row["quad"]))
            if p.get("ms") is not None:
                g["ms"].append(float(p["ms"]))
            if p.get("orientation") is not None and row["orientation"] is not None:
                g["orient"].append(1.0 if p["orientation"] == row["orientation"] else 0.0)
    report = {"sources": {}, "all": None}
    if kinds:
        report["kinds"] = {name: _summary(kinds[name]) for name in sorted(kinds)}
    total = {"err": [], "iou": [], "ms": [], "orient": [], "n": 0, "fail": 0}
    for name in sorted(groups):
        g = groups[name]
        report["sources"][name] = _summary(g)
        for k in total:
            total[k] = total[k] + g[k] if isinstance(g[k], list) else total[k] + g[k]
    report["all"] = _summary(total)
    return report


def _summary(g):
    iou = np.asarray(g["iou"], dtype=float)
    out = {
        "images": g["n"],
        "failures": g["fail"],
        "corner_error": _stats(g["err"]),
        "iou": _stats(g["iou"]),
        "iou_ge_90": round(float((iou >= 0.90).mean()), 4) if len(iou) else None,
        "iou_ge_95": round(float((iou >= 0.95).mean()), 4) if len(iou) else None,
        "latency_ms": _stats(g["ms"]),
        "orientation_accuracy": round(float(np.mean(g["orient"])), 4) if g["orient"] else None,
    }
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--corpus", required=True)
    ap.add_argument("--pred", required=True)
    ap.add_argument("--out", help="write the report here instead of stdout")
    args = ap.parse_args(argv)
    report = score(read_jsonl(args.corpus), read_jsonl(args.pred))
    text = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.out:
        with open(args.out, "w") as f:
            f.write(text)
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()
