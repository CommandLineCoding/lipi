// lipi-cli: run the Lipi core on image files from the command line.
#include <sys/stat.h>

#include <algorithm>
#include <chrono>
#include <cmath>
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
    "  lipi-cli detect <image> [--model PREFIX] [--refine] [--json]\n"
    "      Find the page outline. Prints the four corners (top-left, top-right,\n"
    "      bottom-right, bottom-left, in pixels) and a confidence.\n"
    "  lipi-cli scan <image> -o <output> [page options] [--quad x0,y0,x1,y1,x2,y2,x3,y3]\n"
    "                [--debug-dir DIR]\n"
    "      Flatten the page, clean it up and save it upright (.png, .jpg or .bmp).\n"
    "      Without --quad the page is detected first. --debug-dir saves the input with\n"
    "      the outline drawn on it, the flattened page and the final result.\n"
    "  lipi-cli pdf <image>... -o <output.pdf> [page options] [--quality Q] [--dpi D]\n"
    "      Scan each image and write the pages to one PDF (JPEG pages, quality 1-100,\n"
    "      default 80; the page size follows --dpi, default 150).\n"
    "  lipi-cli bench <image> [page options] [--repeat N] [--json]\n"
    "      Time each stage of the pipeline on one image (median and 95th percentile\n"
    "      over N runs after a warm-up, default 30), on this machine's CPU.\n"
    "  lipi-cli --version | --help\n"
    "\n"
    "Page options (detect, scan, pdf, bench):\n"
    "  --model PREFIX         use the learned detector from PREFIX.ncnn.param and PREFIX.ncnn.bin\n"
    "                         (needs a build with LIPI_WITH_NCNN=ON); the classical detector is the\n"
    "                         fallback, and the page is turned upright using the model's orientation\n"
    "  --filter auto|bw|gray|color|none   the look of the result (default auto)\n"
    "  --brightness B, --contrast C       between -1 and 1 (default 0)\n"
    "  --no-shadow-removal    keep the original lighting\n"
    "  --no-deskew            do not straighten tilted text lines\n"
    "  --no-rotate            do not turn the page upright\n"
    "  --refine               nudge the model's corners onto the nearest page edges (off by default:\n"
    "                         it did not improve results on real photos; moves over 2% of the image\n"
    "                         diagonal are ignored)\n"
    "  --max-side N           cap the longer side of the page (default 2400)\n"
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

