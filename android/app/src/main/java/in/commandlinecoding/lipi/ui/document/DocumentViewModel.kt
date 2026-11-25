package `in`.commandlinecoding.lipi.ui.document

import android.net.Uri
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import `in`.commandlinecoding.lipi.data.DocumentRepository
import `in`.commandlinecoding.lipi.scan.DocumentActions
import `in`.commandlinecoding.lipi.scan.ExportFormat
import `in`.commandlinecoding.lipi.scan.pageSizeLabel
import `in`.commandlinecoding.lipi.scan.readPageSize
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/** What the user is told after an action, shown once. */
enum class Notice { Exported, ExportFailed, ShareFailed }

data class DocumentViewState(
    val loaded: Boolean = false,
    /** True once the document no longer exists, for example after it was deleted. */
    val gone: Boolean = false,
    val title: String = "",
    val createdAt: Long = 0,
    val pages: List<String> = emptyList(),
    /** "A4" or "Letter" when the first page has that shape. */
    val pageSize: String? = null,
    val busy: Boolean = false,
    val notice: Notice? = null,
)

class DocumentViewModel(
    private val documentId: Long,
    repository: DocumentRepository,
    private val actions: DocumentActions,
) : ViewModel() {
    private val extra = MutableStateFlow(Triple<String?, Boolean, Notice?>(null, false, null)) // page size, busy, notice

    val state: StateFlow<DocumentViewState> = combine(repository.observeDocument(documentId), repository.observePagePaths(documentId), extra) { doc, pages, e ->
        if (doc == null) DocumentViewState(loaded = true, gone = true)
        else DocumentViewState(loaded = true, title = doc.title, createdAt = doc.createdAt, pages = pages, pageSize = e.first, busy = e.second, notice = e.third)
    }.stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), DocumentViewState())

    init {
        viewModelScope.launch {
            val first = repository.pagePaths(documentId).firstOrNull() ?: return@launch
            val label = withContext(Dispatchers.IO) { readPageSize(first)?.let { pageSizeLabel(it.first, it.second) } }
            extra.update { it.copy(first = label) }
        }
    }

    fun rename(title: String) {
        viewModelScope.launch { actions.rename(documentId, title) }
    }

    fun delete() {
        viewModelScope.launch { actions.delete(documentId) }
    }

    fun export(format: ExportFormat, target: Uri) {
        extra.update { it.copy(second = true) }
        viewModelScope.launch {
            val ok = actions.export(documentId, format, target)
            extra.update { it.copy(second = false, third = if (ok) Notice.Exported else Notice.ExportFailed) }
        }
    }

    /** Makes the PDF for sharing; [onReady] gets its address on the main thread, or the notice is set when it fails. */
    fun share(onReady: (Uri) -> Unit) {
        extra.update { it.copy(second = true) }
        viewModelScope.launch {
            val uri = actions.sharePdf(documentId)
            extra.update { it.copy(second = false, third = if (uri == null) Notice.ShareFailed else null) }
            if (uri != null) onReady(uri)
        }
    }

    fun noticeShown() = extra.update { it.copy(third = null) }
}
