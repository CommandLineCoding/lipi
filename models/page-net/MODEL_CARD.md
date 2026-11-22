# Model card: PageNet

A small network that finds the four corners of a page in a photo and says how the page is rotated. It runs on the phone, on every preview frame, and is the first stage of Lipi's scan pipeline.

## Files

| File | What it is |
|---|---|
| `page-net.ncnn.param`, `page-net.ncnn.bin` | The model for ncnn, which the core loads (`lipi_detector_create`). Weights are stored as 16-bit floats. |
| `page-net.onnx` | The same model in ONNX, kept for checking and for other tools. 32-bit floats. |
| `train.log` | Output of the training run. |
| `SHA256SUMS` | Checksums of the three model files. |

## Intended use

Finding the outline of a printed page, receipt, ID card, note or whiteboard in a camera frame or photo, so the page can be flattened. It is not a document classifier or an OCR model.

## Interface

- Input `in0`: a 3 x 256 x 256 float image, RGB, values 0 to 255. The photo is area-averaged down to this size (the core does this); the graph normalises it itself.
- Output `out0`: four corners (x, y between 0 and 1 of the image), ordered top-left, top-right, bottom-right, bottom-left as seen on the image.
- Output `out1`: the peak of each corner's heat map, 0 to 1. This is a rough confidence and has not been calibrated against IoU.
- Output `out2`: four logits for the page orientation, the number of clockwise quarter turns that make the flattened page upright.

## Architecture

A depth-wise separable encoder-decoder with skip connections (687 thousand parameters, 538 MFLOPs per image). The decoder works at stride 4 (64 x 64) and produces four corner heat maps and a page mask. Corners are read with a soft-argmax over each heat map, so they are sub-pixel and differentiable. An orientation head sits on the encoder's last features. The page mask is only used while training.

## Training

- Data: 20,006 synthetic pages and 671 real photos, described in [the dataset card](../datasets/lipi-corpus.md). The real photos are repeated 10 times per epoch so they make up about a quarter of what the model sees.
- Augmentation: random quarter turns, random crops that keep every corner inside, colour, blur and noise changes. No flips, because they would mirror the text and break the orientation label.
- Loss: smooth L1 on the corner positions, a KL term pulling each heat map towards a Gaussian at the true corner, binary cross-entropy on the page mask, and cross-entropy on the orientation (skipped for real photos, which have no orientation label).
- Optimiser: AdamW, learning rate 2e-3 with warm-up and cosine decay, batch size 128, 50 epochs, mixed precision, one A100. Checkpoint chosen by the mean IoU over the synthetic and real validation sets (epoch 43).
- Code: [`ml/train/`](../../ml/train/). Run `python -m train.train` (see its docstring), then `train.export_onnx` and `train.export_ncnn`.

## Results

Held-out test images, never used for training or for choosing the checkpoint. Corner error is the mean corner distance as a fraction of the image diagonal; IoU is the overlap of the predicted and true page quads.

| Split | Images | Corner error (median / p95) | IoU (median / mean) | IoU >= 0.90 | Orientation correct |
|---|---|---|---|---|---|
| Synthetic test | 2000 | 0.14% / 0.33% | 0.990 / 0.988 | 100% | 98.7% |
| Real test | 287 | 0.42% / 7.5% | 0.977 / 0.916 | 78% | not labelled |
| Real validation | 100 | 0.51% / 8.9% | 0.968 / 0.908 | 75% | not labelled |

For comparison, the classical detector scores a median IoU of 0.898 on the same real test photos and finds a page in 83% of them ([reports](../../ml/reports/)). The model always returns a quad.

The ncnn model gives the same numbers as the ONNX one, so the 16-bit weights and the conversion lose nothing measurable. Nudging the corners onto page edges afterwards (`lipi-cli --refine`) made the real results slightly worse (median corner error 0.58%), so it is off by default.

Speed, one thread of an x86 desktop CPU: about 7 ms with ONNX Runtime, about 9 ms through the core's ncnn detector including resizing the photo. Phone speed has not been measured yet ([#9](https://github.com/CommandLineCoding/lipi/issues/9)), so the 10 ms per frame target is unconfirmed.

## Limitations

- **Real photos are the weak spot.** 22% of real test pages are below IoU 0.90, and the 95th percentile of corner error is 7.5% of the diagonal. Failures are mostly pages that are partly out of frame, held in a hand, or low in contrast against the background.
- **Orientation is only measured on synthetic pages.** The real photos carry no orientation label, so the head learned it from synthetic pages alone, and it is visibly less reliable on real ones. Do not trust it without a check, for example the OCR text direction later on.
- **Narrow real-world coverage.** The real photos are mostly posters, flyers and printed pages with Persian or Arabic text, with few receipts, ID cards, books or handwritten notes. Curled pages and books are not covered.
- **The confidence output is uncalibrated.** Use IoU against labelled data, not the raw value, to pick a threshold.
- **Flat, four-cornered pages only.** One page per image; there is no "no page" output, so the app should ask the user to confirm or adjust the corners.

## Provenance and licence

Trained from scratch; no pretrained weights are used. The model files are released under the project's MIT licence. The training data is generated (synthetic part) or CC BY 4.0 (real part, Soheili, Mansouri and Baniadam Dizaj, 2020); see the dataset card for the citation and details.
