package `in`.commandlinecoding.lipi

import android.graphics.Bitmap

/** Flattening and cleaning up pages with the core (see src/main/cpp/lipi_jni_pages.cpp). All bitmaps must be ARGB_8888. */
object NativePages {
    init {
        System.loadLibrary("lipi_jni")
    }

    /** The size {width, height} of the flat page a quad shows; [corners] are eight pixel coordinates in an image of [width] by [height]. */
    external fun estimatePageSize(corners: FloatArray, width: Int, height: Int, maxSide: Int): IntArray?

    /** Flattens the quad of [src] into [dst], which has the size of the flat page. */
    external fun warpQuad(src: Bitmap, corners: FloatArray, dst: Bitmap): Boolean

    /** Cleans [src] up into [dst], which has the same size. [filter] is the core's filter number (1 Auto, 2 B&W, 3 Gray, 4 Color). */
    external fun enhance(src: Bitmap, dst: Bitmap, filter: Int, removeShadows: Boolean, brightness: Float, contrast: Float): Boolean
}
