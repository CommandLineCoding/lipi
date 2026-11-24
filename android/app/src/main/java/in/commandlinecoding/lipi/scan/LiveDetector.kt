package `in`.commandlinecoding.lipi.scan

import androidx.camera.core.ImageAnalysis
import androidx.camera.core.ImageProxy
import `in`.commandlinecoding.lipi.BuildConfig
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/** The page outline found in the latest camera frame, in the upright picture of [width] by [height] pixels. */
data class LiveOutline(val quad: PageQuad, val width: Int, val height: Int)

/**
 * Runs the detector on camera frames (RGBA) and publishes a steadied outline. Frames arrive on the analysis thread, one
 * at a time; slow frames are dropped by the camera, so the outline always follows the newest picture that could be handled.
 */
class LiveDetector(private val detector: PageDetector) : ImageAnalysis.Analyzer, AutoCloseable {
    private val _outline = MutableStateFlow<LiveOutline?>(null)
    val outline: StateFlow<LiveOutline?> = _outline.asStateFlow()

    private var smoother: QuadSmoother? = null
    private var smootherWidth = 0
    private var smootherHeight = 0
    private val latency = LatencyLog("LipiDetect")
    @Volatile private var closed = false

    override fun analyze(image: ImageProxy) {
        try {
            if (closed) return
            val plane = image.planes[0]
            val width = image.width
            val height = image.height
            val started = System.nanoTime()
            val found = detector.detect(plane.buffer, width, height, plane.rowStride)
            if (BuildConfig.DEBUG) latency.record((System.nanoTime() - started) / 1e6, found?.confidence?.toDouble() ?: 0.0)

            if (smoother == null || smootherWidth != width || smootherHeight != height) {
                smoother?.close()
                smoother = QuadSmoother(width, height)
                smootherWidth = width
                smootherHeight = height
            }
            // The network always answers with a quad; a low confidence means there is probably no page to see.
            val corners = found?.takeIf { it.confidence >= MIN_CONFIDENCE }?.corners
            val steady = smoother?.update(corners, image.imageInfo.timestamp / 1e9)
            val turns = image.imageInfo.rotationDegrees / 90
            val quad = steady?.let { PageQuad.fromPixels(it, width, height) }?.rotatedClockwise(turns)
            _outline.value = quad?.let {
                if (turns % 2 == 0) LiveOutline(it, width, height) else LiveOutline(it, height, width)
            }
        } finally {
            image.close()
        }
    }

    /** Stops the analysis and frees the native detector. Frames that arrive afterwards are ignored. */
    override fun close() {
        closed = true
        smoother?.close()
        smoother = null
        detector.close()
    }

    companion object {
        /**
         * Below this the outline is not shown. The confidence is the lowest corner heat-map peak, which tops out at about 0.06
         * (see models/page-net/MODEL_CARD.md). On the held-out real photos 0.04 keeps 94% of the detections that overlap
         * the page by 0.9 or more and drops most of the poor ones.
         */
        const val MIN_CONFIDENCE = 0.04f
    }
}
