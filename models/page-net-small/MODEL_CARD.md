# Model card: PageNet small

A narrower version of [PageNet](../page-net/MODEL_CARD.md) for places where speed matters more than the last bit of accuracy, such as the live camera preview. It has the same inputs and outputs as PageNet, so the two can be swapped without changing any code.

## Files

| File | What it is |
|---|---|
| `page-net-small.ncnn.param`, `page-net-small.ncnn.bin` | The model for ncnn (16-bit weights, 0.5 MB). |
| `page-net-small.onnx` | The same model in ONNX, 32-bit floats. |
| `train.log` | Output of the training run. |
| `SHA256SUMS` | Checksums of the three model files. |

## How it differs from PageNet

The network is the same design with every layer's width multiplied by 0.6: 255,608 parameters against 687,145, and 203 MFLOPs per image against 538. Training is the same too (50 epochs, same loss and augmentation, checkpoint chosen by mean IoU over the synthetic and real validation sets), but the data has one addition: 2,400 ID-card frames from MIDV-500 and MIDV-2019.

## Training data and licence

Synthetic pages, the 671 real training photos (repeated 10 times per epoch) and 2,400 MIDV ID-card frames, described in [the dataset card](../datasets/lipi-corpus.md). **The MIDV frames are licensed CC BY-SA 2.5.** They are not distributed here, only a model trained on them. Whether trained weights count as an adaptation under ShareAlike is legally unsettled, so anyone who redistributes this model should make their own assessment. Attribution: Arlazarov V.V., Bulatov K., Chernov T., Arlazarov V.L., "MIDV-500: a dataset for identity document analysis and recognition on mobile devices in video stream", Computer Optics 43(5), 2019; and Bulatov K., Matalov D., Arlazarov V.V., "MIDV-2019: challenges of the modern mobile-based document OCR", Proc. SPIE 11433 (ICMV 2019), 2020. PageNet itself was trained without MIDV data and is not affected.

## Results

Held-out images, never used for training or for choosing the checkpoint. Corner error is the mean corner distance as a fraction of the image diagonal.

| Split | Images | Corner error (median / p95) | IoU (median / mean) | IoU >= 0.90 | Orientation correct |
|---|---|---|---|---|---|
| Real test | 287 | 0.51% / 6.5% | 0.972 / 0.919 | 80.5% | not labelled |
| Real validation | 100 | 0.68% / 8.2% | 0.961 / 0.905 | 78.0% | not labelled |
| Synthetic test | 2000 | 0.18% / 0.6% | 0.988 / 0.982 | 99.0% | 97.4% |

Against PageNet on the same real test photos (0.977 / 0.916, 78.4%, p95 7.4%), the small model is the same within noise. A paired bootstrap over the photos gives a mean IoU difference of +0.003 on the test set (95% interval -0.004 to +0.010) and -0.003 on the validation set (-0.015 to +0.009); the share at IoU 0.90 or better differs by +2.1 points (-1.4 to +5.6) on test. It is clearly worse on synthetic pages (mean IoU 0.982 against 0.988, orientation 97.4% against 98.6%), and orientation on whiteboards is only 84.4% (receipts 95.9%, documents 99.6%, ID cards and notes 100%).

The ncnn model gives the same real-test numbers as the ONNX one (IoU 0.9186 mean in both).

## Speed

Detecting a 640 x 480 preview frame, median over 30 runs:

| | Full PageNet | PageNet small |
|---|---|---|
| Samsung SM-G781B (Snapdragon 865), 1 thread | 30.3 ms | 16.6 ms |
| same phone, 2 threads | about 20 ms | about 12.6 ms |
| same phone, 4 threads | about 18 ms | about 13 ms (11.8 and 15.1 in two rounds) |
| x86 desktop, 1 thread | 10.1 ms | 6.4 ms |

That is about 45% faster on one thread and 35 to 40% faster on two. It does not reach the 10 ms goal, and extra threads help it little because the network is so small. Part of every figure is the fixed cost of shrinking the frame to the model's 256 x 256 input. Only one phone was measured ([report](../../ml/reports/on-device-small-sm-g781b.json)).

## Limitations

Everything in the PageNet card applies, and in addition:

- **Weaker on synthetic pages and on orientation**, as above, so use it for finding the page, not for deciding which way up the page is.
- **No gain on real photos over PageNet**, only speed. Where there is time (for example the final detection after the shutter), PageNet is the better choice.
- **Whiteboards are the weakest document type** for orientation (84.4%).
- **MIDV frames are ID cards photographed from video**, with 50 document types, so the gain they may bring is mostly for ID cards, which are rare in the real test photos.
