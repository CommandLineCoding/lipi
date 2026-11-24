package `in`.commandlinecoding.lipi.scan

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder

class PageEditTest {
    @get:Rule val tmp = TemporaryFolder()

    @Test
    fun slidersMapToTheCoreRange() {
        val edit = PageEdit(brightness = 50, contrast = -100)
        assertEquals(0.5f, edit.brightnessAmount, 1e-6f)
        assertEquals(-1f, edit.contrastAmount, 1e-6f)
        assertEquals(1f, PageEdit(brightness = 250).brightnessAmount, 1e-6f)
    }

    @Test
    fun turningWrapsAfterFour() {
        var edit = PageEdit()
        repeat(5) { edit = edit.turnedClockwise() }
        assertEquals(1, edit.quarterTurns)
    }

    @Test
    fun originalHasNoCoreFilter() {
        assertNull(EnhanceFilter.Original.core)
        assertEquals(listOf(1, 2, 3, 4), EnhanceFilter.values().mapNotNull { it.core })
    }

    @Test
    fun valuesAreShownWithASignOnlyWhenPositive() {
        assertEquals("+12", signed(12))
        assertEquals("0", signed(0))
        assertEquals("-5", signed(-5))
    }

    @Test
    fun sessionKeepsEditsPerPageAndForgetsThemWithThePage() {
        val session = ScanSession(tmp.newFolder("s"))
        val a = session.newRawFile().also { it.writeText("a") }
        val b = session.newRawFile().also { it.writeText("b") }
        session.add(a)
        session.add(b)
        assertEquals(PageEdit(), session.editOf(a))
        val edit = PageEdit(filter = EnhanceFilter.BlackWhite, brightness = 12)
        session.setEdit(a, edit)
        assertEquals(edit, session.editOf(a))
        assertEquals(PageEdit(), session.editOf(b))
        session.remove(a)
        assertEquals(PageEdit(), session.editOf(a))
    }
}
