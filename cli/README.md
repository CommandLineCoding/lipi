# lipi-cli

A command-line front end for the Lipi core. It decodes images (JPEG, PNG, BMP and others, with EXIF rotation applied), runs the core, and writes the result. It is the main way to develop and evaluate the pipeline without a phone.

```bash
cmake -S . -B build -G Ninja && cmake --build build
./build/cli/lipi-cli scan photo.jpg -o page.png
./build/cli/lipi-cli detect photo.jpg --json
```

## Commands

| Command | What it does |
|---|---|
| `detect <image> [--model PREFIX] [--json]` | Finds the page outline and prints its four corners (top-left, top-right, bottom-right, bottom-left, in pixels), a confidence and the time taken. `--json` prints one JSON line (with the page orientation and which detector found it), which `ml/tools/predict_cli.py` reads. |
| `scan <image> -o <output>` | Finds the page, estimates its real aspect ratio, flattens it, cleans it up and saves it upright (`.png`, `.jpg` or `.bmp`). |
| `pdf <image>... -o <output.pdf>` | Scans each image and writes the pages to one PDF. Pages are JPEG (`--quality 1-100`, default 80) and sized from `--dpi` (default 150). Takes the same page options as `scan`. |
| `bench <image> [--repeat N] [--json]` | Times each stage (detect, warp, enhance, deskew) over N runs after a warm-up and prints the median and 95th percentile, on this machine. |
| `--version`, `--help` | |

Options for `scan`:

| Option | Meaning |
|---|---|
| `--quad x0,y0,x1,y1,x2,y2,x3,y3` | Use these corners instead of detecting them (top-left, top-right, bottom-right, bottom-left). |
| `--filter auto\|bw\|gray\|color\|none` | The look of the result, default `auto` (gray for a page with no colour content, else colour). |
| `--brightness B`, `--contrast C` | Between -1 and 1, default 0. |
| `--no-shadow-removal` | Keep the original lighting. |
| `--no-deskew` | Do not straighten tilted text lines. By default the page is straightened when the tilt is at least 0.3 degrees and the estimate is confident. |
| `--no-rotate` | Do not turn the page upright using the detector's orientation. |
| `--refine` | With `--model`, nudge the corners onto the nearest page edges (off by default). |
| `--max-side N` | Cap the longer side of the result, default 2400. |
| `--debug-dir DIR` | Save `01-input-quad.png` (the input with the outline drawn on it), `02-flattened.png` and `03-result.png`. |

## Learned detector

Build with `-DLIPI_WITH_NCNN=ON` (see [CONTRIBUTING.md](../CONTRIBUTING.md)), then pass `--model PREFIX`, where `PREFIX.ncnn.param` and `PREFIX.ncnn.bin` are an exported model. The learned detector finds the corners and the page orientation, and the classical detector is the fallback if it finds no page. Without `--model` the classical detector is used. `--refine` nudges the model's corners onto the nearest page edges on the full photo; it is off by default because on real photos it did not beat the model's own corners.

## Exit codes

| Code | Meaning |
|---|---|
| 0 | Success |
| 1 | Usage error (unknown command or option, invalid `--quad`) |
| 2 | A file could not be read, decoded or written |
| 3 | No page found |
| 4 | Internal error |
