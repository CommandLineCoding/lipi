package `in`.commandlinecoding.lipi.scan

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class PageQuadTest {
    private val square = PageQuad.inset(0.2f)

    @Test
    fun insetIsValidAndInOrder() {
        assertTrue(PageQuad.isValid(square.corners))
        assertEquals(Corner(0.2f, 0.2f), square.corners[0])
        assertEquals(Corner(0.8f, 0.8f), square.corners[2])
    }

    @Test
    fun cornerMovesWhereAsked() {
        val moved = square.moveCorner(0, 0.1f, 0.15f)
        assertEquals(Corner(0.1f, 0.15f), moved.corners[0])
        assertEquals(square.corners[1], moved.corners[1])
    }

    @Test
    fun cornerStaysInsideTheImage() {
        val moved = square.moveCorner(2, 1.5f, 0.9f)
        assertEquals(1f, moved.corners[2].x, 1e-6f)
        assertEquals(0.9f, moved.corners[2].y, 1e-6f)
        // Far outside the image in the other direction still ends up inside it and valid.
        val far = square.moveCorner(2, -9f, -9f)
        assertTrue(PageQuad.isValid(far.corners))
    }

    @Test
    fun cornerCannotBeDraggedPastTheOppositeSide() {
        // Dragging top-left far beyond bottom-right would fold the shape; the corner stops where it is still convex.
        val moved = square.moveCorner(0, 0.95f, 0.95f)
        assertTrue(PageQuad.isValid(moved.corners))
        assertTrue(moved.corners[0].x < moved.corners[2].x)
        assertTrue(moved.corners[0].y < moved.corners[2].y)
    }

    @Test
    fun edgeShiftsBothItsCorners() {
        val moved = square.moveEdge(0, 0f, -0.1f)
        assertEquals(0.1f, moved.corners[0].y, 1e-6f)
        assertEquals(0.1f, moved.corners[1].y, 1e-6f)
        assertEquals(square.corners[2], moved.corners[2])
    }

    @Test
    fun edgeStopsAtTheImageBorder() {
        val moved = square.moveEdge(1, 5f, 0f)
        assertEquals(1f, moved.corners[1].x, 1e-6f)
        assertEquals(1f, moved.corners[2].x, 1e-6f)
        assertEquals(square.corners[1].y, moved.corners[1].y, 1e-6f)
    }

    @Test
    fun edgeMidpointIsBetweenItsCorners() {
        assertEquals(Corner(0.5f, 0.2f), square.edgeMidpoint(0))
        assertEquals(Corner(0.2f, 0.5f), square.edgeMidpoint(3))
    }

    @Test
    fun fromPixelsScalesToFractions() {
        val q = PageQuad.fromPixels(floatArrayOf(100f, 50f, 900f, 50f, 900f, 450f, 100f, 450f), 1000, 500)
        assertNotNull(q)
        assertEquals(Corner(0.1f, 0.1f), q!!.corners[0])
        assertEquals(Corner(0.9f, 0.9f), q.corners[2])
    }

    @Test
    fun fromPixelsRejectsBadShapes() {
        // Corners in the wrong order (a bow tie) and the wrong number of values.
        assertNull(PageQuad.fromPixels(floatArrayOf(0f, 0f, 100f, 100f, 100f, 0f, 0f, 100f), 100, 100))
        assertNull(PageQuad.fromPixels(floatArrayOf(0f, 0f), 100, 100))
        assertNull(PageQuad.fromPixels(FloatArray(8), 0, 100))
    }

    @Test
    fun tinyOrDegenerateShapesAreInvalid() {
        assertFalse(PageQuad.isValid(listOf(Corner(0.5f, 0.5f), Corner(0.5f, 0.5f), Corner(0.5f, 0.5f), Corner(0.5f, 0.5f))))
        assertFalse(PageQuad.isValid(listOf(Corner(0f, 0f), Corner(1f, 0f), Corner(1f, 1f))))
    }

    @Test
    fun floatArrayRoundTrip() {
        val back = PageQuad.fromPixels(square.toFloatArray().map { it * 100f }.toFloatArray().let { a ->
            FloatArray(8) { a[it] }
        }, 100, 100)
        assertEquals(square.corners[0].x, back!!.corners[0].x, 1e-5f)
    }

    @Test
    fun differsFromNoticesAChange() {
        assertFalse(square.differsFrom(PageQuad.inset(0.2f)))
        assertTrue(square.differsFrom(square.moveCorner(1, 0.7f, 0.3f)))
    }

    // A tall page in the left part of a landscape frame: top-left (0.1, 0.1), bottom-right (0.4, 0.9).
    private val tall = PageQuad.of(listOf(Corner(0.1f, 0.1f), Corner(0.4f, 0.1f), Corner(0.4f, 0.9f), Corner(0.1f, 0.9f)))!!

    @Test
    fun quarterTurnMovesCornersAndKeepsTheOrder() {
        val turned = tall.rotatedClockwise(1)
        // The image turned clockwise: what was at the left is now at the top, and the old left edge is the new top edge.
        assertNear(Corner(0.1f, 0.1f), turned.corners[0])
        assertNear(Corner(0.9f, 0.1f), turned.corners[1])
        assertNear(Corner(0.9f, 0.4f), turned.corners[2])
        assertNear(Corner(0.1f, 0.4f), turned.corners[3])
        assertTrue(PageQuad.isValid(turned.corners))
    }

    @Test
    fun fourQuarterTurnsAreNoTurn() {
        assertSame(tall, tall.rotatedClockwise(4), 1e-5f)
        assertSame(tall.rotatedClockwise(3), tall.rotatedClockwise(1).rotatedClockwise(2), 1e-5f)
        assertSame(tall, tall.rotatedClockwise(0), 1e-5f)
    }

    @Test
    fun negativeTurnsGoTheOtherWay() = assertSame(tall.rotatedClockwise(3), tall.rotatedClockwise(-1), 1e-6f)

    private fun assertNear(expected: Corner, actual: Corner) {
        assertEquals(expected.x, actual.x, 1e-5f)
        assertEquals(expected.y, actual.y, 1e-5f)
    }

    private fun assertSame(expected: PageQuad, actual: PageQuad, tolerance: Float) =
        expected.corners.indices.forEach { assertNear(expected.corners[it], actual.corners[it]).also { _ -> } }
}
