package `in`.commandlinecoding.lipi.ui.adjust

import `in`.commandlinecoding.lipi.scan.PageQuad
import kotlin.math.hypot

/** Where an image of a given size is drawn inside a view when it is scaled to fit and centred. */
data class FitRect(val left: Float, val top: Float, val width: Float, val height: Float) {
    fun viewX(nx: Float) = left + nx * width
    fun viewY(ny: Float) = top + ny * height
    fun normX(vx: Float) = (vx - left) / width
    fun normY(vy: Float) = (vy - top) / height
}

fun fitInside(viewWidth: Float, viewHeight: Float, imageWidth: Int, imageHeight: Int): FitRect {
    if (imageWidth <= 0 || imageHeight <= 0) return FitRect(0f, 0f, viewWidth, viewHeight)
    val scale = minOf(viewWidth / imageWidth, viewHeight / imageHeight)
    val w = imageWidth * scale
    val h = imageHeight * scale
    return FitRect((viewWidth - w) / 2, (viewHeight - h) / 2, w, h)
}

/** What can be dragged: corners 0 to 3, and the edge from corner i to i + 1 as 4 + i. */
object Handle {
    const val EDGE_BASE = 4
    fun isEdge(handle: Int) = handle >= EDGE_BASE
}

/** The handle nearest to a touch within [radius] view pixels. Corners win over edge midpoints when both are in reach. */
fun hitHandle(quad: PageQuad, fit: FitRect, x: Float, y: Float, radius: Float): Int? {
    fun distance(nx: Float, ny: Float) = hypot(fit.viewX(nx) - x, fit.viewY(ny) - y)
    val corner = quad.corners.indices.minByOrNull { distance(quad.corners[it].x, quad.corners[it].y) }!!
    if (distance(quad.corners[corner].x, quad.corners[corner].y) <= radius) return corner
    val edge = (0 until 4).minByOrNull { val m = quad.edgeMidpoint(it); distance(m.x, m.y) }!!
    val mid = quad.edgeMidpoint(edge)
    return if (distance(mid.x, mid.y) <= radius) Handle.EDGE_BASE + edge else null
}
