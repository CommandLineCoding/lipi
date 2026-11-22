// lipi-cli: run the Lipi core on image files from the command line.
#include <sys/stat.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "imageio.hpp"
#include "lipi/lipi.h"

namespace {

enum Exit { kOk = 0, kUsage = 1, kIo = 2, kNoPage = 3, kInternal = 4 };

const char* kUsageText =
    "Usage:\n"
    "  lipi-cli detect <image> [--json]\n"
    "      Find the page outline. Prints the four corners (top-left, top-right,\n"
    "      bottom-right, bottom-left, in pixels) and a confidence.\n"
    "  lipi-cli scan <image> -o <output> [--quad x0,y0,x1,y1,x2,y2,x3,y3]\n"
    "                [--max-side N] [--debug-dir DIR]\n"
    "      Flatten the page to an upright image. Output is .png, .jpg or .bmp.\n"
    "      Without --quad the page is detected first. --max-side caps the longer\n"
    "      side of the result (default 2400). --debug-dir saves the input with the\n"
    "      quad drawn on it and the flattened page.\n"
    "  lipi-cli --version | --help\n"
    "\n"
    "Exit codes: 0 success, 1 usage error, 2 file could not be read or written,\n"
    "3 no page found, 4 internal error.\n";

int usage_error(const std::string& msg) {
  std::fprintf(stderr, "lipi-cli: %s\n\n%s", msg.c_str(), kUsageText);
  return kUsage;
}

int status_exit(lipi_status s, const char* what) {
  std::fprintf(stderr, "lipi-cli: %s: %s\n", what, lipi_status_string(s));
  switch (s) {
    case LIPI_OK: return kOk;
    case LIPI_ERR_NOT_FOUND: return kNoPage;
    case LIPI_ERR_INVALID_ARGUMENT: return kUsage;
    default: return kInternal;
  }
}

void print_json(const lipi_quad* q, float conf, double ms) {
  if (q == nullptr) {
    std::printf("{\"quad\":null,\"confidence\":0,\"ms\":%.2f}\n", ms);
    return;
  }
  std::printf("{\"quad\":[");
  for (int i = 0; i < 4; ++i) std::printf("%s[%.2f,%.2f]", i ? "," : "", static_cast<double>(q->p[i].x), static_cast<double>(q->p[i].y));
  std::printf("],\"confidence\":%.3f,\"ms\":%.2f}\n", static_cast<double>(conf), ms);
}

bool parse_quad(const std::string& text, lipi_quad* q) {
  float v[8];
  const char* p = text.c_str();
  for (int i = 0; i < 8; ++i) {
    char* end = nullptr;
    v[i] = std::strtof(p, &end);
    if (end == p) return false;
    p = end;
    if (i < 7) {
      if (*p != ',') return false;
      ++p;
    }
  }
  if (*p != '\0') return false;
  for (int i = 0; i < 4; ++i) q->p[i] = {v[2 * i], v[2 * i + 1]};
  return true;
}

int cmd_detect(const std::vector<std::string>& args) {
  std::string path;
  bool json = false;
  for (const std::string& a : args) {
    if (a == "--json") json = true;
    else if (!a.empty() && a[0] == '-') return usage_error("unknown option " + a);
    else if (path.empty()) path = a;
    else return usage_error("more than one input image");
  }
  if (path.empty()) return usage_error("detect needs an input image");
  lipi_cli::Pixels img;
  std::string err;
  if (!lipi_cli::load_image(path, &img, &err)) {
    std::fprintf(stderr, "lipi-cli: %s\n", err.c_str());
    return kIo;
  }
  const lipi_image_view view = img.view();
  lipi_quad q{};
  float conf = 0;
  const auto t0 = std::chrono::steady_clock::now();
  const lipi_status s = lipi_detect_page_classical(&view, &q, &conf);
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  if (s == LIPI_ERR_NOT_FOUND) {
    if (json) print_json(nullptr, 0, ms);
    else std::fprintf(stderr, "lipi-cli: no page found\n");
    return kNoPage;
  }
  if (s != LIPI_OK) return status_exit(s, "detect");
  if (json) {
    print_json(&q, conf, ms);
  } else {
    std::printf("image %dx%d, %.1f ms, confidence %.2f\n", img.width, img.height, ms, static_cast<double>(conf));
    const char* names[4] = {"top-left", "top-right", "bottom-right", "bottom-left"};
    for (int i = 0; i < 4; ++i) std::printf("%-13s %.1f, %.1f\n", names[i], static_cast<double>(q.p[i].x), static_cast<double>(q.p[i].y));
  }
  return kOk;
}

int cmd_scan(const std::vector<std::string>& args) {
  std::string in, out, quad_text, debug_dir;
  int max_side = 2400;
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& a = args[i];
    auto next = [&](std::string* dst) {
      if (i + 1 >= args.size()) return false;
      *dst = args[++i];
      return true;
    };
    if (a == "-o") { if (!next(&out)) return usage_error("-o needs a value"); }
    else if (a == "--quad") { if (!next(&quad_text)) return usage_error("--quad needs a value"); }
    else if (a == "--debug-dir") { if (!next(&debug_dir)) return usage_error("--debug-dir needs a value"); }
    else if (a == "--max-side") {
      std::string v;
      if (!next(&v)) return usage_error("--max-side needs a value");
      max_side = std::atoi(v.c_str());
      if (max_side < 16 || max_side > 16384) return usage_error("--max-side must be between 16 and 16384");
    } else if (!a.empty() && a[0] == '-') return usage_error("unknown option " + a);
    else if (in.empty()) in = a;
    else return usage_error("more than one input image");
  }
  if (in.empty()) return usage_error("scan needs an input image");
  if (out.empty()) return usage_error("scan needs -o <output>");

