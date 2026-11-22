#include <cmath>
#include <vector>

#include "doctest.h"
#include "lipi/lipi.h"

namespace {

constexpr int W = 520, H = 700;

// A page of text-like lines: short dark dashes in rows, with ragged ends.
std::vector<uint8_t> text_page(bool colour = false) {
  const int ch = colour ? 3 : 1;
  std::vector<uint8_t> px(static_cast<size_t>(W * H * ch), 238);
  for (int row = 0; row < 26; ++row) {
    const int y0 = 70 + row * 22;
    int x = 50 + (row % 3) * 4;
    unsigned seed = static_cast<unsigned>(row) * 7919u;
    while (x < W - 60) {
      seed = (seed * 1103515245u + 12345u) & 0x7fffffffu;
      const int len = 6 + static_cast<int>((seed >> 8) % 22u);
      for (int xx = x; xx < std::min(W - 50, x + len); ++xx)
        for (int yy = y0; yy < y0 + 7; ++yy)
          for (int c = 0; c < ch; ++c) px[static_cast<size_t>((yy * W + xx) * ch + c)] = 40;
      x += len + 5 + static_cast<int>((seed >> 16) % 6u);
    }
  }
  return px;
}

lipi_image_view view_of(const std::vector<uint8_t>& px, int channels = 1) {
  return {px.data(), W, H, W * channels, channels == 1 ? LIPI_PIXEL_GRAY8 : LIPI_PIXEL_RGB8};
}

}  // namespace

TEST_CASE("the estimate undoes a known rotation") {
  const auto px = text_page();
  const lipi_image_view v = view_of(px);
  for (const float tilt : {-6.0f, -3.0f, -1.0f, -0.5f, 0.5f, 1.0f, 2.5f, 5.0f, 7.0f}) {
    lipi_image* rotated = nullptr;
    REQUIRE(lipi_rotate(&v, tilt, &rotated) == LIPI_OK);
    const lipi_image_view rv = lipi_image_view_of(rotated);
    float degrees = 99, confidence = 0;
    REQUIRE(lipi_estimate_skew(&rv, &degrees, &confidence) == LIPI_OK);
    INFO("tilt " << tilt << " estimated correction " << degrees);
    CHECK(std::fabs(degrees + tilt) < 0.35f);  // rotating by `degrees` straightens the page
    CHECK(confidence > 0.3f);
    lipi_image_free(rotated);
  }
}

TEST_CASE("deskewing a tilted page makes the lines horizontal again") {
  const auto px = text_page();
  const lipi_image_view v = view_of(px);
  lipi_image* tilted = nullptr;
  REQUIRE(lipi_rotate(&v, 3.0f, &tilted) == LIPI_OK);
  const lipi_image_view tv = lipi_image_view_of(tilted);
  float correction = 0;
  REQUIRE(lipi_estimate_skew(&tv, &correction, nullptr) == LIPI_OK);
  lipi_image* fixed = nullptr;
  REQUIRE(lipi_rotate(&tv, correction, &fixed) == LIPI_OK);
  const lipi_image_view fv = lipi_image_view_of(fixed);
  float left = 99;
  REQUIRE(lipi_estimate_skew(&fv, &left, nullptr) == LIPI_OK);
  CHECK(std::fabs(left) < 0.25f);
  lipi_image_free(tilted);
  lipi_image_free(fixed);
}

TEST_CASE("an upright page and colour pages give no correction") {
  const auto px = text_page();
  float degrees = 99;
  const lipi_image_view v = view_of(px);
  REQUIRE(lipi_estimate_skew(&v, &degrees, nullptr) == LIPI_OK);
  CHECK(std::fabs(degrees) < 0.3f);
  const auto rgb = text_page(true);
  const lipi_image_view cv = view_of(rgb, 3);
  REQUIRE(lipi_estimate_skew(&cv, &degrees, nullptr) == LIPI_OK);
  CHECK(std::fabs(degrees) < 0.3f);
}

TEST_CASE("a blank page and a page of noise are left alone") {
  std::vector<uint8_t> blank(static_cast<size_t>(W * H), 230);
  const lipi_image_view bv = view_of(blank);
  float degrees = 99, confidence = 99;
  REQUIRE(lipi_estimate_skew(&bv, &degrees, &confidence) == LIPI_OK);
  CHECK(degrees == 0.0f);
  CHECK(confidence == 0.0f);
  std::vector<uint8_t> noise(static_cast<size_t>(W * H));
  unsigned seed = 1;
  for (auto& p : noise) { seed = seed * 1664525u + 1013904223u; p = static_cast<uint8_t>(seed >> 24); }
  const lipi_image_view nv = view_of(noise);
  REQUIRE(lipi_estimate_skew(&nv, &degrees, &confidence) == LIPI_OK);
  CHECK(confidence < 0.5f);
}

TEST_CASE("rotating by zero is the identity and the corners take the paper colour") {
  const auto px = text_page();
  const lipi_image_view v = view_of(px);
  lipi_image* same = nullptr;
  REQUIRE(lipi_rotate(&v, 0.0f, &same) == LIPI_OK);
  const lipi_image_view sv = lipi_image_view_of(same);
  for (size_t i = 0; i < px.size(); ++i) REQUIRE(int(sv.data[i]) == int(px[i]));
  lipi_image_free(same);
  lipi_image* rotated = nullptr;
  REQUIRE(lipi_rotate(&v, 10.0f, &rotated) == LIPI_OK);
  const lipi_image_view rv = lipi_image_view_of(rotated);
  CHECK(int(rv.data[0]) == 238);  // the exposed corner is paper coloured, not black
  lipi_image_free(rotated);
}

TEST_CASE("skew functions reject bad arguments") {
  const auto px = text_page();
  const lipi_image_view v = view_of(px);
  float degrees = 0;
  lipi_image* out = reinterpret_cast<lipi_image*>(0x1);
  CHECK(lipi_estimate_skew(nullptr, &degrees, nullptr) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(lipi_estimate_skew(&v, nullptr, nullptr) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(lipi_rotate(nullptr, 1.0f, &out) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(out == nullptr);
  CHECK(lipi_rotate(&v, NAN, &out) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(lipi_rotate(&v, 1.0f, nullptr) == LIPI_ERR_INVALID_ARGUMENT);
}
