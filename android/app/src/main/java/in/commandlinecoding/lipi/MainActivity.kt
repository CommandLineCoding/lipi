package `in`.commandlinecoding.lipi

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.SystemBarStyle
import androidx.activity.enableEdgeToEdge
import android.graphics.Color
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.launch
import `in`.commandlinecoding.lipi.data.ThemeMode
import `in`.commandlinecoding.lipi.ui.LipiApp
import `in`.commandlinecoding.lipi.ui.theme.LipiTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        lifecycleScope.launch { DemoSeed.maybeSeed(application as LipiApplication, intent) }
        setContent {
            val mode by (application as LipiApplication).preferences.theme.collectAsState()
            val dark = DebugOptions.forcedDarkTheme(intent) ?: when (mode) {
                ThemeMode.System -> isSystemInDarkTheme()
                ThemeMode.Light -> false
                ThemeMode.Dark -> true
            }
            // The clock and icons in the system bars must be light on a dark app and dark on a light one, whatever the phone's
            // own theme is, so the bars are set up again whenever the theme changes.
            DisposableEffect(dark) {
                enableEdgeToEdge(
                    statusBarStyle = SystemBarStyle.auto(Color.TRANSPARENT, Color.TRANSPARENT) { dark },
                    navigationBarStyle = SystemBarStyle.auto(NAV_LIGHT_SCRIM, NAV_DARK_SCRIM) { dark },
                )
                onDispose { }
            }
            LipiTheme(darkTheme = dark) {
                LipiApp()
            }
        }
    }

    private companion object {
        // The scrims enableEdgeToEdge uses by default behind a three-button navigation bar.
        val NAV_LIGHT_SCRIM = Color.argb(0xe6, 0xFF, 0xFF, 0xFF)
        val NAV_DARK_SCRIM = Color.argb(0x80, 0x1b, 0x1b, 0x1b)
    }
}
