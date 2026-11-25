package `in`.commandlinecoding.lipi.scan

import android.content.ContentResolver
import android.graphics.Bitmap
import android.net.Uri
import `in`.commandlinecoding.lipi.data.DocumentRepository
import java.io.File
import java.io.FileOutputStream
import java.io.IOException

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

    private val writer = PageWriter(resolver)

    fun writePdf(pages: List<RenderedPage>, target: Uri) = writer.writePdf(pages, target)
    fun writeJpeg(page: RenderedPage, target: Uri) = writer.writeJpeg(page, target)
    fun writeJpegs(pages: List<RenderedPage>, tree: Uri, title: String) = writer.writeJpegs(pages, tree, title)
}
