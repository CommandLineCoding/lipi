"""Score a trained recogniser on the held-out test sets.

    python -m ocr.eval_rec --checkpoint runs/rec-s/best.pt --root ~/lipi-train/ocr --out reports/rec-s-test.json

The Mozhi test words were never used for training or for choosing the checkpoint. Reports the character error rate
(edit distance over the number of reference characters) and the share of exactly right words, in total and by word
length, and lists the words with the most errors, so the failures can be read and not only counted.
"""
import argparse
import glob
import json
import os
import random

import torch

from . import charset as cs
from .data import MozhiWords, collate
from .fonts import load_or_scan
from .model import RecNet, count_parameters
from .synth_text import Corpus, SyntheticSource
from .charset import edit_distance
from .train_rec import predict


@torch.no_grad()
def run(model, samples, charset, device, batch=256):
    model.eval()
    rows = []
    for i in range(0, len(samples), batch):
        part = samples[i:i + batch]
        images, steps, _, _, texts = collate(part, charset)
        with torch.autocast("cuda", dtype=torch.bfloat16):
            pred = predict(model, images, steps, charset, device)
        rows += [(t, p) for t, p in zip(texts, pred)]
    return rows


def summarise(rows):
    errors = sum(edit_distance(p, t) for t, p in rows)
    chars = sum(len(t) for t, _ in rows)
    buckets = {"1-3": [0, 0, 0], "4-6": [0, 0, 0], "7-10": [0, 0, 0], "11+": [0, 0, 0]}  # errors, chars, words
    for t, p in rows:
        b = "1-3" if len(t) <= 3 else "4-6" if len(t) <= 6 else "7-10" if len(t) <= 10 else "11+"
        buckets[b][0] += edit_distance(p, t)
        buckets[b][1] += len(t)
        buckets[b][2] += 1
    # An answer much shorter than the truth means the line was not read at all, which is worse than a few wrong letters.
    collapsed = sum(len(p) < 0.5 * len(t) for t, p in rows if len(t) >= 8)
    long_rows = sum(len(t) >= 8 for t, _ in rows)
    worst = sorted(((edit_distance(p, t), t, p) for t, p in rows), reverse=True)[:12]
    return {
        "words": len(rows), "characters": chars, "collapsed_share": collapsed / max(1, long_rows), "cer": errors / chars, "exact": sum(p == t for t, p in rows) / len(rows),
        "by_length": {k: {"words": v[2], "cer": v[0] / max(1, v[1])} for k, v in buckets.items()},
        "worst": [{"edits": e, "truth": t, "predicted": p} for e, t, p in worst],
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--checkpoint", required=True)
    ap.add_argument("--root", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--synthetic", type=int, default=5000, help="number of synthetic lines in the test set")
    ap.add_argument("--seed", type=int, default=98765, help="seed of the synthetic test lines (training uses others)")
    args = ap.parse_args()
    device = "cuda"
    ck = torch.load(args.checkpoint, map_location="cpu")
    charset = cs.Charset(ck["charset"])
    model = RecNet(len(charset), **ck.get("config", {})).to(device)
    model.load_state_dict(ck["model"])
    report = {"checkpoint_step": ck["step"], "parameters": count_parameters(model), "charset": len(charset)}
    for lang in ("bengali", "hindi"):
        ds = MozhiWords(args.root, lang, "test")
        report[f"mozhi_{lang}_test"] = summarise(run(model, [ds.get(i) for i in range(len(ds))], charset, device))
    fonts = load_or_scan(os.path.join(args.root, "fonts-src"))
    wiki = lambda c: sorted(glob.glob(os.path.join(args.root, "data", "wiki", c, "*.parquet")))
    corpora = {"bengali": Corpus(wiki("20231101.bn"), "bengali"), "devanagari": Corpus(wiki("20231101.hi"), "devanagari"),
               "latin": Corpus(wiki("20231101.simple"), "latin")}
    source = SyntheticSource(corpora, fonts)
    rng = random.Random(args.seed)
    report["synthetic_test"] = summarise(run(model, [source.sample(rng) for _ in range(args.synthetic)], charset, device))
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=1)
    print({k: (round(v["cer"] * 100, 2), round(v["exact"] * 100, 1)) for k, v in report.items() if isinstance(v, dict)})


if __name__ == "__main__":
    main()
