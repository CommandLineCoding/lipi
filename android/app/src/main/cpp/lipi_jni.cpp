// JNI glue between the Kotlin app and the portable core. Kept thin on purpose: the core has no Android types.
#include <android/bitmap.h>
#include <jni.h>

#include <cstdint>
#include <vector>

#include "lipi/lipi.h"

extern "C" JNIEXPORT jstring JNICALL Java_in_commandlinecoding_lipi_NativeCore_version(JNIEnv* env, jobject /*thiz*/) {
  return env->NewStringUTF(lipi_version());
}

extern "C" JNIEXPORT jboolean JNICALL Java_in_commandlinecoding_lipi_NativeCore_detectorAvailable(JNIEnv* /*env*/, jobject /*thiz*/) {
  return lipi_detector_available() != 0 ? JNI_TRUE : JNI_FALSE;
}

// Looks for a page in an ARGB_8888 bitmap with the core's classical detector. Returns eight floats (x, y of each corner
// in bitmap pixels, in the core's corner order), or null if the bitmap is of another kind or no page was found.
extern "C" JNIEXPORT jfloatArray JNICALL Java_in_commandlinecoding_lipi_NativeCore_detectPageClassical(JNIEnv* env, jobject /*thiz*/,
                                                                                                      jobject bitmap) {
  AndroidBitmapInfo info;
  if (AndroidBitmap_getInfo(env, bitmap, &info) != ANDROID_BITMAP_RESULT_SUCCESS || info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
    return nullptr;
  }
  void* pixels = nullptr;
  if (AndroidBitmap_lockPixels(env, bitmap, &pixels) != ANDROID_BITMAP_RESULT_SUCCESS || pixels == nullptr) return nullptr;

  lipi_image_view view{};
  view.data = static_cast<const uint8_t*>(pixels);
  view.width = static_cast<int>(info.width);
  view.height = static_cast<int>(info.height);
  view.stride = static_cast<int>(info.stride);
  view.format = LIPI_PIXEL_RGBA8;

  lipi_quad quad{};
  const lipi_status status = lipi_detect_page_classical(&view, &quad, nullptr);
  AndroidBitmap_unlockPixels(env, bitmap);
  if (status != LIPI_OK) return nullptr;

  jfloat xy[8];
  for (int i = 0; i < 4; ++i) {
    xy[i * 2] = quad.p[i].x;
    xy[i * 2 + 1] = quad.p[i].y;
  }
  jfloatArray out = env->NewFloatArray(8);
  if (out != nullptr) env->SetFloatArrayRegion(out, 0, 8, xy);
  return out;
}

// ---- Learned page detector -------------------------------------------------------------------------------------------
// A detector is not thread-safe: the Kotlin side must call detect from one thread at a time. A handle is the pointer.

namespace {
std::vector<uint8_t> bytesOf(JNIEnv* env, jbyteArray array) {
  const jsize n = env->GetArrayLength(array);
  std::vector<uint8_t> out(static_cast<size_t>(n));
  if (n > 0) env->GetByteArrayRegion(array, 0, n, reinterpret_cast<jbyte*>(out.data()));
  return out;
}

// Runs the detector and writes ten floats to `out`: the eight corner coordinates in pixels, the confidence and the
// orientation (0 to 3).
jboolean detect(JNIEnv* env, jlong handle, const lipi_image_view& view, jfloatArray out) {
  if (handle == 0 || out == nullptr || env->GetArrayLength(out) < 10) return JNI_FALSE;
  lipi_detection result{};
  if (lipi_detector_detect(reinterpret_cast<lipi_detector*>(handle), &view, &result) != LIPI_OK) return JNI_FALSE;
  jfloat values[10];
  for (int i = 0; i < 4; ++i) {
    values[i * 2] = result.quad.p[i].x;
    values[i * 2 + 1] = result.quad.p[i].y;
  }
  values[8] = result.confidence;
  values[9] = static_cast<jfloat>(result.orientation);
  env->SetFloatArrayRegion(out, 0, 10, values);
  return JNI_TRUE;
}
}  // namespace

// The model is passed as the two ncnn files (text .param and binary .bin). Returns 0 if the core has no detector or the
// model is not valid.
extern "C" JNIEXPORT jlong JNICALL Java_in_commandlinecoding_lipi_NativeCore_createDetector(JNIEnv* env, jobject, jbyteArray param,
                                                                                           jbyteArray weights, jint threads) {
  const std::vector<uint8_t> p = bytesOf(env, param);
  const std::vector<uint8_t> w = bytesOf(env, weights);
  lipi_detector* detector = nullptr;
  if (lipi_detector_create(p.data(), p.size(), w.data(), w.size(), threads, &detector) != LIPI_OK) return 0;
  return reinterpret_cast<jlong>(detector);
}

