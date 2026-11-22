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
  LIPI_ERR_INTERNAL = 4
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

#ifdef __cplusplus
}
#endif

#endif /* LIPI_LIPI_H */
