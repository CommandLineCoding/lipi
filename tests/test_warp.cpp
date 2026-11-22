#include <cmath>
#include <vector>

#include "doctest.h"
#include "lipi/lipi.h"

namespace {

lipi_quad make_quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3) {
  lipi_quad q{};
  q.p[0] = {x0, y0};
  q.p[1] = {x1, y1};
  q.p[2] = {x2, y2};
  q.p[3] = {x3, y3};
  return q;
}

lipi_quad rect(float x, float y, float w, float h) {
  return make_quad(x, y, x + w, y, x + w, y + h, x, y + h);
}

// Gray image whose value is the column index.
std::vector<uint8_t> ramp(int w, int h) {
  std::vector<uint8_t> px(static_cast<size_t>(w) * static_cast<size_t>(h));
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      px[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] =
          static_cast<uint8_t>(x);
  return px;
}

// Projects a page of size pw x ph (plane z = 0, centered), rotated about the
// y then x axis, at distance d from a pinhole camera with focal length f.
lipi_quad project_page(double pw, double ph, double rx, double ry, double d, double f, int iw,
                       int ih) {
  const double corners[4][2] = {{-pw / 2, -ph / 2}, {pw / 2, -ph / 2}, {pw / 2, ph / 2},
                                {-pw / 2, ph / 2}};
  lipi_quad q{};
  for (int i = 0; i < 4; ++i) {
    double x = corners[i][0], y = corners[i][1], z = 0.0;
    const double x1 = x * std::cos(ry) + z * std::sin(ry);
    const double z1 = -x * std::sin(ry) + z * std::cos(ry);
    const double y2 = y * std::cos(rx) - z1 * std::sin(rx);
    const double z2 = y * std::sin(rx) + z1 * std::cos(rx);
    q.p[i].x = static_cast<float>(f * x1 / (z2 + d) + iw / 2.0);
    q.p[i].y = static_cast<float>(f * y2 / (z2 + d) + ih / 2.0);
  }
  return q;
}

}  // namespace

TEST_CASE("quad validation accepts a clockwise convex quad") {
  const lipi_quad q = rect(10, 10, 50, 40);
  CHECK(lipi_quad_validate(&q) == LIPI_OK);
}

TEST_CASE("quad validation rejects bad quads") {
  CHECK(lipi_quad_validate(nullptr) == LIPI_ERR_INVALID_ARGUMENT);
  // Counter-clockwise order.
  CHECK(lipi_quad_validate(&(const lipi_quad&)make_quad(0, 0, 0, 10, 10, 10, 10, 0)) ==
        LIPI_ERR_INVALID_ARGUMENT);
  // Self-intersecting bow-tie.
  const lipi_quad bow = make_quad(0, 0, 10, 10, 10, 0, 0, 10);
  CHECK(lipi_quad_validate(&bow) == LIPI_ERR_INVALID_ARGUMENT);
  // Three collinear corners.
  const lipi_quad line = make_quad(0, 0, 5, 0, 10, 0, 0, 10);
  CHECK(lipi_quad_validate(&line) == LIPI_ERR_INVALID_ARGUMENT);
  // Reflex corner.
  const lipi_quad dart = make_quad(0, 0, 10, 0, 2, 2, 0, 10);
  CHECK(lipi_quad_validate(&dart) == LIPI_ERR_INVALID_ARGUMENT);
  // Not finite.
  const lipi_quad nan = make_quad(0, 0, NAN, 0, 10, 10, 0, 10);
  CHECK(lipi_quad_validate(&nan) == LIPI_ERR_INVALID_ARGUMENT);
}

TEST_CASE("warping the full image is the identity") {
  const int w = 17, h = 9;
  std::vector<uint8_t> px(static_cast<size_t>(w * h * 3));
  for (size_t i = 0; i < px.size(); ++i) px[i] = static_cast<uint8_t>((i * 37) % 251);
  const lipi_image_view src{px.data(), w, h, w * 3, LIPI_PIXEL_RGB8};
  const lipi_quad q = rect(0, 0, w, h);
  lipi_image* out = nullptr;
  REQUIRE(lipi_warp_quad(&src, &q, w, h, &out) == LIPI_OK);
  const lipi_image_view v = lipi_image_view_of(out);
  CHECK(v.format == LIPI_PIXEL_RGB8);
  for (size_t i = 0; i < px.size(); ++i) CHECK(int(v.data[i]) == int(px[i]));
  lipi_image_free(out);
}

TEST_CASE("warping a sub-rectangle crops it exactly") {
  const auto px = ramp(100, 80);
  const lipi_image_view src{px.data(), 100, 80, 100, LIPI_PIXEL_GRAY8};
  const lipi_quad q = rect(10, 5, 40, 20);
  lipi_image* out = nullptr;
  REQUIRE(lipi_warp_quad(&src, &q, 40, 20, &out) == LIPI_OK);
  const lipi_image_view v = lipi_image_view_of(out);
  for (int y = 0; y < 20; ++y)
    for (int x = 0; x < 40; ++x) CHECK(int(v.data[y * v.stride + x]) == 10 + x);
  lipi_image_free(out);
}

