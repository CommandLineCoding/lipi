# ml

Training scripts and the evaluation harness for Lipi's page detection. Everything here is plain Python; the app and the core do not depend on it.

```bash
python3 -m venv .venv && . .venv/bin/activate
pip install -r requirements.txt
python -m pytest -q
```

## Corpus

The corpus has a synthetic part (exact labels, no third-party images) and a real part (phone photos from a public CC BY 4.0 dataset). See [the dataset card](../models/datasets/lipi-corpus.md) for sources, licenses and the fixed splits.

```bash
# synthetic splits (needs Noto fonts: Latin, Bengali and Devanagari)
python tools/synth.py --out data/synth --split train --seed 1 --count 20000
python tools/synth.py --out data/synth-val --split val --seed 2 --count 1000
python tools/synth.py --out data/synth-test --split test --seed 3 --count 2000

# real validation and test photos (about 1 GB is downloaded, 512 px copies are kept)
python tools/fetch_mendeley.py --out data/real --count 400
```

The annotation format is documented in [lipi_eval/corpus.py](lipi_eval/corpus.py). Corners are always top-left, top-right, bottom-right, bottom-left as seen on the image, matching [docs/CORE_API.md](../docs/CORE_API.md).

## Scoring a detector

A detector writes one JSON line per image (`id`, `quad` or `null`, optional `orientation` and `ms`). The scorer compares it with the corpus:

```bash
python -m lipi_eval.score --corpus data/synth-test/test.jsonl --pred preds.jsonl --out report.json
```

The report has, per source and overall: mean, median and 95th percentile corner error (mean corner distance divided by the image diagonal, taking the best of the four cyclic shifts), IoU of the page quad, the share of images above 0.90 and 0.95 IoU, failures (no quad, counted as IoU 0), latency and orientation accuracy. Keys are sorted and numbers rounded so the file can be diffed in CI; [tests/fixtures/](tests/fixtures/) holds a tiny example that the tests compare against.

