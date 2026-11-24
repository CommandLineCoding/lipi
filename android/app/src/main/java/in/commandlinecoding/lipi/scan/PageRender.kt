package `in`.commandlinecoding.lipi.scan

import android.graphics.Bitmap
import android.graphics.Matrix
import `in`.commandlinecoding.lipi.NativePages

/** Turns a photo and the user's choices into the page as it will look: flatten, rotate, clean up. All of this is slow; call it off the main thread. */
object PageRender {
    /** The longer side of the photo that is decoded for the on-screen page. */
    const val SOURCE_EDGE = 2048

    /** The longer side of the flattened page shown on screen. */
    const val PREVIEW_SIDE = 1280

    /** Flattens the page inside [quad] (fractions of [source]) into a bitmap whose longer side is at most [maxSide]. */
    fun flatten(source: Bitmap, quad: PageQuad, maxSide: Int): Bitmap? {
        val w = source.width
        val h = source.height
        val corners = FloatArray(8) { if (it % 2 == 0) quad.corners[it / 2].x * w else quad.corners[it / 2].y * h }
        val size = NativePages.estimatePageSize(corners, w, h, maxSide) ?: return null
        val flat = Bitmap.createBitmap(size[0], size[1], Bitmap.Config.ARGB_8888)
        return if (NativePages.warpQuad(source, corners, flat)) flat else null
    }

    fun rotate(bitmap: Bitmap, quarterTurns: Int): Bitmap {
        val turns = ((quarterTurns % 4) + 4) % 4
        if (turns == 0) return bitmap
        val matrix = Matrix().apply { postRotate(90f * turns) }
        return Bitmap.createBitmap(bitmap, 0, 0, bitmap.width, bitmap.height, matrix, true)
    }

    /** Applies the filter and the sliders of [edit] to an already flattened and rotated page. Original returns [flat] itself. */
    fun enhance(flat: Bitmap, edit: PageEdit): Bitmap? {
        val filter = edit.filter.core ?: return flat
        val out = Bitmap.createBitmap(flat.width, flat.height, Bitmap.Config.ARGB_8888)
        return if (NativePages.enhance(flat, out, filter, true, edit.brightnessAmount, edit.contrastAmount)) out else null
    }
}
