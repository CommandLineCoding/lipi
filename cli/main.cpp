// lipi-cli: run the Lipi core on image files from the command line.
#include <sys/stat.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "exif.hpp"
#include "imageio.hpp"
#include "lipi/lipi.h"

namespace {

enum Exit { kOk = 0, kUsage = 1, kIo = 2, kNoPage = 3, kInternal = 4 };

const char* kUsageText =
    "Usage:\n"
    "  lipi-cli detect <image> [--model PREFIX] [--json]\n"
    "      Find the page outline. Prints the four corners (top-left, top-right,\n"
    "      bottom-right, bottom-left, in pixels) and a confidence.\n"
    "  lipi-cli scan <image> -o <output> [--model PREFIX] [--quad x0,y0,x1,y1,x2,y2,x3,y3]\n"
    "                [--filter auto|bw|gray|color|none] [--brightness B] [--contrast C]\n"
    "                [--no-shadow-removal] [--no-rotate] [--max-side N] [--debug-dir DIR]\n"
    "      Flatten the page, clean it up and save it upright (.png, .jpg or .bmp).\n"
    "      Without --quad the page is detected first. --filter picks the look (default\n"
    "      auto); brightness and contrast are between -1 and 1. --max-side caps the\n"
    "      longer side of the result (default 2400). --debug-dir saves the input with\n"
    "      the outline drawn on it, the flattened page and the final result.\n"
    "  lipi-cli --version | --help\n"
    "\n"
    "--model PREFIX uses the learned detector from PREFIX.ncnn.param and PREFIX.ncnn.bin\n"
    "(needs a build with LIPI_WITH_NCNN=ON). The classical detector is the fallback when\n"
    "the model finds no page. With a model, scan also turns the page upright.\n"
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

bool read_file(const std::string& path, std::vector<char>* out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  out->assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return !out->empty();
}

// Owns an optional learned detector.
struct Model {
  lipi_detector* detector = nullptr;
  ~Model() { lipi_detector_free(detector); }

