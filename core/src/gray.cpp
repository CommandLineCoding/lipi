#include "gray.hpp"

namespace lipi {

namespace {

float luminance(const lipi_image_view& src, int x, int y) {
  const int c = lipi_pixel_format_channels(src.format);
  const uint8_t* p = src.data + static_cast<size_t>(y) * static_cast<size_t>(src.stride) +
                     static_cast<size_t>(x) * static_cast<size_t>(c);
  if (c == 1) return static_cast<float>(p[0]);
  return 0.299f * static_cast<float>(p[0]) + 0.587f * static_cast<float>(p[1]) +
         0.114f * static_cast<float>(p[2]);
}

}  // namespace

Gray to_gray(const lipi_image_view& src, int max_side, float* scale) {
  const int longest = std::max(src.width, src.height);
  const float s = longest > max_side ? static_cast<float>(longest) / static_cast<float>(max_side) : 1.0f;
  Gray g;
  g.w = std::max(1, static_cast<int>(std::lround(static_cast<float>(src.width) / s)));
  g.h = std::max(1, static_cast<int>(std::lround(static_cast<float>(src.height) / s)));
  g.v.assign(static_cast<size_t>(g.w) * static_cast<size_t>(g.h), 0.0f);
  const float sx = static_cast<float>(src.width) / static_cast<float>(g.w);
  const float sy = static_cast<float>(src.height) / static_cast<float>(g.h);
  for (int y = 0; y < g.h; ++y) {
    const int y0 = static_cast<int>(static_cast<float>(y) * sy);
    const int y1 = std::min(src.height, std::max(y0 + 1, static_cast<int>(static_cast<float>(y + 1) * sy)));
    for (int x = 0; x < g.w; ++x) {
      const int x0 = static_cast<int>(static_cast<float>(x) * sx);
      const int x1 = std::min(src.width, std::max(x0 + 1, static_cast<int>(static_cast<float>(x + 1) * sx)));
      float sum = 0.0f;
      for (int yy = y0; yy < y1; ++yy)
        for (int xx = x0; xx < x1; ++xx) sum += luminance(src, xx, yy);
      g.v[static_cast<size_t>(y) * static_cast<size_t>(g.w) + static_cast<size_t>(x)] =
          sum / static_cast<float>((y1 - y0) * (x1 - x0));
    }
  }
  if (scale != nullptr) *scale = (sx + sy) / 2.0f;
  return g;
}

}  // namespace lipi
