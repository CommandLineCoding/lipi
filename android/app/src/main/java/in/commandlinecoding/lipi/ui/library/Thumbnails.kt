package `in`.commandlinecoding.lipi.ui.library

import android.graphics.BitmapFactory
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.produceState
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/** Decodes an image no larger than about [maxEdge] pixels on its longer side, off the main thread. */
@Composable
fun rememberThumbnail(path: String?, maxEdge: Int): ImageBitmap? {
    val bitmap by produceState<ImageBitmap?>(initialValue = null, path, maxEdge) {
        value = if (path == null) null else withContext(Dispatchers.IO) { decodeSampled(path, maxEdge) }
    }
    return bitmap
}

internal fun sampleSizeFor(width: Int, height: Int, maxEdge: Int): Int {
    var sample = 1
    while (maxOf(width, height) / (sample * 2) >= maxEdge) sample *= 2
    return sample
}

private fun decodeSampled(path: String, maxEdge: Int): ImageBitmap? {
    val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
    BitmapFactory.decodeFile(path, bounds)
    if (bounds.outWidth <= 0 || bounds.outHeight <= 0) return null
    val options = BitmapFactory.Options().apply { inSampleSize = sampleSizeFor(bounds.outWidth, bounds.outHeight, maxEdge) }
    return BitmapFactory.decodeFile(path, options)?.asImageBitmap()
}
