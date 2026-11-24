package `in`.commandlinecoding.lipi.scan

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class LatencyLogTest {
    @Test
    fun summarizeFindsMedianAndTail() {
        val (median, p95) = summarize((1..100).map { it.toDouble() })
        assertEquals(51.0, median, 0.0)
        assertEquals(96.0, p95, 0.0)
    }

    @Test
    fun summarizeOfNothingIsZero() = assertEquals(0.0 to 0.0, summarize(emptyList()))

    @Test
    fun summarizeOfOneNumberIsThatNumber() = assertEquals(7.0 to 7.0, summarize(listOf(7.0)))

    @Test
    fun logWritesOncePerWindow() {
        val lines = ArrayList<String>()
        val log = LatencyLog("t", window = 3, sink = { lines += it })
        repeat(7) { log.record(10.0 + it, 0.5) }
        assertEquals(2, lines.size)
        assertTrue(lines[0].contains("3 frames"))
    }
}
