package `in`.commandlinecoding.lipi.ui.library

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import `in`.commandlinecoding.lipi.data.DocumentRepository
import `in`.commandlinecoding.lipi.data.DocumentSummary
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

data class LibraryState(
    val query: String = "",
    val documents: List<DocumentSummary> = emptyList(),
    /** How many documents exist, whatever the search says. */
    val total: Int = 0,
    val loaded: Boolean = false,
)

/** Case-insensitive match of the search text against document titles. */
fun filterDocuments(documents: List<DocumentSummary>, query: String): List<DocumentSummary> {
    val q = query.trim()
    return if (q.isEmpty()) documents else documents.filter { it.document.title.contains(q, ignoreCase = true) }
}

class LibraryViewModel(private val repository: DocumentRepository) : ViewModel() {
    private val query = MutableStateFlow("")

    val state: StateFlow<LibraryState> = combine(repository.observe(), query) { all, q ->
        LibraryState(query = q, documents = filterDocuments(all, q), total = all.size, loaded = true)
    }.stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), LibraryState())

    fun onQueryChange(value: String) {
        query.value = value
    }

    fun delete(documentId: Long) {
        viewModelScope.launch { repository.delete(documentId) }
    }
}
