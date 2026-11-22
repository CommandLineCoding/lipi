package `in`.commandlinecoding.lipi

import android.content.Intent

/** Release builds have no demo data. */
object DemoSeed {
    @Suppress("UNUSED_PARAMETER")
    suspend fun maybeSeed(app: LipiApplication, intent: Intent) = Unit
}
