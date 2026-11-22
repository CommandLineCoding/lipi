#ifndef LIPI_SRC_GRAY_HPP
#define LIPI_SRC_GRAY_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "lipi/lipi.h"

namespace lipi {

struct Gray {
  int w = 0, h = 0;
  std::vector<float> v;

  float at(int x, int y) const {
    x = std::min(std::max(x, 0), w - 1);
    y = std::min(std::max(y, 0), h - 1);
    return v[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
  }
  // Bilinear sample; pixel centers are at integer + 0.5 in image coordinates.
  float sample(float x, float y) const {
    const float fx = x - 0.5f, fy = y - 0.5f;
    const int x0 = static_cast<int>(std::floor(fx)), y0 = static_cast<int>(std::floor(fy));
    const float tx = fx - static_cast<float>(x0), ty = fy - static_cast<float>(y0);
    return (at(x0, y0) * (1 - tx) + at(x0 + 1, y0) * tx) * (1 - ty) +
           (at(x0, y0 + 1) * (1 - tx) + at(x0 + 1, y0 + 1) * tx) * ty;
  }
};

// Luminance of the view, box-downscaled so that the long side is at most
// max_side. *scale receives source pixels per output pixel (>= 1).
Gray to_gray(const lipi_image_view& src, int max_side, float* scale);

}  // namespace lipi

#endif