  // Returns 0 on success (or when no prefix was given), otherwise a process exit code.
  int load(const std::string& prefix) {
    if (prefix.empty()) return kOk;
    if (!lipi_detector_available()) {
      std::fprintf(stderr, "lipi-cli: --model needs a build with LIPI_WITH_NCNN=ON\n");
      return kUsage;
    }
    std::vector<char> param, weights;
    if (!read_file(prefix + ".ncnn.param", &param) || !read_file(prefix + ".ncnn.bin", &weights)) {
      std::fprintf(stderr, "lipi-cli: cannot read %s.ncnn.param and %s.ncnn.bin\n", prefix.c_str(), prefix.c_str());
      return kIo;
    }
    const lipi_status s = lipi_detector_create(param.data(), param.size(), weights.data(), weights.size(), 1, &detector);
    if (s != LIPI_OK) {
      std::fprintf(stderr, "lipi-cli: cannot load the model: %s\n", lipi_status_string(s));
      return kIo;
    }
    return kOk;
  }
};

struct Found {
  lipi_quad quad{};
  float confidence = 0;
  int orientation = -1;  // -1 when the detector does not know
  bool from_model = false;
};

// Learned detector first (corners then refined on the full photo), classical detector as the fallback.
lipi_status find_page(const lipi_image_view& view, const Model& model, Found* out) {
  if (model.detector != nullptr) {
    lipi_detection det{};
    if (lipi_detector_detect(model.detector, &view, &det) == LIPI_OK) {
      lipi_quad refined{};
      out->quad = lipi_refine_quad(&view, &det.quad, &refined) == LIPI_OK ? refined : det.quad;
      out->confidence = det.confidence;
      out->orientation = det.orientation;
      out->from_model = true;
      return LIPI_OK;
    }
  }
  out->from_model = false;
  out->orientation = -1;
  return lipi_detect_page_classical(&view, &out->quad, &out->confidence);
}

void print_json(const Found* f, double ms) {
  if (f == nullptr) {
    std::printf("{\"quad\":null,\"confidence\":0,\"orientation\":null,\"source\":null,\"ms\":%.2f}\n", ms);
    return;
  }
  std::printf("{\"quad\":[");
  for (int i = 0; i < 4; ++i) std::printf("%s[%.2f,%.2f]", i ? "," : "", static_cast<double>(f->quad.p[i].x), static_cast<double>(f->quad.p[i].y));
  std::printf("],\"confidence\":%.4f,", static_cast<double>(f->confidence));
  if (f->orientation >= 0) std::printf("\"orientation\":%d,", f->orientation);
  else std::printf("\"orientation\":null,");
  std::printf("\"source\":\"%s\",\"ms\":%.2f}\n", f->from_model ? "model" : "classical", ms);
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

bool parse_float(const std::string& text, float* out) {
  char* end = nullptr;
  *out = std::strtof(text.c_str(), &end);
  return end != text.c_str() && *end == '\0';
}

bool parse_filter(const std::string& name, int* out) {
  if (name == "none") *out = LIPI_FILTER_NONE;
  else if (name == "auto") *out = LIPI_FILTER_AUTO;
  else if (name == "bw") *out = LIPI_FILTER_BW;
  else if (name == "gray" || name == "grey") *out = LIPI_FILTER_GRAY;
  else if (name == "color" || name == "colour") *out = LIPI_FILTER_COLOR;
  else return false;
  return true;
}

int cmd_detect(const std::vector<std::string>& args) {
  std::string path, model_prefix;
  bool json = false;
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& a = args[i];
    if (a == "--json") json = true;
    else if (a == "--model") {
      if (i + 1 >= args.size()) return usage_error("--model needs a value");
      model_prefix = args[++i];
    } else if (!a.empty() && a[0] == '-') return usage_error("unknown option " + a);
    else if (path.empty()) path = a;
    else return usage_error("more than one input image");
  }
  if (path.empty()) return usage_error("detect needs an input image");
  Model model;
  if (const int rc = model.load(model_prefix)) return rc;
  lipi_cli::Pixels img;
  std::string err;
  if (!lipi_cli::load_image(path, &img, &err)) {
    std::fprintf(stderr, "lipi-cli: %s\n", err.c_str());
    return kIo;
  }
  const lipi_image_view view = img.view();
  Found f;
  const auto t0 = std::chrono::steady_clock::now();
  const lipi_status s = find_page(view, model, &f);
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  if (s == LIPI_ERR_NOT_FOUND) {
    if (json) print_json(nullptr, ms);
    else std::fprintf(stderr, "lipi-cli: no page found\n");
    return kNoPage;
  }
  if (s != LIPI_OK) return status_exit(s, "detect");
  if (json) {
    print_json(&f, ms);
  } else {
    std::printf("image %dx%d, %.1f ms, %s detector, confidence %.2f\n", img.width, img.height, ms,
                f.from_model ? "learned" : "classical", static_cast<double>(f.confidence));
    const char* names[4] = {"top-left", "top-right", "bottom-right", "bottom-left"};
    for (int i = 0; i < 4; ++i) std::printf("%-13s %.1f, %.1f\n", names[i], static_cast<double>(f.quad.p[i].x), static_cast<double>(f.quad.p[i].y));
    if (f.orientation >= 0) std::printf("orientation   %d (quarter turns clockwise to upright)\n", f.orientation);
  }
  return kOk;
}

int cmd_scan(const std::vector<std::string>& args) {
  std::string in, out, quad_text, debug_dir, model_prefix, filter_name = "auto";
  int max_side = 2400;
  float brightness = 0, contrast = 0;
  bool shadows = true, rotate = true;
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& a = args[i];
    auto next = [&](std::string* dst) {
      if (i + 1 >= args.size()) return false;
      *dst = args[++i];
      return true;
    };
    std::string v;
    if (a == "-o") { if (!next(&out)) return usage_error("-o needs a value"); }
    else if (a == "--quad") { if (!next(&quad_text)) return usage_error("--quad needs a value"); }
    else if (a == "--debug-dir") { if (!next(&debug_dir)) return usage_error("--debug-dir needs a value"); }
    else if (a == "--model") { if (!next(&model_prefix)) return usage_error("--model needs a value"); }
    else if (a == "--filter") { if (!next(&filter_name)) return usage_error("--filter needs a value"); }
    else if (a == "--brightness") {
      if (!next(&v) || !parse_float(v, &brightness) || brightness < -1 || brightness > 1) return usage_error("--brightness needs a number between -1 and 1");
    } else if (a == "--contrast") {
      if (!next(&v) || !parse_float(v, &contrast) || contrast < -1 || contrast > 1) return usage_error("--contrast needs a number between -1 and 1");
    } else if (a == "--no-shadow-removal") shadows = false;
    else if (a == "--no-rotate") rotate = false;
    else if (a == "--max-side") {
      if (!next(&v)) return usage_error("--max-side needs a value");
      max_side = std::atoi(v.c_str());
      if (max_side < 16 || max_side > 16384) return usage_error("--max-side must be between 16 and 16384");
    } else if (!a.empty() && a[0] == '-') return usage_error("unknown option " + a);
    else if (in.empty()) in = a;
    else return usage_error("more than one input image");
  }
  if (in.empty()) return usage_error("scan needs an input image");
  if (out.empty()) return usage_error("scan needs -o <output>");
  int filter = 0;
  if (!parse_filter(filter_name, &filter)) return usage_error("--filter must be auto, bw, gray, color or none");

