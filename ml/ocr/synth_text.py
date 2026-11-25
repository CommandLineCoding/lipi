"""Synthetic text lines: sentences from a text corpus drawn with real fonts, then damaged the way scans and photos are.

The drawing uses Raqm (HarfBuzz), so Bengali conjuncts and Devanagari matras are shaped correctly. Lines are
returned as 8-bit grayscale arrays 48 pixels high.
"""
import io
import random
import re

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

from fontTools.ttLib import TTFont

from .charset import BASE, BENGALI_DIGITS, DEVANAGARI_DIGITS, normalise
from .model import LINE_HEIGHT

MAX_WIDTH = 640
# Characters a synthetic line may contain: the base set plus the Bengali and Devanagari blocks.
ALLOWED = set(BASE) | {chr(c) for c in range(0x0980, 0x0A00)} | {chr(c) for c in range(0x0900, 0x0980)}
_cmaps = {}


def font_has(path, text):
    """True if the font file has a glyph for every visible character of text (cached per font)."""
    if path not in _cmaps:
        _cmaps[path] = set((TTFont(path, lazy=True, fontNumber=0).getBestCmap() or {}).keys())
    return all(ord(c) in _cmaps[path] for c in text if not c.isspace())


SCRIPT_RANGES = {"bengali": (0x0980, 0x09FF), "devanagari": (0x0900, 0x097F)}


def script_share(text, script):
    """Fraction of the letters of text that belong to the script."""
    letters = [c for c in text if c.isalpha()]
    if not letters:
        return 0.0
    if script == "latin":
        return sum(c.isascii() for c in letters) / len(letters)
    lo, hi = SCRIPT_RANGES[script]
    return sum(lo <= ord(c) <= hi for c in letters) / len(letters)


class Corpus:
    """Sentences of one language, read from Wikipedia parquet files (column "text")."""

    def __init__(self, parquet_files, script, max_sentences=300_000, seed=0):
        import pyarrow.parquet as pq
        rng = random.Random(seed)
        sentences = []
        for path in parquet_files:
            for article in pq.read_table(path, columns=["text"]).column("text").to_pylist():
                for s in re.split(r"[।\.\n!?]+", article):
                    s = normalise(s)
                    if 8 <= len(s) <= 400 and script_share(s, script) >= 0.8:
                        sentences.append(s)
                if len(sentences) > max_sentences * 3:
                    break
        rng.shuffle(sentences)
        self.sentences = sentences[:max_sentences]
        self.words = [w for s in self.sentences[:20000] for w in s.split() if 2 <= len(w) <= 14]

    def line(self, rng, max_chars=48):
        s = rng.choice(self.sentences).split()
        start = rng.randrange(max(1, len(s)))
        out = []
        for w in s[start:]:
            if sum(len(x) + 1 for x in out) + len(w) > max_chars:
                break
            out.append(w)
        if not out:
            out = [s[start][:max_chars]]
        return " ".join(out)


def localise_digits(text, script, rng):
    table = BENGALI_DIGITS if script == "bengali" else DEVANAGARI_DIGITS
    if script in ("bengali", "devanagari") and rng.random() < 0.3:
        return "".join(table[int(c)] if c.isdigit() and c.isascii() else c for c in text)
    return text


