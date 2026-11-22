#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "doctest.h"
#include "jpeg_fixtures.hpp"
#include "lipi/lipi.h"

namespace {

size_t append_to_string(const void* data, size_t size, void* user) {
  static_cast<std::string*>(user)->append(static_cast<const char*>(data), size);
  return size;
}

size_t always_fail(const void*, size_t, void*) { return 0; }

struct Sink {
  std::string bytes;
};

std::string make_pdf(int pages_gray, int pages_rgb, float dpi = 72.0f) {
  Sink s;
  lipi_pdf* pdf = nullptr;
  REQUIRE(lipi_pdf_create(append_to_string, &s.bytes, &pdf) == LIPI_OK);
  for (int i = 0; i < pages_gray; ++i) REQUIRE(lipi_pdf_add_page_jpeg(pdf, kGrayJpeg, sizeof kGrayJpeg, dpi) == LIPI_OK);
  for (int i = 0; i < pages_rgb; ++i) REQUIRE(lipi_pdf_add_page_jpeg(pdf, kRgbJpeg, sizeof kRgbJpeg, dpi) == LIPI_OK);
  REQUIRE(lipi_pdf_finish(pdf) == LIPI_OK);
  lipi_pdf_free(pdf);
  return s.bytes;
}

// Reads the cross-reference table and checks that every offset points at "N 0 obj".
void check_structure(const std::string& pdf, int expected_pages) {
  REQUIRE(pdf.compare(0, 8, "%PDF-1.4") == 0);
  REQUIRE(pdf.size() > 6);
  CHECK(pdf.compare(pdf.size() - 6, 6, "%%EOF\n") == 0);
  const size_t sx = pdf.rfind("startxref\n");
  REQUIRE(sx != std::string::npos);
  const size_t xref = static_cast<size_t>(std::stoull(pdf.substr(sx + 10)));
  REQUIRE(pdf.compare(xref, 5, "xref\n") == 0);
  size_t pos = xref + 5;
  const size_t space = pdf.find(' ', pos);
  const size_t count = static_cast<size_t>(std::stoull(pdf.substr(space + 1)));
  pos = pdf.find('\n', pos) + 1;
  CHECK(pdf.compare(pos, 20, "0000000000 65535 f \n") == 0);
  pos += 20;
  for (size_t i = 1; i < count; ++i, pos += 20) {
    REQUIRE(pdf.compare(pos + 10, 10, " 00000 n \n") == 0);
    const size_t off = static_cast<size_t>(std::stoull(pdf.substr(pos, 10)));
    const std::string head = std::to_string(i) + " 0 obj\n";
    CHECK(pdf.compare(off, head.size(), head) == 0);
  }
  CHECK(pdf.find("/Count " + std::to_string(expected_pages) + " ") != std::string::npos);
  CHECK(pdf.find("/Size " + std::to_string(count)) != std::string::npos);
}

int run_quiet(const std::string& cmd) { return std::system((cmd + " >/dev/null 2>&1").c_str()); }

bool tool_exists(const char* name) { return run_quiet(std::string("command -v ") + name) == 0; }

}  // namespace

TEST_CASE("a one-page pdf has a valid structure and the right page size") {
  const std::string pdf = make_pdf(1, 0, 72.0f);
  check_structure(pdf, 1);
  CHECK(pdf.find("/MediaBox [0 0 24.00 16.00]") != std::string::npos);
  CHECK(pdf.find("/DCTDecode") != std::string::npos);
  CHECK(pdf.find("/DeviceGray") != std::string::npos);
}

TEST_CASE("the page size follows the dpi") {
  const std::string pdf = make_pdf(0, 1, 144.0f);
  CHECK(pdf.find("/MediaBox [0 0 16.00 10.00]") != std::string::npos);
  CHECK(pdf.find("/DeviceRGB") != std::string::npos);
}

TEST_CASE("a multi-page pdf lists every page") {
  const std::string pdf = make_pdf(2, 3);
  check_structure(pdf, 5);
}

TEST_CASE("the jpeg bytes are embedded unchanged") {
  const std::string pdf = make_pdf(1, 0);
  const std::string jpeg(reinterpret_cast<const char*>(kGrayJpeg), sizeof kGrayJpeg);
  CHECK(pdf.find(jpeg) != std::string::npos);
}

TEST_CASE("output is deterministic") { CHECK(make_pdf(2, 1) == make_pdf(2, 1)); }

