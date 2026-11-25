package `in`.commandlinecoding.lipi.data

import android.content.Context
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/** Which theme the app uses: follow the phone, or always light or dark. */
enum class ThemeMode {
    System, Light, Dark;

    companion object {
        /** The mode stored under [value]; anything unknown or missing means following the system. */
        fun fromStored(value: String?): ThemeMode = entries.firstOrNull { it.name == value } ?: System
    }
}

/** Small settings kept in SharedPreferences, which needs no extra library. */
class AppPreferences(context: Context) {
    private val prefs = context.getSharedPreferences("lipi", Context.MODE_PRIVATE)
    private val _theme = MutableStateFlow(ThemeMode.fromStored(prefs.getString(KEY_THEME, null)))
    val theme: StateFlow<ThemeMode> = _theme.asStateFlow()

    fun setTheme(mode: ThemeMode) {
        prefs.edit().putString(KEY_THEME, mode.name).apply()
        _theme.value = mode
    }

    private companion object {
        const val KEY_THEME = "theme"
    }
}
