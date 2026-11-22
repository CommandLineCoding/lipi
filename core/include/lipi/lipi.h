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

#ifdef __cplusplus
}
#endif

#endif /* LIPI_LIPI_H */
