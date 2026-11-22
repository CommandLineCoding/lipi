# Lipi — architecture

An offline-first document scanner built on a portable C++ core, on-device models and a Kotlin app. This document describes the planned shape of the system and traces one scan end to end, from the camera preview to an exported file.

Nothing leaves the device. The app requests no network permission, and CI will check that ([#40](https://github.com/CommandLineCoding/lipi/issues/40)). Language packs are the one open question here ([#20](https://github.com/CommandLineCoding/lipi/issues/20)).

---

## 1. The three tiers

**Android app — Kotlin / Jetpack Compose**

Three concerns, kept separate. Compose screens follow the [Figma draft](https://www.figma.com/design/7BVcimN4rnR0zgMLh78RXF) and hold only UI state. CameraX owns the preview and capture. Room (SQLite with FTS5 for text search) holds documents and pages, so the library renders without touching the pipeline. WorkManager runs slow work, such as OCR, in the background.

**Native core — C++ behind a C ABI**

The whole image pipeline lives here: corner detection, perspective correction, orientation and deskew, filters, and the PDF writer. The public surface is a C header with opaque handles and no Android types, so the same code runs in the desktop CLI, in the app through JNI, and later on iOS. Whether the core stays C++ or becomes Rust is still open ([#1](https://github.com/CommandLineCoding/lipi/issues/1)); the C ABI is the contract either way. See [CORE_API.md](CORE_API.md).

**Models — ONNX Runtime**

Small networks run through ONNX Runtime (ncnn is the alternative, [#16](https://github.com/CommandLineCoding/lipi/issues/16)): a page-corner detector, an orientation classifier and, later, OCR and layout models. A small core set is bundled, and the rest are downloadable with checksums. Every model has a model card, a dataset card and its training script in `models/` and `ml/`.

---

## 2. One scan, step by step

### Step 1 — the preview frame reaches the detector

CameraX delivers a low-resolution frame, which the app passes to the core. The detector works at about 256 px and must stay under 10 ms per frame on a mid-tier device, so it can run on every preview frame without dropping the preview.

### Step 2 — corners are refined and smoothed

The network gives four corners with a confidence. A classical refinement step moves them to the true page edge, and temporal smoothing keeps them from jittering between frames. The app draws the result over the preview.

### Step 3 — capture

Auto-capture fires when the quad has been stable for a short time and the quality hints (blur, glare, low light) pass. The full-resolution frame is captured, and the same detection runs on it.

### Step 4 — the user adjusts the corners

The Adjust screen shows the detected quad. The user can drag any corner, with a magnifier following the finger, or re-run detection.

### Step 5 — the page is flattened and cleaned

The core warps the quad to a rectangle that keeps the page's aspect ratio, fixes orientation and residual skew, removes shadows, and applies the chosen filter (Auto, B&W, Gray or Color) with brightness and contrast on top.

### Step 6 — export

The pages go through Lipi's own PDF writer, which avoids AGPL libraries such as MuPDF and iText. Later milestones add an invisible OCR text layer for searchable PDFs, MRC compression and PDF/A.

---

## 3. Evaluation

Every change to the pipeline or a model is measured by the harness in `ml/`, with the same corpus each time: corner error and IoU, latency per device tier, and PDF size at a fixed quality (and CER/WER once OCR lands). The harness comes first in the plan ([#8](https://github.com/CommandLineCoding/lipi/issues/8), [#9](https://github.com/CommandLineCoding/lipi/issues/9)) so no model work is judged by eye.

---

## 4. Distribution constraints

These shape the design, so they are worth knowing before you add a dependency:

- **F-Droid first.** No proprietary dependencies (no ML Kit, Firebase or Play Services), reproducible builds, and fastlane-style store metadata in `metadata/`.
- **MIT license.** No (A)GPL code can be bundled.
- **Native libraries.** `arm64-v8a` first, and every `.so` must be 16 KB page-aligned.
