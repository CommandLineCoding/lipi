package `in`.commandlinecoding.lipi.scan

import android.content.Context
import android.net.Uri
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.withContext
import java.io.File

/**
 * The pages captured or imported for the document being scanned, before they are cropped, cleaned up and saved.
 * Images are kept as files in the app's cache, so they survive screen changes and are removed when the session ends.
 */
class ScanSession(private val dir: File) {
    private val _pages = MutableStateFlow<List<File>>(emptyList())
    val pages: StateFlow<List<File>> = _pages.asStateFlow()
    private var counter = 0

    /** A new, empty file for the next raw page image. */
    @Synchronized
    fun newRawFile(): File {
        dir.mkdirs()
        counter += 1
        return File(dir, "raw-${System.nanoTime()}-$counter.jpg")
    }

    fun add(file: File) {
        _pages.update { it + file }
    }

    /** Copies an image the user picked into the session. Returns false if it could not be read. */
    suspend fun addImported(context: Context, uri: Uri): Boolean = withContext(Dispatchers.IO) {
        val target = newRawFile()
        try {
            val input = context.contentResolver.openInputStream(uri) ?: return@withContext false
            input.use { source -> target.outputStream().use { source.copyTo(it) } }
            add(target)
            true
        } catch (e: java.io.IOException) {
            target.delete()
            false
        }
    }

    /** Throws away every page of the session and its files. */
    fun clear() {
        _pages.value.forEach { it.delete() }
        _pages.value = emptyList()
        dir.listFiles()?.forEach { it.delete() }
    }
}
