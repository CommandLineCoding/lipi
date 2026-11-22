package `in`.commandlinecoding.lipi.ui.library

import `in`.commandlinecoding.lipi.data.DocumentEntity
import `in`.commandlinecoding.lipi.data.DocumentSummary
import org.junit.Assert.assertEquals
import org.junit.Test

class LibraryLogicTest {
    private fun doc(id: Long, title: String) = DocumentSummary(DocumentEntity(id, title, createdAt = id, pageCount = 1), coverPath = null)
    private val docs = listOf(doc(1, "Lease agreement"), doc(2, "Cafe receipt"), doc(3, "Invoice 0231"))

    @Test fun emptyQueryKeepsEverything() = assertEquals(docs, filterDocuments(docs, "  "))

    @Test fun matchIgnoresCaseAndSurroundingSpaces() =
        assertEquals(listOf(docs[0]), filterDocuments(docs, " LEASE "))

    @Test fun matchesAnywhereInTheTitle() =
        assertEquals(listOf(docs[1], docs[2]), filterDocuments(docs, "ce"))

    @Test fun noMatchGivesAnEmptyList() = assertEquals(emptyList<DocumentSummary>(), filterDocuments(docs, "passport"))

    @Test fun thumbnailsAreNotDecodedLargerThanNeeded() {
        assertEquals(1, sampleSizeFor(600, 780, 480))
        assertEquals(4, sampleSizeFor(4000, 3000, 900))
        assertEquals(1, sampleSizeFor(300, 200, 480))
    }
}