double now_ms() {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
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

struct Refine {
  bool enabled = false;  // measured: on real photos it does not beat the model's own corners
  float limit = 0.02f;   // largest corner move, as a fraction of the image diagonal, that is accepted
};

// Learned detector first, classical detector as the fallback.
lipi_status find_page(const lipi_image_view& view, const Model& model, const Refine& refine, Found* out) {
  if (model.detector != nullptr) {
    lipi_detection det{};
    if (lipi_detector_detect(model.detector, &view, &det) == LIPI_OK) {
      out->quad = det.quad;
      lipi_quad refined{};
      if (refine.enabled && lipi_refine_quad(&view, &det.quad, &refined) == LIPI_OK) {
        // Snapping to an edge is only trusted for small moves; a big one means it found the wrong edge.
        const double diag = std::hypot(static_cast<double>(view.width), static_cast<double>(view.height));
        double worst = 0;
        for (int i = 0; i < 4; ++i)
          worst = std::max(worst, std::hypot(static_cast<double>(refined.p[i].x - det.quad.p[i].x), static_cast<double>(refined.p[i].y - det.quad.p[i].y)));
        if (worst <= static_cast<double>(refine.limit) * diag) out->quad = refined;
      }
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

// Options shared by detect, scan, pdf and bench.
struct PageOptions {
  std::string model_prefix;
  std::string filter_name = "auto";
  int filter = LIPI_FILTER_AUTO;
  float brightness = 0, contrast = 0;
  bool shadows = true, rotate = true, deskew = true;
  int max_side = 2400;
  Refine refine;
};

// Tries to consume one page option at args[i]. Returns 1 if it did (advancing i), 0 if the argument is not a page
// option, and -1 after printing an error.
int parse_page_option(const std::vector<std::string>& args, size_t* i, PageOptions* o) {
  const std::string& a = args[*i];
  auto value = [&](std::string* dst) {
    if (*i + 1 >= args.size()) return false;
    *dst = args[++*i];
    return true;
  };
  std::string v;
  if (a == "--model") { if (!value(&o->model_prefix)) { usage_error("--model needs a value"); return -1; } return 1; }
  if (a == "--filter") { if (!value(&o->filter_name)) { usage_error("--filter needs a value"); return -1; } return 1; }
  if (a == "--brightness") {
    if (!value(&v) || !parse_float(v, &o->brightness) || o->brightness < -1 || o->brightness > 1) { usage_error("--brightness needs a number between -1 and 1"); return -1; }
    return 1;
  }
  if (a == "--contrast") {
    if (!value(&v) || !parse_float(v, &o->contrast) || o->contrast < -1 || o->contrast > 1) { usage_error("--contrast needs a number between -1 and 1"); return -1; }
    return 1;
  }
  if (a == "--max-side") {
    if (!value(&v)) { usage_error("--max-side needs a value"); return -1; }
    o->max_side = std::atoi(v.c_str());
    if (o->max_side < 16 || o->max_side > 16384) { usage_error("--max-side must be between 16 and 16384"); return -1; }
    return 1;
  }
  if (a == "--no-shadow-removal") { o->shadows = false; return 1; }
  if (a == "--no-deskew") { o->deskew = false; return 1; }
  if (a == "--no-rotate") { o->rotate = false; return 1; }
  if (a == "--refine") { o->refine.enabled = true; return 1; }
  return 0;
}

// The finished page and what was done to get it.
struct PageResult {
  std::vector<uint8_t> pixels;
  int width = 0, height = 0;
  lipi_pixel_format format = LIPI_PIXEL_RGB8;
  Found found;
  float skew_degrees = 0;
  double detect_ms = 0, warp_ms = 0, enhance_ms = 0, deskew_ms = 0;
  std::vector<uint8_t> flattened;  // for --debug-dir
  int flat_width = 0, flat_height = 0;
  lipi_pixel_format flat_format = LIPI_PIXEL_RGB8;

  lipi_image_view view() const {
    return {pixels.data(), width, height, width * lipi_pixel_format_channels(format), format};
  }
};

void copy_out(lipi_image* image, std::vector<uint8_t>* pixels, int* w, int* h, lipi_pixel_format* fmt) {
  const lipi_image_view v = lipi_image_view_of(image);
  const int ch = lipi_pixel_format_channels(v.format);
  pixels->assign(v.data, v.data + static_cast<size_t>(v.width) * static_cast<size_t>(v.height) * static_cast<size_t>(ch));
  *w = v.width;
  *h = v.height;
  *fmt = v.format;
}

// Detect (or use the given quad), flatten, clean up, straighten and turn upright.
int run_pipeline(const lipi_image_view& view, const PageOptions& o, const Model& model, const lipi_quad* given,
                 bool keep_debug, PageResult* r) {
  double t = now_ms();
  if (given != nullptr) {
    r->found.quad = *given;
  } else {
    const lipi_status s = find_page(view, model, o.refine, &r->found);
    if (s != LIPI_OK) return status_exit(s, "no page found");
  }
  r->detect_ms = now_ms() - t;

  t = now_ms();
  int pw = 0, ph = 0;
  lipi_status s = lipi_estimate_page_size(&r->found.quad, view.width, view.height, o.max_side, &pw, &ph);
  if (s != LIPI_OK) return status_exit(s, "page size");
  lipi_image* page = nullptr;
  s = lipi_warp_quad(&view, &r->found.quad, pw, ph, &page);
  if (s != LIPI_OK) return status_exit(s, "warp");
  r->warp_ms = now_ms() - t;
  if (keep_debug) copy_out(page, &r->flattened, &r->flat_width, &r->flat_height, &r->flat_format);

  t = now_ms();
  lipi_enhance_options opt = lipi_enhance_defaults();
  opt.filter = o.filter;
  opt.remove_shadows = o.shadows ? 1 : 0;
  opt.brightness = o.brightness;
  opt.contrast = o.contrast;
  const lipi_image_view warped = lipi_image_view_of(page);
  lipi_image* enhanced = nullptr;
  s = lipi_enhance(&warped, &opt, &enhanced);
  lipi_image_free(page);
  if (s != LIPI_OK) return status_exit(s, "enhance");
  r->enhance_ms = now_ms() - t;

  t = now_ms();
  if (o.deskew) {
    const lipi_image_view ev = lipi_image_view_of(enhanced);
    float degrees = 0, confidence = 0;
    if (lipi_estimate_skew(&ev, &degrees, &confidence) == LIPI_OK && confidence >= 0.5f && std::fabs(degrees) >= 0.3f) {
      lipi_image* straight = nullptr;
      if (lipi_rotate(&ev, degrees, &straight) == LIPI_OK) {
        lipi_image_free(enhanced);
        enhanced = straight;
        r->skew_degrees = degrees;
      }
    }
  }
  r->deskew_ms = now_ms() - t;

  copy_out(enhanced, &r->pixels, &r->width, &r->height, &r->format);
  lipi_image_free(enhanced);

  // Turn the page upright when the detector knows how it is rotated.
  if (o.rotate && r->found.orientation > 0) {
    const int channels = lipi_pixel_format_channels(r->format);
    const int exif_code = r->found.orientation == 1 ? 6 : (r->found.orientation == 2 ? 3 : 8);  // quarter turns as EXIF codes
    int w = r->width, h = r->height;
    r->pixels = lipi_cli::apply_orientation(r->pixels, &w, &h, channels, exif_code);
    r->width = w;
    r->height = h;
  }
  return kOk;
}

int finish_options(PageOptions* o) {
  if (!parse_filter(o->filter_name, &o->filter)) return usage_error("--filter must be auto, bw, gray, color or none");
  return kOk;
}

int cmd_detect(const std::vector<std::string>& args) {
  std::string path;
  PageOptions o;
  bool json = false;
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& a = args[i];
    if (a == "--json") { json = true; continue; }
    const int took = parse_page_option(args, &i, &o);
    if (took < 0) return kUsage;
    if (took > 0) continue;
    if (!a.empty() && a[0] == '-') return usage_error("unknown option " + a);
    if (path.empty()) path = a;
    else return usage_error("more than one input image");
  }
  if (path.empty()) return usage_error("detect needs an input image");
  Model model;
  if (const int rc = model.load(o.model_prefix)) return rc;
  lipi_cli::Pixels img;
  std::string err;
  if (!lipi_cli::load_image(path, &img, &err)) {
    std::fprintf(stderr, "lipi-cli: %s\n", err.c_str());
    return kIo;
  }
  const lipi_image_view view = img.view();
  Found f;
  const double t0 = now_ms();
  const lipi_status s = find_page(view, model, o.refine, &f);
  const double ms = now_ms() - t0;
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
  std::string in, out, quad_text, debug_dir;
  PageOptions o;
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& a = args[i];
    auto value = [&](std::string* dst) {
      if (i + 1 >= args.size()) return false;
      *dst = args[++i];
      return true;
    };
    const int took = parse_page_option(args, &i, &o);
    if (took < 0) return kUsage;
    if (took > 0) continue;
    if (a == "-o") { if (!value(&out)) return usage_error("-o needs a value"); }
    else if (a == "--quad") { if (!value(&quad_text)) return usage_error("--quad needs a value"); }
    else if (a == "--debug-dir") { if (!value(&debug_dir)) return usage_error("--debug-dir needs a value"); }
    else if (!a.empty() && a[0] == '-') return usage_error("unknown option " + a);
    else if (in.empty()) in = a;
    else return usage_error("more than one input image");
  }
  if (in.empty()) return usage_error("scan needs an input image");
  if (out.empty()) return usage_error("scan needs -o <output>");
  if (const int rc = finish_options(&o)) return rc;

  Model model;
  if (const int rc = model.load(o.model_prefix)) return rc;
  lipi_cli::Pixels img;
  std::string err;
  if (!lipi_cli::load_image(in, &img, &err)) {
    std::fprintf(stderr, "lipi-cli: %s\n", err.c_str());
    return kIo;
  }
  const lipi_image_view view = img.view();
  lipi_quad given{};
  bool have_quad = false;
  if (!quad_text.empty()) {
    if (!parse_quad(quad_text, &given)) return usage_error("--quad needs eight comma-separated numbers");
    if (lipi_quad_validate(&given) != LIPI_OK) {
      std::fprintf(stderr, "lipi-cli: --quad is not a valid page outline (corners must be convex, clockwise from the top-left)\n");
      return kUsage;
    }
    have_quad = true;
  }
  PageResult r;
  if (const int rc = run_pipeline(view, o, model, have_quad ? &given : nullptr, !debug_dir.empty(), &r)) return rc;

  int code = kOk;
  if (!lipi_cli::save_image(out, r.view(), &err)) {
    std::fprintf(stderr, "lipi-cli: %s\n", err.c_str());
    code = kIo;
  }
  if (code == kOk && !debug_dir.empty()) {
    mkdir(debug_dir.c_str(), 0777);
    lipi_cli::Pixels overlay = img;
    lipi_cli::draw_quad(&overlay, r.found.quad);
    const lipi_image_view flat = {r.flattened.data(), r.flat_width, r.flat_height,
                                  r.flat_width * lipi_pixel_format_channels(r.flat_format), r.flat_format};
    if (!lipi_cli::save_image(debug_dir + "/01-input-quad.png", overlay.view(), &err) ||
        !lipi_cli::save_image(debug_dir + "/02-flattened.png", flat, &err) ||
        !lipi_cli::save_image(debug_dir + "/03-result.png", r.view(), &err)) {
      std::fprintf(stderr, "lipi-cli: %s\n", err.c_str());
      code = kIo;
    }
  }
  if (code == kOk) {
    std::printf("wrote %s (%dx%d)", out.c_str(), r.width, r.height);
    if (r.skew_degrees != 0) std::printf(", straightened by %.1f degrees", static_cast<double>(r.skew_degrees));
    std::printf("\n");
  }
  return code;
}

size_t write_to_file(const void* data, size_t size, void* user) {
  return std::fwrite(data, 1, size, static_cast<FILE*>(user));
}

int cmd_pdf(const std::vector<std::string>& args) {
  std::vector<std::string> inputs;
  std::string out;
  int quality = 80;
  float dpi = 150;
  PageOptions o;
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& a = args[i];
    auto value = [&](std::string* dst) {
      if (i + 1 >= args.size()) return false;
      *dst = args[++i];
      return true;
    };
    const int took = parse_page_option(args, &i, &o);
    if (took < 0) return kUsage;
    if (took > 0) continue;
    std::string v;
    if (a == "-o") { if (!value(&out)) return usage_error("-o needs a value"); }
    else if (a == "--quality") {
      if (!value(&v)) return usage_error("--quality needs a value");
      quality = std::atoi(v.c_str());
      if (quality < 1 || quality > 100) return usage_error("--quality must be between 1 and 100");
    } else if (a == "--dpi") {
      if (!value(&v) || !parse_float(v, &dpi) || dpi < 10 || dpi > 2400) return usage_error("--dpi must be between 10 and 2400");
    } else if (!a.empty() && a[0] == '-') return usage_error("unknown option " + a);
    else inputs.push_back(a);
  }
  if (inputs.empty()) return usage_error("pdf needs at least one input image");
  if (out.empty()) return usage_error("pdf needs -o <output.pdf>");
  if (const int rc = finish_options(&o)) return rc;

  Model model;
  if (const int rc = model.load(o.model_prefix)) return rc;
  FILE* file = std::fopen(out.c_str(), "wb");
  if (file == nullptr) {
    std::fprintf(stderr, "lipi-cli: cannot write %s\n", out.c_str());
    return kIo;
  }
  lipi_pdf* pdf = nullptr;
  int code = kOk;
  if (lipi_pdf_create(write_to_file, file, &pdf) != LIPI_OK) code = kIo;
  for (const std::string& in : inputs) {
    if (code != kOk) break;
    lipi_cli::Pixels img;
    std::string err;
    if (!lipi_cli::load_image(in, &img, &err)) {
      std::fprintf(stderr, "lipi-cli: %s\n", err.c_str());
      code = kIo;
      break;
    }
    PageResult r;
    if (const int rc = run_pipeline(img.view(), o, model, nullptr, false, &r)) {
      code = rc;
      break;
    }
    std::vector<uint8_t> jpeg;
    if (!lipi_cli::encode_jpeg(r.view(), quality, &jpeg)) {
      std::fprintf(stderr, "lipi-cli: cannot encode %s\n", in.c_str());
      code = kInternal;
      break;
    }
    if (lipi_pdf_add_page_jpeg(pdf, jpeg.data(), jpeg.size(), dpi) != LIPI_OK) {
      std::fprintf(stderr, "lipi-cli: cannot add %s to the PDF\n", in.c_str());
      code = kInternal;
      break;
    }
  }
  if (code == kOk && lipi_pdf_finish(pdf) != LIPI_OK) code = kIo;
  lipi_pdf_free(pdf);
  if (std::fclose(file) != 0 && code == kOk) code = kIo;
  if (code == kOk) {
    struct stat st {};
    stat(out.c_str(), &st);
    std::printf("wrote %s (%zu page%s, %.1f KB)\n", out.c_str(), inputs.size(), inputs.size() == 1 ? "" : "s", static_cast<double>(st.st_size) / 1024.0);
  }
  return code;
}

struct Stat {
  double median = 0, p95 = 0;
};

Stat summarize(std::vector<double> v) {
  std::sort(v.begin(), v.end());
  Stat s;
  s.median = v[v.size() / 2];
  s.p95 = v[std::min(v.size() - 1, static_cast<size_t>(std::ceil(0.95 * static_cast<double>(v.size()))) - 1)];
  return s;
}

int cmd_bench(const std::vector<std::string>& args) {
  std::string path;
  PageOptions o;
  int repeat = 30;
  bool json = false;
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& a = args[i];
    if (a == "--json") { json = true; continue; }
    if (a == "--repeat") {
      if (i + 1 >= args.size()) return usage_error("--repeat needs a value");
      repeat = std::atoi(args[++i].c_str());
      if (repeat < 1 || repeat > 10000) return usage_error("--repeat must be between 1 and 10000");
      continue;
    }
    const int took = parse_page_option(args, &i, &o);
    if (took < 0) return kUsage;
    if (took > 0) continue;
    if (!a.empty() && a[0] == '-') return usage_error("unknown option " + a);
    if (path.empty()) path = a;
    else return usage_error("more than one input image");
  }
  if (path.empty()) return usage_error("bench needs an input image");
  if (const int rc = finish_options(&o)) return rc;
  Model model;
  if (const int rc = model.load(o.model_prefix)) return rc;
  lipi_cli::Pixels img;
  std::string err;
  if (!lipi_cli::load_image(path, &img, &err)) {
    std::fprintf(stderr, "lipi-cli: %s\n", err.c_str());
    return kIo;
  }
  std::vector<double> detect, warp, enhance, deskew, total;
  for (int i = 0; i < 3 + repeat; ++i) {
    PageResult r;
    const double t = now_ms();
    if (const int rc = run_pipeline(img.view(), o, model, nullptr, false, &r)) return rc;
    if (i < 3) continue;  // warm-up
    detect.push_back(r.detect_ms);
    warp.push_back(r.warp_ms);
    enhance.push_back(r.enhance_ms);
    deskew.push_back(r.deskew_ms);
    total.push_back(now_ms() - t);
  }
  const struct { const char* name; const std::vector<double>* v; } rows[] = {
      {"detect", &detect}, {"warp", &warp}, {"enhance", &enhance}, {"deskew", &deskew}, {"total", &total}};
  if (json) {
    std::printf("{\"image\":[%d,%d],\"detector\":\"%s\",\"runs\":%d", img.width, img.height, model.detector ? "model" : "classical", repeat);
    for (const auto& row : rows) {
      const Stat s = summarize(*row.v);
      std::printf(",\"%s\":{\"median_ms\":%.2f,\"p95_ms\":%.2f}", row.name, s.median, s.p95);
    }
    std::printf("}\n");
  } else {
    std::printf("image %dx%d, %s detector, %d runs after a warm-up, one thread\n", img.width, img.height, model.detector ? "learned" : "classical", repeat);
    std::printf("%-8s %10s %10s\n", "stage", "median ms", "p95 ms");
    for (const auto& row : rows) {
      const Stat s = summarize(*row.v);
      std::printf("%-8s %10.2f %10.2f\n", row.name, s.median, s.p95);
    }
  }
  return kOk;
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
  if (cmd == "pdf") return cmd_pdf(args);
  if (cmd == "bench") return cmd_bench(args);
  return usage_error("unknown command '" + cmd + "'");
}
