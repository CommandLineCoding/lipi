package `in`.commandlinecoding.lipi.scan

import android.content.ContentResolver
import android.graphics.Bitmap
import android.net.Uri
import android.provider.DocumentsContract
import `in`.commandlinecoding.lipi.NativePages
import `in`.commandlinecoding.lipi.data.DocumentRepository
import java.io.File
import java.io.FileOutputStream
import java.io.IOException

/** A finished page image: the JPEG file and its size in pixels. */
class RenderedPage(val file: File, val width: Int, val height: Int)

/** Turns the pages of a scan into files: the final JPEG of each page, and a PDF or a set of JPEGs at a place the user chose. */
class PageExporter(private val session: ScanSession, private val repository: DocumentRepository, private val resolver: ContentResolver) {

    /**
     * Renders each page of the session at the strength of [compression] and writes it as a JPEG into the app's storage.
     * [progress] gets the number of pages done and the total. Returns the pages, or null (and leaves no files) on failure.
     */
    fun render(compression: Compression, progress: (Int, Int) -> Unit): List<RenderedPage>? {
        val pages = session.pages.value
        val token = System.currentTimeMillis().toString()
        val done = ArrayList<RenderedPage>()
        try {
            pages.forEachIndexed { index, page ->
                progress(index, pages.size)
                val bitmap = PageRender.renderFinal(page.path, session.quadOf(page), session.editOf(page), compression.maxSide) ?: throw IOException("render")
                val target = repository.newPageFile(token, index)
                FileOutputStream(target).use { bitmap.compress(Bitmap.CompressFormat.JPEG, compression.jpegQuality, it) }
                done += RenderedPage(target, bitmap.width, bitmap.height)
            }
            progress(pages.size, pages.size)
            return done
        } catch (e: IOException) {
            done.forEach { it.file.delete() }
            done.firstOrNull()?.file?.parentFile?.delete()
            return null
        }
    }

    /** Writes the pages as one PDF to [target]. */
    fun writePdf(pages: List<RenderedPage>, target: Uri): Boolean {
        val descriptor = resolver.openFileDescriptor(target, "wt") ?: return false
        return descriptor.use {
            NativePages.writePdf(pages.map { p -> p.file.path }.toTypedArray(), FloatArray(pages.size) { i -> pdfDpi(maxOf(pages[i].width, pages[i].height)) }, it.fd)
        }
    }

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