extern "C" JNIEXPORT void JNICALL Java_in_commandlinecoding_lipi_NativeCore_destroyDetector(JNIEnv*, jobject, jlong handle) {
  if (handle != 0) lipi_detector_free(reinterpret_cast<lipi_detector*>(handle));
}

// Detects on an ARGB_8888 bitmap.
extern "C" JNIEXPORT jboolean JNICALL Java_in_commandlinecoding_lipi_NativeCore_detectBitmap(JNIEnv* env, jobject, jlong handle,
                                                                                            jobject bitmap, jfloatArray out) {
  AndroidBitmapInfo info;
  if (AndroidBitmap_getInfo(env, bitmap, &info) != ANDROID_BITMAP_RESULT_SUCCESS || info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
    return JNI_FALSE;
  }
  void* pixels = nullptr;
  if (AndroidBitmap_lockPixels(env, bitmap, &pixels) != ANDROID_BITMAP_RESULT_SUCCESS || pixels == nullptr) return JNI_FALSE;
  lipi_image_view view{};
  view.data = static_cast<const uint8_t*>(pixels);
  view.width = static_cast<int>(info.width);
  view.height = static_cast<int>(info.height);
  view.stride = static_cast<int>(info.stride);
  view.format = LIPI_PIXEL_RGBA8;
  const jboolean ok = detect(env, handle, view, out);
  AndroidBitmap_unlockPixels(env, bitmap);
  return ok;
}

// Detects on a camera frame: a direct buffer of RGBA pixels, `rowStride` bytes per row.
extern "C" JNIEXPORT jboolean JNICALL Java_in_commandlinecoding_lipi_NativeCore_detectRgba(JNIEnv* env, jobject, jlong handle,
                                                                                          jobject buffer, jint width, jint height,
                                                                                          jint rowStride, jfloatArray out) {
  const void* data = buffer == nullptr ? nullptr : env->GetDirectBufferAddress(buffer);
  if (data == nullptr || width <= 0 || height <= 0 || rowStride < width * 4) return JNI_FALSE;
  if (env->GetDirectBufferCapacity(buffer) < static_cast<jlong>(rowStride) * (height - 1) + width * 4) return JNI_FALSE;
  lipi_image_view view{};
  view.data = static_cast<const uint8_t*>(data);
  view.width = width;
  view.height = height;
  view.stride = rowStride;
  view.format = LIPI_PIXEL_RGBA8;
  return detect(env, handle, view, out);
}

// ---- Temporal smoothing of the quad ----------------------------------------------------------------------------------

extern "C" JNIEXPORT jlong JNICALL Java_in_commandlinecoding_lipi_NativeCore_createSmoother(JNIEnv*, jobject, jint width, jint height) {
  const lipi_smoother_options options = lipi_smoother_defaults();
  lipi_smoother* smoother = nullptr;
  if (lipi_smoother_create(width, height, &options, &smoother) != LIPI_OK) return 0;
  return reinterpret_cast<jlong>(smoother);
}

extern "C" JNIEXPORT void JNICALL Java_in_commandlinecoding_lipi_NativeCore_destroySmoother(JNIEnv*, jobject, jlong handle) {
  if (handle != 0) lipi_smoother_free(reinterpret_cast<lipi_smoother*>(handle));
}

extern "C" JNIEXPORT void JNICALL Java_in_commandlinecoding_lipi_NativeCore_resetSmoother(JNIEnv*, jobject, jlong handle) {
  if (handle != 0) lipi_smoother_reset(reinterpret_cast<lipi_smoother*>(handle));
}

// Feeds one frame: eight corner coordinates in pixels (or null when nothing was detected). Writes the smoothed corners to
// `out` (eight floats) and returns true, or returns false when there is nothing to show.
extern "C" JNIEXPORT jboolean JNICALL Java_in_commandlinecoding_lipi_NativeCore_updateSmoother(JNIEnv* env, jobject, jlong handle,
                                                                                              jfloatArray detected, jdouble seconds,
                                                                                              jfloatArray out) {
  if (handle == 0 || out == nullptr || env->GetArrayLength(out) < 8) return JNI_FALSE;
  lipi_quad in{};
  const lipi_quad* inPtr = nullptr;
  if (detected != nullptr && env->GetArrayLength(detected) >= 8) {
    jfloat v[8];
    env->GetFloatArrayRegion(detected, 0, 8, v);
    for (int i = 0; i < 4; ++i) in.p[i] = {v[i * 2], v[i * 2 + 1]};
    inPtr = &in;
  }
  lipi_quad smoothed{};
  if (lipi_smoother_update(reinterpret_cast<lipi_smoother*>(handle), inPtr, seconds, &smoothed) != LIPI_OK) return JNI_FALSE;
  jfloat v[8];
  for (int i = 0; i < 4; ++i) {
    v[i * 2] = smoothed.p[i].x;
    v[i * 2 + 1] = smoothed.p[i].y;
  }
  env->SetFloatArrayRegion(out, 0, 8, v);
  return JNI_TRUE;
}
