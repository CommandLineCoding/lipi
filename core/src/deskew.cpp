// Skew estimation by projection profile, and rotation with bilinear resampling.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <new>
#include <vector>

#include "gray.hpp"
#include "image.hpp"
#include "lipi/lipi.h"

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kMaxSkew = 8.0;     // degrees searched on each side
constexpr int kAnalysisSide = 640;   // long side of the image the angle is estimated on
constexpr double kStep = 0.1;        // degrees

}  // namespace

extern "C" {

lipi_status lipi_estimate_skew(const lipi_image_view* src, float* degrees, float* confidence) {
  if (src == nullptr || src->data == nullptr || degrees == nullptr) return LIPI_ERR_INVALID_ARGUMENT;
  const int channels = lipi_pixel_format_channels(src->format);
  if (channels == 0 || src->width < 16 || src->height < 16 || src->stride < src->width * channels) return LIPI_ERR_INVALID_ARGUMENT;
  *degrees = 0.0f;
  if (confidence != nullptr) *confidence = 0.0f;
  try {
    float scale = 1.0f;
    const lipi::Gray g = lipi::to_gray(*src, kAnalysisSide, &scale);
    const int w = g.w, h = g.h;
    // Ink: clearly darker than the local average (a box average over a wide window).
    const int win = std::max(15, std::min(w, h) / 6) | 1;
    std::vector<double> integral(static_cast<size_t>(w + 1) * static_cast<size_t>(h + 1), 0.0);
    for (int y = 0; y < h; ++y) {
      double row = 0;
      for (int x = 0; x < w; ++x) {
        row += static_cast<double>(g.at(x, y));
        integral[static_cast<size_t>(y + 1) * static_cast<size_t>(w + 1) + static_cast<size_t>(x + 1)] =
            integral[static_cast<size_t>(y) * static_cast<size_t>(w + 1) + static_cast<size_t>(x + 1)] + row;
      }
    }
    std::vector<float> xs, ys;
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        const int x0 = std::max(0, x - win / 2), x1 = std::min(w, x + win / 2 + 1);
        const int y0 = std::max(0, y - win / 2), y1 = std::min(h, y + win / 2 + 1);
        const double sum = integral[static_cast<size_t>(y1) * static_cast<size_t>(w + 1) + static_cast<size_t>(x1)] -
                           integral[static_cast<size_t>(y0) * static_cast<size_t>(w + 1) + static_cast<size_t>(x1)] -
                           integral[static_cast<size_t>(y1) * static_cast<size_t>(w + 1) + static_cast<size_t>(x0)] +
                           integral[static_cast<size_t>(y0) * static_cast<size_t>(w + 1) + static_cast<size_t>(x0)];
        const double mean = sum / static_cast<double>((x1 - x0) * (y1 - y0));
        if (static_cast<double>(g.at(x, y)) < mean * 0.78) {
          xs.push_back(static_cast<float>(x) - static_cast<float>(w) / 2.0f);
          ys.push_back(static_cast<float>(y) - static_cast<float>(h) / 2.0f);
        }
      }
    // Too little ink to find lines (or a mostly dark picture): leave the page alone.
    const double ink_fraction = static_cast<double>(xs.size()) / (static_cast<double>(w) * h);
    if (xs.size() < 400 || ink_fraction > 0.35) return LIPI_OK;

    const int diag = static_cast<int>(std::ceil(std::hypot(static_cast<double>(w), static_cast<double>(h))));
    std::vector<double> scores;
    std::vector<double> angles;
    std::vector<int> hist(static_cast<size_t>(2 * diag + 3));
    for (double a = -kMaxSkew; a <= kMaxSkew + 1e-9; a += kStep) {
      const double rad = a * kPi / 180.0, c = std::cos(rad), s = std::sin(rad);
      std::fill(hist.begin(), hist.end(), 0);
      for (size_t i = 0; i < xs.size(); ++i) {
        // Row coordinate of the point after rotating the page clockwise by a degrees.
        const double r = static_cast<double>(ys[i]) * c + static_cast<double>(xs[i]) * s;
        ++hist[static_cast<size_t>(static_cast<int>(std::lround(r)) + diag + 1)];
      }
      double score = 0;
      for (const int v : hist) score += static_cast<double>(v) * v;
      scores.push_back(score);
      angles.push_back(a);
    }
    const size_t best = static_cast<size_t>(std::max_element(scores.begin(), scores.end()) - scores.begin());
    // Confidence: how much sharper the best angle is than the typical one.
    std::vector<double> sorted(scores);
    std::nth_element(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(sorted.size() / 2), sorted.end());
    const double median = sorted[sorted.size() / 2];
    const double ratio = scores[best] / std::max(1.0, median);
    const double conf = std::min(1.0, std::max(0.0, (ratio - 1.0) / 0.6));
    // A peak on the edge of the search range is not a real optimum.
    if (best == 0 || best + 1 == scores.size()) return LIPI_OK;
    // Parabolic refinement around the peak.
    const double y0 = scores[best - 1], y1 = scores[best], y2 = scores[best + 1];
    const double denom = y0 - 2.0 * y1 + y2;
    const double shift = std::fabs(denom) < 1e-9 ? 0.0 : 0.5 * (y0 - y2) / denom;
    *degrees = static_cast<float>(angles[best] + shift * kStep);
    if (confidence != nullptr) *confidence = static_cast<float>(conf);
    return LIPI_OK;
  } catch (const std::bad_alloc&) {
    return LIPI_ERR_OUT_OF_MEMORY;
  } catch (...) {
    return LIPI_ERR_INTERNAL;
  }
}

