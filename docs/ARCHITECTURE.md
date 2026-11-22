# Lipi — architecture

An offline-first document scanner built on a portable C++ core, on-device models and a Kotlin app. This document describes the planned shape of the system and traces one scan end to end, from the camera preview to an exported file.

Nothing leaves the device. The app requests no network permission, and CI will check that ([#40](https://github.com/CommandLineCoding/lipi/issues/40)). Language packs are the one open question here ([#20](https://github.com/CommandLineCoding/lipi/issues/20)).

---

## 1. The three tiers

**Android app — Kotlin / Jetpack Compose**

Three concerns, kept separate. Compose screens follow the [Figma draft](https://www.figma.com/design/7BVcimN4rnR0zgMLh78RXF) and hold only UI state. CameraX owns the preview and capture. Room (SQLite with FTS5 for text search) holds documents and pages, so the library renders without touching the pipeline. WorkManager runs slow work, such as OCR, in the background.

**Native core — C++ behind a C ABI**

The whole image pipeline lives here: corner detection, perspective correction, orientation and deskew, filters, and the PDF writer. The public surface is a C header with opaque handles and no Android types, so the same code runs in the desktop CLI, in the app through JNI, and later on iOS. See [CORE_API.md](CORE_API.md).

**Models — ncnn**

Small networks run through [ncnn](https://github.com/Tencent/ncnn) (BSD-3-Clause): a page-corner detector that also predicts the page orientation and, later, OCR and layout models. A small core set is bundled, and the rest are downloadable with checksums. Every model has a model card, a dataset card and its training script in `models/` and `ml/`.

ncnn was chosen over ONNX Runtime for three reasons, measured on the page detector:

- **Size.** The prebuilt full ONNX Runtime library for Android arm64 is 35 MB; ncnn's arm64 static library is 7.7 MB before linking, and it only pulls in the layers a model uses.
- **Build from source.** ncnn builds with plain CMake and no dependencies, which F-Droid's build-from-source rule needs. ONNX Runtime would need a custom minimal build.
- **Portability.** The same code and model run in the desktop CLI and on Android.

Speed is a wash on a desktop CPU. For the same page detector (687 thousand parameters, 538 MFLOPs per 256 x 256 image), one thread of an x86 desktop: ONNX Runtime 1.32 takes a median 7.2 ms per image, and the ncnn detector in the core 9.3 ms, which includes resizing the photo and copying it into the network. Both return the same corners to within 0.01 pixel. Phone timings have not been measured yet ([#9](https://github.com/CommandLineCoding/lipi/issues/9)); the on-device target of 10 ms per frame is not yet confirmed.

The model is trained in PyTorch, exported to ONNX for checking, and converted to ncnn with pnnx. The core is built with the detector only when `LIPI_WITH_NCNN=ON`; without it everything else, including the classical detector, still builds and runs. The model's contract is: input blob `in0`, a 3 x 256 x 256 float image with RGB values 0 to 255 (the graph normalises it); outputs `out0` (four corners, x and y in 0 to 1, in the usual order), `out1` (heat-map peak per corner) and `out2` (four orientation logits).

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

## 4. Performance

The pipeline is measured with `lipi-cli bench` (per-stage latency) and `ml/tools/bench_pdf.py` (PDF size), see [ml/README.md](../ml/README.md). On a desktop CPU, one thread, a 2000 x 1500 photo takes 27 ms to detect and 217 ms to scan. On one phone, a Samsung SM-G781B (Snapdragon 865, a high-end device) running the same CLI built with the Android NDK, a 640 x 480 preview frame takes 30.6 ms to detect on one thread, 20.2 ms on two and 15.0 ms on four, and a 12 megapixel photo takes 0.49 s to scan on one thread ([report](../ml/reports/on-device-sm-g781b.json)).

Device tiers used for targets, which are goals and not yet measurements:

| Tier | Example hardware | Live preview detection | Capture to finished page (12 MP photo) |
|---|---|---|---|
| Low | 2019 budget phone, Cortex-A53/A55 cores, 3 GB RAM | at most 33 ms per frame (30 fps) | at most 3 s |
| Mid | Snapdragon 6 or 7 series, 4 to 6 GB RAM | at most 15 ms | at most 1.5 s |
| High | Snapdragon 8 series or equivalent, 8 GB RAM or more | at most 10 ms | at most 0.8 s |

What the one phone shows so far, against the high tier: capture to finished page is within its target (0.49 s against 0.8 s), but preview detection is not (15 ms with four threads, 30.6 ms with one, against 10 ms). It does clear 30 frames per second (33 ms) on a single thread, and running detection every other frame with the quad smoother filling the gaps is an option. Getting to 10 ms would take a smaller network. Low and mid tier devices have not been measured; the first Android build will run this timing inside the app on more devices and fill the table in.

---

## 5. The Android app

Kotlin and Jetpack Compose with Material 3, themed from the design ([docs/DESIGN.md](DESIGN.md)). The portable core is built by the app's own CMake file and reached through a thin JNI layer (`android/app/src/main/cpp/lipi_jni.cpp`); the core has no Android types.

**minSdk 26** (Android 8.0), decided for [#19](https://github.com/CommandLineCoding/lipi/issues/19): it keeps the app on the large majority of devices still in use and lets the adaptive icon and CameraX work without compatibility code, at the cost of handling storage through the system file picker instead of the simpler rules of newer versions. targetSdk and compileSdk are 36. The libraries are pinned to releases that compile against SDK 36, which is the highest version the Gradle plugin in use supports. The APK is built for `arm64-v8a` and, for the emulator, `x86_64`; native libraries are 16 KB page-aligned.

---

## 6. Distribution constraints

These shape the design, so they are worth knowing before you add a dependency:

- **F-Droid first.** No proprietary dependencies (no ML Kit, Firebase or Play Services), reproducible builds, and fastlane-style store metadata in `metadata/`.
- **MIT license.** No (A)GPL code can be bundled.
- **Native libraries.** `arm64-v8a` first, and every `.so` must be 16 KB page-aligned.
