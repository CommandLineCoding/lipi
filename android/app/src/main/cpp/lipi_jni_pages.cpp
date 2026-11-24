// JNI glue for flattening and cleaning up a page. Works on Android bitmaps (ARGB_8888, so RGBA bytes); the core does the
// work. Kept apart from lipi_jni.cpp, which holds the detector.
#include <android/bitmap.h>
#include <jni.h>

#include <cstdint>
#include <cstring>

#include "lipi/lipi.h"

namespace {

// Locks a bitmap's pixels for the lifetime of the object.
class LockedBitmap {
 public:
  LockedBitmap(JNIEnv* env, jobject bitmap) : env_(env), bitmap_(bitmap) {
    if (AndroidBitmap_getInfo(env, bitmap, &info_) != ANDROID_BITMAP_RESULT_SUCCESS) return;
    if (info_.format != ANDROID_BITMAP_FORMAT_RGBA_8888) return;
    if (AndroidBitmap_lockPixels(env, bitmap, &pixels_) != ANDROID_BITMAP_RESULT_SUCCESS) pixels_ = nullptr;
  }
  ~LockedBitmap() {
    if (pixels_ != nullptr) AndroidBitmap_unlockPixels(env_, bitmap_);
  }
  LockedBitmap(const LockedBitmap&) = delete;
  LockedBitmap& operator=(const LockedBitmap&) = delete;

  bool ok() const { return pixels_ != nullptr; }
  int width() const { return static_cast<int>(info_.width); }
  int height() const { return static_cast<int>(info_.height); }
  uint8_t* data() const { return static_cast<uint8_t*>(pixels_); }

  lipi_image_view view() const {
    lipi_image_view v{};
    v.data = data();
    v.width = width();
    v.height = height();
    v.stride = static_cast<int>(info_.stride);
    v.format = LIPI_PIXEL_RGBA8;
    return v;
  }

  // Copies a core image of the same size into the bitmap, widening gray and RGB to RGBA.
  bool store(const lipi_image* image) {
    const lipi_image_view v = lipi_image_view_of(image);
    if (!ok() || v.width != width() || v.height != height()) return false;
    const int channels = lipi_pixel_format_channels(v.format);
    for (int y = 0; y < v.height; ++y) {
      const uint8_t* in = v.data + static_cast<size_t>(y) * v.stride;
      uint8_t* out = data() + static_cast<size_t>(y) * info_.stride;
      if (channels == 4) {
        std::memcpy(out, in, static_cast<size_t>(v.width) * 4);
      } else if (channels == 3) {
        for (int x = 0; x < v.width; ++x) {
          out[x * 4] = in[x * 3];
          out[x * 4 + 1] = in[x * 3 + 1];
          out[x * 4 + 2] = in[x * 3 + 2];
          out[x * 4 + 3] = 255;
        }
      } else if (channels == 1) {
        for (int x = 0; x < v.width; ++x) {
          out[x * 4] = out[x * 4 + 1] = out[x * 4 + 2] = in[x];
          out[x * 4 + 3] = 255;
        }
      } else {
        return false;
      }
    }
    return true;
  }

 private:
  JNIEnv* env_;
  jobject bitmap_;
  AndroidBitmapInfo info_{};
  void* pixels_ = nullptr;
};

bool readQuad(JNIEnv* env, jfloatArray array, lipi_quad* quad) {
  if (array == nullptr || env->GetArrayLength(array) < 8) return false;
  jfloat v[8];
  env->GetFloatArrayRegion(array, 0, 8, v);
  for (int i = 0; i < 4; ++i) quad->p[i] = {v[i * 2], v[i * 2 + 1]};
  return true;
}

}  // namespace

// The size of the flat page that a quad (corner pixels of an image of width x height) shows, the longer side at most
// maxSide. Returns {width, height}, or null if the quad is not usable.
extern "C" JNIEXPORT jintArray JNICALL Java_in_commandlinecoding_lipi_NativePages_estimatePageSize(JNIEnv* env, jobject, jfloatArray corners,
                                                                                                  jint width, jint height, jint maxSide) {
  lipi_quad quad{};
  if (!readQuad(env, corners, &quad)) return nullptr;
  int w = 0;
  int h = 0;
  if (lipi_estimate_page_size(&quad, width, height, maxSide, &w, &h) != LIPI_OK) return nullptr;
  jintArray out = env->NewIntArray(2);
  if (out == nullptr) return nullptr;
  const jint v[2] = {w, h};
  env->SetIntArrayRegion(out, 0, 2, v);
  return out;
}

// Flattens the quad of `src` into `dst`, whose size is the size of the flat page.
extern "C" JNIEXPORT jboolean JNICALL Java_in_commandlinecoding_lipi_NativePages_warpQuad(JNIEnv* env, jobject, jobject src,
                                                                                         jfloatArray corners, jobject dst) {
  lipi_quad quad{};
  if (!readQuad(env, corners, &quad)) return JNI_FALSE;
  LockedBitmap in(env, src);
  LockedBitmap out(env, dst);
  if (!in.ok() || !out.ok()) return JNI_FALSE;
  const lipi_image_view view = in.view();
  lipi_image* result = nullptr;
  if (lipi_warp_quad(&view, &quad, out.width(), out.height(), &result) != LIPI_OK) return JNI_FALSE;
  const bool stored = out.store(result);
  lipi_image_free(result);
  return stored ? JNI_TRUE : JNI_FALSE;
}

// Cleans up `src` into `dst` (same size). `filter` is a lipi_filter value.
extern "C" JNIEXPORT jboolean JNICALL Java_in_commandlinecoding_lipi_NativePages_enhance(JNIEnv* env, jobject, jobject src, jobject dst,
                                                                                        jint filter, jboolean removeShadows,
                                                                                        jfloat brightness, jfloat contrast) {
  LockedBitmap in(env, src);
  LockedBitmap out(env, dst);
  if (!in.ok() || !out.ok()) return JNI_FALSE;
  lipi_enhance_options options = lipi_enhance_defaults();
  options.filter = filter;
  options.remove_shadows = removeShadows == JNI_TRUE ? 1 : 0;
  options.brightness = brightness;
  options.contrast = contrast;
  const lipi_image_view view = in.view();
  lipi_image* result = nullptr;
  if (lipi_enhance(&view, &options, &result) != LIPI_OK) return JNI_FALSE;
  const bool stored = out.store(result);
  lipi_image_free(result);
  return stored ? JNI_TRUE : JNI_FALSE;
}
