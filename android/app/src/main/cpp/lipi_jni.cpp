// JNI glue between the Kotlin app and the portable core. Kept thin on purpose: the core has no Android types.
#include <jni.h>

#include "lipi/lipi.h"

extern "C" JNIEXPORT jstring JNICALL Java_in_commandlinecoding_lipi_NativeCore_version(JNIEnv* env, jobject /*thiz*/) {
  return env->NewStringUTF(lipi_version());
}

extern "C" JNIEXPORT jboolean JNICALL Java_in_commandlinecoding_lipi_NativeCore_detectorAvailable(JNIEnv* /*env*/, jobject /*thiz*/) {
  return lipi_detector_available() != 0 ? JNI_TRUE : JNI_FALSE;
}
