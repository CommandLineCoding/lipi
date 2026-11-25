package `in`.commandlinecoding.lipi.data

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.withContext
import java.io.File

/** Documents and their page images. The images live under the app's private storage, so no storage permission is needed. */
class DocumentRepository(context: Context, private val dao: DocumentDao) {
    private val root = File(context.filesDir, "documents")

    fun observe(): Flow<List<DocumentSummary>> = dao.observeSummaries()

    /** One document, or null once it was deleted. */
    fun observeDocument(documentId: Long): Flow<DocumentEntity?> = dao.observeDocument(documentId)

    fun observePagePaths(documentId: Long): Flow<List<String>> = dao.observePagePaths(documentId)

    suspend fun document(documentId: Long): DocumentEntity? = dao.document(documentId)

    suspend fun pagePaths(documentId: Long): List<String> = dao.pagePaths(documentId)

    suspend fun rename(documentId: Long, title: String) = dao.rename(documentId, title)

    /** Stores the pages (as JPEG files already written under [pageFiles]) as a new document and returns its id. */
    suspend fun add(title: String, createdAt: Long, pageFiles: List<File>): Long = withContext(Dispatchers.IO) {
        dao.insertWithPages(DocumentEntity(title = title, createdAt = createdAt, pageCount = pageFiles.size), pageFiles.map { it.absolutePath })
    }

    /** Where a new document's page images should be written. */
    fun newPageFile(documentToken: String, index: Int): File {
        val dir = File(root, documentToken).apply { mkdirs() }
        return File(dir, "page-${index + 1}.jpg")
    }

    suspend fun delete(documentId: Long) = withContext(Dispatchers.IO) {
        dao.pagePaths(documentId).map { File(it).parentFile }.distinct().forEach { it?.deleteRecursively() }
        dao.deleteDocument(documentId)
    }
}
