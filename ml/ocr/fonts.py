"""Find which fonts can draw which script.

    python -m ocr.fonts /path/to/fonts

A font is kept for a script only if it has a glyph for every sample character, so synthetic lines never contain
missing-glyph boxes. Only font files that are OFL or Apache licensed should be put in the folder.
"""
import json
import os
import sys

from fontTools.ttLib import TTFont

SAMPLES = {
    "bengali": "কখগঘঙচছজঝঞটঠডঢণতথদধনপফবভমযরলশষসহড়ঢ়য়ৎংঃঁািীুূৃেৈোৌ্০১২৩৪৫৬৭৮৯।",
    "devanagari": "कखगघङचछजझञटठडढणतथदधनपफबभमयरलवशषसहािीुूृेैोौ्ंःँ०१२३४५६७८९।",
    "latin": "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,;:!?-()",
}


def covers(path, text):
    """True if the font file has a glyph for every non-space character of text."""
    try:
        cmap = TTFont(path, lazy=True, fontNumber=0).getBestCmap() or {}
    except Exception:
        return False
    return all(ord(c) in cmap for c in text if not c.isspace())


def scan(root):
    """Maps each script (and "bengali+latin", "devanagari+latin") to the fonts under root that cover it."""
    files = sorted(os.path.join(d, f) for d, _, fs in os.walk(root) for f in fs if f.lower().endswith((".ttf", ".otf")))
    out = {s: [] for s in list(SAMPLES) + ["bengali+latin", "devanagari+latin"]}
    for path in files:
        has = {s: covers(path, t) for s, t in SAMPLES.items()}
        for s in SAMPLES:
            if has[s]:
                out[s].append(path)
        if has["bengali"] and has["latin"]:
            out["bengali+latin"].append(path)
        if has["devanagari"] and has["latin"]:
            out["devanagari+latin"].append(path)
    return out


def load_or_scan(root, cache="fonts.json"):
    path = os.path.join(root, cache)
    if os.path.exists(path):
        return json.load(open(path))
    found = scan(root)
    with open(path, "w") as f:
        json.dump(found, f, indent=1)
    return found


if __name__ == "__main__":
    found = scan(sys.argv[1])
    print({k: len(v) for k, v in found.items()})
