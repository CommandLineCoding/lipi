"""Training data: reads corpus JSONL files and applies geometry-aware augmentation."""
import os
import random

import cv2
import numpy as np
import torch
from torch.utils.data import Dataset

from lipi_eval.corpus import read_jsonl
from lipi_eval.quad import sort_quad

from .model import HEAT_SIZE, INPUT_SIZE

cv2.setNumThreads(1)  # data loader workers are separate processes
cv2.ocl.setUseOpenCL(False)  # avoid slow OpenCL kernel compilation in every worker


def page_order(quad, orientation):
    """Corners in the page's own order (its top-left first) from sorted corners and the label."""
    return np.roll(np.asarray(quad, dtype=np.float32), orientation, axis=0)


def sorted_and_orientation(page_quad):
    """Sort page-order corners geometrically and recover the orientation label."""
    s = sort_quad(page_quad).astype(np.float32)
    k = int(np.argmin(np.linalg.norm(page_quad - s[0], axis=1)))
    return s, k


def rotate_cw(image, page_quad, times):
    """Rotate the image by 90 degrees clockwise `times` times, moving the corners with it."""
    q = page_quad.copy()
    for _ in range(times % 4):
        h = image.shape[0]
        image = cv2.rotate(image, cv2.ROTATE_90_CLOCKWISE)
        q = np.stack([h - q[:, 1], q[:, 0]], axis=1)
    return image, q


def crop_and_resize(image, quad, rng, out_size=INPUT_SIZE, zoom_max=1.35):
    """Random crop that keeps every corner inside, then resize to a square."""
    h, w = image.shape[:2]
    x0, y0 = quad.min(axis=0)
    x1, y1 = quad.max(axis=0)
    # the crop can shrink towards the quad's bounding box by up to zoom_max
    z = rng.uniform(1.0, zoom_max)
    cx0 = rng.uniform(0, x0) * (1 - 1 / z)
    cy0 = rng.uniform(0, y0) * (1 - 1 / z)
    cx1 = w - rng.uniform(0, w - x1) * (1 - 1 / z)
    cy1 = h - rng.uniform(0, h - y1) * (1 - 1 / z)
    cx0, cy0, cx1, cy1 = int(cx0), int(cy0), int(np.ceil(cx1)), int(np.ceil(cy1))
    crop = image[cy0:cy1, cx0:cx1]
    q = quad - np.array([cx0, cy0], dtype=np.float32)
    sx, sy = out_size / crop.shape[1], out_size / crop.shape[0]
    crop = cv2.resize(crop, (out_size, out_size), interpolation=cv2.INTER_AREA)
    return crop, q * np.array([sx, sy], dtype=np.float32)


def color_jitter(img, rng):
    img = img.astype(np.float32)
    img = img * rng.uniform(0.7, 1.3) + rng.uniform(-25, 25)
    gray = img.mean(axis=2, keepdims=True)
    img = gray + (img - gray) * rng.uniform(0.6, 1.4)
    img *= np.array([rng.uniform(0.9, 1.1) for _ in range(3)], dtype=np.float32)
    if rng.random() < 0.3:
        img = cv2.GaussianBlur(img, (0, 0), rng.uniform(0.3, 1.4))
    if rng.random() < 0.5:
        img += np.random.default_rng(int(rng.random() * 1e9)).normal(0, rng.uniform(0, 6), img.shape).astype(np.float32)
    return np.clip(img, 0, 255).astype(np.uint8)


def make_targets(sorted_quad, size=INPUT_SIZE):
    """Normalised corners and a page mask at heat-map resolution."""
    norm = (sorted_quad / size).astype(np.float32)
    mask = np.zeros((HEAT_SIZE, HEAT_SIZE), dtype=np.uint8)
    cv2.fillConvexPoly(mask, np.round(norm * HEAT_SIZE).astype(np.int32), 1)
    return norm, mask.astype(np.float32)


class PageDataset(Dataset):
    def __init__(self, splits, train, seed=0):
        """`splits` is a list of JSONL paths; images are resolved relative to each file."""
        self.rows = []
        for path in splits:
            root = os.path.dirname(os.path.abspath(path))
            for r in read_jsonl(path):
                r["_root"] = root
                self.rows.append(r)
        self.train = train
        self.seed = seed

    def __len__(self):
        return len(self.rows)

    def __getitem__(self, i):
        r = self.rows[i]
        rng = random.Random(f"{self.seed}:{i}:{random.random()}" if self.train else f"eval:{i}")
        img = cv2.cvtColor(cv2.imread(os.path.join(r["_root"], r["image"])), cv2.COLOR_BGR2RGB)
        has_orientation = r["orientation"] is not None
        quad = np.asarray(r["quad"], dtype=np.float32)
        page_quad = page_order(quad, r["orientation"] if has_orientation else 0)
        if self.train:
            img, page_quad = rotate_cw(img, page_quad, rng.randrange(4))
            img, page_quad = crop_and_resize(img, page_quad, rng)
            img = color_jitter(img, rng)
        else:
            sx, sy = INPUT_SIZE / img.shape[1], INPUT_SIZE / img.shape[0]
            img = cv2.resize(img, (INPUT_SIZE, INPUT_SIZE), interpolation=cv2.INTER_AREA)
            page_quad = page_quad * np.array([sx, sy], dtype=np.float32)
        sq, k = sorted_and_orientation(page_quad)
        corners, mask = make_targets(sq)
        return {
            "image": torch.from_numpy(np.ascontiguousarray(img.transpose(2, 0, 1))),  # uint8; converted on the GPU
            "corners": torch.from_numpy(corners),
            "mask": torch.from_numpy(mask)[None],
            "orientation": torch.tensor(k if has_orientation else -1, dtype=torch.long),
        }
