package `in`.commandlinecoding.lipi.scan

import android.content.ContentResolver
import android.graphics.BitmapFactory
import android.net.Uri
import android.os.ParcelFileDescriptor
import android.provider.DocumentsContract
import `in`.commandlinecoding.lipi.NativePages
import java.io.File
import java.io.IOException

/** A finished page image: the JPEG file and its size in pixels. */
class RenderedPage(val file: File, val width: Int, val height: Int)

/** The size of the JPEG at [path] without decoding it, or null if it cannot be read. */
fun readPageSize(path: String): Pair<Int, Int>? {
    val options = BitmapFactory.Options().apply { inJustDecodeBounds = true }
    BitmapFactory.decodeFile(path, options)
    return if (options.outWidth > 0 && options.outHeight > 0) options.outWidth to options.outHeight else null
}

/** The pages stored at [paths] as [RenderedPage]s, or null if any file is missing or unreadable. */
fun storedPages(paths: List<String>): List<RenderedPage>? {
    if (paths.isEmpty()) return null
    return paths.map { path ->
        val size = readPageSize(path) ?: return null
        RenderedPage(File(path), size.first, size.second)
    }
}

/** Writes finished pages as a PDF or JPEG files, to places the user chose or to a file of the app. */
class PageWriter(private val resolver: ContentResolver) {

    /** Writes the pages as one PDF to [target]. The JPEGs go in as they are, without being compressed again. */
    fun writePdf(pages: List<RenderedPage>, target: Uri): Boolean {
        val descriptor = resolver.openFileDescriptor(target, "wt") ?: return false
        return descriptor.use { pdfTo(pages, it.fd) }
    }

    /** Writes the pages as one PDF to a file of the app, for sharing. */
    fun writePdf(pages: List<RenderedPage>, target: File): Boolean = try {
        ParcelFileDescriptor.open(target, ParcelFileDescriptor.MODE_WRITE_ONLY or ParcelFileDescriptor.MODE_CREATE or ParcelFileDescriptor.MODE_TRUNCATE)
            .use { pdfTo(pages, it.fd) }
    } catch (e: IOException) {
        false
    }

    private fun pdfTo(pages: List<RenderedPage>, fd: Int): Boolean =
        NativePages.writePdf(pages.map { it.file.path }.toTypedArray(), FloatArray(pages.size) { pdfDpi(maxOf(pages[it].width, pages[it].height)) }, fd)

    /** Copies the single page to [target], a file the user chose. */
    fun writeJpeg(page: RenderedPage, target: Uri): Boolean = try {
        resolver.openOutputStream(target, "wt")?.use { out -> page.file.inputStream().use { it.copyTo(out) } } != null
    } catch (e: IOException) {
        false
    }

    /** Writes each page as its own JPEG into the folder [tree] the user chose, named after [title]. Returns false if any failed. */
    fun writeJpegs(pages: List<RenderedPage>, tree: Uri, title: String): Boolean {
        val parent = DocumentsContract.buildDocumentUriUsingTree(tree, DocumentsContract.getTreeDocumentId(tree))
        val names = jpgFileNames(title, pages.size)
        return pages.indices.all { i ->
            val created = try {
                DocumentsContract.createDocument(resolver, parent, ExportFormat.Jpg.mimeType, names[i])
            } catch (e: IllegalArgumentException) {
                null
            } ?: return@all false
            writeJpeg(pages[i], created)
        }
    }
}
