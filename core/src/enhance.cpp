// Page enhancement: shadow removal, filters, brightness and contrast.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <new>
#include <vector>

#include "image.hpp"
#include "lipi/lipi.h"

namespace {

constexpr int kBackgroundSide = 160;  // long side of the image used to estimate the lighting

struct Plane {
  int w = 0, h = 0;
  std::vector<float> v;
  float at(int x, int y) const {
    x = std::min(std::max(x, 0), w - 1);
    y = std::min(std::max(y, 0), h - 1);
    return v[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
  }
  float sample(float x, float y) const {  // bilinear, pixel centres at +0.5
    const float fx = x - 0.5f, fy = y - 0.5f;
    const int x0 = static_cast<int>(std::floor(fx)), y0 = static_cast<int>(std::floor(fy));
    const float tx = fx - static_cast<float>(x0), ty = fy - static_cast<float>(y0);
    return (at(x0, y0) * (1 - tx) + at(x0 + 1, y0) * tx) * (1 - ty) +
           (at(x0, y0 + 1) * (1 - tx) + at(x0 + 1, y0 + 1) * tx) * ty;
  }
};

Plane channel_box_downscale(const lipi_image_view& src, int c, int channels, int out_w, int out_h) {
  Plane p;
  p.w = out_w;
  p.h = out_h;
  p.v.assign(static_cast<size_t>(out_w) * static_cast<size_t>(out_h), 0.0f);
  for (int y = 0; y < out_h; ++y) {
    const int y0 = static_cast<int>(static_cast<int64_t>(y) * src.height / out_h);
    const int y1 = std::max(y0 + 1, static_cast<int>(static_cast<int64_t>(y + 1) * src.height / out_h));
    for (int x = 0; x < out_w; ++x) {
      const int x0 = static_cast<int>(static_cast<int64_t>(x) * src.width / out_w);
      const int x1 = std::max(x0 + 1, static_cast<int>(static_cast<int64_t>(x + 1) * src.width / out_w));
      float sum = 0;
      for (int yy = y0; yy < y1; ++yy)
        for (int xx = x0; xx < x1; ++xx)
          sum += src.data[static_cast<size_t>(yy) * static_cast<size_t>(src.stride) + static_cast<size_t>(xx * channels + c)];
      p.v[static_cast<size_t>(y) * static_cast<size_t>(out_w) + static_cast<size_t>(x)] =
          sum / static_cast<float>((y1 - y0) * (x1 - x0));
    }
  }
  return p;
}

// Running maximum with a (2r+1) window along one axis.
void max_filter(Plane* p, int radius, bool horizontal) {
  Plane out = *p;
  const int n = horizontal ? p->w : p->h, m = horizontal ? p->h : p->w;
  for (int j = 0; j < m; ++j)
    for (int i = 0; i < n; ++i) {
      float best = 0;
      for (int k = std::max(0, i - radius); k <= std::min(n - 1, i + radius); ++k)
        best = std::max(best, horizontal ? p->at(k, j) : p->at(j, k));
      out.v[static_cast<size_t>(horizontal ? j : i) * static_cast<size_t>(p->w) + static_cast<size_t>(horizontal ? i : j)] = best;
    }
  *p = out;
}

void box_blur(Plane* p, int radius, bool horizontal) {
  Plane out = *p;
  const int n = horizontal ? p->w : p->h, m = horizontal ? p->h : p->w;
  for (int j = 0; j < m; ++j)
    for (int i = 0; i < n; ++i) {
      float sum = 0;
      int cnt = 0;
      for (int k = std::max(0, i - radius); k <= std::min(n - 1, i + radius); ++k, ++cnt)
        sum += horizontal ? p->at(k, j) : p->at(j, k);
      out.v[static_cast<size_t>(horizontal ? j : i) * static_cast<size_t>(p->w) + static_cast<size_t>(horizontal ? i : j)] =
          sum / static_cast<float>(cnt);
    }
  *p = out;
}

// Estimated lighting of the page (what a blank sheet would look like) for one channel.
Plane background(const lipi_image_view& src, int c, int channels) {
  const int longest = std::max(src.width, src.height);
  const double s = longest > kBackgroundSide ? static_cast<double>(kBackgroundSide) / longest : 1.0;
  const int w = std::max(1, static_cast<int>(std::lround(src.width * s)));
  const int h = std::max(1, static_cast<int>(std::lround(src.height * s)));
  Plane p = channel_box_downscale(src, c, channels, w, h);
  max_filter(&p, 4, true);  // removes dark text and strokes
  max_filter(&p, 4, false);
  for (int i = 0; i < 2; ++i) {  // smooth the result
    box_blur(&p, 6, true);
    box_blur(&p, 6, false);
  }
  return p;
}

uint8_t clamp8(float v) { return static_cast<uint8_t>(std::lround(std::min(255.0f, std::max(0.0f, v)))); }

int percentile(const std::array<int, 256>& hist, int total, double frac) {
  const double target = frac * total;
  double acc = 0;
  for (int i = 0; i < 256; ++i) {
    acc += hist[static_cast<size_t>(i)];
    if (acc >= target) return i;
  }
  return 255;
}

}  // namespace

extern "C" {

lipi_enhance_options lipi_enhance_defaults(void) {
  lipi_enhance_options o{};
  o.filter = LIPI_FILTER_AUTO;
  o.remove_shadows = 1;
  o.brightness = 0.0f;
  o.contrast = 0.0f;
  return o;
}

lipi_status lipi_enhance(const lipi_image_view* src, const lipi_enhance_options* options, lipi_image** out) {
  if (out == nullptr) return LIPI_ERR_INVALID_ARGUMENT;
  *out = nullptr;
  if (src == nullptr || src->data == nullptr || options == nullptr) return LIPI_ERR_INVALID_ARGUMENT;
  const int channels = lipi_pixel_format_channels(src->format);
  if (channels == 0 || src->width <= 0 || src->height <= 0 || src->stride < src->width * channels) return LIPI_ERR_INVALID_ARGUMENT;
  if (options->filter < LIPI_FILTER_NONE || options->filter > LIPI_FILTER_COLOR) return LIPI_ERR_INVALID_ARGUMENT;
  if (!(options->brightness >= -1.0f && options->brightness <= 1.0f) || !(options->contrast >= -1.0f && options->contrast <= 1.0f)) {
    return LIPI_ERR_INVALID_ARGUMENT;
  }
  try {
    const int w = src->width, h = src->height;
    const int colour_channels = channels == 1 ? 1 : 3;
    const size_t npix = static_cast<size_t>(w) * static_cast<size_t>(h);

    // 1. Even out the lighting: divide each channel by its estimated background.
    std::vector<float> norm(npix * static_cast<size_t>(colour_channels));
    std::vector<Plane> bg;
    if (options->remove_shadows) {
      for (int c = 0; c < colour_channels; ++c) bg.push_back(background(*src, c, channels));
    }
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x)
        for (int c = 0; c < colour_channels; ++c) {
          const float v = static_cast<float>(src->data[static_cast<size_t>(y) * static_cast<size_t>(src->stride) + static_cast<size_t>(x * channels + c)]);
          float value = v;
          if (options->remove_shadows) {
            const float b = std::max(40.0f, bg[static_cast<size_t>(c)].sample((static_cast<float>(x) + 0.5f) * static_cast<float>(bg[static_cast<size_t>(c)].w) / static_cast<float>(w),
                                                                              (static_cast<float>(y) + 0.5f) * static_cast<float>(bg[static_cast<size_t>(c)].h) / static_cast<float>(h)));
            value = std::min(255.0f, v / b * 255.0f);
          }
          norm[(static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * static_cast<size_t>(colour_channels) + static_cast<size_t>(c)] = value;
        }

    // 2. Luminance statistics of the evened-out page.
    auto lum_at = [&](size_t i) {
      if (colour_channels == 1) return norm[i];
      const float* p = &norm[i * 3];
      return 0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2];
    };
    std::array<int, 256> hist{};
    double chroma = 0;
    for (size_t i = 0; i < npix; ++i) {
      ++hist[static_cast<size_t>(clamp8(lum_at(i)))];
      if (colour_channels == 3) {
        const float* p = &norm[i * 3];
        chroma += std::max({p[0], p[1], p[2]}) - std::min({p[0], p[1], p[2]});
      }
    }
    const double mean_chroma = colour_channels == 3 ? chroma / static_cast<double>(npix) / 255.0 : 0.0;

    const int filter = options->filter;
    const bool gray_out = filter == LIPI_FILTER_GRAY || filter == LIPI_FILTER_BW ||
                          (filter == LIPI_FILTER_AUTO && mean_chroma < 0.05) || colour_channels == 1;
    const int out_channels = gray_out ? 1 : 3;

    // 3. Levels: push the paper to white and the ink darker.
    float black = 0.0f;
    if (filter == LIPI_FILTER_AUTO || filter == LIPI_FILTER_COLOR || filter == LIPI_FILTER_GRAY || filter == LIPI_FILTER_BW) {
      black = std::min(110.0f, static_cast<float>(percentile(hist, static_cast<int>(npix), 0.01)));
    }
    const float white = filter == LIPI_FILTER_NONE ? 255.0f : std::max(200.0f, static_cast<float>(percentile(hist, static_cast<int>(npix), 0.90)));
    const float scale = 255.0f / std::max(1.0f, white - black);
    auto stretched = [&](size_t i) { return static_cast<double>((lum_at(i) - black) * scale); };

    // Black and white: a pixel is ink if it is clearly darker than its neighbourhood (Bradley and Roth),
    // judged after the levels stretch so that faint print still stands out.
    std::vector<uint8_t> bw;
    if (filter == LIPI_FILTER_BW) {
      const int win = std::max(15, std::min(w, h) / 8) | 1;
      std::vector<double> integral(static_cast<size_t>(w + 1) * static_cast<size_t>(h + 1), 0.0);
      for (int y = 0; y < h; ++y) {
        double row = 0;
        for (int x = 0; x < w; ++x) {
          row += stretched(static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x));
          integral[static_cast<size_t>(y + 1) * static_cast<size_t>(w + 1) + static_cast<size_t>(x + 1)] =
              integral[static_cast<size_t>(y) * static_cast<size_t>(w + 1) + static_cast<size_t>(x + 1)] + row;
        }
      }
      bw.assign(npix, 255);
      for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
          const int x0 = std::max(0, x - win / 2), x1 = std::min(w, x + win / 2 + 1);
          const int y0 = std::max(0, y - win / 2), y1 = std::min(h, y + win / 2 + 1);
          const double sum = integral[static_cast<size_t>(y1) * static_cast<size_t>(w + 1) + static_cast<size_t>(x1)] -
                             integral[static_cast<size_t>(y0) * static_cast<size_t>(w + 1) + static_cast<size_t>(x1)] -
                             integral[static_cast<size_t>(y1) * static_cast<size_t>(w + 1) + static_cast<size_t>(x0)] +
                             integral[static_cast<size_t>(y0) * static_cast<size_t>(w + 1) + static_cast<size_t>(x0)];
          const double mean = sum / static_cast<double>((x1 - x0) * (y1 - y0));
          const double v = stretched(static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x));
          if (v <= mean * 0.85 && v < 235.0) bw[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = 0;
        }
    }

