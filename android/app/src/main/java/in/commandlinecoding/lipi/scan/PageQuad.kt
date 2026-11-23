package `in`.commandlinecoding.lipi.scan

import kotlin.math.abs
import kotlin.math.hypot

/** A point as a fraction of the image width and height, so it does not depend on the size the image is shown or decoded at. */
data class Corner(val x: Float, val y: Float)

/**
 * The four corners of a page, in the order the core uses: top-left, top-right, bottom-right, bottom-left, as seen on
 * the image. Instances are always convex and inside the image; the moving functions return a quad that keeps this.
 */
class PageQuad private constructor(val corners: List<Corner>) {

    /** Moves corner [index] towards ([x], [y]) as far as the shape stays valid. */
    fun moveCorner(index: Int, x: Float, y: Float): PageQuad {
        val target = Corner(x.coerceIn(0f, 1f), y.coerceIn(0f, 1f))
        return slide { t ->
            val from = corners[index]
            corners.toMutableList().also { it[index] = Corner(from.x + (target.x - from.x) * t, from.y + (target.y - from.y) * t) }
        }
    }

    /** Moves the edge from corner [index] to the next one by ([dx], [dy]), as far as the shape stays valid. */
    fun moveEdge(index: Int, dx: Float, dy: Float): PageQuad {
        val a = index
        val b = (index + 1) % 4
        // Keep both ends inside the image first, so that the move is a shift and does not squash the edge.
        val cx = dx.coerceIn(-minOf(corners[a].x, corners[b].x), 1f - maxOf(corners[a].x, corners[b].x))
        val cy = dy.coerceIn(-minOf(corners[a].y, corners[b].y), 1f - maxOf(corners[a].y, corners[b].y))
        return slide { t ->
            corners.toMutableList().also {
                it[a] = Corner(corners[a].x + cx * t, corners[a].y + cy * t)
                it[b] = Corner(corners[b].x + cx * t, corners[b].y + cy * t)
            }
        }
    }

    /** The position of edge [index]'s midpoint. */
    fun edgeMidpoint(index: Int): Corner {
        val a = corners[index]
        val b = corners[(index + 1) % 4]
        return Corner((a.x + b.x) / 2, (a.y + b.y) / 2)
    }

    /** Eight numbers, x then y of each corner, as fractions of the image. */
    fun toFloatArray(): FloatArray = FloatArray(8) { if (it % 2 == 0) corners[it / 2].x else corners[it / 2].y }

    // Finds the largest step t in 0..1 along the way to the wanted shape that is still valid (the current shape is).
    private fun slide(shape: (Float) -> List<Corner>): PageQuad {
        val full = shape(1f)
        if (isValid(full)) return PageQuad(full)
        var low = 0f
        var high = 1f
        repeat(14) {
            val mid = (low + high) / 2
            if (isValid(shape(mid))) low = mid else high = mid
        }
        return PageQuad(shape(low))
    }

    override fun equals(other: Any?) = other is PageQuad && other.corners == corners
    override fun hashCode() = corners.hashCode()
    override fun toString() = "PageQuad($corners)"

    companion object {
        private const val MIN_SIDE = 0.03f
        private const val MIN_TURN = 1e-4f

        /** True if the corners are in the image, no side is tiny and every turn goes the same way (a convex shape). */
        fun isValid(c: List<Corner>): Boolean {
            if (c.size != 4) return false
            for (i in 0 until 4) {
                val a = c[i]
                val b = c[(i + 1) % 4]
                val d = c[(i + 2) % 4]
                if (!a.x.isFinite() || !a.y.isFinite() || a.x < 0f || a.x > 1f || a.y < 0f || a.y > 1f) return false
                if (hypot(b.x - a.x, b.y - a.y) < MIN_SIDE) return false
                // On screen (y down) the order top-left, top-right, bottom-right is a clockwise turn, which is positive here.
                if ((b.x - a.x) * (d.y - b.y) - (b.y - a.y) * (d.x - b.x) <= MIN_TURN) return false
            }
            return true
        }

        fun of(corners: List<Corner>): PageQuad? = if (isValid(corners)) PageQuad(corners) else null

        /** From eight pixel coordinates (x, y per corner) in an image [width] by [height]; null if they do not make a valid quad. */
        fun fromPixels(xy: FloatArray, width: Int, height: Int): PageQuad? {
            if (xy.size != 8 || width <= 0 || height <= 0) return null
            val corners = List(4) { Corner((xy[it * 2] / width).coerceIn(0f, 1f), (xy[it * 2 + 1] / height).coerceIn(0f, 1f)) }
            return of(corners)
        }

        /** A rectangle a little inside the image, for when no page was found. */
        fun inset(fraction: Float = 0.1f): PageQuad {
            val lo = fraction
            val hi = 1f - fraction
            return PageQuad(listOf(Corner(lo, lo), Corner(hi, lo), Corner(hi, hi), Corner(lo, hi)))
        }
    }
}

/** True when two quads differ by more than a hair, used to tell whether the user changed the detected outline. */
fun PageQuad.differsFrom(other: PageQuad, tolerance: Float = 1e-4f): Boolean =
    corners.indices.any { abs(corners[it].x - other.corners[it].x) > tolerance || abs(corners[it].y - other.corners[it].y) > tolerance }
