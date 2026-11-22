// Minimal PDF writer: one image per page, streamed to a callback.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "lipi/lipi.h"

struct lipi_pdf {
  lipi_write_fn write = nullptr;
  void* user = nullptr;
  size_t offset = 0;
  bool failed = false;
  bool finished = false;
  std::vector<size_t> object_offsets{0, 0, 0};  // index = object number; 1 = catalog, 2 = page tree
  std::vector<int> pages;                        // object numbers of the page objects
};

namespace {

constexpr double kMinDpi = 10.0, kMaxDpi = 2400.0;

void emit(lipi_pdf* pdf, const void* data, size_t size) {
  if (pdf->failed || size == 0) return;
  if (pdf->write(data, size, pdf->user) != size) {
    pdf->failed = true;
    return;
  }
  pdf->offset += size;
}

void emit(lipi_pdf* pdf, const std::string& s) { emit(pdf, s.data(), s.size()); }

// Locale-independent fixed-point formatting with two decimals.
std::string fixed2(double v) {
  const long long scaled = std::llround(v * 100.0);
  const long long a = scaled < 0 ? -scaled : scaled;
  char buf[48];
  std::snprintf(buf, sizeof buf, "%s%lld.%02lld", scaled < 0 ? "-" : "", a / 100, a % 100);
  return buf;
}

int begin_object(lipi_pdf* pdf) {
  const int id = static_cast<int>(pdf->object_offsets.size());
  pdf->object_offsets.push_back(pdf->offset);
  emit(pdf, std::to_string(id) + " 0 obj\n");
  return id;
}

void begin_object_numbered(lipi_pdf* pdf, int id) {
  pdf->object_offsets[static_cast<size_t>(id)] = pdf->offset;
  emit(pdf, std::to_string(id) + " 0 obj\n");
}

struct JpegInfo {
  int width = 0, height = 0, components = 0;
};

bool parse_jpeg(const uint8_t* d, size_t n, JpegInfo* info) {
  if (n < 4 || d[0] != 0xFF || d[1] != 0xD8) return false;
  size_t pos = 2;
  while (pos + 4 <= n) {
    if (d[pos] != 0xFF) return false;
    const uint8_t marker = d[pos + 1];
    if (marker == 0xFF) {
      ++pos;
      continue;
    }
    if (marker == 0xD8 || (marker >= 0xD0 && marker <= 0xD7) || marker == 0x01) {
      pos += 2;
      continue;
    }
    if (marker == 0xD9 || marker == 0xDA) return false;  // reached the data without a frame header
    const size_t len = static_cast<size_t>((d[pos + 2] << 8) | d[pos + 3]);
    if (len < 2 || pos + 2 + len > n) return false;
    if (marker == 0xC0 || marker == 0xC1 || marker == 0xC2) {
      if (len < 8 || d[pos + 4] != 8) return false;  // 8 bits per sample only
      info->height = (d[pos + 5] << 8) | d[pos + 6];
      info->width = (d[pos + 7] << 8) | d[pos + 8];
      info->components = d[pos + 9];
      return info->width > 0 && info->height > 0;
    }
    pos += 2 + len;
  }
  return false;
}

uint32_t adler32(const uint8_t* d, size_t n) {
  uint32_t a = 1, b = 0;
  for (size_t i = 0; i < n; ++i) {
    a = (a + d[i]) % 65521u;
    b = (b + a) % 65521u;
  }
  return (b << 16) | a;
}

// zlib stream made of stored (uncompressed) blocks: valid, but not smaller.
std::vector<uint8_t> zlib_stored(const std::vector<uint8_t>& raw) {
  std::vector<uint8_t> out{0x78, 0x01};
  size_t pos = 0;
  do {
    const size_t chunk = std::min<size_t>(65535, raw.size() - pos);
    const bool last = pos + chunk >= raw.size();
    out.push_back(last ? 1 : 0);
    out.push_back(static_cast<uint8_t>(chunk & 0xFF));
    out.push_back(static_cast<uint8_t>(chunk >> 8));
    out.push_back(static_cast<uint8_t>(~chunk & 0xFF));
    out.push_back(static_cast<uint8_t>((~chunk >> 8) & 0xFF));
    out.insert(out.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos), raw.begin() + static_cast<std::ptrdiff_t>(pos + chunk));
    pos += chunk;
  } while (pos < raw.size());
  const uint32_t ad = adler32(raw.data(), raw.size());
  for (int s = 24; s >= 0; s -= 8) out.push_back(static_cast<uint8_t>(ad >> s));
  return out;
}