lipi_status lipi_rotate(const lipi_image_view* src, float degrees, lipi_image** out) {
  if (out == nullptr) return LIPI_ERR_INVALID_ARGUMENT;
  *out = nullptr;
  if (src == nullptr || src->data == nullptr || !std::isfinite(degrees)) return LIPI_ERR_INVALID_ARGUMENT;
  const int channels = lipi_pixel_format_channels(src->format);
  if (channels == 0 || src->width <= 0 || src->height <= 0 || src->stride < src->width * channels) return LIPI_ERR_INVALID_ARGUMENT;
  lipi_image* image = nullptr;
  lipi_status st = lipi_image_create(src->width, src->height, src->format, &image);
  if (st != LIPI_OK) return st;
  const int w = src->width, h = src->height;
  // Fill colour: the median-ish colour of the image border, so rotated corners match the paper.
  std::vector<uint8_t> fill(static_cast<size_t>(channels), 255);
  for (int c = 0; c < channels; ++c) {
    std::vector<uint8_t> border;
    for (int x = 0; x < w; x += std::max(1, w / 64)) {
      border.push_back(src->data[static_cast<size_t>(x * channels + c)]);
      border.push_back(src->data[static_cast<size_t>(h - 1) * static_cast<size_t>(src->stride) + static_cast<size_t>(x * channels + c)]);
    }
    for (int y = 0; y < h; y += std::max(1, h / 64)) {
      border.push_back(src->data[static_cast<size_t>(y) * static_cast<size_t>(src->stride) + static_cast<size_t>(c)]);
      border.push_back(src->data[static_cast<size_t>(y) * static_cast<size_t>(src->stride) + static_cast<size_t>((w - 1) * channels + c)]);
    }
    std::nth_element(border.begin(), border.begin() + static_cast<std::ptrdiff_t>(border.size() * 3 / 4), border.end());
    fill[static_cast<size_t>(c)] = border[border.size() * 3 / 4];  // paper is usually the bright majority
  }
  const double rad = static_cast<double>(degrees) * kPi / 180.0;
  const double c = std::cos(rad), s = std::sin(rad);
  const double cx = w / 2.0, cy = h / 2.0;
  uint8_t* dst = image->pixels.data();
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      // Inverse map: where in the source does this output pixel come from (clockwise rotation by `degrees`).
      const double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
      const double sx = c * dx + s * dy + cx - 0.5;
      const double sy = -s * dx + c * dy + cy - 0.5;
      const int x0 = static_cast<int>(std::floor(sx)), y0 = static_cast<int>(std::floor(sy));
      const double tx = sx - x0, ty = sy - y0;
      for (int ch = 0; ch < channels; ++ch) {
        auto at = [&](int xx, int yy) -> double {
          if (xx < 0 || yy < 0 || xx >= w || yy >= h) return static_cast<double>(fill[static_cast<size_t>(ch)]);
          return static_cast<double>(src->data[static_cast<size_t>(yy) * static_cast<size_t>(src->stride) + static_cast<size_t>(xx * channels + ch)]);
        };
        const double v = (at(x0, y0) * (1 - tx) + at(x0 + 1, y0) * tx) * (1 - ty) + (at(x0, y0 + 1) * (1 - tx) + at(x0 + 1, y0 + 1) * tx) * ty;
        dst[(static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * static_cast<size_t>(channels) + static_cast<size_t>(ch)] =
            static_cast<uint8_t>(std::lround(std::min(255.0, std::max(0.0, v))));
      }
    }
  *out = image;
  return LIPI_OK;
}

}  // extern "C"
