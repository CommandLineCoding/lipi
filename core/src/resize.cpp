#include "resize.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace lipi {

namespace {

struct Taps {
  std::vector<int> first;
  std::vector<std::vector<float>> weights;
};

// For each output index, the source pixels it covers and their fractional weights.
Taps make_taps(int in, int out) {
  Taps t;
  t.first.resize(static_cast<size_t>(out));
  t.weights.resize(static_cast<size_t>(out));
  const double scale = static_cast<double>(in) / out;
  for (int i = 0; i < out; ++i) {
    const double a = i * scale, b = (i + 1) * scale;
    const int k0 = static_cast<int>(std::floor(a));
    const int k1 = std::min(in, static_cast<int>(std::ceil(b)));
    t.first[static_cast<size_t>(i)] = k0;
    for (int k = k0; k < k1; ++k) {
      const double w = std::min(b, static_cast<double>(k + 1)) - std::max(a, static_cast<double>(k));
      t.weights[static_cast<size_t>(i)].push_back(static_cast<float>(w / scale));
    }
  }
  return t;
}

}  // namespace

std::vector<float> resize_area_planar_rgb(const lipi_image_view& src, int out_w, int out_h) {
  const int channels = lipi_pixel_format_channels(src.format);
  const Taps tx = make_taps(src.width, out_w), ty = make_taps(src.height, out_h);
  // Horizontal pass: height x out_w x 3.
  std::vector<float> tmp(static_cast<size_t>(src.height) * static_cast<size_t>(out_w) * 3, 0.0f);
  for (int y = 0; y < src.height; ++y) {
    const uint8_t* row = src.data + static_cast<size_t>(y) * static_cast<size_t>(src.stride);
    for (int x = 0; x < out_w; ++x) {
      const std::vector<float>& w = tx.weights[static_cast<size_t>(x)];
      const int k0 = tx.first[static_cast<size_t>(x)];
      float acc[3] = {0, 0, 0};
      for (size_t j = 0; j < w.size(); ++j) {
        const uint8_t* p = row + static_cast<size_t>(k0 + static_cast<int>(j)) * static_cast<size_t>(channels);
        if (channels == 1) {
          const float v = static_cast<float>(p[0]) * w[j];
          acc[0] += v; acc[1] += v; acc[2] += v;
        } else {
          acc[0] += static_cast<float>(p[0]) * w[j];
          acc[1] += static_cast<float>(p[1]) * w[j];
          acc[2] += static_cast<float>(p[2]) * w[j];
        }
      }
      float* o = &tmp[(static_cast<size_t>(y) * static_cast<size_t>(out_w) + static_cast<size_t>(x)) * 3];
      o[0] = acc[0]; o[1] = acc[1]; o[2] = acc[2];
    }
  }
  // Vertical pass into planar output.
  const size_t plane = static_cast<size_t>(out_w) * static_cast<size_t>(out_h);
  std::vector<float> out(plane * 3, 0.0f);
  for (int y = 0; y < out_h; ++y) {
    const std::vector<float>& w = ty.weights[static_cast<size_t>(y)];
    const int k0 = ty.first[static_cast<size_t>(y)];
    for (int x = 0; x < out_w; ++x) {
      float acc[3] = {0, 0, 0};
      for (size_t j = 0; j < w.size(); ++j) {
        const float* p = &tmp[(static_cast<size_t>(k0 + static_cast<int>(j)) * static_cast<size_t>(out_w) + static_cast<size_t>(x)) * 3];
        acc[0] += p[0] * w[j];
        acc[1] += p[1] * w[j];
        acc[2] += p[2] * w[j];
      }
      const size_t i = static_cast<size_t>(y) * static_cast<size_t>(out_w) + static_cast<size_t>(x);
      out[i] = acc[0];
      out[plane + i] = acc[1];
      out[2 * plane + i] = acc[2];
    }
  }
  return out;
}

}  // namespace lipi
