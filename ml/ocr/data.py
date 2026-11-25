"""Training data for the recogniser: synthetic lines mixed with the real printed words of Mozhi (CC BY 4.0).

Mozhi (IIIT Hyderabad) has about 100,000 word images each for Bengali and Hindi, cropped from scanned book pages,
with a tab-separated file of "images/name.jpeg<TAB>text" per split.
"""
import glob
import os
import random

import numpy as np
import torch
from PIL import Image, ImageFilter
from torch.utils.data import Dataset, IterableDataset

from .charset import normalise
from .model import LINE_HEIGHT, STRIDE
from .synth_text import MAX_WIDTH


def find_gt(root, lang, split):
    hits = glob.glob(os.path.join(root, "data", "mozhi", lang, split, "**", "*_gt.txt"), recursive=True)
    if not hits:
        raise FileNotFoundError(f"no ground-truth file for {lang}/{split} under {root}")
    return hits[0]


class MozhiWords(Dataset):
    """Real printed words. With augment=True the images get a light random blur, noise and brightness change."""

    def __init__(self, root, lang, split, augment=False):
        gt = find_gt(root, lang, split)
        self.base = os.path.dirname(gt)
        self.items = []
        for line in open(gt, encoding="utf-8"):
            name, _, text = line.rstrip("\n").partition("\t")
            text = normalise(text)
            if text:
                self.items.append((name, text))
        self.augment = augment

    def __len__(self):
        return len(self.items)

    def get(self, i, rng=None):
        name, text = self.items[i]
        img = Image.open(os.path.join(self.base, name)).convert("L")
        scale = LINE_HEIGHT / img.height
        w = min(MAX_WIDTH, max(8, round(img.width * scale)))
        img = img.resize((w, LINE_HEIGHT), Image.BILINEAR)
        if self.augment and rng is not None:
            if rng.random() < 0.4:
                img = img.filter(ImageFilter.GaussianBlur(rng.uniform(0.3, 1.2)))
            a = np.asarray(img, dtype=np.float32)
            if rng.random() < 0.5:
                a = a + np.random.RandomState(rng.randrange(1 << 30)).normal(0, rng.uniform(2, 10), a.shape)
            if rng.random() < 0.5:
                a = (a - a.mean()) * rng.uniform(0.6, 1.1) + a.mean() + rng.uniform(-25, 25)
            img = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))
        return np.asarray(img, dtype=np.uint8), text

    def __getitem__(self, i):
        return self.get(i)


class Mixture(IterableDataset):
    """Endless stream mixing synthetic lines and real words. Every worker has its own random generator."""

    def __init__(self, synthetic, words, weights=(0.65, 0.35), seed=0):
        self.synthetic, self.words, self.weights, self.seed = synthetic, words, weights, seed

    def __iter__(self):
        info = torch.utils.data.get_worker_info()
        rng = random.Random(self.seed * 1000003 + (info.id if info else 0))
        while True:
            if rng.random() < self.weights[0] or not self.words:
                yield self.synthetic.sample(rng)
            else:
                ds = rng.choice(self.words)
                yield ds.get(rng.randrange(len(ds)), rng)


def collate(batch, charset):
    """Pads the images to the widest one in the batch and builds the CTC targets. Images stay 8-bit (shared memory is
    small); the training loop turns them into floats on the GPU."""
    pairs = [(img, charset.encode(text)) for img, text in batch]
    pairs = [(img, ids) for img, ids in pairs if ids]
    widest = max(img.shape[1] for img, _ in pairs)
    width = -(-widest // STRIDE) * STRIDE
    images = np.empty((len(pairs), 1, LINE_HEIGHT, width), dtype=np.uint8)
    steps, targets, lengths = [], [], []
    for k, (img, ids) in enumerate(pairs):
        fill = float(np.median(np.concatenate([img[:, 0], img[:, -1]])))
        images[k, 0] = int(fill)
        images[k, 0, :, :img.shape[1]] = img
        steps.append(-(-img.shape[1] // STRIDE))
        targets.extend(ids)
        lengths.append(len(ids))
    return (torch.from_numpy(images), torch.tensor(steps, dtype=torch.long),
            torch.tensor(targets, dtype=torch.long), torch.tensor(lengths, dtype=torch.long), [t for _, t in batch if charset.encode(t)])
