# Lipi

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)](LICENSE)
[![Core](https://github.com/CommandLineCoding/lipi/actions/workflows/core.yaml/badge.svg)](https://github.com/CommandLineCoding/lipi/actions/workflows/core.yaml)
[![Android](https://github.com/CommandLineCoding/lipi/actions/workflows/android.yaml/badge.svg)](https://github.com/CommandLineCoding/lipi/actions/workflows/android.yaml)

Lipi is a free document scanner for Android. It finds a page in a photo, straightens it, evens out the lighting and saves the result as an image or a PDF. Everything runs on the device, and the app does not request the network permission.

## Status

[v0.0.1](https://github.com/CommandLineCoding/lipi/releases/tag/v0.0.1) is the core library and a command-line tool for Linux. The Android app is in development for v0.1.0, and text recognition (OCR) for Bengali and Hindi is planned for v0.2.0. See the [milestones](https://github.com/CommandLineCoding/lipi/milestones).

## What works today

- Page detection with a small neural network. On held-out real photos the outline overlaps the true page with a median IoU of 0.98. On a Snapdragon 865 a preview frame takes 15 ms with four threads.
- Perspective correction, deskew, shadow removal and the Auto, B&W, Gray and Color filters.
- Multi-page PDF export with Lipi's own PDF writer.

Measurements and limits are in the [model card](models/page-net/MODEL_CARD.md).

## Build and try it

You need CMake 3.20 or newer and a C++17 compiler.

```
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
./build/cli/lipi-cli scan photo.jpg -o page.png
```

To use the neural detector, build with ncnn as described in [CONTRIBUTING.md](CONTRIBUTING.md) and pass `--model models/page-net/page-net`. The [CLI reference](cli/README.md) lists all commands.

The Android app is built from `android/` with `./gradlew assembleDebug`.

## Repository

| Path | Contents |
|---|---|
| `core/` | Portable C++ library with a C API |
| `cli/` | `lipi-cli` command-line tool |
| `android/` | Kotlin and Compose app |
| `ml/`, `models/` | Training and evaluation code, the model, model and dataset cards |
| `docs/` | [Architecture](docs/ARCHITECTURE.md), [C API](docs/CORE_API.md), [design tokens](docs/DESIGN.md), [roadmap](docs/ROADMAP.md) |

The UI follows this [Figma design](https://www.figma.com/design/7BVcimN4rnR0zgMLh78RXF).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Report security issues as described in [SECURITY.md](SECURITY.md).

## License

[MIT](LICENSE). Dependencies licensed under the GPL or AGPL cannot be bundled.
