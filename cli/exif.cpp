#include "exif.hpp"

#include <cstring>

namespace lipi_cli {

namespace {

struct Reader {
  const uint8_t* d;
  size_t n;
  bool big;
  bool ok(size_t off, size_t len) const { return off <= n && len <= n - off; }
  uint16_t u16(size_t off) const {
    return big ? static_cast<uint16_t>((d[off] << 8) | d[off + 1]) : static_cast<uint16_t>(d[off] | (d[off + 1] << 8));
  }
  uint32_t u32(size_t off) const {
    return big ? (static_cast<uint32_t>(d[off]) << 24) | (static_cast<uint32_t>(d[off + 1]) << 16) |
                     (static_cast<uint32_t>(d[off + 2]) << 8) | d[off + 3]
               : (static_cast<uint32_t>(d[off + 3]) << 24) | (static_cast<uint32_t>(d[off + 2]) << 16) |
                     (static_cast<uint32_t>(d[off + 1]) << 8) | d[off];
  }
};

int orientation_from_tiff(const uint8_t* tiff, size_t size) {
  if (size < 8) return 1;
  Reader r{tiff, size, false};
  if (tiff[0] == 'M' && tiff[1] == 'M') r.big = true;
  else if (!(tiff[0] == 'I' && tiff[1] == 'I')) return 1;
  if (r.u16(2) != 42) return 1;
  const size_t ifd = r.u32(4);
  if (!r.ok(ifd, 2)) return 1;
  const size_t count = r.u16(ifd);
  for (size_t i = 0; i < count; ++i) {
    const size_t e = ifd + 2 + i * 12;
    if (!r.ok(e, 12)) return 1;
    if (r.u16(e) == 0x0112) {
      const int v = r.u16(e + 8);
      return v >= 1 && v <= 8 ? v : 1;
    }
  }
  return 1;
}

}  // namespace

int jpeg_exif_orientation(const uint8_t* data, size_t size) {
  if (data == nullptr || size < 4 || data[0] != 0xFF || data[1] != 0xD8) return 1;
  size_t pos = 2;
  while (pos + 4 <= size) {
    if (data[pos] != 0xFF) return 1;
    const uint8_t marker = data[pos + 1];
    if (marker == 0xFF) {  // fill byte
      ++pos;
      continue;
    }
    if (marker == 0xD9 || marker == 0xDA) return 1;  // end of image / start of scan
    const size_t len = static_cast<size_t>((data[pos + 2] << 8) | data[pos + 3]);
    if (len < 2 || pos + 2 + len > size) return 1;
    if (marker == 0xE1 && len >= 8 && std::memcmp(data + pos + 4, "Exif\0\0", 6) == 0) {
      return orientation_from_tiff(data + pos + 10, len - 8);
    }
    pos += 2 + len;
  }
  return 1;
}

std::vector<uint8_t> apply_orientation(const std::vector<uint8_t>& pixels, int* width, int* height,
                                       int channels, int orientation) {
  if (orientation < 2 || orientation > 8) return pixels;
  const int sw = *width, sh = *height;
  const bool swap = orientation >= 5;
  const int dw = swap ? sh : sw, dh = swap ? sw : sh;
  std::vector<uint8_t> out(pixels.size());
  const size_t ch = static_cast<size_t>(channels);
  for (int y = 0; y < dh; ++y)
    for (int x = 0; x < dw; ++x) {
      int sx = x, sy = y;
      switch (orientation) {
        case 2: sx = sw - 1 - x; break;
        case 3: sx = sw - 1 - x; sy = sh - 1 - y; break;
        case 4: sy = sh - 1 - y; break;
        case 5: sx = y; sy = x; break;
        case 6: sx = y; sy = sh - 1 - x; break;
        case 7: sx = sw - 1 - y; sy = sh - 1 - x; break;
        case 8: sx = sw - 1 - y; sy = x; break;
      }
      std::memcpy(&out[(static_cast<size_t>(y) * static_cast<size_t>(dw) + static_cast<size_t>(x)) * ch],
                  &pixels[(static_cast<size_t>(sy) * static_cast<size_t>(sw) + static_cast<size_t>(sx)) * ch], ch);
    }
  *width = dw;
  *height = dh;
  return out;
}

}  // namespace lipi_cli
