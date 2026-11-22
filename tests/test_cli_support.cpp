#include <cstdio>
#include <string>
#include <vector>

#include "doctest.h"
#include "exif.hpp"
#include "imageio.hpp"

using lipi_cli::apply_orientation;
using lipi_cli::jpeg_exif_orientation;

namespace {

std::vector<uint8_t> jpeg_with_orientation(int value, bool big_endian) {
  std::vector<uint8_t> t;
  auto u16 = [&](int v) {
    if (big_endian) { t.push_back(static_cast<uint8_t>(v >> 8)); t.push_back(static_cast<uint8_t>(v)); }
    else { t.push_back(static_cast<uint8_t>(v)); t.push_back(static_cast<uint8_t>(v >> 8)); }
  };
  auto u32 = [&](unsigned v) {
    if (big_endian) for (int s = 24; s >= 0; s -= 8) t.push_back(static_cast<uint8_t>(v >> s));
    else for (int s = 0; s <= 24; s += 8) t.push_back(static_cast<uint8_t>(v >> s));
  };
  t.push_back(big_endian ? 'M' : 'I');
  t.push_back(big_endian ? 'M' : 'I');
  u16(42);
  u32(8);
  u16(1);           // one entry
  u16(0x0112);      // orientation
  u16(3);           // SHORT
  u32(1);
  u16(value);
  u16(0);
  u32(0);           // no next IFD
  std::vector<uint8_t> j = {0xFF, 0xD8, 0xFF, 0xE1};
  const size_t len = 2 + 6 + t.size();
  j.push_back(static_cast<uint8_t>(len >> 8));
  j.push_back(static_cast<uint8_t>(len));
  for (const char c : std::string("Exif\0\0", 6)) j.push_back(static_cast<uint8_t>(c));
  j.insert(j.end(), t.begin(), t.end());
  j.push_back(0xFF);
  j.push_back(0xD9);
  return j;
}

}  // namespace

TEST_CASE("exif orientation is read in both byte orders") {
  for (int v = 1; v <= 8; ++v) {
    const auto le = jpeg_with_orientation(v, false);
    const auto be = jpeg_with_orientation(v, true);
    CHECK(jpeg_exif_orientation(le.data(), le.size()) == v);
    CHECK(jpeg_exif_orientation(be.data(), be.size()) == v);
  }
}

TEST_CASE("exif orientation defaults to 1 for bad or missing data") {
  const uint8_t not_jpeg[] = {1, 2, 3, 4, 5};
  CHECK(jpeg_exif_orientation(not_jpeg, sizeof not_jpeg) == 1);
  CHECK(jpeg_exif_orientation(nullptr, 0) == 1);
  const uint8_t bare[] = {0xFF, 0xD8, 0xFF, 0xD9};
  CHECK(jpeg_exif_orientation(bare, sizeof bare) == 1);
  auto j = jpeg_with_orientation(6, false);
  for (size_t cut = 0; cut < j.size(); ++cut) {  // truncation never crashes or misreads
    const int o = jpeg_exif_orientation(j.data(), cut);
    CHECK((o == 1 || o == 6));
  }
  j = jpeg_with_orientation(9, false);  // out of range
  CHECK(jpeg_exif_orientation(j.data(), j.size()) == 1);
}

TEST_CASE("orientation transforms match the EXIF definitions") {
  // 3 wide, 2 tall:  1 2 3 / 4 5 6
  const std::vector<uint8_t> src = {1, 2, 3, 4, 5, 6};
  struct Case { int o; int w, h; std::vector<uint8_t> px; };
  const Case cases[] = {
      {1, 3, 2, {1, 2, 3, 4, 5, 6}},
      {2, 3, 2, {3, 2, 1, 6, 5, 4}},
      {3, 3, 2, {6, 5, 4, 3, 2, 1}},
      {4, 3, 2, {4, 5, 6, 1, 2, 3}},
      {5, 2, 3, {1, 4, 2, 5, 3, 6}},
      {6, 2, 3, {4, 1, 5, 2, 6, 3}},
      {7, 2, 3, {6, 3, 5, 2, 4, 1}},
      {8, 2, 3, {3, 6, 2, 5, 1, 4}},
  };
  for (const Case& c : cases) {
    int w = 3, h = 2;
    const auto out = apply_orientation(src, &w, &h, 1, c.o);
    CHECK(w == c.w);
    CHECK(h == c.h);
    CHECK(out == c.px);
  }
}

TEST_CASE("an image survives a write and read round trip") {
  lipi_cli::Pixels img;
  img.width = 5;
  img.height = 4;
  img.rgb.resize(5 * 4 * 3);
  for (size_t i = 0; i < img.rgb.size(); ++i) img.rgb[i] = static_cast<uint8_t>(i * 3);
  const std::string path = "lipi_roundtrip_test.png";
  std::string err;
  REQUIRE(lipi_cli::save_image(path, img.view(), &err));
  lipi_cli::Pixels back;
  REQUIRE(lipi_cli::load_image(path, &back, &err));
  CHECK(back.width == 5);
  CHECK(back.height == 4);
  CHECK(back.rgb == img.rgb);
  std::remove(path.c_str());
  CHECK_FALSE(lipi_cli::save_image("x.tiff", img.view(), &err));
  CHECK_FALSE(lipi_cli::load_image("does-not-exist.png", &back, &err));
}

TEST_CASE("draw_quad marks the outline and stays inside the image") {
  lipi_cli::Pixels img;
  img.width = 40;
  img.height = 30;
  img.rgb.assign(40 * 30 * 3, 0);
  lipi_quad q{};
  q.p[0] = {5, 5};
  q.p[1] = {35, 5};
  q.p[2] = {35, 25};
  q.p[3] = {5, 25};
  lipi_cli::draw_quad(&img, q);
  CHECK(img.rgb[(15 * 40 + 20) * 3 + 1] == 0);   // interior untouched
  CHECK(img.rgb[(5 * 40 + 20) * 3 + 1] == 255);  // top edge is green
  q.p[1] = {500, -50};
  lipi_cli::draw_quad(&img, q);                   // out of range points are clipped
}
