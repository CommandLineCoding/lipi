#include <algorithm>
#include <cmath>
#include <cstddef>

#include "geometry.hpp"
#include "image.hpp"
#include "lipi/lipi.h"

extern "C" lipi_status lipi_warp_quad(const lipi_image_view* src, const lipi_quad* quad,
                                      int out_width, int out_height, lipi_image** out) {
  if (out == nullptr) return LIPI_ERR_INVALID_ARGUMENT;
  *out = nullptr;
  if (src == nullptr || src->data == nullptr || quad == nullptr) return LIPI_ERR_INVALID_ARGUMENT;
  const int channels = lipi_pixel_format_channels(src->format);
  if (channels == 0 || src->width <= 0 || src->height <= 0 ||
      src->stride < src->width * channels) {
    return LIPI_ERR_INVALID_ARGUMENT;
  }
  lipi_status status = lipi_quad_validate(quad);
  if (status != LIPI_OK) return status;
  lipi::Homography H{};
  if (!lipi::unit_square_to_quad(*quad, &H)) return LIPI_ERR_INVALID_ARGUMENT;

  lipi_image* image = nullptr;
  status = lipi_image_create(out_width, out_height, src->format, &image);
  if (status != LIPI_OK) return status;

  const int sw = src->width, sh = src->height;
  const auto at = [&](int x, int y, int c) {
    x = std::min(std::max(x, 0), sw - 1);
    y = std::min(std::max(y, 0), sh - 1);
    return static_cast<double>(src->data[static_cast<size_t>(y) * static_cast<size_t>(src->stride) +
                                         static_cast<size_t>(x * channels + c)]);
  };

  uint8_t* dst = image->pixels.data();
  for (int y = 0; y < out_height; ++y) {
    const double v = (y + 0.5) / out_height;
    for (int x = 0; x < out_width; ++x) {
      const double u = (x + 0.5) / out_width;
      double sx = 0.0, sy = 0.0;
      H.map(u, v, &sx, &sy);
      // Sample positions refer to pixel centers at integer + 0.5.
      const double fx = sx - 0.5, fy = sy - 0.5;
      const int x0 = static_cast<int>(std::floor(fx)), y0 = static_cast<int>(std::floor(fy));
      const double tx = fx - x0, ty = fy - y0;
      for (int c = 0; c < channels; ++c) {
        const double top = at(x0, y0, c) * (1.0 - tx) + at(x0 + 1, y0, c) * tx;
        const double bottom = at(x0, y0 + 1, c) * (1.0 - tx) + at(x0 + 1, y0 + 1, c) * tx;
        const double value = top * (1.0 - ty) + bottom * ty;
        dst[(static_cast<size_t>(y) * static_cast<size_t>(out_width) + static_cast<size_t>(x)) *
                static_cast<size_t>(channels) +
            static_cast<size_t>(c)] = static_cast<uint8_t>(std::lround(std::min(255.0, std::max(0.0, value))));
      }
    }
  }
  *out = image;
  return LIPI_OK;
}
