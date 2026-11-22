package `in`.commandlinecoding.lipi

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.launch
import `in`.commandlinecoding.lipi.ui.LipiApp
import `in`.commandlinecoding.lipi.ui.theme.LipiTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        lifecycleScope.launch { DemoSeed.maybeSeed(application as LipiApplication, intent) }
        setContent {
            LipiTheme {
                LipiApp()
            }
        }
    }
}
