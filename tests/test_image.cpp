#include <cstring>

#include "doctest.h"
#include "lipi/lipi.h"

TEST_CASE("status strings are never null") {
  for (int s = 0; s <= 5; ++s) {
    CHECK(lipi_status_string(static_cast<lipi_status>(s)) != nullptr);
  }
  CHECK(std::strcmp(lipi_status_string(LIPI_OK), "ok") == 0);
}

TEST_CASE("version string matches the version macros") {
  CHECK(std::strcmp(lipi_version(), "0.0.1") == 0);
}

TEST_CASE("image create returns zeroed pixels") {
  lipi_image* image = nullptr;
  REQUIRE(lipi_image_create(4, 3, LIPI_PIXEL_RGB8, &image) == LIPI_OK);
  const lipi_image_view view = lipi_image_view_of(image);
  CHECK(view.width == 4);
  CHECK(view.height == 3);
  CHECK(view.stride == 12);
  for (int i = 0; i < 4 * 3 * 3; ++i) CHECK(view.data[i] == 0);
  lipi_image_free(image);
}

TEST_CASE("image create rejects bad arguments") {
  lipi_image* image = reinterpret_cast<lipi_image*>(0x1);
  CHECK(lipi_image_create(0, 3, LIPI_PIXEL_RGB8, &image) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(image == nullptr);
  CHECK(lipi_image_create(3, -1, LIPI_PIXEL_GRAY8, &image) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(lipi_image_create(3, 3, static_cast<lipi_pixel_format>(2), &image) ==
        LIPI_ERR_INVALID_ARGUMENT);
  CHECK(lipi_image_create(3, 3, LIPI_PIXEL_GRAY8, nullptr) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(lipi_image_create(1 << 17, 1, LIPI_PIXEL_GRAY8, &image) == LIPI_ERR_INVALID_ARGUMENT);
}

TEST_CASE("image copy honours the source stride") {
  // 2x2 gray image with 2 bytes of padding per row.
  const uint8_t src[8] = {1, 2, 99, 99, 3, 4, 99, 99};
  lipi_image_view view{src, 2, 2, 4, LIPI_PIXEL_GRAY8};
  lipi_image* copy = nullptr;
  REQUIRE(lipi_image_copy(&view, &copy) == LIPI_OK);
  const lipi_image_view out = lipi_image_view_of(copy);
  CHECK(out.stride == 2);
  CHECK(out.data[0] == 1);
  CHECK(out.data[1] == 2);
  CHECK(out.data[2] == 3);
  CHECK(out.data[3] == 4);
  lipi_image_free(copy);
}

TEST_CASE("image copy rejects a stride that is too small") {
  const uint8_t src[16] = {0};
  lipi_image_view view{src, 4, 2, 3, LIPI_PIXEL_GRAY8};
  lipi_image* copy = nullptr;
  CHECK(lipi_image_copy(&view, &copy) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(copy == nullptr);
}

TEST_CASE("freeing NULL is allowed") { lipi_image_free(nullptr); }
