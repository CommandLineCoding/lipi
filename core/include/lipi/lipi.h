/*
 * Lipi core: public C API.
 *
 * Plain C, opaque handles, explicit ownership, status codes. No exceptions,
 * file paths or platform types cross this boundary. See docs/CORE_API.md.
 */
#ifndef LIPI_LIPI_H
#define LIPI_LIPI_H

#include <stddef.h>
#include <stdint.h>

#if defined(LIPI_SHARED) && defined(_WIN32)
#  ifdef LIPI_BUILDING_CORE
#    define LIPI_API __declspec(dllexport)
#  else
#    define LIPI_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define LIPI_API __attribute__((visibility("default")))
#else
#  define LIPI_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define LIPI_VERSION_MAJOR 0
#define LIPI_VERSION_MINOR 0
#define LIPI_VERSION_PATCH 1

typedef enum lipi_status {
  LIPI_OK = 0,
  LIPI_ERR_INVALID_ARGUMENT = 1,
  LIPI_ERR_OUT_OF_MEMORY = 2,
  LIPI_ERR_UNSUPPORTED = 3,
  LIPI_ERR_INTERNAL = 4,
  LIPI_ERR_NOT_FOUND = 5 /* no page could be found; an expected outcome, not a fault */
} lipi_status;

/* Returns a static, human-readable description of a status code. */
LIPI_API const char* lipi_status_string(lipi_status status);

/* Returns "major.minor.patch". The string is static. */
LIPI_API const char* lipi_version(void);

typedef enum lipi_pixel_format {
  LIPI_PIXEL_GRAY8 = 1, /* 1 byte per pixel */
  LIPI_PIXEL_RGB8 = 3,  /* R, G, B */
  LIPI_PIXEL_RGBA8 = 4  /* R, G, B, A (alpha is ignored by the pipeline) */
} lipi_pixel_format;

/* A borrowed, read-only view of pixels. The caller keeps ownership of data.
 * Rows are `stride` bytes apart; stride >= width * channels. */
typedef struct lipi_image_view {
  const uint8_t* data;
  int width;
  int height;
  int stride;
  lipi_pixel_format format;
} lipi_image_view;

/* An image owned by the core. Free it with lipi_image_free. */
typedef struct lipi_image lipi_image;

/* Allocates a zero-filled image. On failure *out is set to NULL. */
LIPI_API lipi_status lipi_image_create(int width, int height, lipi_pixel_format format,
                                       lipi_image** out);

/* Copies a view into a new owned image. */
LIPI_API lipi_status lipi_image_copy(const lipi_image_view* view, lipi_image** out);

/* Frees an image. Passing NULL is allowed. */
LIPI_API void lipi_image_free(lipi_image* image);

/* A borrowed view of an owned image, valid until the image is freed.
 * The data pointer is writable through lipi_image_data. */
LIPI_API lipi_image_view lipi_image_view_of(const lipi_image* image);
LIPI_API uint8_t* lipi_image_data(lipi_image* image);

/* Returns the number of channels for a pixel format, or 0 if it is invalid. */
LIPI_API int lipi_pixel_format_channels(lipi_pixel_format format);

/* Image coordinates: x to the right, y down, origin at the top-left corner of
 * the top-left pixel. */
typedef struct lipi_point {
  float x;
  float y;
} lipi_point;

/* The four corners of a page, always in this order as seen on the image:
 * top-left, top-right, bottom-right, bottom-left. */
typedef struct lipi_quad {
  lipi_point p[4];
} lipi_quad;

/* Returns LIPI_OK if the quad is a finite, convex, non-degenerate quadrilateral
 * in the corner order described above. */
LIPI_API lipi_status lipi_quad_validate(const lipi_quad* quad);

/* Estimates the size of the flat page that a quad in an image of the given size
 * shows, keeping the page's aspect ratio. The aspect is recovered from the
 * perspective of the quad (a pinhole camera with the principal point at the
 * image center is assumed); if that fails, the average side lengths are used.
 * The longer side is at most `max_side` pixels. */
LIPI_API lipi_status lipi_estimate_page_size(const lipi_quad* quad, int image_width,
                                             int image_height, int max_side, int* out_width,
                                             int* out_height);

/* Flattens the quad to an out_width x out_height image with bilinear
 * resampling. The output has the same pixel format as the source. */
LIPI_API lipi_status lipi_warp_quad(const lipi_image_view* src, const lipi_quad* quad,
                                    int out_width, int out_height, lipi_image** out);

/* Finds the page outline with classical image processing (edges, Hough lines,
 * quad search) and refines its corners to sub-pixel accuracy. On success
 * *out_quad is in the corner order described above, in src pixels, and
 * *out_confidence (optional, may be NULL) is between 0 and 1.
 * Returns LIPI_ERR_NOT_FOUND when no plausible page is visible. */
LIPI_API lipi_status lipi_detect_page_classical(const lipi_image_view* src, lipi_quad* out_quad,
                                                float* out_confidence);

/* Moves the corners of a rough quad onto the nearest page edges. The corners
 * keep their order. If an edge cannot be refined, its coarse position is kept. */
LIPI_API lipi_status lipi_refine_quad(const lipi_image_view* src, const lipi_quad* in_quad,
                                      lipi_quad* out_quad);

/* Temporal smoothing of a page quad across camera frames: a One-Euro filter on
 * the corners (steady when the page is still, quick when it moves) that
 * restarts when the page changes. Coordinates are filtered relative to the
 * image diagonal, so the settings do not depend on resolution. */
typedef struct lipi_smoother_options {
  float min_cutoff_hz;  /* cut-off when the page is still; lower is steadier (default 1.5) */
  float beta;           /* how quickly the cut-off rises with speed (default 25) */
  float reset_distance; /* corner jump, as a fraction of the diagonal, that starts a new track (default 0.12) */
  float hold_seconds;   /* how long to keep the last quad when detection drops out (default 0.4) */
} lipi_smoother_options;

typedef struct lipi_smoother lipi_smoother;

LIPI_API lipi_smoother_options lipi_smoother_defaults(void);
LIPI_API lipi_status lipi_smoother_create(int image_width, int image_height,
                                          const lipi_smoother_options* options, lipi_smoother** out);

/* Feeds one frame. Pass the detected quad, or NULL when nothing was found.
 * `timestamp_seconds` must not go backwards. On LIPI_OK, *out is the smoothed
 * quad. LIPI_ERR_NOT_FOUND means detection has been missing for longer than
 * hold_seconds (or never succeeded). */
LIPI_API lipi_status lipi_smoother_update(lipi_smoother* smoother, const lipi_quad* detected,
                                          double timestamp_seconds, lipi_quad* out);

/* How long the quad has barely moved, in seconds; 0 right after a move or a
 * reset. Useful for auto-capture. */
LIPI_API double lipi_smoother_steady_seconds(const lipi_smoother* smoother);

LIPI_API void lipi_smoother_reset(lipi_smoother* smoother);
LIPI_API void lipi_smoother_free(lipi_smoother* smoother);

#ifdef __cplusplus
}
#endif

#endif /* LIPI_LIPI_H */
