"""Generate a synthetic page corpus with exact corner labels.

    python tools/synth.py --out /data/lipi-synth --count 20000 --seed 1

Pages (documents, receipts, ID cards, ruled notes, whiteboards) are rendered
upright with Noto fonts (SIL OFL) in Latin, Bengali and Devanagari, then
placed on a procedural background through a random pinhole camera with
lighting, shadows, blur, sensor noise and JPEG compression. Because the
camera is known, the page outline is exact. Nothing third-party is copied
into the images, so the corpus is released under the repository's license.
"""
import argparse
import glob
import math
import os
import random
import sys

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from lipi_eval.corpus import write_jsonl
from lipi_eval.quad import sort_quad

cv2.setNumThreads(1)  # shards run as separate processes; avoid oversubscribing the CPU
cv2.ocl.setUseOpenCL(False)  # OpenCL kernel compilation makes every process slow to start on GPU machines

WORDS = {
    "latin": ("the of and to in is that for it as was with be by on not he this are or his from at "
              "which but have an had they you were their one all we can her has there been if more "
              "when will would who so no total invoice date amount paid tax cash change items qty "
              "price name address phone email page notes meeting agreement tenant lease signed").split(),
    "devanagari": ("और का है में की को से पर यह एक लिए नहीं भी हैं था कि जो इस तक कर "
                   "दिनांक नाम पता राशि कुल रसीद धन्यवाद किराया समझौता हस्ताक्षर विषय पृष्ठ").split(),
    "bengali": ("এবং এর এই যে না জন্য থেকে করে হয় আমি তার একটি সে "
                "তারিখ নাম ঠিকানা মোট টাকা রসিদ ধন্যবাদ ভাড়া চুক্তি স্বাক্ষর বিষয় পৃষ্ঠা").split(),
}
FONT_FILES = {
    "latin": ["NotoSans[wght].ttf", "NotoSerif[wght].ttf", "NotoSans-Regular.ttf", "NotoSerif-Regular.ttf"],
    "devanagari": ["NotoSansDevanagari[wght].ttf", "NotoSerifDevanagari[wght].ttf",
                   "NotoSansDevanagari-Regular.ttf", "NotoSerifDevanagari-Regular.ttf"],
    "bengali": ["NotoSansBengali[wght].ttf", "NotoSerifBengali[wght].ttf",
                "NotoSansBengali-Regular.ttf", "NotoSerifBengali-Regular.ttf"],
    "mono": ["NotoSansMono[wght].ttf", "NotoSansMono-Regular.ttf"],
}
KINDS = ["document", "receipt", "id_card", "notes", "whiteboard"]
KIND_WEIGHTS = [0.40, 0.20, 0.15, 0.15, 0.10]


class Fonts:
    def __init__(self, dirs):
        self.paths = {}
        for key, names in FONT_FILES.items():
            found = []
            for d in dirs:
                for n in names:
                    found += glob.glob(os.path.join(d, "**", glob.escape(n)), recursive=True)
            if not found and key == "mono":
                for d in dirs:
                    found += glob.glob(os.path.join(d, "**", "DejaVuSansMono.ttf"), recursive=True)
            if not found:
                raise SystemExit(f"no font found for {key}; pass --fonts with a Noto font directory")
            self.paths[key] = sorted(found)
        self.cache = {}

    def get(self, script, size, rng):
        path = rng.choice(self.paths[script])
        key = (path, size)
        if key not in self.cache:
            self.cache[key] = ImageFont.truetype(path, size)
        return self.cache[key]


def rand_text(rng, script, n_words):
    return " ".join(rng.choice(WORDS[script]) for _ in range(n_words))


def pick_script(rng):
    return rng.choices(["latin", "devanagari", "bengali"], [0.55, 0.22, 0.23])[0]


def paper_color(rng):
    base = rng.randint(222, 255)
    tint = [rng.randint(-8, 6) for _ in range(3)]
    return tuple(int(np.clip(base + t, 0, 255)) for t in tint)


