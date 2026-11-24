package `in`.commandlinecoding.lipi.ui.export

import `in`.commandlinecoding.lipi.scan.Compression
import `in`.commandlinecoding.lipi.scan.defaultTitle
import `in`.commandlinecoding.lipi.scan.jpgFileNames
import `in`.commandlinecoding.lipi.scan.pageSizeLabel
import `in`.commandlinecoding.lipi.scan.pdfDpi
import `in`.commandlinecoding.lipi.scan.sanitizeFileName
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.Locale

class ExportOptionsTest {
    @Test
    fun fileNamesLoseReservedCharacters() {
        assertEquals("a-b-c", sanitizeFileName("a/b\\c"))
        assertEquals("Lease agreement", sanitizeFileName("  Lease agreement  "))
        assertEquals("Q1-report- final", sanitizeFileName("Q1:report* final"))
        assertEquals("hidden", sanitizeFileName("..hidden"))
    }

    @Test
    fun emptyTitlesGetAName() {
        assertEquals("Scan", sanitizeFileName(""))
        assertEquals("Scan", sanitizeFileName("  ...  "))
    }

    @Test
    fun longTitlesAreCut() = assertTrue(sanitizeFileName("x".repeat(500)).length <= 100)

    @Test
    fun jpgNamesAreNumberedOnlyForSeveralPages() {
        assertEquals(listOf("Lease.jpg"), jpgFileNames("Lease", 1))
        assertEquals(listOf("Lease 1.jpg", "Lease 2.jpg", "Lease 3.jpg"), jpgFileNames("Lease", 3))
    }

    @Test
    fun pageShapesAreRecognised() {
        assertEquals("A4", pageSizeLabel(2100, 2970))
        assertEquals("A4", pageSizeLabel(2970, 2100)) // turned sideways
        assertEquals("Letter", pageSizeLabel(2550, 3300))
        assertNull(pageSizeLabel(1000, 1000))
        assertNull(pageSizeLabel(0, 100))
    }

    @Test
    fun titleStartsWithScanAndHasTheDate() {
        val title = defaultTitle(1_732_466_520_000L, Locale.US) // 24 Nov 2024 UTC; the exact day depends on the zone
        assertTrue(title.startsWith("Scan "))
        assertTrue(title.contains("2024"))
    }

    @Test
    fun compressionLevelsGrowInSizeAndQuality() {
        val levels = Compression.values().toList()
        assertTrue(levels.zipWithNext().all { (a, b) -> a.maxSide < b.maxSide && a.jpegQuality < b.jpegQuality })
    }

    @Test
    fun dpiPutsTheLongSideAtAnA4Sheet() = assertEquals(200f, pdfDpi(2338), 0.5f)
}
