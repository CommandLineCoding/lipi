package `in`.commandlinecoding.lipi.data

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.Query
import androidx.room.Transaction
import kotlinx.coroutines.flow.Flow

@Dao
interface DocumentDao {
    @Query(
        """
        SELECT d.*, (SELECT p.imagePath FROM pages p WHERE p.documentId = d.id ORDER BY p.position LIMIT 1) AS coverPath
        FROM documents d ORDER BY d.createdAt DESC
        """,
    )
    fun observeSummaries(): Flow<List<DocumentSummary>>

    @Query("SELECT imagePath FROM pages WHERE documentId = :documentId ORDER BY position")
    suspend fun pagePaths(documentId: Long): List<String>

    @Query("SELECT * FROM documents WHERE id = :documentId")
    fun observeDocument(documentId: Long): Flow<DocumentEntity?>

    @Query("SELECT imagePath FROM pages WHERE documentId = :documentId ORDER BY position")
    fun observePagePaths(documentId: Long): Flow<List<String>>

    @Query("SELECT * FROM documents WHERE id = :documentId")
    suspend fun document(documentId: Long): DocumentEntity?

    @Query("UPDATE documents SET title = :title WHERE id = :documentId")
    suspend fun rename(documentId: Long, title: String)

    @Insert
    suspend fun insertDocument(document: DocumentEntity): Long

    @Insert
    suspend fun insertPages(pages: List<PageEntity>)

    @Query("DELETE FROM documents WHERE id = :documentId")
    suspend fun deleteDocument(documentId: Long)

    @Transaction
    suspend fun insertWithPages(document: DocumentEntity, imagePaths: List<String>): Long {
        val id = insertDocument(document.copy(pageCount = imagePaths.size))
        insertPages(imagePaths.mapIndexed { i, path -> PageEntity(documentId = id, position = i, imagePath = path) })
        return id
    }
}
