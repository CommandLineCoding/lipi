// Learned page detector on top of ncnn (compiled in only with LIPI_WITH_NCNN).
#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "lipi/lipi.h"

#ifdef LIPI_HAVE_NCNN
#include "net.h"
#include "resize.hpp"

namespace {
constexpr int kInput = 256;
}

struct lipi_detector {
  ncnn::Net net;
  std::string param;                  // null-terminated copy of the text graph
  std::vector<unsigned char> weights; // ncnn reads weights straight from this buffer
};

extern "C" {

int lipi_detector_available(void) { return 1; }

lipi_status lipi_detector_create(const void* param, size_t param_size, const void* weights, size_t weights_size,
                                 int num_threads, lipi_detector** out) {
  if (out == nullptr) return LIPI_ERR_INVALID_ARGUMENT;
  *out = nullptr;
  if (param == nullptr || weights == nullptr || param_size == 0 || weights_size == 0) return LIPI_ERR_INVALID_ARGUMENT;
  try {
    auto* d = new lipi_detector();
    d->param.assign(static_cast<const char*>(param), param_size);
    d->weights.assign(static_cast<const unsigned char*>(weights), static_cast<const unsigned char*>(weights) + weights_size);
    d->weights.resize(weights_size + 8, 0);  // ncnn may read a little past the end when decoding
    d->net.opt.num_threads = num_threads > 0 ? num_threads : 1;
    d->net.opt.use_vulkan_compute = false;
    d->net.opt.use_fp16_storage = false;
    d->net.opt.use_fp16_arithmetic = false;
    d->net.opt.use_bf16_storage = false;
    // load_param_mem returns 0 on success; load_model returns the number of bytes it consumed.
    if (d->net.load_param_mem(d->param.c_str()) != 0 || d->net.load_model(d->weights.data()) <= 0) {
      delete d;
      return LIPI_ERR_INVALID_ARGUMENT;
    }
    *out = d;
    return LIPI_OK;
  } catch (const std::bad_alloc&) {
    return LIPI_ERR_OUT_OF_MEMORY;
  } catch (...) {
    return LIPI_ERR_INTERNAL;
  }
}

lipi_status lipi_detector_detect(lipi_detector* d, const lipi_image_view* src, lipi_detection* out) {
  if (d == nullptr || src == nullptr || src->data == nullptr || out == nullptr) return LIPI_ERR_INVALID_ARGUMENT;
  const int channels = lipi_pixel_format_channels(src->format);
  if (channels == 0 || src->width <= 0 || src->height <= 0 || src->stride < src->width * channels) return LIPI_ERR_INVALID_ARGUMENT;
  try {
    std::vector<float> planar = lipi::resize_area_planar_rgb(*src, kInput, kInput);
    ncnn::Mat in(kInput, kInput, 3);
    for (int c = 0; c < 3; ++c) std::memcpy(in.channel(c), &planar[static_cast<size_t>(c) * kInput * kInput], sizeof(float) * kInput * kInput);

    ncnn::Extractor ex = d->net.create_extractor();
    if (ex.input("in0", in) != 0) return LIPI_ERR_INTERNAL;
    ncnn::Mat corners, confidence, orientation;
    if (ex.extract("out0", corners) != 0 || ex.extract("out1", confidence) != 0 || ex.extract("out2", orientation) != 0) {
      return LIPI_ERR_INTERNAL;
    }
    if (corners.total() < 8 || confidence.total() < 4 || orientation.total() < 4) return LIPI_ERR_INTERNAL;
    const float* c = static_cast<const float*>(corners.data);
    const float* cf = static_cast<const float*>(confidence.data);
    const float* o = static_cast<const float*>(orientation.data);
    for (int i = 0; i < 4; ++i) {
      out->quad.p[i].x = std::min(1.0f, std::max(0.0f, c[2 * i])) * static_cast<float>(src->width);
      out->quad.p[i].y = std::min(1.0f, std::max(0.0f, c[2 * i + 1])) * static_cast<float>(src->height);
    }
    out->confidence = std::min(std::min(cf[0], cf[1]), std::min(cf[2], cf[3]));
    out->orientation = static_cast<int>(std::max_element(o, o + 4) - o);
    return lipi_quad_validate(&out->quad) == LIPI_OK ? LIPI_OK : LIPI_ERR_NOT_FOUND;
  } catch (const std::bad_alloc&) {
    return LIPI_ERR_OUT_OF_MEMORY;
  } catch (...) {
    return LIPI_ERR_INTERNAL;
  }
}

void lipi_detector_free(lipi_detector* d) { delete d; }

}  // extern "C"

#else  // built without a neural backend

struct lipi_detector {};

extern "C" {

int lipi_detector_available(void) { return 0; }

lipi_status lipi_detector_create(const void* param, size_t param_size, const void* weights, size_t weights_size, int,
                                 lipi_detector** out) {
  if (out == nullptr) return LIPI_ERR_INVALID_ARGUMENT;
  *out = nullptr;
  if (param == nullptr || weights == nullptr || param_size == 0 || weights_size == 0) return LIPI_ERR_INVALID_ARGUMENT;
  return LIPI_ERR_UNSUPPORTED;
}

lipi_status lipi_detector_detect(lipi_detector*, const lipi_image_view*, lipi_detection*) { return LIPI_ERR_UNSUPPORTED; }

void lipi_detector_free(lipi_detector*) {}

}  // extern "C"

#endif
