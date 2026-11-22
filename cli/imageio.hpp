#ifndef LIPI_CLI_IMAGEIO_HPP
#define LIPI_CLI_IMAGEIO_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "lipi/lipi.h"

namespace lipi_cli {

struct Pixels {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> rgb;  // tightly packed RGB
  lipi_image_view view() const { return {rgb.data(), width, height, width * 3, LIPI_PIXEL_RGB8}; }
};

// Decodes a JPEG, PNG, BMP or similar file to RGB and applies EXIF orientation.
bool load_image(const std::string& path, Pixels* out, std::string* error);

// Writes .png, .jpg/.jpeg or .bmp, chosen by the file extension.
bool save_image(const std::string& path, const lipi_image_view& view, std::string* error);

// Encodes tightly packed gray or RGB pixels as a baseline JPEG.
bool encode_jpeg(const lipi_image_view& view, int quality, std::vector<uint8_t>* out);

// Draws the quad outline (and a dot on corner 0) onto the pixels.
void draw_quad(Pixels* img, const lipi_quad& quad);

}  // namespace lipi_cli

#endif
