package `in`.commandlinecoding.lipi

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import `in`.commandlinecoding.lipi.ui.LipiApp
import `in`.commandlinecoding.lipi.ui.theme.LipiTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            LipiTheme {
                LipiApp()
            }
        }
    }
}
