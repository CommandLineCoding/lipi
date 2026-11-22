[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)](LICENSE)
[![Milestones](https://img.shields.io/github/milestones/open/CommandLineCoding/lipi?style=flat-square&color=blue)](https://github.com/CommandLineCoding/lipi/milestones)
[![Core](https://github.com/CommandLineCoding/lipi/actions/workflows/core.yaml/badge.svg)](https://github.com/CommandLineCoding/lipi/actions/workflows/core.yaml)
[![Issues](https://img.shields.io/github/issues/CommandLineCoding/lipi?style=flat-square)](https://github.com/CommandLineCoding/lipi/issues)

---

# Lipi
Lipi is a privacy-first, open-source document scanner for Android. It finds the page in the camera view with an on-device model, flattens and cleans it up, and exports a PDF or image, with multi-script OCR (including Bengali and Hindi) planned for searchable PDFs. Everything runs on the phone, and the app needs no network permission.

> **Status:** early development. [v0.0.1](https://github.com/CommandLineCoding/lipi/releases/tag/v0.0.1) is the core library and a desktop command-line tool (`lipi-cli`); the Android app is next. The [milestones](https://github.com/CommandLineCoding/lipi/milestones) and [issues](https://github.com/CommandLineCoding/lipi/issues) show what is planned and what is free to pick up.

## Design

The UI draft is in Figma: [FOSS Document Scanner – App Design](https://www.figma.com/design/7BVcimN4rnR0zgMLh78RXF). It has six screens: Library, Camera, Adjust corners, Enhance, Export, and Settings (models and privacy). It has no components, tokens or dark theme yet; those are tracked in [#23](https://github.com/CommandLineCoding/lipi/issues/23).

## Features

Features are grouped by priority. `P0` is the first release and `P1` comes after it; the same labels are on the issues.

- **Live page detection (P0):** A small on-device model finds the four corners of the page in the camera preview, with the result smoothed across frames. Auto-capture takes the shot once the page is steady.
- **Scanning essentials (P0):** Batch capture, manual corner adjustment with a magnifier, perspective correction, orientation and deskew, shadow removal, and the Auto, B&W, Gray and Color filters.
- **Export and library (P0):** PDF and JPG export, written by Lipi's own PDF writer, and a local library of documents.
- **Private by design (P0):** No account, ads or trackers. The app asks for no network permission, and CI will check that.
- **OCR (P1):** On-device text recognition with downloadable language packs for Bengali and Hindi, searchable PDF, and full-text search over the library.
- **More tools (P1):** Scan modes (document, ID card, book, whiteboard, receipt), merge/split/reorder, annotation and signing, PII redaction, glare reduction, dewarping, layout and tables, MRC compression, PDF/A, an app lock, and WebDAV, Nextcloud and Paperless-ngx upload.

## Principles

- **Fully FOSS.** F-Droid comes first, so there are no proprietary dependencies: no ML Kit, Firebase or Play Services. Builds are reproducible, and store text lives in [metadata/](metadata/).
- **Portable core.** The image pipeline is a C++ library behind a C ABI with no Android types, so it also runs in a desktop CLI and could later run on iOS.
- **Measured.** An evaluation harness (corner error and IoU, CER and WER, latency per device tier, PDF size at fixed quality) comes before model work.
- **Models have provenance.** Every model ships with a model card, a dataset card and its training script.

## Tech Stack

| Layer | Technology |
|---|---|
| App | Kotlin, Jetpack Compose (Material 3), CameraX |
| Core | Portable C++ behind a C ABI, called through NDK/JNI |
| ML runtime | ncnn, converted from PyTorch with pnnx (ONNX Runtime was measured and rejected for its size, see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)) |
| Storage | Room / SQLite with FTS5 |
| Background work | WorkManager |
| Targets | Android, `arm64-v8a` first, with 16 KB page-aligned native libraries |
| CI/CD | GitHub Actions |

## Roadmap

| Milestone | Goal |
|---|---|
| [v0.0.1 — Core & CLI](https://github.com/CommandLineCoding/lipi/milestone/1?closed=1) (released) | Image in, clean deskewed page out. Test corpus, evaluation harness, first detector model. |
| [v0.1.0 — Android MVP](https://github.com/CommandLineCoding/lipi/milestones) | CameraX capture, live detection, crop, enhance, PDF export, library. Released on F-Droid and GitHub. |
| [v0.2.0 — OCR & searchable PDF](https://github.com/CommandLineCoding/lipi/milestones) | On-device OCR, searchable PDF, language packs, full-text search. Google Play. |
| [v0.3.0 — Layout, dewarp & redaction](https://github.com/CommandLineCoding/lipi/milestones) | Layout and tables, dewarping, redaction, compression. |

## Project Structure

The layout below is the plan; directories appear as their issues land.

```
lipi/
├── .github/                  # Issue forms, PR template and GitHub Actions workflows
├── CMakeLists.txt            # Top-level build for the core, CLI and tests
├── docs/
│   ├── ARCHITECTURE.md       # How the pieces fit together
│   └── CORE_API.md           # C ABI contract of the core library
├── metadata/                 # F-Droid / store listing metadata
├── core/                     # Portable C++ image pipeline (C ABI, no Android types)
├── cli/                      # Desktop CLI built on the core
├── android/                  # Kotlin + Compose app (Gradle project; see CONTRIBUTING.md)
├── tests/                    # Unit tests (doctest) and small test images
├── third_party/              # Vendored single-header libraries with their licenses
├── ml/                       # Training scripts and the evaluation harness
├── models/                   # Model cards, dataset cards and checksums
├── CONTRIBUTING.md
├── SECURITY.md
├── CODE_OF_CONDUCT.md
└── LICENSE
```

## Contributing

Contributions are welcome, and you don't need to know machine learning to help. Read [CONTRIBUTING.md](CONTRIBUTING.md) for setup, how to claim an issue, and how to open a PR.

- New here? Pick a [`good first issue`](https://github.com/CommandLineCoding/lipi/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22).
- See what's planned in the [milestones](https://github.com/CommandLineCoding/lipi/milestones).
- Questions and ideas go in [Discussions](https://github.com/CommandLineCoding/lipi/discussions).

Please also read the [Code of Conduct](CODE_OF_CONDUCT.md). To report a security issue, follow [SECURITY.md](SECURITY.md) instead of opening a public issue.

## License

Lipi is licensed under the MIT License. See [LICENSE](LICENSE) for details. Because of this, dependencies under (A)GPL cannot be bundled.
