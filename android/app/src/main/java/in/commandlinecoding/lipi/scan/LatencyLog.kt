package `in`.commandlinecoding.lipi.scan

import android.util.Log

/** Median and 95th percentile of the numbers in [values] (milliseconds), by nearest rank. Empty input gives zeros. */
fun summarize(values: List<Double>): Pair<Double, Double> {
    if (values.isEmpty()) return 0.0 to 0.0
    val sorted = values.sorted()
    fun rank(p: Double) = sorted[((sorted.size * p).toInt()).coerceIn(0, sorted.size - 1)]
    return rank(0.5) to rank(0.95)
}

/** Collects how long each detection took and writes the median and 95th percentile to the log every [window] frames. */
class LatencyLog(private val tag: String, private val window: Int = 60, private val sink: (String) -> Unit = { Log.i(tag, it) }) {
    private val millis = ArrayList<Double>(window)
    private val confidences = ArrayList<Double>(window)

    @Synchronized
    fun record(durationMillis: Double, confidence: Double) {
        millis += durationMillis
        confidences += confidence
        if (millis.size < window) return
        val (median, p95) = summarize(millis)
        val (confMedian, _) = summarize(confidences)
        sink("detect over ${millis.size} frames: median %.1f ms, p95 %.1f ms, median confidence %.2f".format(median, p95, confMedian))
        millis.clear()
        confidences.clear()
    }
}
