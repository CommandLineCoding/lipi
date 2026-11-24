package `in`.commandlinecoding.lipi.scan

import android.content.Context
import android.graphics.Bitmap
import `in`.commandlinecoding.lipi.NativeCore
import java.nio.ByteBuffer

/** What the detector saw: the page corners in pixels of the image it was given, how sure it is, and which way up the page is. */
class RawDetection(val corners: FloatArray, val confidence: Float, val orientation: Int)

/**
 * The learned page detector (PageNet, bundled with the app), running on the CPU through the core.
 * One instance serves one thread at a time; the calls are synchronised so that [close] is safe while a frame is in flight.
 */
class PageDetector private constructor(private var handle: Long) : AutoCloseable {
    private val out = FloatArray(10)

    @Synchronized
    fun detect(bitmap: Bitmap): RawDetection? = if (handle != 0L && NativeCore.detectBitmap(handle, bitmap, out)) result() else null

    /** Detects on a camera frame: RGBA pixels in a direct buffer, [rowStride] bytes per row. */
    @Synchronized
    fun detect(buffer: ByteBuffer, width: Int, height: Int, rowStride: Int): RawDetection? =
        if (handle != 0L && NativeCore.detectRgba(handle, buffer, width, height, rowStride, out)) result() else null

    private fun result() = RawDetection(out.copyOfRange(0, 8), out[8], out[9].toInt())

    @Synchronized
    override fun close() {
        if (handle != 0L) NativeCore.destroyDetector(handle)
        handle = 0L
    }

    companion object {
        const val MODEL = "models/page-net"

        /** Loads the bundled model. Returns null if this build of the core has no detector or the model cannot be read. */
        fun create(context: Context, threads: Int = 2): PageDetector? = try {
            val param = context.assets.open("$MODEL.ncnn.param").use { it.readBytes() }
            val weights = context.assets.open("$MODEL.ncnn.bin").use { it.readBytes() }
            NativeCore.createDetector(param, weights, threads).takeIf { it != 0L }?.let { PageDetector(it) }
        } catch (e: java.io.IOException) {
            null
        }
    }
}

/** Steadies the outline from frame to frame (the core's One-Euro filter) and drops it when the page has been missing for a while. */
class QuadSmoother(width: Int, height: Int) : AutoCloseable {
    private var handle = NativeCore.createSmoother(width, height)
    private val out = FloatArray(8)

    /** Feeds one frame. [corners] is null when nothing was detected. Returns the smoothed corners in pixels, or null. */
    @Synchronized
    fun update(corners: FloatArray?, seconds: Double): FloatArray? =
        if (handle != 0L && NativeCore.updateSmoother(handle, corners, seconds, out)) out.copyOf() else null

    @Synchronized
    fun reset() {
        if (handle != 0L) NativeCore.resetSmoother(handle)
    }

    @Synchronized
    override fun close() {
        if (handle != 0L) NativeCore.destroySmoother(handle)
        handle = 0L
    }
}