lipi_status write_page(lipi_pdf* pdf, int width, int height, const char* color_space, const char* filter,
                       const uint8_t* data, size_t size, double dpi) {
  const double w_pt = width * 72.0 / dpi, h_pt = height * 72.0 / dpi;
  const int image = begin_object(pdf);
  emit(pdf, "<< /Type /XObject /Subtype /Image /Width " + std::to_string(width) + " /Height " + std::to_string(height) +
                " /ColorSpace /" + color_space + " /BitsPerComponent 8 /Filter /" + filter + " /Length " +
                std::to_string(size) + " >>\nstream\n");
  emit(pdf, data, size);
  emit(pdf, "\nendstream\nendobj\n");

  const std::string content = "q " + fixed2(w_pt) + " 0 0 " + fixed2(h_pt) + " 0 0 cm /Im0 Do Q\n";
  const int content_id = begin_object(pdf);
  emit(pdf, "<< /Length " + std::to_string(content.size()) + " >>\nstream\n" + content + "endstream\nendobj\n");

  const int page = begin_object(pdf);
  emit(pdf, "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " + fixed2(w_pt) + " " + fixed2(h_pt) +
                "] /Resources << /XObject << /Im0 " + std::to_string(image) + " 0 R >> >> /Contents " +
                std::to_string(content_id) + " 0 R >>\nendobj\n");
  pdf->pages.push_back(page);
  return pdf->failed ? LIPI_ERR_IO : LIPI_OK;
}

bool valid_dpi(float dpi) { return std::isfinite(dpi) && dpi >= kMinDpi && dpi <= kMaxDpi; }

}  // namespace

