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
| `detect <image> [--json]` | Finds the page outline and prints its four corners (top-left, top-right, bottom-right, bottom-left, in pixels), a confidence and the time taken. `--json` prints one JSON line, which `ml/tools/predict_cli.py` reads. |
| `scan <image> -o <output>` | Finds the page, estimates its real aspect ratio and flattens it to an upright image (`.png`, `.jpg` or `.bmp`). |
| `--version`, `--help` | |

Options for `scan`:

| Option | Meaning |
|---|---|
| `--quad x0,y0,x1,y1,x2,y2,x3,y3` | Use these corners instead of detecting them (top-left, top-right, bottom-right, bottom-left). |
| `--max-side N` | Cap the longer side of the result, default 2400. |
| `--debug-dir DIR` | Save `01-input-quad.png` (the input with the outline drawn on it) and `02-flattened.png`. |

Filters (Auto, B&W, Gray, Color) are not wired in yet; they arrive with [#13](https://github.com/CommandLineCoding/lipi/issues/13).

## Exit codes

| Code | Meaning |
|---|---|
| 0 | Success |
| 1 | Usage error (unknown command or option, invalid `--quad`) |
| 2 | A file could not be read, decoded or written |
| 3 | No page found |
| 4 | Internal error |
