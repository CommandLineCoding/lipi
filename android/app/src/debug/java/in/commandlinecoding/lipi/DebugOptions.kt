package `in`.commandlinecoding.lipi

import android.content.Intent

/** Debug builds only: `--es theme light` or `--es theme dark` overrides the system theme, to compare both with the design. */
object DebugOptions {
    fun forcedDarkTheme(intent: Intent): Boolean? = when (intent.getStringExtra("theme")) {
        "light" -> false
        "dark" -> true
        else -> null
    }
}