extern "C" {

lipi_status lipi_pdf_create(lipi_write_fn write, void* user, lipi_pdf** out) {
  if (out == nullptr) return LIPI_ERR_INVALID_ARGUMENT;
  *out = nullptr;
  if (write == nullptr) return LIPI_ERR_INVALID_ARGUMENT;
  try {
    auto* pdf = new lipi_pdf();
    pdf->write = write;
    pdf->user = user;
    emit(pdf, std::string("%PDF-1.4\n%\xE2\xE3\xCF\xD3\n"));
    if (pdf->failed) {
      delete pdf;
      return LIPI_ERR_IO;
    }
    *out = pdf;
    return LIPI_OK;
  } catch (const std::bad_alloc&) {
    return LIPI_ERR_OUT_OF_MEMORY;
  } catch (...) {
    return LIPI_ERR_INTERNAL;
  }
}

lipi_status lipi_pdf_add_page_jpeg(lipi_pdf* pdf, const uint8_t* jpeg, size_t size, float dpi) {
  if (pdf == nullptr || jpeg == nullptr || !valid_dpi(dpi)) return LIPI_ERR_INVALID_ARGUMENT;
  if (pdf->finished) return LIPI_ERR_INVALID_ARGUMENT;
  if (pdf->failed) return LIPI_ERR_IO;
  JpegInfo info;
  if (!parse_jpeg(jpeg, size, &info)) return LIPI_ERR_INVALID_ARGUMENT;
  if (info.components != 1 && info.components != 3) return LIPI_ERR_UNSUPPORTED;
  try {
    return write_page(pdf, info.width, info.height, info.components == 1 ? "DeviceGray" : "DeviceRGB", "DCTDecode",
                      jpeg, size, static_cast<double>(dpi));
  } catch (const std::bad_alloc&) {
    return LIPI_ERR_OUT_OF_MEMORY;
  } catch (...) {
    return LIPI_ERR_INTERNAL;
  }
}

lipi_status lipi_pdf_add_page_image(lipi_pdf* pdf, const lipi_image_view* image, float dpi) {
  if (pdf == nullptr || image == nullptr || image->data == nullptr || !valid_dpi(dpi)) return LIPI_ERR_INVALID_ARGUMENT;
  if (pdf->finished) return LIPI_ERR_INVALID_ARGUMENT;
  if (pdf->failed) return LIPI_ERR_IO;
  const int channels = lipi_pixel_format_channels(image->format);
  if (channels == 0 || image->width <= 0 || image->height <= 0 || image->stride < image->width * channels) {
    return LIPI_ERR_INVALID_ARGUMENT;
  }
  try {
    const int out_channels = channels == 1 ? 1 : 3;
    std::vector<uint8_t> raw(static_cast<size_t>(image->width) * static_cast<size_t>(image->height) * static_cast<size_t>(out_channels));
    for (int y = 0; y < image->height; ++y) {
      const uint8_t* row = image->data + static_cast<size_t>(y) * static_cast<size_t>(image->stride);
      uint8_t* dst = raw.data() + static_cast<size_t>(y) * static_cast<size_t>(image->width) * static_cast<size_t>(out_channels);
      for (int x = 0; x < image->width; ++x)
        for (int c = 0; c < out_channels; ++c) dst[x * out_channels + c] = row[x * channels + c];
    }
    const std::vector<uint8_t> z = zlib_stored(raw);
    return write_page(pdf, image->width, image->height, out_channels == 1 ? "DeviceGray" : "DeviceRGB", "FlateDecode",
                      z.data(), z.size(), static_cast<double>(dpi));
  } catch (const std::bad_alloc&) {
    return LIPI_ERR_OUT_OF_MEMORY;
  } catch (...) {
    return LIPI_ERR_INTERNAL;
  }
}

lipi_status lipi_pdf_finish(lipi_pdf* pdf) {
  if (pdf == nullptr || pdf->finished || pdf->pages.empty()) return LIPI_ERR_INVALID_ARGUMENT;
  if (pdf->failed) return LIPI_ERR_IO;
  try {
    const int info = begin_object(pdf);
    emit(pdf, "<< /Producer (Lipi " + std::string(lipi_version()) + ") >>\nendobj\n");

    begin_object_numbered(pdf, 2);
    std::string kids;
    for (const int p : pdf->pages) kids += std::to_string(p) + " 0 R ";
    emit(pdf, "<< /Type /Pages /Kids [" + kids + "] /Count " + std::to_string(pdf->pages.size()) + " >>\nendobj\n");

    begin_object_numbered(pdf, 1);
    emit(pdf, "<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");

    const size_t xref = pdf->offset;
    const size_t count = pdf->object_offsets.size();
    std::string table = "xref\n0 " + std::to_string(count) + "\n0000000000 65535 f \n";
    for (size_t i = 1; i < count; ++i) {
      char line[32];
      std::snprintf(line, sizeof line, "%010zu 00000 n \n", pdf->object_offsets[i]);
      table += line;
    }
    emit(pdf, table);
    emit(pdf, "trailer\n<< /Size " + std::to_string(count) + " /Root 1 0 R /Info " + std::to_string(info) +
                  " 0 R >>\nstartxref\n" + std::to_string(xref) + "\n%%EOF\n");
    pdf->finished = true;
    return pdf->failed ? LIPI_ERR_IO : LIPI_OK;
  } catch (const std::bad_alloc&) {
    return LIPI_ERR_OUT_OF_MEMORY;
  } catch (...) {
    return LIPI_ERR_INTERNAL;
  }
}

void lipi_pdf_free(lipi_pdf* pdf) { delete pdf; }

}  // extern "C"
