package `in`.commandlinecoding.lipi.ui.camera

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.unit.dp
import `in`.commandlinecoding.lipi.scan.LiveOutline
import `in`.commandlinecoding.lipi.ui.adjust.fitInside
import `in`.commandlinecoding.lipi.ui.theme.CameraPalette

/** The page outline drawn over the preview, which shows the frame scaled to fit and centred. Draws nothing without an outline. */
@Composable
fun OutlineOverlay(outline: LiveOutline?, modifier: Modifier = Modifier) {
    Canvas(modifier = modifier.fillMaxSize()) {
        val o = outline ?: return@Canvas
        val fit = fitInside(size.width, size.height, o.width, o.height)
        val points = o.quad.corners.map { Offset(fit.viewX(it.x), fit.viewY(it.y)) }
        val path = Path().apply {
            moveTo(points[0].x, points[0].y)
            for (i in 1..3) lineTo(points[i].x, points[i].y)
            close()
        }
        drawPath(path, CameraPalette.Accent.copy(alpha = 0.2f))
        drawPath(path, CameraPalette.Accent, style = Stroke(width = 2.5.dp.toPx()))
        points.forEach {
            drawCircle(Color.White, 8.dp.toPx(), it)
            drawCircle(CameraPalette.Accent, 8.dp.toPx(), it, style = Stroke(width = 2.dp.toPx()))
        }
    }
}
