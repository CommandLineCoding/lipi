package `in`.commandlinecoding.lipi.scan

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext

/** Loads the page detector the first time it is asked for, once, and frees it with [close]. */
class LazyDetector(private val context: Context, private val threads: Int = 4) : AutoCloseable {
    private val mutex = Mutex()
    private var detector: PageDetector? = null
    private var tried = false
    private var closed = false

    /** The detector, or null if this build has none or it could not be loaded. */
    suspend fun get(): PageDetector? = mutex.withLock {
        if (closed) return@withLock null
        if (!tried) {
            tried = true
            detector = withContext(Dispatchers.Default) { PageDetector.create(context, threads) }
        }
        detector
    }

    override fun close() {
        closed = true
        detector?.close()
        detector = null
    }
}
