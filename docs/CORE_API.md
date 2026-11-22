# Lipi core — C ABI

The contract between the portable C++ core and its callers (the desktop CLI, the Android JNI layer and, later, iOS). The core exposes a C ABI with no Android types.

> **Status:** under construction. The header is [core/include/lipi/lipi.h](../core/include/lipi/lipi.h); this page records the rules it follows and lists each area as it lands.

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
| Detector | `lipi_detect_page_classical` finds the page with edges, Hough lines and a quad search (`LIPI_ERR_NOT_FOUND` when no page is visible); model-based detection will sit beside it |
| Quad | `lipi_refine_quad` snaps corners onto the page edges at sub-pixel accuracy; smoothing across frames will follow |
| Quad and warp | `lipi_quad_validate`, `lipi_estimate_page_size` (aspect recovered from perspective, with a phone-camera focal-length fallback) and `lipi_warp_quad` (bilinear) |
| Enhance | Orientation, deskew, shadow removal, filters, brightness and contrast |
| PDF writer | `lipi_pdf_create`, `lipi_pdf_add_page_jpeg` (embeds the JPEG as it is), `lipi_pdf_add_page_image` (raw pixels) and `lipi_pdf_finish`, streaming to a write callback. Output has no timestamps, so identical input gives identical bytes. A text layer and compression come later |

## Conventions

- **Corner order.** A page quad is always `top-left, top-right, bottom-right, bottom-left` as seen on the image, in pixel coordinates with x to the right, y down and the origin at the top-left corner of the top-left pixel.
- **Pixel buffers.** `lipi_image_view` borrows caller memory (`data`, `width`, `height`, `stride` in bytes, `format`). `lipi_image` is owned by the core and freed with `lipi_image_free`. Supported formats are gray, RGB and RGBA, 8 bits per channel.
- **Status codes.** `LIPI_OK`, `LIPI_ERR_INVALID_ARGUMENT`, `LIPI_ERR_OUT_OF_MEMORY`, `LIPI_ERR_UNSUPPORTED`, `LIPI_ERR_INTERNAL`, `LIPI_ERR_NOT_FOUND` (nothing to find, an expected outcome rather than a fault) and `LIPI_ERR_IO` (a write callback failed). `lipi_status_string` describes them.
- **Outputs.** Functions that create an object write it through an `out` pointer, which is set to `NULL` on failure.
- **Decoding.** The core does not decode or encode image files; the CLI and the app do that.