  Model model;
  if (const int rc = model.load(model_prefix)) return rc;
  lipi_cli::Pixels img;
  std::string err;
  if (!lipi_cli::load_image(in, &img, &err)) {
    std::fprintf(stderr, "lipi-cli: %s\n", err.c_str());
    return kIo;
  }
  const lipi_image_view view = img.view();
  Found f;
  if (!quad_text.empty()) {
    if (!parse_quad(quad_text, &f.quad)) return usage_error("--quad needs eight comma-separated numbers");
    if (lipi_quad_validate(&f.quad) != LIPI_OK) {
      std::fprintf(stderr, "lipi-cli: --quad is not a valid page outline (corners must be convex, clockwise from the top-left)\n");
      return kUsage;
    }
  } else {
    const lipi_status s = find_page(view, model, &f);
    if (s != LIPI_OK) return status_exit(s, "no page found");
  }
  int pw = 0, ph = 0;
  lipi_status s = lipi_estimate_page_size(&f.quad, img.width, img.height, max_side, &pw, &ph);
  if (s != LIPI_OK) return status_exit(s, "page size");
  lipi_image* page = nullptr;
  s = lipi_warp_quad(&view, &f.quad, pw, ph, &page);
  if (s != LIPI_OK) return status_exit(s, "warp");

  lipi_enhance_options opt = lipi_enhance_defaults();
  opt.filter = filter;
  opt.remove_shadows = shadows ? 1 : 0;
  opt.brightness = brightness;
  opt.contrast = contrast;
  const lipi_image_view warped = lipi_image_view_of(page);
  lipi_image* enhanced = nullptr;
  s = lipi_enhance(&warped, &opt, &enhanced);
  if (s != LIPI_OK) {
    lipi_image_free(page);
    return status_exit(s, "enhance");
  }

  // Turn the page upright when the detector knows how it is rotated.
  lipi_cli::Pixels turned;
  lipi_image_view result = lipi_image_view_of(enhanced);
  if (rotate && f.orientation > 0) {
    const int channels = lipi_pixel_format_channels(result.format);
    std::vector<uint8_t> pixels(result.data, result.data + static_cast<size_t>(result.width) * static_cast<size_t>(result.height) * static_cast<size_t>(channels));
    int w = result.width, h = result.height;
    const int exif_code = f.orientation == 1 ? 6 : (f.orientation == 2 ? 3 : 8);  // clockwise quarter turns as EXIF codes
    turned.rgb = lipi_cli::apply_orientation(pixels, &w, &h, channels, exif_code);
    turned.width = w;
    turned.height = h;
    result = {turned.rgb.data(), w, h, w * channels, result.format};
  }

  int code = kOk;
  if (!lipi_cli::save_image(out, result, &err)) {
    std::fprintf(stderr, "lipi-cli: %s\n", err.c_str());
    code = kIo;
  }
  if (code == kOk && !debug_dir.empty()) {
    mkdir(debug_dir.c_str(), 0777);
    lipi_cli::Pixels overlay = img;
    lipi_cli::draw_quad(&overlay, f.quad);
    if (!lipi_cli::save_image(debug_dir + "/01-input-quad.png", overlay.view(), &err) ||
        !lipi_cli::save_image(debug_dir + "/02-flattened.png", warped, &err) ||
        !lipi_cli::save_image(debug_dir + "/03-result.png", result, &err)) {
      std::fprintf(stderr, "lipi-cli: %s\n", err.c_str());
      code = kIo;
    }
  }
  lipi_image_free(page);
  lipi_image_free(enhanced);
  if (code == kOk) std::printf("wrote %s (%dx%d)\n", out.c_str(), result.width, result.height);
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
