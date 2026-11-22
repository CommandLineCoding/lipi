# Lipi

Open-source, offline-first document scanner for Android, with on-device ML edge detection and multi-script OCR (including Indic scripts such as Bengali and Hindi).

> **Status:** planning. Nothing is released yet. See the [roadmap](#roadmap) and the [milestones](https://github.com/CommandLineCoding/lipi/milestones).

## Principles

- **Private by design.** No account, ads or trackers. Scanning and OCR run on the device and the app requests no network permission.
- **Fully FOSS.** Aimed at F-Droid first, so no proprietary dependencies: no ML Kit, Firebase or Play Services. Reproducible builds, fastlane metadata.
- **Portable core.** The image pipeline is a C++ library behind a C ABI with no Android types, so it can run in a desktop CLI (for testing and evaluation) and, later, on iOS.
- **Measured.** An evaluation harness (corner error / IoU, CER / WER, latency per device tier, PDF size at fixed quality) comes before model work.

## Planned stack

| Layer | Choice |
| --- | --- |
| UI | Kotlin, Jetpack Compose, Material 3, CameraX |
| Core | Portable C++ (NDK/JNI over a C ABI) |
| ML runtime | ONNX Runtime (or ncnn) |
| Storage | Room / SQLite with FTS5 |
| Background work | WorkManager |
| Targets | Android, arm64-v8a first; native libraries must be 16 KB page-aligned |

Several of these are still open questions; each is tracked as a `decision` issue.

## Design

UI draft: [FOSS Document Scanner – App Design](https://www.figma.com/design/7BVcimN4rnR0zgMLh78RXF) (Library, Camera, Adjust corners, Enhance, Export, Settings). The draft has no components, tokens or dark theme yet; that work is tracked in the issues.

## Roadmap

1. **Core + desktop CLI:** image in, clean deskewed page out; test corpus, evaluation harness, first detector model.
2. **Android v0.1:** CameraX capture, live quad detection, crop, enhance, PDF export, library. Released on F-Droid and GitHub.
3. **v0.2:** OCR, searchable PDF, language packs, full-text search. Google Play.
4. **v0.3+:** layout and tables, dewarp, redaction, compression.

Feature priorities are tracked with the `P0` / `P1` / `P2` labels.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Project documentation lives in [docs/](docs/) and follows the Diátaxis structure.

## License

[MIT](LICENSE). Because the project is MIT-licensed, dependencies under (A)GPL cannot be bundled.
