"""Train the text recogniser.

    python -m ocr.train_rec --root ~/lipi-train/ocr --out runs/rec-s --steps 150000

Training mixes endless synthetic lines (fonts, Wikipedia text, scan damage) with the real printed words of Mozhi.
Every --eval-every steps it scores the Mozhi validation words for Bengali and Hindi and a fixed synthetic set of
three scripts, and keeps the checkpoint with the lowest mean character error rate on the two Mozhi sets.
"""
import argparse
import glob
import json
import os
import random
import time

import torch
import torch.nn as nn
from torch.utils.data import DataLoader

from . import charset as cs
from .data import MozhiWords, Mixture, collate
from .fonts import load_or_scan
from .model import RecNet, count_parameters
from .synth_text import Corpus, SyntheticSource


def edit_distance(a, b):
    """Levenshtein distance between two strings."""
    prev = list(range(len(b) + 1))
    for i, ca in enumerate(a, 1):
        cur = [i]
        for j, cb in enumerate(b, 1):
            cur.append(min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (ca != cb)))
        prev = cur
    return prev[-1]


@torch.no_grad()
def predict(model, images, steps, charset, device):
    out = model(images.to(device).float() / 255.0).argmax(2).transpose(0, 1).cpu()  # N, T
    return [charset.decode(out[k, :int(steps[k])].tolist()) for k in range(len(steps))]


@torch.no_grad()
def score(model, samples, charset, device, batch=256):
    """samples: list of (image, text). Returns character error rate and the share of exactly right texts."""
    model.eval()
    errors = chars = exact = 0
    for i in range(0, len(samples), batch):
        images, steps, _, _, texts = collate(samples[i:i + batch], charset)
        with torch.autocast("cuda", dtype=torch.bfloat16):
            pred = predict(model, images, steps, charset, device)
        for p, t in zip(pred, texts):
            errors += edit_distance(p, t)
            chars += len(t)
            exact += p == t
    model.train()
    return {"cer": errors / max(1, chars), "exact": exact / max(1, len(samples)), "n": len(samples)}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", required=True, help="folder with data/ (mozhi, wiki) and fonts-src/")
    ap.add_argument("--out", required=True)
    ap.add_argument("--steps", type=int, default=150_000)
    ap.add_argument("--bs", type=int, default=192)
    ap.add_argument("--lr", type=float, default=2e-3)
    ap.add_argument("--workers", type=int, default=12)
    ap.add_argument("--eval-every", type=int, default=5000)
    ap.add_argument("--synthetic-share", type=float, default=0.65)
    ap.add_argument("--seed", type=int, default=0)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    device = "cuda"
    torch.manual_seed(args.seed)

    fonts = load_or_scan(os.path.join(args.root, "fonts-src"))
    wiki = lambda cfg: sorted(glob.glob(os.path.join(args.root, "data", "wiki", cfg, "*.parquet")))
    corpora = {"bengali": Corpus(wiki("20231101.bn"), "bengali"), "devanagari": Corpus(wiki("20231101.hi"), "devanagari"),
               "latin": Corpus(wiki("20231101.simple"), "latin")}
    print("fonts", {k: len(v) for k, v in fonts.items()}, "sentences", {k: len(c.sentences) for k, c in corpora.items()}, flush=True)

    train_words = [MozhiWords(args.root, lang, "train", augment=True) for lang in ("bengali", "hindi")]
    charset_path = os.path.join(args.out, "charset.txt")
    if os.path.exists(charset_path):
        charset = cs.Charset.load(charset_path)
    else:
        texts = [t for ds in train_words for _, t in ds.items] + [s for c in corpora.values() for s in c.sentences[:20000]]
        charset = cs.build(texts)
        charset.save(charset_path)
    print("charset", len(charset), flush=True)

    synthetic = SyntheticSource(corpora, fonts)
    val_words = {lang: MozhiWords(args.root, lang, "val") for lang in ("bengali", "hindi")}
    val_samples = {lang: [ds.get(i) for i in range(0, len(ds), 4)] for lang, ds in val_words.items()}  # a quarter of each, fixed
    rng = random.Random(12345)
    val_samples["synthetic"] = [synthetic.sample(rng) for _ in range(3000)]

    stream = Mixture(synthetic, train_words, weights=(args.synthetic_share, 1 - args.synthetic_share), seed=args.seed)
    loader = DataLoader(stream, batch_size=args.bs, num_workers=args.workers, collate_fn=lambda b: collate(b, charset),
                        prefetch_factor=2, persistent_workers=True)

    model = RecNet(len(charset)).to(device)
    print("parameters", count_parameters(model), flush=True)
    opt = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=1e-2)
    sched = torch.optim.lr_scheduler.OneCycleLR(opt, max_lr=args.lr, total_steps=args.steps, pct_start=0.05)
    ctc = nn.CTCLoss(blank=0, zero_infinity=True)
    best, started, running = 1e9, time.time(), 0.0
    log = open(os.path.join(args.out, "metrics.jsonl"), "a")

    for step, (images, steps, targets, lengths, _) in enumerate(loader, 1):
        images = images.to(device, non_blocking=True).float() / 255.0
        with torch.autocast("cuda", dtype=torch.bfloat16):
            logp = model(images)
        loss = ctc(logp.float(), targets, steps, lengths)
        opt.zero_grad(set_to_none=True)
        loss.backward()
        nn.utils.clip_grad_norm_(model.parameters(), 5.0)
        opt.step()
        sched.step()
        running = 0.98 * running + 0.02 * loss.item() if step > 1 else loss.item()
        if step % 200 == 0:
            print(f"step {step} loss {running:.3f} lr {sched.get_last_lr()[0]:.5f} {(time.time() - started) / 60:.1f} min", flush=True)
        if step % args.eval_every == 0 or step == args.steps:
            result = {k: score(model, v, charset, device) for k, v in val_samples.items()}
            mean_cer = (result["bengali"]["cer"] + result["hindi"]["cer"]) / 2
            record = {"step": step, "loss": running, "mean_cer": mean_cer, **{f"{k}_cer": v["cer"] for k, v in result.items()},
                      **{f"{k}_exact": v["exact"] for k, v in result.items()}, "minutes": (time.time() - started) / 60}
            log.write(json.dumps(record) + "\n")
            log.flush()
            print("EVAL", json.dumps(record), flush=True)
            torch.save({"model": model.state_dict(), "charset": charset.chars, "step": step}, os.path.join(args.out, "last.pt"))
            if mean_cer < best:
                best = mean_cer
                torch.save({"model": model.state_dict(), "charset": charset.chars, "step": step, "metrics": record}, os.path.join(args.out, "best.pt"))
        if step >= args.steps:
            break


if __name__ == "__main__":
    main()
