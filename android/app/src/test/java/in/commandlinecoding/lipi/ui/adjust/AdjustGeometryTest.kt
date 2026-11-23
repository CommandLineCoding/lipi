package `in`.commandlinecoding.lipi.ui.adjust

import `in`.commandlinecoding.lipi.scan.PageQuad
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class AdjustGeometryTest {
    @Test
    fun wideImageIsLetterboxedTopAndBottom() {
        val fit = fitInside(400f, 800f, 2000, 1000)
        assertEquals(0f, fit.left, 1e-3f)
        assertEquals(400f, fit.width, 1e-3f)
        assertEquals(200f, fit.height, 1e-3f)
        assertEquals(300f, fit.top, 1e-3f)
    }

    @Test
    fun tallImageIsPillarboxed() {
        val fit = fitInside(400f, 400f, 1000, 2000)
        assertEquals(200f, fit.width, 1e-3f)
        assertEquals(100f, fit.left, 1e-3f)
        assertEquals(400f, fit.height, 1e-3f)
    }

    @Test
    fun mappingRoundTrips() {
        val fit = fitInside(390f, 592f, 3000, 4000)
        assertEquals(0.37f, fit.normX(fit.viewX(0.37f)), 1e-5f)
        assertEquals(0.81f, fit.normY(fit.viewY(0.81f)), 1e-5f)
    }

    @Test
    fun zeroSizedImageDoesNotDivideByZero() {
        val fit = fitInside(100f, 100f, 0, 0)
        assertEquals(100f, fit.width, 1e-3f)
    }

    private val quad = PageQuad.inset(0.2f)
    private val fit = FitRect(0f, 0f, 1000f, 1000f)

    @Test
    fun touchNearACornerPicksIt() = assertEquals(2, hitHandle(quad, fit, 790f, 810f, 60f))

    @Test
    fun touchNearAnEdgeMidpointPicksTheEdge() = assertEquals(Handle.EDGE_BASE + 0, hitHandle(quad, fit, 500f, 215f, 60f))

    @Test
    fun cornerBeatsEdgeWhenBothAreInReach() = assertEquals(0, hitHandle(quad, fit, 230f, 205f, 400f))

    @Test
    fun touchFarFromEverythingPicksNothing() = assertNull(hitHandle(quad, fit, 500f, 500f, 60f))
}