  lipi_cli::Pixels img;
  std::string err;
  if (!lipi_cli::load_image(in, &img, &err)) {
    std::fprintf(stderr, "lipi-cli: %s\n", err.c_str());
    return kIo;
  }
  const lipi_image_view view = img.view();
  lipi_quad q{};
  if (!quad_text.empty()) {
    if (!parse_quad(quad_text, &q)) return usage_error("--quad needs eight comma-separated numbers");
    const lipi_status s = lipi_quad_validate(&q);
    if (s != LIPI_OK) {
      std::fprintf(stderr, "lipi-cli: --quad is not a valid page outline (corners must be convex, clockwise from the top-left)\n");
      return kUsage;
    }
  } else {
    const lipi_status s = lipi_detect_page_classical(&view, &q, nullptr);
    if (s != LIPI_OK) return status_exit(s, "no page found");
  }
  int pw = 0, ph = 0;
  lipi_status s = lipi_estimate_page_size(&q, img.width, img.height, max_side, &pw, &ph);
  if (s != LIPI_OK) return status_exit(s, "page size");
  lipi_image* page = nullptr;
  s = lipi_warp_quad(&view, &q, pw, ph, &page);
  if (s != LIPI_OK) return status_exit(s, "warp");

  int code = kOk;
  const lipi_image_view pv = lipi_image_view_of(page);
  if (!lipi_cli::save_image(out, pv, &err)) {
    std::fprintf(stderr, "lipi-cli: %s\n", err.c_str());
    code = kIo;
  }
  if (code == kOk && !debug_dir.empty()) {
    mkdir(debug_dir.c_str(), 0777);
    lipi_cli::Pixels overlay = img;
    lipi_cli::draw_quad(&overlay, q);
    if (!lipi_cli::save_image(debug_dir + "/01-input-quad.png", overlay.view(), &err) ||
        !lipi_cli::save_image(debug_dir + "/02-flattened.png", pv, &err)) {
      std::fprintf(stderr, "lipi-cli: %s\n", err.c_str());
      code = kIo;
    }
  }
  lipi_image_free(page);
  if (code == kOk) std::printf("wrote %s (%dx%d)\n", out.c_str(), pw, ph);
  return code;
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> args(argv + 1, argv + argc);
  if (args.empty()) return usage_error("no command given");
  const std::string cmd = args[0];
  args.erase(args.begin());
  if (cmd == "--help" || cmd == "-h") {
    std::fputs(kUsageText, stdout);
    return kOk;
  }
  if (cmd == "--version") {
    std::printf("lipi-cli %s\n", lipi_version());
    return kOk;
  }
  if (cmd == "detect") return cmd_detect(args);
  if (cmd == "scan") return cmd_scan(args);
  return usage_error("unknown command '" + cmd + "'");
}
