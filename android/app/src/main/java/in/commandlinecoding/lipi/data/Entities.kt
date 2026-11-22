package `in`.commandlinecoding.lipi.data

import androidx.room.Embedded
import androidx.room.Entity
import androidx.room.ForeignKey
import androidx.room.Index
import androidx.room.PrimaryKey

/** A scanned document. Its pages are in [PageEntity]. */
@Entity(tableName = "documents")
data class DocumentEntity(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    val title: String,
    /** Milliseconds since the epoch. */
    val createdAt: Long,
    val pageCount: Int,
)

/** One page of a document; the image lives in the app's private storage. */
@Entity(
    tableName = "pages",
    foreignKeys = [
        ForeignKey(
            entity = DocumentEntity::class,
            parentColumns = ["id"],
            childColumns = ["documentId"],
            onDelete = ForeignKey.CASCADE,
        ),
    ],
    indices = [Index("documentId")],
)
data class PageEntity(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    val documentId: Long,
    val position: Int,
    val imagePath: String,
)

/** A document with the image of its first page, for the library grid. */
data class DocumentSummary(
    @Embedded val document: DocumentEntity,
    val coverPath: String?,
)
