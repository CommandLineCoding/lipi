#ifndef LIPI_SRC_IMAGE_HPP
#define LIPI_SRC_IMAGE_HPP

#include <cstdint>
#include <vector>

#include "lipi/lipi.h"

struct lipi_image {
  int width = 0;
  int height = 0;
  lipi_pixel_format format = LIPI_PIXEL_GRAY8;
  std::vector<uint8_t> pixels;  // tightly packed: stride == width * channels
};

#endif
