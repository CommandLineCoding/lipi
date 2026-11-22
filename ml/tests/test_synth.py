import os
import random
import sys

import cv2
import numpy as np
import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools"))
import synth  # noqa: E402

from lipi_eval.quad import polygon_area  # noqa: E402

FONT_DIRS = ["/usr/share/fonts", os.path.expanduser("~/.fonts")]


@pytest.fixture(scope="module")
def fonts():
    try:
        return synth.Fonts(FONT_DIRS)
    except SystemExit:
        pytest.skip("Noto fonts are not installed")


def scenes(fonts, n):
    out, i = [], 0
    while len(out) < n:
        s = synth.render_scene(random.Random(f"test:{i}"), fonts, 384)
        i += 1
        if s is not None:
            out.append(s)
    return out


def test_quads_are_inside_the_image_clockwise_and_ordered(fonts):
    for s in scenes(fonts, 12):
        h, w = s["image"].shape[:2]
        q = s["quad"]
        assert q[:, 0].min() >= 0 and q[:, 1].min() >= 0
        assert q[:, 0].max() <= w and q[:, 1].max() <= h
        assert polygon_area(q) > 0
        assert s["orientation"] in (0, 1, 2, 3)


def asymmetric_page(rng, fonts, w, h):
    from PIL import Image, ImageDraw

    img = Image.new("RGB", (w, h), (245, 245, 245))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, int(w * 0.45), int(h * 0.25)], fill=(10, 10, 10))
    d.ellipse([int(w * 0.75), int(h * 0.05), int(w * 0.95), int(h * 0.05) + int(w * 0.2)], fill=(10, 10, 10))
    d.rectangle([int(w * 0.1), int(h * 0.6), int(w * 0.5), int(h * 0.62)], fill=(10, 10, 10))
    return img


def test_orientation_label_makes_the_flattened_page_upright(fonts, monkeypatch):
    for kind in synth.MAKERS:
        monkeypatch.setitem(synth.MAKERS, kind, asymmetric_page)
    seen = set()
    for s in scenes(fonts, 40):
        k = s["orientation"]
        page = np.asarray(s["page"].convert("L"), dtype=np.float32)
        ph, pw = page.shape
        gray = cv2.cvtColor(s["image"], cv2.COLOR_RGB2GRAY).astype(np.float32)
        corr = {}
        for kk in range(4):
            size = (pw, ph) if kk % 2 == 0 else (ph, pw)
            rect = np.array([[0, 0], [size[0], 0], [size[0], size[1]], [0, size[1]]], dtype=np.float32)
            m = cv2.getPerspectiveTransform(s["quad"].astype(np.float32), rect)
            flat = cv2.warpPerspective(gray, m, size)
            for _ in range(kk):
                flat = cv2.rotate(flat, cv2.ROTATE_90_CLOCKWISE)
            flat = cv2.resize(flat, (pw, ph), interpolation=cv2.INTER_AREA)
            corr[kk] = np.corrcoef(cv2.GaussianBlur(flat, (0, 0), 3).ravel(), cv2.GaussianBlur(page, (0, 0), 3).ravel())[0, 1]
        assert max(corr, key=corr.get) == k, f"label {k}, correlations {corr}"
        assert corr[k] > 0.6
        seen.add(k)
    assert len(seen) >= 3  # the test covers rotated pages, not just upright ones


def test_same_seed_gives_the_same_scene(fonts):
    a = synth.render_scene(random.Random("x:1"), fonts, 256)
    b = synth.render_scene(random.Random("x:1"), fonts, 256)
    assert a is not None and np.array_equal(a["image"], b["image"])
