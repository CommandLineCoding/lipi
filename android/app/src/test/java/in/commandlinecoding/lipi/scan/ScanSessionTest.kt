package `in`.commandlinecoding.lipi.scan

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder

class ScanSessionTest {
    @get:Rule val tmp = TemporaryFolder()

    @Test fun startsEmpty() = assertEquals(emptyList<Any>(), ScanSession(tmp.newFolder("s")).pages.value)

    @Test fun newFilesAreUniqueAndInsideTheSessionFolder() {
        val dir = tmp.newFolder("s")
        val session = ScanSession(dir)
        val a = session.newRawFile()
        val b = session.newRawFile()
        assertNotEquals(a, b)
        assertEquals(dir, a.parentFile)
    }

    @Test fun addedPagesKeepTheirOrder() {
        val session = ScanSession(tmp.newFolder("s"))
        val files = (1..3).map { session.newRawFile().also { f -> f.writeText("x$it") } }
        files.forEach(session::add)
        assertEquals(files, session.pages.value)
    }

    @Test fun clearRemovesPagesAndFiles() {
        val dir = tmp.newFolder("s")
        val session = ScanSession(dir)
        val f = session.newRawFile().also { it.writeText("x") }
        val stray = java.io.File(dir, "left-over.jpg").also { it.writeText("y") }
        session.add(f)
        session.clear()
        assertTrue(session.pages.value.isEmpty())
        assertFalse(f.exists())
        assertFalse(stray.exists())
    }

    @Test fun outlineIsKeptPerPageAndDroppedWithIt() {
        val session = ScanSession(tmp.newFolder("s"))
        val a = session.newRawFile().also { it.writeText("a") }
        val b = session.newRawFile().also { it.writeText("b") }
        session.add(a)
        session.add(b)
        val quad = PageQuad.inset(0.1f)
        session.setQuad(a, quad)
        assertEquals(quad, session.quadOf(a))
        assertEquals(null, session.quadOf(b))
        session.remove(a)
        assertEquals(listOf(b), session.pages.value)
        assertFalse(a.exists())
        assertEquals(null, session.quadOf(a))
    }

    @Test fun clearForgetsOutlines() {
        val session = ScanSession(tmp.newFolder("s"))
        val f = session.newRawFile().also { it.writeText("x") }
        session.add(f)
        session.setQuad(f, PageQuad.inset())
        session.clear()
        assertEquals(null, session.quadOf(f))
    }

    @Test fun firstUnconfirmedIndexSkipsPagesWithAnOutline() {
        val session = ScanSession(tmp.newFolder("s"))
        assertEquals(0, session.firstUnconfirmedIndex()) // no pages
        val files = (1..3).map { session.newRawFile().also { f -> f.writeText("x$it") } }
        files.forEach(session::add)
        assertEquals(0, session.firstUnconfirmedIndex())
        session.setQuad(files[0], PageQuad.inset())
        session.setQuad(files[1], PageQuad.inset())
        assertEquals(2, session.firstUnconfirmedIndex())
        session.setQuad(files[2], PageQuad.inset())
        assertEquals(0, session.firstUnconfirmedIndex()) // everything confirmed: start from the first page
    }
}