def ink_color(rng):
    return rng.choice([(20, 20, 20), (35, 35, 40), (10, 10, 60), (50, 30, 20), (0, 0, 0)])


def draw_paragraphs(d, rng, fonts, box, script, ink, size, leading=1.45, justify_gap=0.0):
    x0, y0, x1, y1 = box
    font = fonts.get(script, size, rng)
    y = y0
    while y + size < y1:
        if rng.random() < 0.12:  # paragraph gap
            y += int(size * 0.8)
            continue
        line = rand_text(rng, script, rng.randint(4, 12))
        while d.textlength(line, font=font) > (x1 - x0) and " " in line:
            line = line.rsplit(" ", 1)[0]
        indent = int(size * 1.5) if rng.random() < 0.1 else 0
        d.text((x0 + indent, y), line, font=font, fill=ink)
        y += int(size * leading)


def page_document(rng, fonts, w, h):
    img = Image.new("RGB", (w, h), paper_color(rng))
    d = ImageDraw.Draw(img)
    ink, script = ink_color(rng), pick_script(rng)
    m = int(w * rng.uniform(0.07, 0.12))
    y = m
    if rng.random() < 0.8:  # heading
        hs = int(w * rng.uniform(0.045, 0.07))
        d.text((m, y), rand_text(rng, script, rng.randint(2, 4)), font=fonts.get(script, hs, rng), fill=ink)
        y += int(hs * 2.0)
    if rng.random() < 0.3:  # table
        rows, cols = rng.randint(3, 7), rng.randint(2, 4)
        th = int(h * rng.uniform(0.12, 0.25))
        cw, rh = (w - 2 * m) / cols, th / rows
        for r in range(rows + 1):
            d.line([(m, y + r * rh), (w - m, y + r * rh)], fill=ink, width=1)
        for c in range(cols + 1):
            d.line([(m + c * cw, y), (m + c * cw, y + th)], fill=ink, width=1)
        fs = max(8, int(rh * 0.45))
        for r in range(rows):
            for c in range(cols):
                d.text((m + c * cw + 4, y + r * rh + 3), rand_text(rng, script, 1), font=fonts.get(script, fs, rng), fill=ink)
        y += th + int(w * 0.04)
    draw_paragraphs(d, rng, fonts, (m, y, w - m, h - m), script, ink, int(w * rng.uniform(0.026, 0.04)))
    return img


