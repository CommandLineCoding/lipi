package `in`.commandlinecoding.lipi.ui.export

import android.content.ContentResolver
import android.graphics.Bitmap
import android.net.Uri
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import `in`.commandlinecoding.lipi.data.DocumentRepository
import `in`.commandlinecoding.lipi.scan.Compression
import `in`.commandlinecoding.lipi.scan.ExportFormat
import `in`.commandlinecoding.lipi.scan.PageExporter
import `in`.commandlinecoding.lipi.scan.PageRender
import `in`.commandlinecoding.lipi.scan.RenderedPage
import `in`.commandlinecoding.lipi.scan.ScanSession
import `in`.commandlinecoding.lipi.scan.defaultTitle
import `in`.commandlinecoding.lipi.scan.pageSizeLabel
import `in`.commandlinecoding.lipi.scan.sanitizeFileName
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

sealed interface SavePhase {
    data object Idle : SavePhase
    data class Saving(val done: Int, val total: Int) : SavePhase
    data class Saved(val name: String) : SavePhase
    data object Failed : SavePhase
}

data class ExportState(
    val title: String,
    /** False once the user has typed a title of their own. */
    val autoNamed: Boolean = true,
    val format: ExportFormat = ExportFormat.Pdf,
    val compression: Compression = Compression.Balanced,
    val pageCount: Int = 0,
    val pageSize: String? = null,
    val cover: Bitmap? = null,
    val phase: SavePhase = SavePhase.Idle,
)

/** Holds the choices on the Export screen and does the saving: render the pages, write them where the user chose, add the document to the library. */
class ExportViewModel(
    private val session: ScanSession,
    private val repository: DocumentRepository,
    resolver: ContentResolver,
    private val now: () -> Long = System::currentTimeMillis,
) : ViewModel() {
    private val exporter = PageExporter(session, repository, resolver)
    private val _state = MutableStateFlow(ExportState(title = defaultTitle(now()), pageCount = session.pages.value.size))
    val state: StateFlow<ExportState> = _state.asStateFlow()

    init {
        // A small copy of the first page for the summary card; its shape gives the page size.
        val first = session.pages.value.firstOrNull()
        if (first != null) {
            viewModelScope.launch {
                val cover = withContext(Dispatchers.Default) { PageRender.renderFinal(first.path, session.quadOf(first), session.editOf(first), COVER_SIDE) }
                _state.update { it.copy(cover = cover, pageSize = cover?.let { c -> pageSizeLabel(c.width, c.height) }) }
            }
        }
    }

    fun setTitle(title: String) {
        val trimmed = title.trim()
        if (trimmed.isNotEmpty()) _state.update { it.copy(title = trimmed, autoNamed = false) }
    }

    fun setFormat(format: ExportFormat) = _state.update { it.copy(format = format) }
    fun setCompression(compression: Compression) = _state.update { it.copy(compression = compression) }
    fun dismissResult() = _state.update { it.copy(phase = SavePhase.Idle) }

    /** The name suggested to the file picker for a single file. */
    fun suggestedFileName(): String = "${sanitizeFileName(_state.value.title)}.${_state.value.format.extension}"

    /** Renders the pages and writes them to [target]: a file for a PDF or a single JPEG, a folder for several JPEGs. */
    fun save(target: Uri) {
        val current = _state.value
        if (current.phase is SavePhase.Saving || current.pageCount == 0) return
        _state.update { it.copy(phase = SavePhase.Saving(0, current.pageCount)) }
        viewModelScope.launch {
            val outcome = withContext(Dispatchers.IO) { export(current, target) }
            _state.update { it.copy(phase = outcome) }
        }
    }

    private suspend fun export(options: ExportState, target: Uri): SavePhase {
        val pages: List<RenderedPage> = exporter.render(options.compression) { done, total ->
            _state.update { it.copy(phase = SavePhase.Saving(done, total)) }
        } ?: return SavePhase.Failed
        val written = when {
            options.format == ExportFormat.Pdf -> exporter.writePdf(pages, target)
            pages.size == 1 -> exporter.writeJpeg(pages[0], target)
            else -> exporter.writeJpegs(pages, target, options.title)
        }
        if (!written) {
            pages.forEach { it.file.delete() }
            return SavePhase.Failed
        }
        // Only a document that reached its destination goes into the library; its pages stay in the app's storage.
        repository.add(options.title, now(), pages.map { it.file })
        session.clear()
        return SavePhase.Saved(options.title)
    }

    private companion object {
        const val COVER_SIDE = 240
    }
}
