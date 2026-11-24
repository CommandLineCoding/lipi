package `in`.commandlinecoding.lipi.scan

/** The look of a page. [core] is the core's filter number; Original leaves the flattened page as it is. */
enum class EnhanceFilter(val core: Int?) {
    Original(null),
    Auto(1),
    BlackWhite(2),
    Gray(3),
    Color(4),
}

/** What the user chose for one page on the Enhance screen. Brightness and contrast go from -100 to 100, as shown on the sliders. */
data class PageEdit(
    val filter: EnhanceFilter = EnhanceFilter.Auto,
    val brightness: Int = 0,
    val contrast: Int = 0,
    /** Quarter turns clockwise, 0 to 3. */
    val quarterTurns: Int = 0,
) {
    /** Brightness as the core wants it, -1 to 1. */
    val brightnessAmount: Float get() = brightness.coerceIn(-100, 100) / 100f

    /** Contrast as the core wants it, -1 to 1. */
    val contrastAmount: Float get() = contrast.coerceIn(-100, 100) / 100f

    fun turnedClockwise(): PageEdit = copy(quarterTurns = (quarterTurns + 1) % 4)
}

/** "+12", "0" or "-5": a slider value as the screen shows it. */
fun signed(value: Int): String = if (value > 0) "+$value" else value.toString()