def page_receipt(rng, fonts, w, h):
    img = Image.new("RGB", (w, h), tuple(min(255, c + 4) for c in paper_color(rng)))
    d = ImageDraw.Draw(img)
    ink = rng.choice([(30, 30, 30), (60, 60, 60), (0, 0, 0)])
    m = int(w * 0.08)
    fs = int(w * rng.uniform(0.055, 0.075))
    mono = fonts.get("mono", fs, rng)
    script = pick_script(rng)
    y = m
    title = fonts.get(script, int(fs * 1.5), rng)
    d.text((m, y), rand_text(rng, script, 2), font=title, fill=ink)
    y += int(fs * 2.4)
    while y + fs < h - m:
        if rng.random() < 0.1:
            d.line([(m, y + fs // 2), (w - m, y + fs // 2)], fill=ink, width=1)
            y += fs
            continue
        left = rand_text(rng, "latin", rng.randint(1, 3))[: max(4, int((w - 2 * m) / (fs * 0.62)) - 9)]
        price = f"{rng.randint(1, 999)}.{rng.randint(0, 99):02d}"
        d.text((m, y), left, font=mono, fill=ink)
        d.text((w - m - d.textlength(price, font=mono), y), price, font=mono, fill=ink)
        y += int(fs * 1.35)
    return img


def page_id_card(rng, fonts, w, h):
    base = tuple(rng.randint(150, 245) for _ in range(3))
    img = Image.new("RGB", (w, h), base)
    d = ImageDraw.Draw(img)
    accent = tuple(rng.randint(20, 170) for _ in range(3))
    d.rectangle([0, 0, w, int(h * 0.2)], fill=accent)
    script = pick_script(rng)
    d.text((int(w * 0.04), int(h * 0.04)), rand_text(rng, script, 2), font=fonts.get(script, int(h * 0.09), rng), fill=(255, 255, 255))
    ph = (int(w * 0.05), int(h * 0.3), int(w * 0.33), int(h * 0.85))
    d.rectangle(ph, fill=tuple(rng.randint(100, 200) for _ in range(3)), outline=(60, 60, 60))
    d.ellipse([ph[0] + 12, ph[1] + 10, ph[2] - 12, ph[1] + 10 + (ph[2] - ph[0] - 24)], fill=(150, 120, 100))
    fs = int(h * 0.075)
    for i in range(rng.randint(3, 5)):
        d.text((int(w * 0.38), int(h * 0.3) + i * int(fs * 1.7)), rand_text(rng, script, rng.randint(1, 3)), font=fonts.get(script, fs, rng), fill=(20, 20, 20))
    if rng.random() < 0.5:
        for _ in range(rng.randint(20, 60)):  # security pattern
            x, y = rng.randint(0, w), rng.randint(int(h * 0.2), h)
            d.ellipse([x, y, x + 6, y + 6], outline=tuple(min(255, c + 25) for c in base))
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, w - 1, h - 1], radius=int(h * 0.06), fill=255)
    out = Image.new("RGB", (w, h), (0, 0, 0))
    out.paste(img, (0, 0), mask)
    return out


def page_notes(rng, fonts, w, h):
    img = Image.new("RGB", (w, h), paper_color(rng))
    d = ImageDraw.Draw(img)
    line = int(w * rng.uniform(0.06, 0.085))
    rule = rng.choice([(150, 175, 220), (190, 190, 190), (170, 200, 170)])
    for y in range(line * 2, h, line):
        d.line([(0, y), (w, y)], fill=rule, width=1)
    if rng.random() < 0.7:
        d.line([(int(w * 0.14), 0), (int(w * 0.14), h)], fill=(220, 120, 120), width=1)
    ink, script = rng.choice([(20, 40, 120), (30, 30, 30), (110, 30, 30)]), pick_script(rng)
    fs = int(line * 0.62)
    font = fonts.get(script, fs, rng)
    for y in range(line * 2, h - line, line):
        if rng.random() < 0.1:
            continue
        txt = rand_text(rng, script, rng.randint(3, 9))
        while d.textlength(txt, font=font) > w * 0.78 and " " in txt:
            txt = txt.rsplit(" ", 1)[0]
        d.text((int(w * 0.17) + rng.randint(-3, 3), y - int(fs * 1.15) + rng.randint(-2, 2)), txt, font=font, fill=ink)
    return img


def page_whiteboard(rng, fonts, w, h):
    img = Image.new("RGB", (w, h), tuple(rng.randint(225, 248) for _ in range(3)))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, w - 1, h - 1], outline=(120, 120, 125), width=max(2, w // 80))
    marker = [(30, 60, 160), (200, 40, 40), (20, 20, 20), (30, 130, 60)]
    script = pick_script(rng)
    for _ in range(rng.randint(3, 6)):
        c = rng.choice(marker)
        x, y = rng.randint(int(w * 0.05), int(w * 0.5)), rng.randint(int(h * 0.05), int(h * 0.85))
        if rng.random() < 0.55:
            d.text((x, y), rand_text(rng, script, rng.randint(1, 4)), font=fonts.get(script, int(h * rng.uniform(0.05, 0.1)), rng), fill=c)
        else:
            pts = [(x, y)]
            for _ in range(rng.randint(3, 8)):
                pts.append((pts[-1][0] + rng.randint(-60, 90), pts[-1][1] + rng.randint(-50, 50)))
            d.line(pts, fill=c, width=max(2, w // 120), joint="curve")
    return img


MAKERS = {"document": page_document, "receipt": page_receipt, "id_card": page_id_card,
          "notes": page_notes, "whiteboard": page_whiteboard}
ASPECT = {"document": (0.62, 0.78), "receipt": (0.28, 0.45), "id_card": (1.5, 1.65),
          "notes": (0.62, 0.8), "whiteboard": (1.3, 1.9)}


def smooth_noise(rng, h, w, scale):
    gh, gw = max(2, h // scale), max(2, w // scale)
    g = np.array([[rng.random() for _ in range(gw)] for _ in range(gh)], dtype=np.float32)
    return cv2.resize(g, (w, h), interpolation=cv2.INTER_CUBIC)


def make_background(rng, w, h):
    kind = rng.choice(["flat", "wood", "gradient", "tiles", "clutter", "dark"])
    gray = rng.randint(40, 220)
    base = np.array([gray + (rng.randint(30, 230) - gray) * 0.4 for _ in range(3)], dtype=np.float32)
    if kind == "dark":
        base = np.array([rng.randint(10, 60)] * 3, dtype=np.float32)
    n = smooth_noise(rng, h, w, rng.choice([8, 16, 32]))[..., None]
    bg = base[None, None, :] * (0.75 + 0.5 * n)
    if kind == "wood":
        ys = np.arange(h, dtype=np.float32)[:, None, None]
        bg *= 0.85 + 0.15 * np.sin(ys * rng.uniform(0.08, 0.3) + n * 8)
        bg *= np.array([1.0, 0.82, 0.62], dtype=np.float32)
    elif kind == "gradient":
        t = np.linspace(0, 1, w, dtype=np.float32)[None, :, None]
        bg = bg * (0.7 + 0.6 * t) if rng.random() < 0.5 else bg * (1.3 - 0.6 * t)
    elif kind == "tiles":
        s = rng.randint(24, 90)
        yy, xx = np.mgrid[0:h, 0:w]
        bg *= (0.9 + 0.1 * (((yy // s) + (xx // s)) % 2))[..., None]
    elif kind == "clutter":
        img = Image.fromarray(np.clip(bg, 0, 255).astype(np.uint8))
        d = ImageDraw.Draw(img)
        for _ in range(rng.randint(6, 25)):
            c = tuple(rng.randint(20, 240) for _ in range(3))
            x, y = rng.randint(-20, w), rng.randint(-20, h)
            sz = rng.randint(20, 160)
            (d.ellipse if rng.random() < 0.4 else d.rectangle)([x, y, x + sz, y + rng.randint(15, sz)], fill=c)
        bg = np.asarray(img.filter(ImageFilter.GaussianBlur(1.2))).astype(np.float32)
    return np.clip(bg, 0, 255)


def project(page_w, page_h, rx, ry, rz, d, f, iw, ih):
    cx, cy = page_w / 2.0, page_h / 2.0
    corners = np.array([[-cx, -cy, 0], [cx, -cy, 0], [cx, cy, 0], [-cx, cy, 0]], dtype=np.float64)
    cz, sz = math.cos(rz), math.sin(rz)
    cy_, sy_ = math.cos(ry), math.sin(ry)
    cx_, sx_ = math.cos(rx), math.sin(rx)
    rz_m = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]])
    ry_m = np.array([[cy_, 0, sy_], [0, 1, 0], [-sy_, 0, cy_]])
    rx_m = np.array([[1, 0, 0], [0, cx_, -sx_], [0, sx_, cx_]])
    pts = corners @ (rx_m @ ry_m @ rz_m).T
    z = pts[:, 2] + d
    if np.any(z <= 1e-3):
        return None
    return np.stack([f * pts[:, 0] / z + iw / 2.0, f * pts[:, 1] / z + ih / 2.0], axis=1)


def sample_view(rng, page_w, page_h, iw, ih):
    """Random pose; returns the projected corners in page order or None."""
    f = max(iw, ih) * rng.uniform(0.65, 1.0)
    quarter = rng.randrange(4)
    rz = quarter * math.pi / 2 + rng.gauss(0, 0.12) * (1 if rng.random() < 0.8 else 3)
    rx, ry = rng.gauss(0, 0.22), rng.gauss(0, 0.22)
    if rng.random() < 0.15:
        rx, ry = rng.uniform(-0.6, 0.6), rng.uniform(-0.6, 0.6)
    want = rng.uniform(0.4, 0.95)  # fraction of the frame the page should fill
    d0 = f * max(page_w / iw, page_h / ih) / want
    q = project(page_w, page_h, rx, ry, rz, d0, f, iw, ih)
    if q is None:
        return None, quarter
    # centre on a random point and keep every corner inside the frame
    shift = np.array([rng.uniform(-0.08, 0.08) * iw, rng.uniform(-0.08, 0.08) * ih])
    q = q + shift - (q.mean(axis=0) - np.array([iw / 2.0, ih / 2.0]))
    margin = 2.0
    if q[:, 0].min() < margin or q[:, 1].min() < margin or q[:, 0].max() > iw - margin or q[:, 1].max() > ih - margin:
        return None, quarter
    return q, quarter


def render_scene(rng, fonts, long_side=512):
    kind = rng.choices(KINDS, KIND_WEIGHTS)[0]
    portrait = rng.random() < 0.6
    ar = rng.uniform(0.75, 0.8) if rng.random() < 0.8 else rng.uniform(0.55, 0.9)
    iw, ih = (int(long_side * ar), long_side) if portrait else (long_side, int(long_side * ar))
    pw = 640
    lo, hi = ASPECT[kind]
    ph = int(pw / rng.uniform(lo, hi)) if kind in ("id_card", "whiteboard") else int(pw * rng.uniform(1 / hi, 1 / lo))
    page = MAKERS[kind](rng, fonts, pw, ph)
    for _ in range(40):
        quad, quarter = sample_view(rng, pw, ph, iw, ih)
        if quad is not None:
            break
    else:
        return None
    ss = 2  # supersample the composite for clean edges
    W, H = iw * ss, ih * ss
    bg = make_background(rng, W, H)
    src = np.asarray(page).astype(np.float32)  # casting afterwards is much faster on older Pillow/NumPy
    # paper lighting and a soft cast shadow on the page itself
    yy, xx = np.mgrid[0:ph, 0:pw].astype(np.float32)
    light = 1.0 - rng.uniform(0, 0.25) * ((xx / pw) * rng.choice([-1, 1]) + (yy / ph) * rng.choice([-1, 1]) + 1) / 2
    if rng.random() < 0.5:
        a = rng.uniform(0, math.tau)
        t = (np.cos(a) * (xx / pw - 0.5) + np.sin(a) * (yy / ph - 0.5)) * rng.uniform(4, 9)
        light *= 1.0 - rng.uniform(0.15, 0.5) / (1.0 + np.exp(-t))
    src = np.clip(src * light[..., None], 0, 255)
    mask = np.ones((ph, pw), dtype=np.float32)
    srcq = np.array([[0, 0], [pw, 0], [pw, ph], [0, ph]], dtype=np.float32)
    dstq = (quad * ss).astype(np.float32)
    Hm = cv2.getPerspectiveTransform(srcq, dstq)
    warped = cv2.warpPerspective(src, Hm, (W, H), flags=cv2.INTER_LINEAR, borderValue=0)
    alpha = cv2.warpPerspective(mask, Hm, (W, H), flags=cv2.INTER_LINEAR, borderValue=0)[..., None]
    shadow = cv2.GaussianBlur(alpha[..., 0], (0, 0), rng.uniform(4, 14) * ss / 2)
    off = np.array([[1, 0, rng.uniform(-10, 14) * ss], [0, 1, rng.uniform(2, 16) * ss]], dtype=np.float32)
    shadow = cv2.warpAffine(shadow, off, (W, H))[..., None]
    bg = bg * (1.0 - rng.uniform(0.25, 0.6) * shadow)
    out = bg * (1 - alpha) + warped * alpha
    if rng.random() < 0.12:  # a finger or hand overlapping an edge
        tone = np.array([rng.randint(150, 230), rng.randint(110, 180), rng.randint(90, 150)], dtype=np.float32)
        c = quad[rng.randrange(4)] * ss
        axes = (int(rng.uniform(0.04, 0.09) * W), int(rng.uniform(0.07, 0.16) * W))
        blob = np.zeros((H, W), dtype=np.float32)
        cv2.ellipse(blob, (int(c[0]), int(c[1])), axes, rng.uniform(0, 180), 0, 360, 1.0, -1)
        blob = cv2.GaussianBlur(blob, (0, 0), 2 * ss)[..., None]
        out = out * (1 - blob) + tone * blob
    out = cv2.resize(out, (iw, ih), interpolation=cv2.INTER_AREA)
    if rng.random() < 0.5:
        out = cv2.GaussianBlur(out, (0, 0), rng.uniform(0.4, 1.6))
    if rng.random() < 0.15:
        k = rng.choice([5, 7, 9])
        kernel = np.zeros((k, k), dtype=np.float32)
        kernel[k // 2, :] = 1.0 / k
        mat = cv2.getRotationMatrix2D((k / 2 - 0.5, k / 2 - 0.5), rng.uniform(0, 180), 1.0)
        out = cv2.filter2D(out, -1, cv2.warpAffine(kernel, mat, (k, k)))
    out *= np.array([rng.uniform(0.92, 1.08) for _ in range(3)], dtype=np.float32)
    gain = rng.uniform(0.7, 1.15)
    out = out * gain + rng.uniform(-12, 12)
    out += np.random.default_rng(rng.randrange(1 << 30)).normal(0, rng.uniform(0, 7), out.shape)
    img = np.clip(out, 0, 255).astype(np.uint8)
    sorted_quad = sort_quad(quad)
    # which page corner (clockwise from the page's own top-left) sits at the sorted top-left
    k = int(np.argmin(np.linalg.norm(quad - sorted_quad[0], axis=1)))
    return {"image": img, "quad": sorted_quad, "orientation": k, "kind": kind, "quarter": quarter,
            "page": page}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", required=True)
    ap.add_argument("--count", type=int, default=1000)
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--size", type=int, default=512, help="long side in pixels")
    ap.add_argument("--split", default="train", choices=["train", "val", "test"])
    ap.add_argument("--prefix", default="syn")
    ap.add_argument("--start", type=int, default=0, help="first index, for sharding across jobs")
    ap.add_argument("--fonts", nargs="*", default=["/usr/share/fonts", os.path.expanduser("~/.fonts")])
    args = ap.parse_args()

    fonts = Fonts(args.fonts)
    os.makedirs(os.path.join(args.out, "images"), exist_ok=True)
    rows = []
    i = args.start
    made = 0
    while made < args.count:
        rng = random.Random(f"{args.seed}:{i}")
        scene = render_scene(rng, fonts, args.size)
        i += 1
        if scene is None:
            continue
        name = f"{args.prefix}-{i - 1:06d}"
        rel = f"images/{name}.jpg"
        bgr = cv2.cvtColor(scene["image"], cv2.COLOR_RGB2BGR)
        cv2.imwrite(os.path.join(args.out, rel), bgr, [cv2.IMWRITE_JPEG_QUALITY, rng.randint(45, 95)])
        h, w = scene["image"].shape[:2]
        rows.append({"id": name, "image": rel, "width": w, "height": h,
                     "quad": [[round(float(x), 2), round(float(y), 2)] for x, y in scene["quad"]],
                     "orientation": scene["orientation"], "source": "synthetic",
                     "license": "generated, same license as the repository", "split": args.split,
                     "kind": scene["kind"]})
        made += 1
    write_jsonl(os.path.join(args.out, f"{args.split}.jsonl"), rows)
    print(f"wrote {made} images to {args.out}")


if __name__ == "__main__":
    main()
