package `in`.commandlinecoding.lipi.scan

import java.text.DateFormat
import java.util.Date
import java.util.Locale
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min

enum class ExportFormat(val mimeType: String, val extension: String) {
    Pdf("application/pdf", "pdf"),
    Jpg("image/jpeg", "jpg"),
}

/** How hard the page images are compressed: the longer side in pixels and the JPEG quality. */
enum class Compression(val maxSide: Int, val jpegQuality: Int) {
    Small(1600, 65),
    Balanced(2400, 80),
    High(3200, 92),
}

/** The title a new scan gets until the user changes it, for example "Scan 24 Nov 2026, 4:42 pm". */
fun defaultTitle(timeMillis: Long, locale: Locale = Locale.getDefault()): String {
    val text = DateFormat.getDateTimeInstance(DateFormat.MEDIUM, DateFormat.SHORT, locale).format(Date(timeMillis))
    return "Scan $text"
}

private val illegalInFileNames = Regex("""[\\/:*?"<>|\p{Cntrl}]""")

/** A title made safe to use as a file name: no path or reserved characters, no leading dots, not empty. */
fun sanitizeFileName(title: String): String {
    val cleaned = title.replace(illegalInFileNames, "-").trim().trimStart('.').trim().take(100).trim()
    return cleaned.ifEmpty { "Scan" }
}

/** File names for the pages of a JPG export: the title alone for one page, "Title 1", "Title 2" and so on for several. */
fun jpgFileNames(title: String, pageCount: Int): List<String> {
    val base = sanitizeFileName(title)
    return if (pageCount <= 1) listOf("$base.jpg") else List(pageCount) { "$base ${it + 1}.jpg" }
}

/** "A4" or "Letter" when the page has that shape (within 4%), otherwise null. */
fun pageSizeLabel(width: Int, height: Int): String? {
    if (width <= 0 || height <= 0) return null
    val ratio = max(width, height).toDouble() / min(width, height)
    fun near(target: Double) = abs(ratio - target) / target <= 0.04
    return when {
        near(Math.sqrt(2.0)) -> "A4"
        near(11.0 / 8.5) -> "Letter"
        else -> null
    }
}

/** The resolution to give a page of [longSidePx] pixels in a PDF so that its long side is about that of an A4 sheet (11.69 inches). */
fun pdfDpi(longSidePx: Int): Float = (longSidePx / 11.69f).coerceAtLeast(1f)
