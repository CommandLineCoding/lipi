#include <cmath>
#include <vector>

#include "doctest.h"
#include "lipi/lipi.h"

namespace {

constexpr int W = 240, H = 300;

struct Page {
  std::vector<uint8_t> rgb;
  std::vector<uint8_t> stroke;  // 1 where the page has ink
  lipi_image_view view() const { return {rgb.data(), W, H, W * 3, LIPI_PIXEL_RGB8}; }
};

// A paper page with text-like strokes under uneven lighting and a soft shadow.
Page make_page(bool colour_stripe, bool shading = true) {
  Page p;
  p.rgb.resize(static_cast<size_t>(W * H * 3));
  p.stroke.assign(static_cast<size_t>(W * H), 0);
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x) {
      float r = 236, g = 234, b = 226;  // paper
      const bool ink = (y % 18) < 4 && y > 30 && y < H - 30 && x > 24 && x < W - 24 && (x % 50) < 38;
      if (ink) {
        r = g = b = 40;
        p.stroke[static_cast<size_t>(y * W + x)] = 1;
      }
      if (colour_stripe && y > 6 && y < 22 && x > 24 && x < W - 24) {
        r = 200; g = 40; b = 40;
        p.stroke[static_cast<size_t>(y * W + x)] = 1;
      }
      if (shading) {
        const float gradient = 1.0f - 0.5f * static_cast<float>(x) / W;  // lighting falls off to the right
        const float dx = static_cast<float>(x - 60), dy = static_cast<float>(y - 220);
        const float shadow = 1.0f - 0.45f * std::exp(-(dx * dx + dy * dy) / (2 * 40.0f * 40.0f));
        const float f = gradient * shadow;
        r *= f; g *= f; b *= f;
      }
      uint8_t* px = &p.rgb[static_cast<size_t>((y * W + x) * 3)];
      px[0] = static_cast<uint8_t>(r);
      px[1] = static_cast<uint8_t>(g);
      px[2] = static_cast<uint8_t>(b);
    }
  return p;
}

struct Stats {
  double mean, stddev;
};

// Statistics of the first channel over pixels with or without ink.
Stats region_stats(const lipi_image_view& v, const std::vector<uint8_t>& stroke, bool ink) {
  const int ch = lipi_pixel_format_channels(v.format);
  double sum = 0, sq = 0;
  long n = 0;
  for (int y = 0; y < v.height; ++y)
    for (int x = 0; x < v.width; ++x) {
      if ((stroke[static_cast<size_t>(y * v.width + x)] != 0) != ink) continue;
      // keep clear of the edges of the ink so that blur and resampling do not matter
      const double val = v.data[static_cast<size_t>(y * v.stride + x * ch)];
      sum += val;
      sq += val * val;
      ++n;
    }
  const double mean = sum / static_cast<double>(n);
  return {mean, std::sqrt(std::max(0.0, sq / static_cast<double>(n) - mean * mean))};
}

lipi_image* run(const Page& page, int f, bool shadows = true, float brightness = 0, float contrast = 0) {
  lipi_enhance_options o = lipi_enhance_defaults();
  o.filter = f;
  o.remove_shadows = shadows;
  o.brightness = brightness;
  o.contrast = contrast;
  const lipi_image_view v = page.view();
  lipi_image* out = nullptr;
  REQUIRE(lipi_enhance(&v, &o, &out) == LIPI_OK);
  return out;
}

}  // namespace

TEST_CASE("the input has uneven lighting to begin with") {
  const Page page = make_page(false);
  const Stats bg = region_stats(page.view(), page.stroke, false);
  CHECK(bg.stddev > 25.0);
}

TEST_CASE("shadow removal evens out the background and keeps the ink dark") {
  const Page page = make_page(false);
  for (const int f : {LIPI_FILTER_AUTO, LIPI_FILTER_GRAY, LIPI_FILTER_COLOR}) {
    lipi_image* out = run(page, f);
    const lipi_image_view v = lipi_image_view_of(out);
    const Stats bg = region_stats(v, page.stroke, false);
    const Stats ink = region_stats(v, page.stroke, true);
    INFO("filter " << f);
    CHECK(bg.mean > 235.0);
    CHECK(bg.stddev < 8.0);
    CHECK(ink.mean < 110.0);
    lipi_image_free(out);
  }
}

TEST_CASE("without shadow removal the lighting is left alone") {
  const Page page = make_page(false);
  lipi_image* out = run(page, LIPI_FILTER_NONE, false);
  const lipi_image_view v = lipi_image_view_of(out);
  CHECK(region_stats(v, page.stroke, false).stddev > 25.0);
  lipi_image_free(out);
}

