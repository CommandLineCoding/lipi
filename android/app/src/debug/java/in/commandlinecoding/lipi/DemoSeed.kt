package `in`.commandlinecoding.lipi

import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileOutputStream
import java.util.Random

/**
 * Debug builds only: `adb shell am start -n in.commandlinecoding.lipi/.MainActivity --ez seed_demo true` fills an
 * empty library with the documents drawn in the design, so screens can be compared with it.
 */
object DemoSeed {
    private data class Demo(val title: String, val pages: Int, val daysAgo: Long)

    private val demos = listOf(
        Demo("Lease agreement", 6, 0), Demo("Cafe receipt", 1, 0), Demo("ID card", 2, 1),
        Demo("Lecture notes", 14, 4), Demo("Invoice 0231", 2, 12), Demo("Whiteboard", 1, 13),
    )

    suspend fun maybeSeed(app: LipiApplication, intent: Intent) {
        if (!intent.getBooleanExtra("seed_demo", false)) return
        if (app.documents.observe().first().isNotEmpty()) return
        withContext(Dispatchers.IO) {
            val now = System.currentTimeMillis()
            demos.forEachIndexed { i, demo ->
                val token = "demo-$i"
                val files = (0 until demo.pages).map { p -> app.documents.newPageFile(token, p).also { drawPage(it, i * 31 + p) } }
                app.documents.add(demo.title, now - demo.daysAgo * 86_400_000L - i * 60_000L, files)
            }
        }
    }

    private fun drawPage(file: File, seed: Int) {
        val bmp = Bitmap.createBitmap(600, 780, Bitmap.Config.ARGB_8888)
        val c = Canvas(bmp)
        c.drawColor(Color.WHITE)
        val rnd = Random(seed.toLong())
        val paint = Paint().apply { color = Color.rgb(195, 201, 216) }
        c.drawRoundRect(60f, 70f, 60f + 300f, 70f + 30f, 12f, 12f, paint)
        paint.color = Color.rgb(160, 166, 182)
        var y = 140f
        while (y < 700f) {
            c.drawRoundRect(60f, y, 60f + 300f + rnd.nextInt(180), y + 12f, 6f, 6f, paint)
            y += 34f
        }
        FileOutputStream(file).use { bmp.compress(Bitmap.CompressFormat.JPEG, 85, it) }
    }
}
