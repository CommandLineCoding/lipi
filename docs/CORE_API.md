# Lipi core — C ABI

The contract between the portable C++ core and its callers (the desktop CLI, the Android JNI layer and, later, iOS). The core exposes a C ABI with no Android types.

> **Status:** the API is not implemented yet. This page records the rules the API must follow and will list every function as it lands ([#4](https://github.com/CommandLineCoding/lipi/issues/4)).

## Rules

- **C linkage only.** Public headers are plain C, so any language can bind them.
- **Opaque handles.** Callers get pointers to opaque structs and never look inside them.
- **Explicit ownership.** Every function that returns an allocated object has a matching `*_free`. Buffers passed in stay owned by the caller.
- **Error codes, not exceptions.** Functions return a status code; C++ exceptions never cross the boundary.
- **No platform types.** No JNI, Android or file-system handles in the interface; images go in and out as pixel buffers.
- **Stable.** Add functions rather than changing existing signatures.

## Planned surface

| Area | Purpose |
|---|---|
| Image buffer | Describe a pixel buffer (width, height, stride, format) |
| Detector | Load a model, find four page corners with a confidence |
| Quad | Refine corners and smooth them across frames |
| Warp | Flatten a quad to a page rectangle |
| Enhance | Orientation, deskew, shadow removal, filters, brightness and contrast |
| PDF writer | Build a multi-page PDF from page images (and, later, a text layer) |
