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

    // Learned detector. A handle is 0 when it could not be created. One detector must be used from one thread at a time.

    /** Creates a detector from the two ncnn model files. Returns 0 if the build has no detector or the model is not valid. */
    external fun createDetector(param: ByteArray, weights: ByteArray, threads: Int): Long
    external fun destroyDetector(handle: Long)

    /** Detects on an ARGB_8888 bitmap. [out] gets ten numbers: x, y of the four corners in pixels, confidence, orientation. */
    external fun detectBitmap(handle: Long, bitmap: android.graphics.Bitmap, out: FloatArray): Boolean

    /** Detects on a camera frame in a direct buffer of RGBA pixels with [rowStride] bytes per row. [out] as above. */
    external fun detectRgba(handle: Long, buffer: java.nio.ByteBuffer, width: Int, height: Int, rowStride: Int, out: FloatArray): Boolean

    // Smoothing of the outline from frame to frame.
    external fun createSmoother(width: Int, height: Int): Long
    external fun destroySmoother(handle: Long)
    external fun resetSmoother(handle: Long)

    /** Feeds one frame (eight corner pixels, or null for "nothing found"); writes the smoothed eight numbers to [out]. */
    external fun updateSmoother(handle: Long, detected: FloatArray?, seconds: Double, out: FloatArray): Boolean
}
