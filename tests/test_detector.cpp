#include <cmath>
#include <vector>

#include "doctest.h"
#include "lipi/lipi.h"
#include "resize.hpp"

TEST_CASE("area resize averages exactly for whole-number scales") {
  // 4x4 gray image, rows: 0 4 8 12 / 0 4 8 12 / 100 104 108 112 / 100 104 108 112
  std::vector<uint8_t> px;
  for (int y = 0; y < 4; ++y)
    for (int x = 0; x < 4; ++x) px.push_back(static_cast<uint8_t>((y < 2 ? 0 : 100) + 4 * x));
  const lipi_image_view v{px.data(), 4, 4, 4, LIPI_PIXEL_GRAY8};
  const std::vector<float> out = lipi::resize_area_planar_rgb(v, 2, 2);
  REQUIRE(out.size() == 12);
  CHECK(out[0] == doctest::Approx(2.0f));    // R plane, top-left block
  CHECK(out[1] == doctest::Approx(10.0f));
  CHECK(out[2] == doctest::Approx(102.0f));
  CHECK(out[3] == doctest::Approx(110.0f));
  for (int i = 0; i < 4; ++i) {  // gray is replicated into G and B
    CHECK(out[4 + i] == out[i]);
    CHECK(out[8 + i] == out[i]);
  }
}

TEST_CASE("area resize weights fractional coverage like INTER_AREA") {
  const uint8_t px[3] = {0, 30, 60};
  const lipi_image_view v{px, 3, 1, 3, LIPI_PIXEL_GRAY8};
  const std::vector<float> out = lipi::resize_area_planar_rgb(v, 2, 1);
  CHECK(out[0] == doctest::Approx(10.0f));  // (0 * 1 + 30 * 0.5) / 1.5
  CHECK(out[1] == doctest::Approx(50.0f));  // (30 * 0.5 + 60 * 1) / 1.5
}

TEST_CASE("area resize keeps channels apart, ignores alpha and honours the stride") {
  // 2x1 RGBA with 4 bytes of padding per row
  const uint8_t px[12] = {10, 20, 30, 255, 30, 40, 50, 0, 9, 9, 9, 9};
  const lipi_image_view v{px, 2, 1, 12, LIPI_PIXEL_RGBA8};
  const std::vector<float> out = lipi::resize_area_planar_rgb(v, 1, 1);
  CHECK(out[0] == doctest::Approx(20.0f));
  CHECK(out[1] == doctest::Approx(30.0f));
  CHECK(out[2] == doctest::Approx(40.0f));
}

TEST_CASE("upscaling and a no-op resize preserve values") {
  const uint8_t px[4] = {10, 20, 30, 40};
  const lipi_image_view v{px, 2, 2, 2, LIPI_PIXEL_GRAY8};
  const std::vector<float> same = lipi::resize_area_planar_rgb(v, 2, 2);
  for (int i = 0; i < 4; ++i) CHECK(same[static_cast<size_t>(i)] == doctest::Approx(static_cast<float>(px[i])));
  const std::vector<float> up = lipi::resize_area_planar_rgb(v, 4, 4);
  CHECK(up[0] == doctest::Approx(10.0f));
  CHECK(up[15] == doctest::Approx(40.0f));
}

TEST_CASE("the detector reports whether a backend is built in") {
  const int available = lipi_detector_available();
  CHECK((available == 0 || available == 1));
  lipi_detector* d = reinterpret_cast<lipi_detector*>(0x1);
  const char junk[] = "not a model";
  const lipi_status s = lipi_detector_create(junk, sizeof junk, junk, sizeof junk, 1, &d);
  CHECK(d == nullptr);
  if (available) CHECK(s == LIPI_ERR_INVALID_ARGUMENT);
  else CHECK(s == LIPI_ERR_UNSUPPORTED);
}

TEST_CASE("detector rejects bad arguments") {
  lipi_detector* d = nullptr;
  CHECK(lipi_detector_create(nullptr, 0, nullptr, 0, 1, &d) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(lipi_detector_create("x", 1, "y", 1, 1, nullptr) == LIPI_ERR_INVALID_ARGUMENT);
  const uint8_t px[4] = {0, 0, 0, 0};
  const lipi_image_view v{px, 2, 2, 2, LIPI_PIXEL_GRAY8};
  lipi_detection det{};
  const lipi_status s = lipi_detector_detect(nullptr, &v, &det);
  CHECK((s == LIPI_ERR_INVALID_ARGUMENT || s == LIPI_ERR_UNSUPPORTED));
  lipi_detector_free(nullptr);
}
