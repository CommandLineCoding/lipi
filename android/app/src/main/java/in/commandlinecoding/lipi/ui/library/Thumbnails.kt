package `in`.commandlinecoding.lipi.ui.library

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Matrix
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.produceState
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.exifinterface.media.ExifInterface
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/** Decodes an image no larger than about [maxEdge] pixels on its longer side, turned upright, off the main thread. */
@Composable
fun rememberThumbnail(path: String?, maxEdge: Int): ImageBitmap? {
    val bitmap by produceState<ImageBitmap?>(initialValue = null, path, maxEdge) {
        value = if (path == null) null else withContext(Dispatchers.IO) { decodeUpright(path, maxEdge)?.asImageBitmap() }
    }
    return bitmap
}

internal fun sampleSizeFor(width: Int, height: Int, maxEdge: Int): Int {
    var sample = 1
    while (maxOf(width, height) / (sample * 2) >= maxEdge) sample *= 2
    return sample
}

/** The turn and mirror a JPEG's EXIF orientation asks for, as a matrix, or null when it is already upright. */
internal fun exifMatrix(orientation: Int): Matrix? {
    val m = Matrix()
    when (orientation) {
        ExifInterface.ORIENTATION_ROTATE_90 -> m.postRotate(90f)
        ExifInterface.ORIENTATION_ROTATE_180 -> m.postRotate(180f)
        ExifInterface.ORIENTATION_ROTATE_270 -> m.postRotate(270f)
        ExifInterface.ORIENTATION_FLIP_HORIZONTAL -> m.postScale(-1f, 1f)
        ExifInterface.ORIENTATION_FLIP_VERTICAL -> m.postScale(1f, -1f)
        ExifInterface.ORIENTATION_TRANSPOSE -> { m.postRotate(90f); m.postScale(-1f, 1f) }
        ExifInterface.ORIENTATION_TRANSVERSE -> { m.postRotate(270f); m.postScale(-1f, 1f) }
        else -> return null
    }
    return m
}

/** Decodes a photo shrunk to about [maxEdge] pixels and turned the way its EXIF orientation says (camera JPEGs rely on it). */
fun decodeUpright(path: String, maxEdge: Int): Bitmap? {
    val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
    BitmapFactory.decodeFile(path, bounds)
    if (bounds.outWidth <= 0 || bounds.outHeight <= 0) return null
    val options = BitmapFactory.Options().apply { inSampleSize = sampleSizeFor(bounds.outWidth, bounds.outHeight, maxEdge) }
    val decoded = BitmapFactory.decodeFile(path, options) ?: return null
    val orientation = try {
        ExifInterface(path).getAttributeInt(ExifInterface.TAG_ORIENTATION, ExifInterface.ORIENTATION_NORMAL)
    } catch (e: java.io.IOException) {
        ExifInterface.ORIENTATION_NORMAL
    }
    val matrix = exifMatrix(orientation) ?: return decoded
    return Bitmap.createBitmap(decoded, 0, 0, decoded.width, decoded.height, matrix, true)
}
