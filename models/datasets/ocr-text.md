# Dataset card: text recognition (Bengali, Hindi, Latin)

The data for Lipi's text recogniser: real printed words for Bengali and Hindi, and endless synthetic text lines drawn with open fonts from Wikipedia text. Nothing here is distributed by this repository; the scripts in [`ml/ocr/`](../../ml/ocr/) fetch and build it. This card records what is used and under which terms.

## Sources

| Source | What | Licence | Size | Used for |
|---|---|---|---|---|
| [Mozhi-Bengali](https://ilocr.iiit.ac.in/dataset/4/) (IIIT Hyderabad) | Printed word images cropped from 1,000 scanned book pages at 300 DPI, hand-transcribed | CC BY 4.0 (stated on the dataset page) | 100,013 words: 80,113 train, 9,787 validation, 10,113 test | Training, validation, test |
| [Mozhi-Hindi](https://ilocr.iiit.ac.in/dataset/7/) (IIIT Hyderabad) | The same for Hindi (Devanagari), 600 DPI | CC BY 4.0 (stated on the dataset page) | 100,049 words: 79,762 train, 10,114 validation, 10,173 test | Training, validation, test |
| Wikipedia text (Bengali, Hindi, Simple English), 2023-11-01 snapshot from [wikimedia/wikipedia](https://huggingface.co/datasets/wikimedia/wikipedia) | Sentences that are drawn as synthetic lines | CC BY-SA (4.0 for recent text; older text is CC BY-SA 3.0 and GFDL), see the caveat below | 300,000 sentences per language are kept | Text for synthetic lines |
| Open fonts from the [Google Fonts repository](https://github.com/google/fonts) | 18 Bengali, 83 Devanagari and 157 Latin font files (families such as Noto Sans and Serif, Hind, Mukta, Tiro) | SIL Open Font License 1.1 | 157 files, not redistributed | Drawing the lines |

Not used: the IIIT synthetic Hindi sets (IIIT-Synthetic-M-Hindi and -R-Hindi), because their pages state no licence. There is no open real-world Latin set in this collection, so Latin text is only synthetic here.

## How the lines are made

Each training sample is either a real Mozhi word (35% of samples) or a synthetic line (65%). A synthetic line is a random run of words from a Wikipedia sentence, at most 12, 24, 36 or 48 characters (Bengali 40%, Hindi 40%, Latin 20%), with English words inserted into some Bengali and Hindi lines and some digits turned into Bengali or Devanagari digits. It is drawn with a random font that has a glyph for every character of the line, using HarfBuzz so that conjuncts and matras are shaped correctly, and then damaged: tilt, shear, uneven paper, noise, blur, JPEG compression, lower contrast, light text on a dark background, and a neighbouring line cutting into the crop. Lines are scaled to 48 pixels high and are at most 640 pixels wide. The validation set of 3,000 synthetic lines uses a fixed seed.

## Caveats

- **Wikipedia text is CC BY-SA (and partly GFDL).** The text is turned into images and the trained weights contain no text, but whether weights count as an adaptation under ShareAlike is legally unsettled, as with the ID-card data in [the page-detection corpus](lipi-corpus.md). Anyone who redistributes the model should make their own assessment.
- **Mozhi words are cropped from book pages**, so they cover book typography. Receipts, forms, signs and handwriting are not covered.
- **Latin has no real test set**, so claims about English text rest on synthetic lines and need a real test before they are made.

## Attribution

Mozhi-Bengali and Mozhi-Hindi: IIIT Hyderabad, Centre for Visual Information Technology, <https://ilocr.iiit.ac.in/>, licensed CC BY 4.0. Wikipedia text: Wikipedia contributors, CC BY-SA. Fonts: their authors, SIL OFL 1.1.
