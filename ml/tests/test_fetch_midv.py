import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools"))
import fetch_midv as fm  # noqa: E402


def photo(seed, size=(320, 240)):
    rng = np.random.default_rng(seed)
    base = rng.integers(0, 255, (6, 8, 3)).astype(np.uint8)
    return Image.fromarray(base).resize(size, Image.BICUBIC)


def test_dhash_survives_resizing_and_recompression():
    a = photo(1)
    small = a.resize((128, 96), Image.LANCZOS)
    assert fm.hamming(fm.dhash(a), fm.dhash(small)) <= 4


def test_dhash_separates_different_images():
    distances = [fm.hamming(fm.dhash(photo(1)), fm.dhash(photo(s))) for s in range(2, 12)]
    assert min(distances) > 8


def test_only_fully_visible_reasonably_large_quads_are_usable():
    full = [0.1, 0.1, 0.9, 0.1, 0.9, 0.9, 0.1, 0.9]
    assert fm.usable_quad(full)
    assert not fm.usable_quad([-0.2, 0.1, 0.9, 0.1, 0.9, 0.9, 0.1, 0.9])   # a corner is out of the frame
    assert not fm.usable_quad([0.1, 0.1, 1.3, 0.1, 0.9, 0.9, 0.1, 0.9])
    assert not fm.usable_quad([0.5, 0.5, 0.52, 0.5, 0.52, 0.52, 0.5, 0.52])  # tiny
    assert not fm.usable_quad([float("nan")] * 8)


def test_source_prefixes():
    assert "midv500_000123.jpg".startswith(fm.PREFIXES)
    assert "midv2019_000123.jpg".startswith(fm.PREFIXES)
    assert not "smartdoc_000123.jpg".startswith(fm.PREFIXES)
    assert not "id_detections_000123.jpg".startswith(fm.PREFIXES)
