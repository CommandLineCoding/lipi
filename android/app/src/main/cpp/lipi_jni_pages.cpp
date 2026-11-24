// JNI glue for flattening and cleaning up a page. Works on Android bitmaps (ARGB_8888, so RGBA bytes); the core does the
// work. Kept apart from lipi_jni.cpp, which holds the detector.
#include <android/bitmap.h>
#include <jni.h>

#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

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

// Writes to a file descriptor and says how many bytes went through, as the core's PDF writer wants.
size_t writeToFd(const void* data, size_t size, void* user) {
  const int fd = *static_cast<int*>(user);
  const uint8_t* bytes = static_cast<const uint8_t*>(data);
  size_t done = 0;
  while (done < size) {
    const ssize_t n = ::write(fd, bytes + done, size - done);
    if (n < 0) {
      if (errno == EINTR) continue;
      return done;
    }
    if (n == 0) return done;
    done += static_cast<size_t>(n);
  }
  return done;
}

bool readFile(const char* path, std::vector<uint8_t>* out) {
  FILE* f = std::fopen(path, "rb");
  if (f == nullptr) return false;
  std::fseek(f, 0, SEEK_END);
  const long size = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (size <= 0) {
    std::fclose(f);
    return false;
  }
  out->resize(static_cast<size_t>(size));
  const size_t read = std::fread(out->data(), 1, out->size(), f);
  std::fclose(f);
  return read == out->size();
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

// Writes a PDF with one page per JPEG file, in the order given, to the file descriptor `fd` (which stays open and
// belongs to the caller). The images are stored as they are, without being compressed again. dpis[i] sets the size of page i.
extern "C" JNIEXPORT jboolean JNICALL Java_in_commandlinecoding_lipi_NativePages_writePdf(JNIEnv* env, jobject, jobjectArray paths,
                                                                                         jfloatArray dpis, jint fd) {
  if (paths == nullptr || dpis == nullptr) return JNI_FALSE;
  const jsize count = env->GetArrayLength(paths);
  if (count <= 0 || env->GetArrayLength(dpis) < count) return JNI_FALSE;
  std::vector<jfloat> dpi(static_cast<size_t>(count));
  env->GetFloatArrayRegion(dpis, 0, count, dpi.data());

  int out = fd;
  lipi_pdf* pdf = nullptr;
  if (lipi_pdf_create(writeToFd, &out, &pdf) != LIPI_OK) return JNI_FALSE;
  bool ok = true;
  for (jsize i = 0; i < count && ok; ++i) {
    jstring jpath = static_cast<jstring>(env->GetObjectArrayElement(paths, i));
    const char* path = jpath == nullptr ? nullptr : env->GetStringUTFChars(jpath, nullptr);
    std::vector<uint8_t> jpeg;
    ok = path != nullptr && readFile(path, &jpeg) && lipi_pdf_add_page_jpeg(pdf, jpeg.data(), jpeg.size(), dpi[static_cast<size_t>(i)]) == LIPI_OK;
    if (path != nullptr) env->ReleaseStringUTFChars(jpath, path);
    if (jpath != nullptr) env->DeleteLocalRef(jpath);
  }
  ok = ok && lipi_pdf_finish(pdf) == LIPI_OK;
  lipi_pdf_free(pdf);
  return ok ? JNI_TRUE : JNI_FALSE;
}
