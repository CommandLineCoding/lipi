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
  LIPI_ERR_NOT_FOUND = 5, /* no page could be found; an expected outcome, not a fault */
  LIPI_ERR_IO = 6         /* a write callback reported failure */
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

/* Learned page detector. The model is a small network (see docs/ARCHITECTURE.md)
 * that finds the page corners and the page orientation in one pass. The core is
 * built with an inference backend (ncnn) only when LIPI_WITH_NCNN is set;
 * without it, lipi_detector_available() returns 0 and creating a detector fails
 * with LIPI_ERR_UNSUPPORTED. */
typedef struct lipi_detection {
  lipi_quad quad;   /* page corners in source pixels, in the usual order */
  float confidence; /* lowest corner heat-map peak, 0 to 1; low values mean "unsure" */
  int orientation;  /* 0-3: 90 degree clockwise turns that make the flattened page upright */
} lipi_detection;

typedef struct lipi_detector lipi_detector;

LIPI_API int lipi_detector_available(void);

/* Creates a detector from an ncnn model (a text .param and a .bin). The buffers
 * are copied. num_threads <= 0 picks a default. */
LIPI_API lipi_status lipi_detector_create(const void* param, size_t param_size, const void* weights,
                                          size_t weights_size, int num_threads, lipi_detector** out);

/* Runs the model on a whole image. The image is area-averaged to the model's
 * input size, so any resolution works. Returns LIPI_ERR_NOT_FOUND if the
 * network's corners do not form a valid quad. */
LIPI_API lipi_status lipi_detector_detect(lipi_detector* detector, const lipi_image_view* src,
                                          lipi_detection* out);

LIPI_API void lipi_detector_free(lipi_detector* detector);

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

/* PDF output. Pages are streamed to a callback, so the core never touches files.
 * The callback returns the number of bytes it accepted; anything less than
 * `size` is treated as a failure. Output is deterministic: it carries no
 * timestamps, so identical input gives identical bytes. */
typedef size_t (*lipi_write_fn)(const void* data, size_t size, void* user);

typedef struct lipi_pdf lipi_pdf;

LIPI_API lipi_status lipi_pdf_create(lipi_write_fn write, void* user, lipi_pdf** out);

/* Adds a page showing a baseline or progressive JPEG (gray or RGB) as it is,
 * without recompressing. The page is sized from the image and `dpi`. */
LIPI_API lipi_status lipi_pdf_add_page_jpeg(lipi_pdf* pdf, const uint8_t* jpeg, size_t size,
                                            float dpi);

/* Adds a page from raw pixels (stored without lossy compression). */
LIPI_API lipi_status lipi_pdf_add_page_image(lipi_pdf* pdf, const lipi_image_view* image,
                                             float dpi);

/* Writes the page tree and cross-reference table. Call once, after the last
 * page; at least one page is required. */
LIPI_API lipi_status lipi_pdf_finish(lipi_pdf* pdf);

/* Frees the writer. Passing NULL is allowed. */
LIPI_API void lipi_pdf_free(lipi_pdf* pdf);

typedef enum lipi_filter {
  LIPI_FILTER_NONE = 0,  /* leave colours alone; only shadow removal and adjustments apply */
  LIPI_FILTER_AUTO = 1,  /* clean document look: gray if the page has no colour content, else colour */
  LIPI_FILTER_BW = 2,    /* pure black and white (gray output with values 0 and 255) */
  LIPI_FILTER_GRAY = 3,  /* grayscale */
  LIPI_FILTER_COLOR = 4  /* colour with an even white background */
} lipi_filter;

typedef struct lipi_enhance_options {
  int filter;          /* a lipi_filter value; anything else is rejected */
  int remove_shadows;  /* non-zero: even out uneven lighting and shadows first */
  float brightness;    /* -1 (darker) to 1 (brighter); 0 leaves it unchanged */
  float contrast;      /* -1 (flatter) to 1 (stronger); 0 leaves it unchanged */
} lipi_enhance_options;

/* Auto filter with shadow removal and no brightness or contrast change. */
LIPI_API lipi_enhance_options lipi_enhance_defaults(void);

/* Cleans up a flattened page. Gray and BW produce a gray image; the other
 * filters keep the source's colour, except Auto, which returns gray when the
 * page has no colour content. Check the format of the result. */
LIPI_API lipi_status lipi_enhance(const lipi_image_view* src, const lipi_enhance_options* options,
                                  lipi_image** out);

/* Estimates how far the lines of text on a flattened page are tilted, by
 * looking for the rotation that makes the rows of ink sharpest. *degrees is
 * the amount to rotate the page clockwise to straighten it (within +-8
 * degrees); *confidence (optional) is 0 to 1. Pages without enough text
 * return 0 degrees and 0 confidence. */
LIPI_API lipi_status lipi_estimate_skew(const lipi_image_view* src, float* degrees, float* confidence);

/* Rotates an image clockwise by `degrees` about its centre, keeping its size
 * and filling the corners with the page's own background colour. */
LIPI_API lipi_status lipi_rotate(const lipi_image_view* src, float degrees, lipi_image** out);

#ifdef __cplusplus
}
#endif

#endif /* LIPI_LIPI_H */
