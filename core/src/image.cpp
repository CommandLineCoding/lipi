#include "image.hpp"

#include <cstring>
#include <limits>
#include <new>

namespace {

constexpr int kMaxDimension = 1 << 16;

bool valid_size(int width, int height) {
  return width > 0 && height > 0 && width <= kMaxDimension && height <= kMaxDimension;
}

}  // namespace

extern "C" {

int lipi_pixel_format_channels(lipi_pixel_format format) {
  switch (format) {
    case LIPI_PIXEL_GRAY8: return 1;
    case LIPI_PIXEL_RGB8: return 3;
    case LIPI_PIXEL_RGBA8: return 4;
  }
  return 0;
}

lipi_status lipi_image_create(int width, int height, lipi_pixel_format format, lipi_image** out) {
  if (out == nullptr) return LIPI_ERR_INVALID_ARGUMENT;
  *out = nullptr;
  const int channels = lipi_pixel_format_channels(format);
  if (channels == 0 || !valid_size(width, height)) return LIPI_ERR_INVALID_ARGUMENT;
  try {
    auto* image = new lipi_image();
    image->width = width;
    image->height = height;
    image->format = format;
    image->pixels.assign(static_cast<size_t>(width) * static_cast<size_t>(height) *
                             static_cast<size_t>(channels),
                         0);
    *out = image;
    return LIPI_OK;
  } catch (const std::bad_alloc&) {
    return LIPI_ERR_OUT_OF_MEMORY;
  } catch (...) {
    return LIPI_ERR_INTERNAL;
  }
}

lipi_status lipi_image_copy(const lipi_image_view* view, lipi_image** out) {
  if (out == nullptr) return LIPI_ERR_INVALID_ARGUMENT;
  *out = nullptr;
  if (view == nullptr || view->data == nullptr) return LIPI_ERR_INVALID_ARGUMENT;
  const int channels = lipi_pixel_format_channels(view->format);
  if (channels == 0 || !valid_size(view->width, view->height)) return LIPI_ERR_INVALID_ARGUMENT;
  if (view->stride < view->width * channels) return LIPI_ERR_INVALID_ARGUMENT;
  lipi_image* image = nullptr;
  const lipi_status status = lipi_image_create(view->width, view->height, view->format, &image);
  if (status != LIPI_OK) return status;
  const size_t row = static_cast<size_t>(view->width) * static_cast<size_t>(channels);
  for (int y = 0; y < view->height; ++y) {
    std::memcpy(image->pixels.data() + row * static_cast<size_t>(y),
                view->data + static_cast<size_t>(view->stride) * static_cast<size_t>(y), row);
  }
  *out = image;
  return LIPI_OK;
}

void lipi_image_free(lipi_image* image) { delete image; }

lipi_image_view lipi_image_view_of(const lipi_image* image) {
  lipi_image_view view{};
  if (image == nullptr) return view;
  const int channels = lipi_pixel_format_channels(image->format);
  view.data = image->pixels.data();
  view.width = image->width;
  view.height = image->height;
  view.stride = image->width * channels;
  view.format = image->format;
  return view;
}

uint8_t* lipi_image_data(lipi_image* image) {
  return image == nullptr ? nullptr : image->pixels.data();
}

}  // extern "C"