TEST_CASE("a raw image page is written with a flate stream") {
  std::string out;
  lipi_pdf* pdf = nullptr;
  REQUIRE(lipi_pdf_create(append_to_string, &out, &pdf) == LIPI_OK);
  std::vector<uint8_t> px(3 * 2 * 4, 200);  // RGBA, alpha is dropped
  const lipi_image_view v{px.data(), 3, 2, 12, LIPI_PIXEL_RGBA8};
  REQUIRE(lipi_pdf_add_page_image(pdf, &v, 100.0f) == LIPI_OK);
  REQUIRE(lipi_pdf_finish(pdf) == LIPI_OK);
  lipi_pdf_free(pdf);
  check_structure(out, 1);
  CHECK(out.find("/FlateDecode") != std::string::npos);
  CHECK(out.find("/DeviceRGB") != std::string::npos);
}

TEST_CASE("bad arguments and states are rejected") {
  std::string out;
  lipi_pdf* pdf = nullptr;
  CHECK(lipi_pdf_create(nullptr, &out, &pdf) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(lipi_pdf_create(append_to_string, &out, nullptr) == LIPI_ERR_INVALID_ARGUMENT);
  REQUIRE(lipi_pdf_create(append_to_string, &out, &pdf) == LIPI_OK);
  CHECK(lipi_pdf_finish(pdf) == LIPI_ERR_INVALID_ARGUMENT);  // no pages yet
  const unsigned char junk[] = {1, 2, 3, 4, 5, 6};
  CHECK(lipi_pdf_add_page_jpeg(pdf, junk, sizeof junk, 72.0f) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(lipi_pdf_add_page_jpeg(pdf, kGrayJpeg, sizeof kGrayJpeg, 0.0f) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(lipi_pdf_add_page_jpeg(pdf, kGrayJpeg, sizeof kGrayJpeg, 1e9f) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(lipi_pdf_add_page_jpeg(pdf, kGrayJpeg, 10, 72.0f) == LIPI_ERR_INVALID_ARGUMENT);  // truncated
  // A four-component (CMYK) JPEG header is recognised but not supported.
  const unsigned char cmyk[] = {0xFF, 0xD8, 0xFF, 0xC0, 0x00, 0x14, 0x08, 0x00, 0x08, 0x00, 0x08, 0x04,
                                1, 0x11, 0, 2, 0x11, 0, 3, 0x11, 0, 4, 0x11, 0};
  CHECK(lipi_pdf_add_page_jpeg(pdf, cmyk, sizeof cmyk, 72.0f) == LIPI_ERR_UNSUPPORTED);
  REQUIRE(lipi_pdf_add_page_jpeg(pdf, kGrayJpeg, sizeof kGrayJpeg, 72.0f) == LIPI_OK);
  REQUIRE(lipi_pdf_finish(pdf) == LIPI_OK);
  CHECK(lipi_pdf_finish(pdf) == LIPI_ERR_INVALID_ARGUMENT);  // twice
  CHECK(lipi_pdf_add_page_jpeg(pdf, kGrayJpeg, sizeof kGrayJpeg, 72.0f) == LIPI_ERR_INVALID_ARGUMENT);  // after finish
  lipi_pdf_free(pdf);
  lipi_pdf_free(nullptr);
}

TEST_CASE("a failing write callback is reported") {
  lipi_pdf* pdf = nullptr;
  CHECK(lipi_pdf_create(always_fail, nullptr, &pdf) == LIPI_ERR_IO);
  CHECK(pdf == nullptr);
}

TEST_CASE("external tools accept the file") {
  const std::string pdf = make_pdf(2, 2, 150.0f);
  const std::string path = "lipi_test_output.pdf";
  FILE* f = std::fopen(path.c_str(), "wb");
  REQUIRE(f != nullptr);
  std::fwrite(pdf.data(), 1, pdf.size(), f);
  std::fclose(f);

  struct Tool {
    const char* exe;
    std::string cmd;
  };
  const Tool tools[] = {
      {"qpdf", "qpdf --check " + path},
      {"pdfinfo", "pdfinfo " + path},
      {"mutool", "mutool info " + path},
      {"gs", "gs -q -dNODISPLAY -dNOSAFER -dBATCH -dNOPAUSE -c \"(" + path + ") (r) file runpdfbegin pdfpagecount = quit\""},
  };
  int ran = 0;
  for (const Tool& t : tools) {
    if (!tool_exists(t.exe)) continue;
    ++ran;
    INFO("tool: " << t.exe);
    CHECK(run_quiet(t.cmd) == 0);
  }
  std::remove(path.c_str());
  if (std::getenv("LIPI_REQUIRE_PDF_TOOLS") != nullptr) {
    CHECK_MESSAGE(ran >= 2, "install two PDF tools (qpdf, poppler-utils, mupdf-tools or ghostscript)");
  }
}
