package `in`.commandlinecoding.lipi

/** The portable C++ core, reached through JNI (see src/main/cpp/lipi_jni.cpp). */
object NativeCore {
    init {
        System.loadLibrary("lipi_jni")
    }

    /** Version of the core library, for example "0.0.1". */
    external fun version(): String

    /** Whether the core was built with the learned page detector. */
    external fun detectorAvailable(): Boolean
}
