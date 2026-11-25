"""The characters the recogniser can output, and the text normalisation that goes with it.

Index 0 is the CTC blank. Text is normalised to NFC, and the zero-width joiners are dropped because they leave
nothing visible for the network to read.
"""
import string
import unicodedata

BENGALI_DIGITS = "০১২৩৪৫৬৭৮৯"
DEVANAGARI_DIGITS = "०१२३४५६७८९"
EXTRA = "।॥৳₹€£$%&@#*+=/<>[]{}'\"_~^|\\"
BASE = string.ascii_letters + string.digits + string.punctuation + BENGALI_DIGITS + DEVANAGARI_DIGITS + EXTRA + " "


def normalise(text):
    text = unicodedata.normalize("NFC", text)
    return text.replace("‍", "").replace("‌", "").replace(" ", " ").strip()


class Charset:
    def __init__(self, chars):
        seen = []
        for c in chars:
            if c not in seen:
                seen.append(c)
        self.chars = seen
        self.index = {c: i + 1 for i, c in enumerate(seen)}

    def __len__(self):
        return len(self.chars) + 1  # plus the blank

    def encode(self, text):
        """Ids for the text; characters outside the set are skipped."""
        return [self.index[c] for c in normalise(text) if c in self.index]

    def covers(self, text):
        return all(c in self.index for c in normalise(text))

    def decode(self, ids):
        """CTC greedy decoding: collapse repeats, drop blanks."""
        out, prev = [], 0
        for i in ids:
            if i != prev and i != 0:
                out.append(self.chars[i - 1])
            prev = i
        return "".join(out)

    def save(self, path):
        with open(path, "w", encoding="utf-8") as f:
            f.write("\n".join(self.chars))

    @classmethod
    def load(cls, path):
        return cls(open(path, encoding="utf-8").read().split("\n"))


def build(label_texts, min_count=3):
    """Charset from the base characters plus every character seen at least min_count times in the labels."""
    counts = {}
    for t in label_texts:
        for c in normalise(t):
            counts[c] = counts.get(c, 0) + 1
    extra = sorted(c for c, n in counts.items() if n >= min_count and c not in BASE and not c.isspace())
    return Charset(list(BASE) + extra)
