// JNI glue between the Kotlin app and the portable core. Kept thin on purpose: the core has no Android types.
#include <android/bitmap.h>
#include <jni.h>

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