    // 4. Brightness and contrast on top.
    const float cfactor = std::tan((options->contrast + 1.0f) * 0.78539816f);
    const float boffset = options->brightness * 96.0f;
    auto adjust = [&](float v) { return (v - 128.0f) * cfactor + 128.0f + boffset; };

    lipi_image* image = nullptr;
    const lipi_status st = lipi_image_create(w, h, out_channels == 1 ? LIPI_PIXEL_GRAY8 : LIPI_PIXEL_RGB8, &image);
    if (st != LIPI_OK) return st;
    uint8_t* dst = image->pixels.data();
    for (size_t i = 0; i < npix; ++i) {
      if (out_channels == 1) {
        float l = (lum_at(i) - black) * scale;
        if (filter == LIPI_FILTER_BW) {
          dst[i] = bw[i];
        } else {
          dst[i] = clamp8(adjust(l));
        }
      } else {
        for (int c = 0; c < 3; ++c) {
          const float v = (norm[i * 3 + static_cast<size_t>(c)] - black) * scale;
          dst[i * 3 + static_cast<size_t>(c)] = clamp8(adjust(v));
        }
      }
    }
    *out = image;
    return LIPI_OK;
  } catch (const std::bad_alloc&) {
    return LIPI_ERR_OUT_OF_MEMORY;
  } catch (...) {
    return LIPI_ERR_INTERNAL;
  }
}

}  // extern "C"
