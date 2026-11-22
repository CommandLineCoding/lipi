#ifndef LIPI_CLI_EXIF_HPP
#define LIPI_CLI_EXIF_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace lipi_cli {

// Returns the EXIF orientation (1-8) of a JPEG, or 1 when there is none or the
// data is malformed.
int jpeg_exif_orientation(const uint8_t* data, size_t size);

// Rotates or flips tightly packed pixels so that the image is shown upright.
// Orientation values follow the EXIF specification. Width and height are
// swapped in place for orientations 5-8.
std::vector<uint8_t> apply_orientation(const std::vector<uint8_t>& pixels, int* width, int* height,
                                       int channels, int orientation);

}  // namespace lipi_cli

#endif
