package `in`.commandlinecoding.lipi.scan

import android.content.Context
import android.net.Uri
import androidx.core.content.FileProvider
import `in`.commandlinecoding.lipi.data.DocumentRepository
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File

/** What can be done with a document that is already in the library: export it again, share it, rename it, delete it. */
class DocumentActions(private val context: Context, private val repository: DocumentRepository) {
    private val writer = PageWriter(context.contentResolver)

    /** Writes the stored pages to [target]: a file for a PDF or one JPG, a folder for several JPGs. Returns false on any failure. */
    suspend fun export(documentId: Long, format: ExportFormat, target: Uri): Boolean = withContext(Dispatchers.IO) {
        val document = repository.document(documentId) ?: return@withContext false
        val pages = storedPages(repository.pagePaths(documentId)) ?: return@withContext false
        when {
            format == ExportFormat.Pdf -> writer.writePdf(pages, target)
            pages.size == 1 -> writer.writeJpeg(pages[0], target)
            else -> writer.writeJpegs(pages, target, document.title)
        }
    }

    /** A PDF of the document in the app's cache and the content address to give to another app, or null on failure. */
    suspend fun sharePdf(documentId: Long): Uri? = withContext(Dispatchers.IO) {
        val document = repository.document(documentId) ?: return@withContext null
        val pages = storedPages(repository.pagePaths(documentId)) ?: return@withContext null
        val dir = File(context.cacheDir, "share").apply { deleteRecursively(); mkdirs() }
        val file = File(dir, sanitizeFileName(document.title) + ".pdf")
        if (!writer.writePdf(pages, file)) return@withContext null
        FileProvider.getUriForFile(context, "${context.packageName}.files", file)
    }

    suspend fun rename(documentId: Long, title: String) {
        val trimmed = title.trim()
        if (trimmed.isNotEmpty()) repository.rename(documentId, trimmed)
    }

    suspend fun delete(documentId: Long) = repository.delete(documentId)
}