class LineRenderer:
    """Draws a text line with a font that covers its script and applies random scan and camera damage."""

    def __init__(self, fonts):
        self.fonts = fonts  # script or "script+latin" -> list of font files

    def pick_font(self, text, script, rng):
        """A font that covers the script and has a glyph for every character of this line, or None."""
        has_latin = script != "latin" and any(c.isascii() and c.isalpha() for c in text)
        key = f"{script}+latin" if has_latin else script
        files = list(self.fonts.get(key) or self.fonts.get(script))
        rng.shuffle(files)
        for path in files[:6]:
            if font_has(path, text):
                return path
        return None

    def render(self, text, script, rng):
        """Returns an (48, W) uint8 array, or None if the line came out too wide."""
        path = self.pick_font(text, script, rng)
        if path is None:
            return None
        size = rng.randint(30, 64)
        try:
            font = ImageFont.truetype(path, size, layout_engine=ImageFont.Layout.RAQM)
        except OSError:
            return None
        asc, desc = font.getmetrics()
        left, _, right, _ = font.getbbox(text)
        pad_l, pad_r = rng.randint(2, 20), rng.randint(2, 20)
        pad_t, pad_b = rng.randint(1, 12), rng.randint(1, 12)
        w, h = right - left + pad_l + pad_r, asc + desc + pad_t + pad_b
        bg = rng.randint(165, 255)
        fg = rng.randint(0, 95)
        if rng.random() < 0.07:
            bg, fg = fg, bg  # light text on dark
        canvas = Image.new("L", (w, h), bg)
        draw = ImageDraw.Draw(canvas)
        draw.text((pad_l - left, pad_t), text, font=font, fill=fg)
        if rng.random() < 0.12:  # a neighbouring line cutting into the crop
            other = Image.new("L", (w, h), bg)
            ImageDraw.Draw(other).text((pad_l - left, pad_t + (asc + desc) * rng.choice([-1, 1]) * 0.95), text[::-1], font=font, fill=fg)
            canvas = Image.fromarray(np.minimum(np.asarray(canvas), np.asarray(other)))
        img = self.damage(canvas, bg, rng)
        new_w = round(img.width * LINE_HEIGHT / img.height)
        if new_w > MAX_WIDTH or new_w < 8:
            return None
        return np.asarray(img.resize((new_w, LINE_HEIGHT), Image.BILINEAR), dtype=np.uint8)

    @staticmethod
    def damage(img, bg, rng):
        if rng.random() < 0.5:
            img = img.rotate(rng.uniform(-1.5, 1.5), resample=Image.BILINEAR, expand=True, fillcolor=bg)
        if rng.random() < 0.3:
            shear = rng.uniform(-0.08, 0.08)
            img = img.transform(img.size, Image.AFFINE, (1, shear, -shear * img.height / 2, 0, 1, 0), resample=Image.BILINEAR, fillcolor=bg)
        a = np.asarray(img, dtype=np.float32)
        if rng.random() < 0.6:  # uneven paper
            low = np.random.RandomState(rng.randrange(1 << 30)).uniform(-1, 1, (3, 5)).astype(np.float32)
            shade = np.asarray(Image.fromarray(((low + 1) * 127.5).astype(np.uint8)).resize(img.size, Image.BICUBIC), dtype=np.float32)
            a = a + (shade - 127.5) * rng.uniform(0.04, 0.2)
        if rng.random() < 0.6:
            a = a + np.random.RandomState(rng.randrange(1 << 30)).normal(0, rng.uniform(2, 12), a.shape)
        img = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))
        if rng.random() < 0.5:
            img = img.filter(ImageFilter.GaussianBlur(rng.uniform(0.3, 1.3)))
        if rng.random() < 0.4:  # JPEG
            buf = io.BytesIO()
            img.save(buf, "JPEG", quality=rng.randint(35, 90))
            img = Image.open(io.BytesIO(buf.getvalue())).convert("L")
        if rng.random() < 0.3:  # contrast squeeze
            arr = np.asarray(img, dtype=np.float32)
            c = rng.uniform(0.45, 0.9)
            img = Image.fromarray(np.clip((arr - arr.mean()) * c + arr.mean(), 0, 255).astype(np.uint8))
        return img


class SyntheticSource:
    """Endless supply of (line image, text) pairs over the three scripts."""

    SCRIPT_WEIGHTS = (("bengali", 0.4), ("devanagari", 0.4), ("latin", 0.2))

    def __init__(self, corpora, fonts):
        self.corpora = corpora
        self.renderer = LineRenderer(fonts)

    def sample(self, rng):
        for _ in range(8):
            script = rng.choices([s for s, _ in self.SCRIPT_WEIGHTS], [w for _, w in self.SCRIPT_WEIGHTS])[0]
            corpus = self.corpora[script]
            text = corpus.line(rng, max_chars=rng.choice([12, 24, 36, 48]))
            if script != "latin" and rng.random() < 0.15 and self.corpora.get("latin"):  # an English word inside the line
                words = text.split()
                words.insert(rng.randrange(len(words) + 1), rng.choice(self.corpora["latin"].words))
                text = " ".join(words)
            text = localise_digits(text, script, rng)
            if not set(text) <= ALLOWED:
                continue
            img = self.renderer.render(text, script, rng)
            if img is not None:
                return img, normalise(text)
        raise RuntimeError("could not render a line in 8 tries")