TEST_CASE("a keystone quad flattens a ramp monotonically") {
  const auto px = ramp(100, 80);
  const lipi_image_view src{px.data(), 100, 80, 100, LIPI_PIXEL_GRAY8};
  const lipi_quad q = make_quad(20, 10, 80, 20, 90, 70, 10, 60);
  lipi_image* out = nullptr;
  REQUIRE(lipi_warp_quad(&src, &q, 60, 50, &out) == LIPI_OK);
  const lipi_image_view v = lipi_image_view_of(out);
  for (int y = 0; y < 50; ++y)
    for (int x = 1; x < 60; ++x) CHECK(v.data[y * v.stride + x] >= v.data[y * v.stride + x - 1]);
  // The left edge of the output samples the left edge of the quad.
  CHECK(int(v.data[0]) >= 10);
  CHECK(int(v.data[0]) <= 25);
  lipi_image_free(out);
}

TEST_CASE("warp rejects bad arguments and clears the output") {
  const auto px = ramp(10, 10);
  const lipi_image_view src{px.data(), 10, 10, 10, LIPI_PIXEL_GRAY8};
  const lipi_quad q = rect(0, 0, 10, 10);
  lipi_image* out = reinterpret_cast<lipi_image*>(0x1);
  CHECK(lipi_warp_quad(nullptr, &q, 5, 5, &out) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(out == nullptr);
  CHECK(lipi_warp_quad(&src, nullptr, 5, 5, &out) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(lipi_warp_quad(&src, &q, 0, 5, &out) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(lipi_warp_quad(&src, &q, 5, 5, nullptr) == LIPI_ERR_INVALID_ARGUMENT);
  const lipi_image_view bad{px.data(), 10, 10, 5, LIPI_PIXEL_GRAY8};
  CHECK(lipi_warp_quad(&bad, &q, 5, 5, &out) == LIPI_ERR_INVALID_ARGUMENT);
  const lipi_quad outside = rect(-50, -50, 70, 70);
  REQUIRE(lipi_warp_quad(&src, &outside, 8, 8, &out) == LIPI_OK);  // clamps at the border
  lipi_image_free(out);
}

TEST_CASE("page size from an upright rectangle uses its side lengths") {
  const lipi_quad q = rect(100, 100, 600, 800);
  int w = 0, h = 0;
  REQUIRE(lipi_estimate_page_size(&q, 1000, 1000, 4096, &w, &h) == LIPI_OK);
  CHECK(w == 600);
  CHECK(h == 800);
}

TEST_CASE("page size recovers the true aspect from perspective") {
  struct Case {
    double pw, ph, rx, ry;
  };
  // A4 portrait, A4 landscape and a square, seen at different tilts.
  const Case cases[] = {{210, 297, 0.35, 0.25}, {297, 210, -0.3, 0.4}, {200, 200, 0.5, -0.3},
                        {210, 297, 0.0, 0.5},   {85, 55, 0.4, 0.4}};
  const int iw = 3000, ih = 4000;
  for (const Case& c : cases) {
    const lipi_quad q = project_page(c.pw, c.ph, c.rx, c.ry, 600.0, 3200.0, iw, ih);
    int w = 0, h = 0;
    REQUIRE(lipi_estimate_page_size(&q, iw, ih, 4096, &w, &h) == LIPI_OK);
    const double got = static_cast<double>(w) / h;
    const double want = c.pw / c.ph;
    INFO("page " << c.pw << "x" << c.ph << " rx=" << c.rx << " ry=" << c.ry << " got " << got);
    CHECK(std::fabs(got - want) / want < 0.03);
  }
}

TEST_CASE("page size is capped to max_side") {
  const lipi_quad q = rect(0, 0, 3000, 2000);
  int w = 0, h = 0;
  REQUIRE(lipi_estimate_page_size(&q, 3000, 2000, 1000, &w, &h) == LIPI_OK);
  CHECK(w == 1000);
  CHECK(h == 667);
}

TEST_CASE("page size rejects bad arguments") {
  const lipi_quad q = rect(0, 0, 10, 10);
  int w = 0, h = 0;
  CHECK(lipi_estimate_page_size(&q, 10, 10, 100, nullptr, &h) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(lipi_estimate_page_size(&q, 0, 10, 100, &w, &h) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(lipi_estimate_page_size(nullptr, 10, 10, 100, &w, &h) == LIPI_ERR_INVALID_ARGUMENT);
}

TEST_CASE("page size is close even when the focal length is not the assumed one") {
  const int iw = 3000, ih = 4000;
  // Single-axis tilt has one vanishing point, so the assumed focal length is used.
  const lipi_quad q = project_page(210, 297, 0.0, 0.4, 600.0, 2400.0, iw, ih);
  int w = 0, h = 0;
  REQUIRE(lipi_estimate_page_size(&q, iw, ih, 4096, &w, &h) == LIPI_OK);
  CHECK(std::fabs(static_cast<double>(w) / h - 210.0 / 297.0) / (210.0 / 297.0) < 0.08);
}
