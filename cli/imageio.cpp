#include "imageio.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include "exif.hpp"

namespace lipi_cli {

namespace {

bool read_file(const std::string& path, std::vector<uint8_t>* out) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (f == nullptr) return false;
  std::fseek(f, 0, SEEK_END);
  const long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (n < 0) {
    std::fclose(f);
    return false;
  }
  out->resize(static_cast<size_t>(n));
  const size_t got = n == 0 ? 0 : std::fread(out->data(), 1, out->size(), f);
  std::fclose(f);
  return got == out->size();
}

std::string lower_extension(const std::string& path) {
  const size_t dot = path.find_last_of('.');
  if (dot == std::string::npos) return "";
  std::string ext = path.substr(dot + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return ext;
}

void put(Pixels* img, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  if (x < 0 || y < 0 || x >= img->width || y >= img->height) return;
  uint8_t* p = &img->rgb[(static_cast<size_t>(y) * static_cast<size_t>(img->width) + static_cast<size_t>(x)) * 3];
  p[0] = r;
  p[1] = g;
  p[2] = b;
}

void line(Pixels* img, double x0, double y0, double x1, double y1, int thickness) {
  const int steps = std::max(1, static_cast<int>(std::max(std::fabs(x1 - x0), std::fabs(y1 - y0))));
  for (int i = 0; i <= steps; ++i) {
    const double t = static_cast<double>(i) / steps;
    const int x = static_cast<int>(std::lround(x0 + (x1 - x0) * t)), y = static_cast<int>(std::lround(y0 + (y1 - y0) * t));
    for (int dy = -thickness; dy <= thickness; ++dy)
      for (int dx = -thickness; dx <= thickness; ++dx) put(img, x + dx, y + dy, 0, 255, 0);
  }
}

}  // namespace

bool load_image(const std::string& path, Pixels* out, std::string* error) {
  std::vector<uint8_t> bytes;
  if (!read_file(path, &bytes)) {
    *error = "cannot read " + path;
    return false;
  }
  int w = 0, h = 0, comp = 0;
  uint8_t* data = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &comp, 3);
  if (data == nullptr) {
    *error = "cannot decode " + path + ": " + stbi_failure_reason();
    return false;
  }
  std::vector<uint8_t> rgb(data, data + static_cast<size_t>(w) * static_cast<size_t>(h) * 3);
  stbi_image_free(data);
  const int orientation = jpeg_exif_orientation(bytes.data(), bytes.size());
  out->rgb = apply_orientation(rgb, &w, &h, 3, orientation);
  out->width = w;
  out->height = h;
  return true;
}

bool save_image(const std::string& path, const lipi_image_view& v, std::string* error) {
  const std::string ext = lower_extension(path);
  const int channels = lipi_pixel_format_channels(v.format);
  int ok = 0;
  if (ext == "png") ok = stbi_write_png(path.c_str(), v.width, v.height, channels, v.data, v.stride);
  else if (ext == "jpg" || ext == "jpeg") {
    if (v.stride != v.width * channels) {
      *error = "jpeg output needs tightly packed pixels";
      return false;
    }
    ok = stbi_write_jpg(path.c_str(), v.width, v.height, channels, v.data, 92);
  } else if (ext == "bmp") {
    ok = v.stride == v.width * channels ? stbi_write_bmp(path.c_str(), v.width, v.height, channels, v.data) : 0;
  } else {
    *error = "unsupported output extension '." + ext + "' (use .png, .jpg or .bmp)";
    return false;
  }
  if (!ok) *error = "cannot write " + path;
  return ok != 0;
}

namespace {
void append_bytes(void* context, void* data, int size) {
  auto* out = static_cast<std::vector<uint8_t>*>(context);
  const auto* p = static_cast<const uint8_t*>(data);
  out->insert(out->end(), p, p + size);
}
}  // namespace

bool encode_jpeg(const lipi_image_view& v, int quality, std::vector<uint8_t>* out) {
  const int channels = lipi_pixel_format_channels(v.format);
  if (channels == 0 || v.stride != v.width * channels) return false;
  out->clear();
  return stbi_write_jpg_to_func(append_bytes, out, v.width, v.height, channels, v.data, quality) != 0;
}

void draw_quad(Pixels* img, const lipi_quad& q) {
  const int t = std::max(1, std::max(img->width, img->height) / 600);
  for (int i = 0; i < 4; ++i) {
    const lipi_point& a = q.p[i];
    const lipi_point& b = q.p[(i + 1) % 4];
    line(img, static_cast<double>(a.x), static_cast<double>(a.y), static_cast<double>(b.x), static_cast<double>(b.y), t);
  }
  for (int dy = -3 * t; dy <= 3 * t; ++dy)
    for (int dx = -3 * t; dx <= 3 * t; ++dx) put(img, static_cast<int>(q.p[0].x) + dx, static_cast<int>(q.p[0].y) + dy, 255, 0, 0);
}

}  // namespace lipi_cli
