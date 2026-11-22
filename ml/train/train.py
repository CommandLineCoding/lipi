"""Train PageNet.

    python -m train.train --train a/train.jsonl b/train.jsonl --val syn-val/val.jsonl real/val.jsonl \
        --out runs/page-net --epochs 60

Several --train files are concatenated. Each --val file is scored separately,
and the checkpoint with the best mean IoU over them is kept.
"""
import argparse
import json
import math
import os
import time

import numpy as np
import torch
import torch.nn.functional as F
from torch.utils.data import DataLoader

from lipi_eval.quad import corner_error, quad_iou

from .data import PageDataset
from .model import HEAT_SIZE, INPUT_SIZE, PageNet, count_parameters


def gaussian_targets(corners, sigma=1.6):
    """Target distributions over the heat map: a Gaussian around each corner."""
    n = corners.shape[0]
    xs = (torch.arange(HEAT_SIZE, device=corners.device, dtype=torch.float32) + 0.5) / HEAT_SIZE
    dx = (xs.view(1, 1, 1, -1) - corners[:, :, 0].view(n, 4, 1, 1)) * HEAT_SIZE
    dy = (xs.view(1, 1, -1, 1) - corners[:, :, 1].view(n, 4, 1, 1)) * HEAT_SIZE
    g = torch.exp(-(dx ** 2 + dy ** 2) / (2 * sigma ** 2))
    return g / g.sum(dim=(2, 3), keepdim=True).clamp_min(1e-8)


def loss_fn(outputs, batch):
    corners, _, orient, heat, mask = outputs
    n = corners.shape[0]
    l_coord = F.smooth_l1_loss(corners * INPUT_SIZE, batch["corners"] * INPUT_SIZE, beta=1.0)
    logp = F.log_softmax(heat.reshape(n, 4, -1), dim=2)
    target = gaussian_targets(batch["corners"]).reshape(n, 4, -1)
    l_heat = F.kl_div(logp, target, reduction="batchmean") / 4
    l_mask = F.binary_cross_entropy_with_logits(mask, batch["mask"])
    valid = batch["orientation"] >= 0
    l_orient = F.cross_entropy(orient[valid], batch["orientation"][valid]) if valid.any() else corners.sum() * 0
    return l_coord + 2.0 * l_heat + l_mask + 0.5 * l_orient, {
        "coord": l_coord.item(), "heat": l_heat.item(), "mask": l_mask.item(), "orient": float(l_orient.item())}


@torch.no_grad()
def evaluate(model, loader, device):
    model.eval()
    errs, ious, correct, total = [], [], 0, 0
    for batch in loader:
        corners, _, orient, = model(batch["image"].to(device).float())[:3]
        corners = corners.cpu().numpy() * INPUT_SIZE
        gt = batch["corners"].numpy() * INPUT_SIZE
        for p, g in zip(corners, gt):
            errs.append(corner_error(p, g, INPUT_SIZE, INPUT_SIZE))
            ious.append(quad_iou(p, g))
        valid = batch["orientation"] >= 0
        if valid.any():
            correct += (orient.cpu().argmax(1)[valid] == batch["orientation"][valid]).sum().item()
            total += int(valid.sum())
    return {"corner_error": float(np.mean(errs)), "iou": float(np.mean(ious)),
            "iou_ge_90": float(np.mean(np.array(ious) >= 0.9)),
            "orientation_accuracy": correct / total if total else None}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--train", nargs="+", required=True)
    ap.add_argument("--val", nargs="+", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--epochs", type=int, default=60)
    ap.add_argument("--batch", type=int, default=128)
    ap.add_argument("--lr", type=float, default=2e-3)
    ap.add_argument("--workers", type=int, default=16)
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--width", type=float, default=1.0)
    args = ap.parse_args()

    torch.manual_seed(args.seed)
    np.random.seed(args.seed)
    os.makedirs(args.out, exist_ok=True)
    device = "cuda" if torch.cuda.is_available() else "cpu"
    model = PageNet(args.width).to(device)
    print(f"parameters: {count_parameters(model):,}  device: {device}", flush=True)

    train_loader = DataLoader(PageDataset(args.train, True, args.seed), batch_size=args.batch, shuffle=True,
                              num_workers=args.workers, drop_last=True, persistent_workers=args.workers > 0,
                              prefetch_factor=1 if args.workers > 0 else None)
    val_loaders = {os.path.basename(os.path.dirname(os.path.abspath(p))): DataLoader(PageDataset([p], False), batch_size=64, num_workers=4, prefetch_factor=1)
                   for p in args.val}
    opt = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=1e-4)
    steps = args.epochs * len(train_loader)
    sched = torch.optim.lr_scheduler.LambdaLR(opt, lambda s: min(1.0, (s + 1) / 300) * 0.5 * (1 + math.cos(math.pi * s / steps)))
    scaler = torch.cuda.amp.GradScaler(enabled=device == "cuda")

    best, history = -1.0, []
    for epoch in range(args.epochs):
        model.train()
        t0 = time.time()
        agg = {}
        for batch in train_loader:
            batch = {k: v.to(device, non_blocking=True) for k, v in batch.items()}
            with torch.autocast(device_type=device, dtype=torch.float16, enabled=device == "cuda"):
                outputs = model(batch["image"].float(), train_outputs=True)
            outputs = tuple(o.float() for o in outputs)
            loss, parts = loss_fn(outputs, batch)
            opt.zero_grad(set_to_none=True)
            scaler.scale(loss).backward()
            scaler.unscale_(opt)
            torch.nn.utils.clip_grad_norm_(model.parameters(), 5.0)
            scaler.step(opt)
            scaler.update()
            sched.step()
            for k, v in parts.items():
                agg[k] = agg.get(k, 0.0) + v / len(train_loader)
        scores = {name: evaluate(model, dl, device) for name, dl in val_loaders.items()}
        mean_iou = float(np.mean([s["iou"] for s in scores.values()]))
        history.append({"epoch": epoch, "loss": agg, "val": scores})
        print(f"epoch {epoch:3d} {time.time() - t0:5.1f}s  loss {sum(agg.values()):.3f}  " +
              "  ".join(f"{n}: iou {s['iou']:.3f} err {s['corner_error']:.4f}" for n, s in scores.items()), flush=True)
        if mean_iou > best:
            best = mean_iou
            torch.save({"model": model.state_dict(), "width": args.width, "epoch": epoch, "val": scores}, os.path.join(args.out, "best.pt"))
        with open(os.path.join(args.out, "history.json"), "w") as f:
            json.dump(history, f, indent=1)
    print(f"best mean IoU {best:.4f}")


if __name__ == "__main__":
    main()