TEST_CASE("black and white gives only black and white and keeps the text") {
  const Page page = make_page(false);
  lipi_image* out = run(page, LIPI_FILTER_BW);
  const lipi_image_view v = lipi_image_view_of(out);
  REQUIRE(v.format == LIPI_PIXEL_GRAY8);
  long ink_black = 0, ink_total = 0, bg_black = 0, bg_total = 0;
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x) {
      const uint8_t p = v.data[y * v.stride + x];
      REQUIRE((p == 0 || p == 255));
      if (page.stroke[static_cast<size_t>(y * W + x)]) { ++ink_total; ink_black += p == 0; }
      else { ++bg_total; bg_black += p == 0; }
    }
  CHECK(static_cast<double>(ink_black) / static_cast<double>(ink_total) > 0.95);
  CHECK(static_cast<double>(bg_black) / static_cast<double>(bg_total) < 0.01);
  lipi_image_free(out);
}

TEST_CASE("gray gives a gray image and colour keeps colour") {
  const Page page = make_page(true);
  lipi_image* gray = run(page, LIPI_FILTER_GRAY);
  CHECK(lipi_image_view_of(gray).format == LIPI_PIXEL_GRAY8);
  lipi_image_free(gray);

  lipi_image* colour = run(page, LIPI_FILTER_COLOR);
  const lipi_image_view v = lipi_image_view_of(colour);
  REQUIRE(v.format == LIPI_PIXEL_RGB8);
  const uint8_t* stripe = v.data + 14 * v.stride + 100 * 3;  // inside the red stripe
  CHECK(int(stripe[0]) > int(stripe[1]) + 80);
  lipi_image_free(colour);
}

TEST_CASE("auto returns gray for a neutral page and colour for a coloured one") {
  lipi_image* neutral = run(make_page(false), LIPI_FILTER_AUTO);
  CHECK(lipi_image_view_of(neutral).format == LIPI_PIXEL_GRAY8);
  lipi_image_free(neutral);
  // Make the page properly coloured: a large saturated area.
  Page coloured = make_page(false, false);
  for (int y = 100; y < 260; ++y)
    for (int x = 20; x < 220; ++x) {
      uint8_t* px = &coloured.rgb[static_cast<size_t>((y * W + x) * 3)];
      px[0] = 220; px[1] = 90; px[2] = 60;
    }
  lipi_image* out = run(coloured, LIPI_FILTER_AUTO);
  CHECK(lipi_image_view_of(out).format == LIPI_PIXEL_RGB8);
  lipi_image_free(out);
}

TEST_CASE("brightness and contrast move the result in the right direction") {
  const Page page = make_page(false, false);
  auto mean_of = [&](float b, float c) {
    lipi_image* out = run(page, LIPI_FILTER_GRAY, true, b, c);
    const lipi_image_view v = lipi_image_view_of(out);
    double sum = 0;
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x) sum += v.data[y * v.stride + x];
    const double m = sum / (W * H);
    lipi_image_free(out);
    return m;
  };
  auto spread_of = [&](float c) {
    lipi_image* out = run(page, LIPI_FILTER_NONE, false, 0, c);  // no levels step, so the ink has room to move
    const Stats s = region_stats(lipi_image_view_of(out), page.stroke, true);
    lipi_image_free(out);
    return s.mean;
  };
  CHECK(mean_of(-0.5f, 0) < mean_of(0, 0));
  CHECK(mean_of(0.0f, 0) < mean_of(0.2f, 0));
  CHECK(spread_of(0.2f) < spread_of(0.0f));   // stronger contrast darkens the ink further
  CHECK(spread_of(-0.5f) > spread_of(0.0f));  // lower contrast lifts it
}

TEST_CASE("a grayscale source works and a single-channel result is returned") {
  std::vector<uint8_t> g(static_cast<size_t>(W * H), 200);
  for (int y = 40; y < 60; ++y)
    for (int x = 30; x < 200; ++x) g[static_cast<size_t>(y * W + x)] = 30;
  const lipi_image_view v{g.data(), W, H, W, LIPI_PIXEL_GRAY8};
  const lipi_enhance_options o = lipi_enhance_defaults();
  lipi_image* out = nullptr;
  REQUIRE(lipi_enhance(&v, &o, &out) == LIPI_OK);
  CHECK(lipi_image_view_of(out).format == LIPI_PIXEL_GRAY8);
  lipi_image_free(out);
}

TEST_CASE("enhance rejects bad arguments") {
  const Page page = make_page(false);
  const lipi_image_view v = page.view();
  lipi_enhance_options o = lipi_enhance_defaults();
  lipi_image* out = reinterpret_cast<lipi_image*>(0x1);
  CHECK(lipi_enhance(nullptr, &o, &out) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(out == nullptr);
  CHECK(lipi_enhance(&v, nullptr, &out) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(lipi_enhance(&v, &o, nullptr) == LIPI_ERR_INVALID_ARGUMENT);
  o.brightness = 2.0f;
  CHECK(lipi_enhance(&v, &o, &out) == LIPI_ERR_INVALID_ARGUMENT);
  o = lipi_enhance_defaults();
  o.contrast = NAN;
  CHECK(lipi_enhance(&v, &o, &out) == LIPI_ERR_INVALID_ARGUMENT);
  o = lipi_enhance_defaults();
  o.filter = 9;
  CHECK(lipi_enhance(&v, &o, &out) == LIPI_ERR_INVALID_ARGUMENT);
}
