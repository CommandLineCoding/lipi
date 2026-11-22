#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include "doctest.h"
#include "lipi/lipi.h"

namespace {

struct Img {
  int w, h;
  std::vector<uint8_t> px;  // RGB
  lipi_image_view view() const { return {px.data(), w, h, w * 3, LIPI_PIXEL_RGB8}; }
};

bool inside_convex(const lipi_quad& q, double x, double y) {
  for (int i = 0; i < 4; ++i) {
    const lipi_point& a = q.p[i];
    const lipi_point& b = q.p[(i + 1) % 4];
    if ((b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x) < 0) return false;
  }
  return true;
}

// A bright "page" with a few text-like stripes on a dark noisy background.
Img scene(int w, int h, const lipi_quad& q, int bg, int page, unsigned seed, int noise = 6) {
  std::mt19937 rng(seed);
  std::normal_distribution<double> n(0.0, noise);
  Img img{w, h, std::vector<uint8_t>(static_cast<size_t>(w) * static_cast<size_t>(h) * 3)};
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      double v = inside_convex(q, x + 0.5, y + 0.5) ? page : bg;
      if (v == page && (y / 7) % 3 == 0 && (x % 40) < 28 && inside_convex(q, x + 0.5 - 12, y + 0.5 - 12) &&
          inside_convex(q, x + 0.5 + 12, y + 0.5 + 12)) {
        v -= 90;  // text
      }
      const double value = v + n(rng);
      for (int c = 0; c < 3; ++c)
        img.px[(static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 3 + static_cast<size_t>(c)] =
            static_cast<uint8_t>(std::min(255.0, std::max(0.0, value)));
    }
  return img;
}

lipi_quad make(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3) {
  lipi_quad q{};
  q.p[0] = {x0, y0};
  q.p[1] = {x1, y1};
  q.p[2] = {x2, y2};
  q.p[3] = {x3, y3};
  return q;
}

double max_corner_error(const lipi_quad& a, const lipi_quad& b) {
  double worst = 0;
  for (int i = 0; i < 4; ++i)
    worst = std::max(worst, std::hypot(static_cast<double>(a.p[i].x - b.p[i].x), static_cast<double>(a.p[i].y - b.p[i].y)));
  return worst;
}

}  // namespace

TEST_CASE("classical detection finds an upright page") {
  const lipi_quad truth = make(120, 90, 520, 100, 510, 700, 110, 690);
  const Img img = scene(640, 800, truth, 50, 210, 1);
  lipi_quad got{};
  float conf = 0;
  const lipi_image_view v = img.view();
  REQUIRE(lipi_detect_page_classical(&v, &got, &conf) == LIPI_OK);
  CHECK(max_corner_error(got, truth) < 4.0);
  CHECK(conf > 0.5f);
}

TEST_CASE("classical detection finds a tilted page in perspective") {
  const lipi_quad truth = make(150, 60, 560, 150, 520, 640, 90, 520);
  const Img img = scene(640, 720, truth, 70, 200, 2);
  lipi_quad got{};
  const lipi_image_view v = img.view();
  REQUIRE(lipi_detect_page_classical(&v, &got, nullptr) == LIPI_OK);
  CHECK(max_corner_error(got, truth) < 5.0);
}

TEST_CASE("classical detection works on a light page over a lighter background gap") {
  const lipi_quad truth = make(80, 120, 560, 110, 570, 650, 70, 660);
  const Img img = scene(640, 760, truth, 120, 235, 3);
  lipi_quad got{};
  const lipi_image_view v = img.view();
  REQUIRE(lipi_detect_page_classical(&v, &got, nullptr) == LIPI_OK);
  CHECK(max_corner_error(got, truth) < 5.0);
}

TEST_CASE("classical detection reports not found on noise") {
  const lipi_quad nowhere = make(-10, -10, -5, -10, -5, -5, -10, -5);
  const Img img = scene(400, 300, nowhere, 100, 100, 4, 10);
  lipi_quad got{};
  const lipi_image_view v = img.view();
  CHECK(lipi_detect_page_classical(&v, &got, nullptr) == LIPI_ERR_NOT_FOUND);
}

TEST_CASE("classical detection rejects bad arguments") {
  const Img img = scene(100, 100, make(10, 10, 90, 10, 90, 90, 10, 90), 50, 200, 5);
  const lipi_image_view v = img.view();
  lipi_quad q{};
  CHECK(lipi_detect_page_classical(nullptr, &q, nullptr) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(lipi_detect_page_classical(&v, nullptr, nullptr) == LIPI_ERR_INVALID_ARGUMENT);
  const lipi_image_view tiny{img.px.data(), 8, 8, 24, LIPI_PIXEL_RGB8};
  CHECK(lipi_detect_page_classical(&tiny, &q, nullptr) == LIPI_ERR_INVALID_ARGUMENT);
}

TEST_CASE("refinement moves perturbed corners back onto the edges") {
  const lipi_quad truth = make(120.4f, 90.7f, 520.2f, 100.3f, 510.6f, 700.1f, 110.9f, 690.5f);
  const Img img = scene(640, 800, truth, 50, 210, 6, 3);
  const lipi_quad rough = make(125, 86, 516, 104, 515, 696, 106, 694);
  lipi_quad got{};
  const lipi_image_view v = img.view();
  REQUIRE(lipi_refine_quad(&v, &rough, &got) == LIPI_OK);
  CHECK(max_corner_error(got, truth) < 1.2);
  CHECK(max_corner_error(got, truth) < max_corner_error(rough, truth));
}

TEST_CASE("refinement rejects an invalid quad") {
  const Img img = scene(100, 100, make(10, 10, 90, 10, 90, 90, 10, 90), 50, 200, 7);
  const lipi_image_view v = img.view();
  const lipi_quad bow = make(0, 0, 10, 10, 10, 0, 0, 10);
  lipi_quad q{};
  CHECK(lipi_refine_quad(&v, &bow, &q) == LIPI_ERR_INVALID_ARGUMENT);
}
