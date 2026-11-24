package `in`.commandlinecoding.lipi.ui.enhance

import android.graphics.Bitmap
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import `in`.commandlinecoding.lipi.scan.EnhanceFilter
import `in`.commandlinecoding.lipi.scan.PageEdit
import `in`.commandlinecoding.lipi.scan.PageQuad
import `in`.commandlinecoding.lipi.scan.PageRender
import `in`.commandlinecoding.lipi.scan.ScanSession
import `in`.commandlinecoding.lipi.ui.library.decodeUpright
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/** Everything the Enhance screen shows for the page it is on. [preview] and [original] are null while the page is loading. */
data class EnhanceState(
    val pageIndex: Int = 0,
    val pageCount: Int = 0,
    val edit: PageEdit = PageEdit(),
    val preview: Bitmap? = null,
    val original: Bitmap? = null,
    val thumbnails: Map<EnhanceFilter, Bitmap> = emptyMap(),
    val loading: Boolean = true,
    val failed: Boolean = false,
)

/**
 * Flattens each page of the session once and cleans it up again whenever the filter or a slider moves. Only the newest
 * request is carried out: while the user drags a slider, results that are already out of date are thrown away.
 */
class EnhanceViewModel(private val session: ScanSession) : ViewModel() {
    private val pages = session.pages.value
    private val _state = MutableStateFlow(EnhanceState(pageCount = pages.size))
    val state: StateFlow<EnhanceState> = _state.asStateFlow()

    private var flat: Bitmap? = null // the current page flattened, before turning and cleaning up
    private var loadJob: Job? = null
    private val requests = MutableSharedFlow<PageEdit>(replay = 1, onBufferOverflow = BufferOverflow.DROP_OLDEST)

    init {
        viewModelScope.launch { requests.collectLatest { render(it) } }
        if (pages.isEmpty()) _state.update { it.copy(loading = false, failed = true) } else show(0)
    }

    /** Moves to page [index], which is counted from 0. */
    fun show(index: Int) {
        val file = pages.getOrNull(index) ?: return
        loadJob?.cancel()
        flat = null
        _state.update {
            it.copy(pageIndex = index, edit = session.editOf(file), preview = null, original = null, thumbnails = emptyMap(), loading = true, failed = false)
        }
        loadJob = viewModelScope.launch {
            val loaded = withContext(Dispatchers.Default) { load(index) }
            if (loaded == null) {
                _state.update { it.copy(loading = false, failed = true) }
                return@launch
            }
            flat = loaded
            requests.emit(_state.value.edit)
            val thumbnails = withContext(Dispatchers.Default) { thumbnailsOf(loaded, _state.value.edit.quarterTurns) }
            _state.update { it.copy(thumbnails = thumbnails) }
        }
    }

    fun showNext() = show(_state.value.pageIndex + 1)
    fun showPrevious() = show(_state.value.pageIndex - 1)

    fun setFilter(filter: EnhanceFilter) = change { it.copy(filter = filter) }
    fun setBrightness(value: Int) = change { it.copy(brightness = value.coerceIn(-100, 100)) }
    fun setContrast(value: Int) = change { it.copy(contrast = value.coerceIn(-100, 100)) }

    private fun change(transform: (PageEdit) -> PageEdit) {
        val file = pages.getOrNull(_state.value.pageIndex) ?: return
        val edit = transform(_state.value.edit)
        session.setEdit(file, edit)
        _state.update { it.copy(edit = edit) }
        requests.tryEmit(edit)
    }

    private suspend fun render(edit: PageEdit) {
        val source = flat ?: return
        val (turned, cleaned) = withContext(Dispatchers.Default) {
            val turned = PageRender.rotate(source, edit.quarterTurns)
            turned to PageRender.enhance(turned, edit)
        }
        _state.update {
            // A newer edit or another page has come in meanwhile: this result is out of date.
            if (it.edit != edit || flat !== source) it else it.copy(original = turned, preview = cleaned ?: turned, loading = false, failed = cleaned == null)
        }
    }

    private fun load(index: Int): Bitmap? {
        val file = pages[index]
        val source = decodeUpright(file.path, PageRender.SOURCE_EDGE) ?: return null
        val quad = session.quadOf(file) ?: PageQuad.inset(0.02f)
        return PageRender.flatten(source, quad, PageRender.PREVIEW_SIDE)
    }

    // Small versions of the page with each filter, for the filter row.
    private fun thumbnailsOf(page: Bitmap, quarterTurns: Int): Map<EnhanceFilter, Bitmap> {
        val scale = THUMB_SIDE.toFloat() / maxOf(page.width, page.height)
        val small = Bitmap.createScaledBitmap(page, (page.width * scale).toInt().coerceAtLeast(1), (page.height * scale).toInt().coerceAtLeast(1), true)
        val turned = PageRender.rotate(small, quarterTurns)
        return EnhanceFilter.values().associateWith { filter ->
            PageRender.enhance(turned, PageEdit(filter = filter)) ?: turned
        }
    }

    private companion object {
        const val THUMB_SIDE = 200
    }
}
