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

    /**
     * Looks for a page in [bitmap], which must be ARGB_8888, with the core's classical detector. Returns the corners as
     * eight numbers (x, y of each, in pixels of the bitmap; top-left, top-right, bottom-right, bottom-left), or null.
     */
    external fun detectPageClassical(bitmap: android.graphics.Bitmap): FloatArray?
}