Latency here is wall time on the machine that ran the detector. It says nothing about phones; the on-device target is measured separately ([#9](https://github.com/CommandLineCoding/lipi/issues/9)).

## Baselines

Reports are kept in [reports/](reports/). They are produced by the classical detector in the core (`lipi-cli detect`, Release build) through [tools/predict_cli.py](tools/predict_cli.py):

```bash
python tools/predict_cli.py --cli ../build/cli/lipi-cli --corpus data/real/test.jsonl --out preds.jsonl
python -m lipi_eval.score --corpus data/real/test.jsonl --pred preds.jsonl --out report.json
```

| Detector | Split | Images | Found | Corner error (median / p95) | IoU (median) | IoU >= 0.90 | Latency (mean, desktop) |
|---|---|---|---|---|---|---|---|
| Classical | synthetic test | 2000 | 69% | 0.08% / 4.4% | 0.978 | 61% | 18.6 ms |
| Classical | real validation | 100 | 84% | 1.8% / 12.9% | 0.830 | 43% | 18.3 ms |
| Classical | real test | 287 | 83% | 0.95% / 17.4% | 0.898 | 49% | 20.0 ms |

Corner error is a fraction of the image diagonal. IoU counts a missed page as 0, so it also reflects how often a page is found. The classical detector is accurate when it locks onto the page outline and misses it too often on cluttered backgrounds, which is what the learned detector is meant to fix. Latency is desktop wall time including process start-up, not a phone measurement.

### Learned detector

[PageNet](../models/page-net/MODEL_CARD.md), run through the ncnn detector in `lipi-cli --model` (corners as the model predicts them, no refinement):

| Detector | Split | Images | Found | Corner error (median / p95) | IoU (median) | IoU >= 0.90 |
|---|---|---|---|---|---|---|
| PageNet | synthetic test | 2000 | 100% | 0.14% / 0.33% | 0.990 | 100% |
| PageNet | real validation | 100 | 100% | 0.51% / 8.9% | 0.968 | 75% |
| PageNet | real test | 287 | 100% | 0.42% / 7.5% | 0.977 | 78% |
| PageNet + `--refine` | real test | 287 | 100% | 0.58% / 7.4% | 0.967 | 78% |

The learned detector always returns a quad, so there are no misses; compare IoU with the classical rows above, where a miss counts as 0. Refinement is reported to show why it is off by default. Reports: `reports/page-net-*.json`. To reproduce, build the CLI with `LIPI_WITH_NCNN=ON` and run `tools/predict_cli.py --model ../models/page-net/page-net` on each split.

### Orientation by document type

The orientation head's accuracy on the 2000 synthetic test images, from `reports/page-net-synth-test.json` (the `kinds` section):

| Document type | Images | Orientation correct | IoU (mean) |
|---|---|---|---|
| Document | 827 | 99.8% | 0.991 |
| ID card | 289 | 100% | 0.984 |
| Notes | 261 | 100% | 0.991 |
| Receipt | 418 | 97.6% | 0.985 |
| Whiteboard | 205 | 92.2% | 0.984 |

Real photos are not labelled for orientation, so this is only measured on synthetic pages.

### PDF size and latency

From `tools/bench_pdf.py` on the 97 real validation photos at 2000 x 1500, with the learned detector, one thread of an x86 desktop CPU ([report](reports/pdf-size-and-latency.json)).

| Setting | KB per page, mean (median) |
|---|---|
| Auto filter, JPEG quality 60 | 105 (96) |
| Auto filter, JPEG quality 80 | 159 (145) |
| Auto filter, JPEG quality 90 | 234 (212) |
| Colour, quality 80 | 161 (151) |
| Gray, quality 80 | 151 (142) |
| B&W, quality 80 | 175 (150) |

B&W pages are not smaller because they are still stored as JPEG; real bi-level and MRC compression is planned ([#66](https://github.com/CommandLineCoding/lipi/issues/66)).

Time per stage for one 2000 x 1500 photo (median of the per-image medians over 20 runs): detection 27 ms (most of it resizing the photo down to the model's 256 x 256 input; the network itself takes about 9 ms), flattening 73 ms, clean-up 92 ms, straightening 38 ms, 217 ms in all, measured on an otherwise idle machine. Live preview only runs detection, on a smaller frame.

On a phone (Samsung SM-G781B, Snapdragon 865, `lipi-cli` cross-compiled with the Android NDK, one thread): detection takes 30.6 ms on a 640 x 480 preview frame (20.2 ms with two threads, 15.0 ms with four), and a whole scan takes 52 ms for 640 x 480, 167 ms for 2000 x 1500 and 492 ms for 12 megapixels ([report](reports/on-device-sm-g781b.json)).

### Retraining with ID-card data, and the small model

A second training run added 2,400 MIDV ID-card frames to the data ([dataset card](../models/datasets/lipi-corpus.md)); a third run trained a narrower network (width 0.6) on the same data. All three are scored on the same held-out sets, all through ONNX (the ncnn files give identical results). The paired bootstrap compares per-photo IoU on the 287 real test photos.

| Model | Real test: mean / median IoU, share >= 0.90 | Synthetic test: mean IoU, orientation | Parameters | Phone, 1 thread (640 x 480 frame) |
|---|---|---|---|---|
| PageNet (shipped) | 0.916 / 0.977, 78.4% | 0.988, 98.6% | 687,145 | 30.3 ms |
| Retrained with MIDV (not shipped) | 0.924 / 0.976, 81.2% | 0.987, 98.2% | 687,145 | same network |
| [PageNet small](../models/page-net-small/MODEL_CARD.md) | 0.919 / 0.972, 80.5% | 0.982, 97.4% | 255,608 | 16.6 ms |

The MIDV retrain is +0.008 mean IoU on real test photos (95% interval +0.002 to +0.016; share at 0.90: +2.8 points, interval -0.7 to +6.3; better on 58 photos, worse on 45, about equal on 184), and slightly worse on synthetic pages, so it was not shipped. The small model is within noise of PageNet on real photos (+0.003, interval -0.004 to +0.010) and 45% faster on the phone, but worse on synthetic pages and on orientation. Reports: `reports/page-net-v3-*.json`, `reports/page-net-small-*.json`, [`reports/on-device-small-sm-g781b.json`](reports/on-device-small-sm-g781b.json).
