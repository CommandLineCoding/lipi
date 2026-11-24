package `in`.commandlinecoding.lipi.ui.adjust

import android.graphics.Bitmap
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.clipPath
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import `in`.commandlinecoding.lipi.NativeCore
import `in`.commandlinecoding.lipi.R
import `in`.commandlinecoding.lipi.scan.PageQuad
import `in`.commandlinecoding.lipi.scan.ScanSession
import `in`.commandlinecoding.lipi.scan.differsFrom
import `in`.commandlinecoding.lipi.ui.components.LightSystemBarIcons
import `in`.commandlinecoding.lipi.ui.components.LipiIcon
import `in`.commandlinecoding.lipi.ui.library.decodeUpright
import `in`.commandlinecoding.lipi.ui.theme.CameraPalette
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/** The longer side the page is shown and searched at. The full-size photo is used later, when the page is flattened. */
private const val WORKING_EDGE = 1600
private const val LOUPE_ZOOM = 2.5f

/**
 * Lets the user confirm or correct the four corners of each captured page, one page after the other.
 * [onRetake] is called after the current page was removed; [onDone] after the last page was confirmed.
 */
@Composable
fun AdjustScreen(session: ScanSession, onBack: () -> Unit, onRetake: () -> Unit, onDone: () -> Unit) {
    val pages by session.pages.collectAsState()
    var index by rememberSaveable { mutableIntStateOf(0) }
    val page = pages.getOrNull(index)
    val scope = rememberCoroutineScope()

    // Set before a button removes the last page, so that the check below does not also leave a second time.
    var leaving by remember { mutableStateOf(false) }
    // Nothing to adjust (for example the pages were discarded): leave.
    LaunchedEffect(page == null) { if (page == null && !leaving) onBack() }
    if (page == null) return

    var bitmap by remember(page) { mutableStateOf<Bitmap?>(null) }
    var quad by remember(page) { mutableStateOf(session.quadOf(page) ?: PageQuad.inset()) }
    var detected by remember(page) { mutableStateOf<PageQuad?>(null) } // what Auto found, to tell when the user changed it
    var searching by remember(page) { mutableStateOf(session.quadOf(page) == null) }
    var notFound by remember(page) { mutableStateOf(false) }

    // Finds the page in the working-size image. The same call serves the first look and the Auto button.
    suspend fun runAuto(source: Bitmap) {
        searching = true
        val found = withContext(Dispatchers.Default) {
            NativeCore.detectPageClassical(source)?.let { PageQuad.fromPixels(it, source.width, source.height) }
        }
        notFound = found == null
        if (found != null) { quad = found; detected = found } else { quad = PageQuad.inset(); detected = null }
        searching = false
    }

    LaunchedEffect(page) {
        val decoded = withContext(Dispatchers.IO) { decodeUpright(page.path, WORKING_EDGE) } ?: return@LaunchedEffect
        bitmap = decoded
        if (session.quadOf(page) == null) runAuto(decoded)
    }

    LightSystemBarIcons()
    val autoActive = detected?.let { !quad.differsFrom(it) } == true
    val close = { onBack() }
    BackHandler(onBack = close)

    Column(modifier = Modifier.fillMaxSize().background(CameraPalette.Background)) {
        Header(autoActive = autoActive, onBack = close, onAuto = { bitmap?.let { b -> scope.launch { runAuto(b) } } })
        Box(modifier = Modifier.weight(1f).fillMaxWidth()) {
            val shown = bitmap
            if (shown != null) {
                CornerEditor(bitmap = shown, quad = quad, onQuad = { quad = it })
            }
            if (searching) {
                Text(
                    stringResource(R.string.adjust_loading),
                    style = MaterialTheme.typography.bodyMedium,
                    color = CameraPalette.ModeInactive,
                    modifier = Modifier.align(Alignment.Center),
                )
            }
        }
        val hint = when {
            notFound -> stringResource(R.string.adjust_not_found)
            pages.size > 1 -> stringResource(R.string.page_of, index + 1, pages.size) + " · " + stringResource(R.string.adjust_hint)
            else -> stringResource(R.string.adjust_hint)
        }
        Text(
            hint,
            style = MaterialTheme.typography.bodySmall,
            color = CameraPalette.ModeInactive,
            modifier = Modifier.fillMaxWidth().padding(horizontal = 20.dp, vertical = 14.dp),
            textAlign = androidx.compose.ui.text.style.TextAlign.Center,
        )
        Row(
            modifier = Modifier.fillMaxWidth().navigationBarsPadding().padding(start = 20.dp, end = 20.dp, bottom = 16.dp),
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            ActionButton(
                label = stringResource(R.string.adjust_retake),
                background = CameraPalette.Control,
                modifier = Modifier.width(104.dp),
                onClick = { leaving = true; session.remove(page); onRetake() },
            )
            ActionButton(
                label = stringResource(R.string.adjust_next),
                background = CameraPalette.Indigo,
                modifier = Modifier.weight(1f),
                trailingIcon = R.drawable.ic_chevron_right,
                enabled = bitmap != null && !searching,
                onClick = {
                    session.setQuad(page, quad)
                    if (index + 1 < pages.size) index += 1 else { leaving = true; onDone() }
                },
            )
        }
    }
}

@Composable
private fun Header(autoActive: Boolean, onBack: () -> Unit, onAuto: () -> Unit) {
    Box(modifier = Modifier.fillMaxWidth().statusBarsPadding().padding(horizontal = 16.dp, vertical = 8.dp).height(40.dp)) {
        Box(
            modifier = Modifier
                .size(40.dp)
                .align(Alignment.CenterStart)
                .background(CameraPalette.Control, CircleShape)
                .clickable(role = Role.Button, onClick = onBack)
                .semantics { contentDescription = "" },
            contentAlignment = Alignment.Center,
        ) { LipiIcon(R.drawable.ic_chevron_left, Color.White) }
        Text(
            stringResource(R.string.adjust_title),
            style = MaterialTheme.typography.titleMedium,
            color = Color.White,
            modifier = Modifier.align(Alignment.Center),
        )
        Row(
            modifier = Modifier
                .align(Alignment.CenterEnd)
                .height(30.dp)
                .background(if (autoActive) CameraPalette.Indigo else CameraPalette.Control, RoundedCornerShape(50))
                .clickable(role = Role.Button, onClick = onAuto)
                .padding(horizontal = 12.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            LipiIcon(R.drawable.ic_sparkle, Color.White, size = 14.dp)
            Text(stringResource(R.string.adjust_auto), style = MaterialTheme.typography.labelMedium, color = Color.White, fontWeight = FontWeight.SemiBold)
        }
    }
}

@Composable
private fun ActionButton(
    label: String,
    background: Color,
    modifier: Modifier = Modifier,
    trailingIcon: Int? = null,
    enabled: Boolean = true,
    onClick: () -> Unit,
) {
    Row(
        modifier = modifier
            .height(54.dp)
            .background(background.copy(alpha = if (enabled) background.alpha else background.alpha * 0.5f), RoundedCornerShape(20.dp))
            .clickable(enabled = enabled, role = Role.Button, onClick = onClick),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.Center,
    ) {
        Text(label, style = MaterialTheme.typography.titleSmall, color = Color.White, fontWeight = FontWeight.SemiBold)
        if (trailingIcon != null) LipiIcon(trailingIcon, Color.White, modifier = Modifier.padding(start = 6.dp))
    }
}

/** The photo with the draggable outline, the corner and edge handles, and the loupe while one is held. */
@Composable
private fun CornerEditor(bitmap: Bitmap, quad: PageQuad, onQuad: (PageQuad) -> Unit) {
    val density = LocalDensity.current
    val image = remember(bitmap) { bitmap.asImageBitmap() }
    val currentQuad by rememberUpdatedState(quad)
    val currentOnQuad by rememberUpdatedState(onQuad)
    var active by remember { mutableStateOf<Int?>(null) }
    var size by remember { mutableStateOf(IntSize.Zero) }
    val fit = fitInside(size.width.toFloat(), size.height.toFloat(), bitmap.width, bitmap.height)
    val currentFit by rememberUpdatedState(fit)

    val hitRadius = with(density) { 36.dp.toPx() }
    val cornerRadius = with(density) { 13.dp.toPx() }
    val edgeRadius = with(density) { 6.dp.toPx() }
    val stroke = with(density) { 2.dp.toPx() }
    val loupeRadius = with(density) { 48.dp.toPx() }
    val loupeGap = with(density) { 40.dp.toPx() }
    val indigo = CameraPalette.Indigo

    Canvas(
        modifier = Modifier
            .fillMaxSize()
            .pointerInput(Unit) {
                var handle: Int? = null
                var lastFinger = Offset.Zero
                detectDragGestures(
                    onDragStart = { start ->
                        handle = hitHandle(currentQuad, currentFit, start.x, start.y, hitRadius)
                        lastFinger = start
                        active = handle
                    },
                    onDragEnd = { handle = null; active = null },
                    onDragCancel = { handle = null; active = null },
                ) { change, _ ->
                    val h = handle ?: return@detectDragGestures
                    val f = currentFit
                    if (Handle.isEdge(h)) {
                        val d = change.position - lastFinger
                        currentOnQuad(currentQuad.moveEdge(h - Handle.EDGE_BASE, d.x / f.width, d.y / f.height))
                    } else {
                        // The corner keeps its offset from the finger, so it does not jump to the fingertip.
                        val c = currentQuad.corners[h]
                        val d = change.position - lastFinger
                        currentOnQuad(currentQuad.moveCorner(h, c.x + d.x / f.width, c.y + d.y / f.height))
                    }
                    lastFinger = change.position
                    change.consume()
                }
            },
    ) {
        if (size != IntSize(this.size.width.toInt(), this.size.height.toInt())) size = IntSize(this.size.width.toInt(), this.size.height.toInt())
        drawImage(
            image,
            dstOffset = IntOffset(fit.left.toInt(), fit.top.toInt()),
            dstSize = IntSize(fit.width.toInt(), fit.height.toInt()),
        )
        val pts = quad.corners.map { Offset(fit.viewX(it.x), fit.viewY(it.y)) }
        val outline = Path().apply {
            moveTo(pts[0].x, pts[0].y)
            for (i in 1..3) lineTo(pts[i].x, pts[i].y)
            close()
        }
        drawPath(outline, indigo.copy(alpha = 0.12f))
        drawPath(outline, indigo, style = Stroke(width = stroke))
        for (i in 0 until 4) {
            val m = quad.edgeMidpoint(i)
            val p = Offset(fit.viewX(m.x), fit.viewY(m.y))
            drawCircle(Color.White, edgeRadius, p)
            drawCircle(indigo, edgeRadius, p, style = Stroke(width = stroke))
        }
        pts.forEach {
            drawCircle(Color.White, cornerRadius, it)
            drawCircle(indigo, cornerRadius, it, style = Stroke(width = stroke * 1.5f))
        }

        active?.let { h ->
            val focus = if (Handle.isEdge(h)) quad.edgeMidpoint(h - Handle.EDGE_BASE) else quad.corners[h]
            val at = Offset(fit.viewX(focus.x), fit.viewY(focus.y))
            // The loupe sits above the finger; near the top of the picture it moves to the side of it that has room.
            var centre = Offset(at.x, at.y - loupeGap - loupeRadius)
            if (centre.y - loupeRadius < 0f) centre = Offset(if (at.x < this.size.width / 2) at.x + loupeGap + loupeRadius else at.x - loupeGap - loupeRadius, at.y)
            centre = Offset(centre.x.coerceIn(loupeRadius, this.size.width - loupeRadius), centre.y.coerceIn(loupeRadius, this.size.height - loupeRadius))
            val circle = Path().apply { addOval(androidx.compose.ui.geometry.Rect(centre, loupeRadius)) }
            // The part of the photo under the finger, in photo pixels, shown LOUPE_ZOOM times larger.
            val scale = fit.width / bitmap.width
            val half = loupeRadius / LOUPE_ZOOM / scale
            val cx = focus.x * bitmap.width
            val cy = focus.y * bitmap.height
            clipPath(circle) {
                drawRect(Color.Black, topLeft = centre - Offset(loupeRadius, loupeRadius), size = androidx.compose.ui.geometry.Size(loupeRadius * 2, loupeRadius * 2))
                drawImage(
                    image,
                    srcOffset = IntOffset((cx - half).toInt(), (cy - half).toInt()),
                    srcSize = IntSize((half * 2).toInt().coerceAtLeast(1), (half * 2).toInt().coerceAtLeast(1)),
                    dstOffset = IntOffset((centre.x - loupeRadius).toInt(), (centre.y - loupeRadius).toInt()),
                    dstSize = IntSize((loupeRadius * 2).toInt(), (loupeRadius * 2).toInt()),
                )
                drawLine(indigo, Offset(centre.x - loupeRadius, centre.y), Offset(centre.x + loupeRadius, centre.y), strokeWidth = 1.dp.toPx())
                drawLine(indigo, Offset(centre.x, centre.y - loupeRadius), Offset(centre.x, centre.y + loupeRadius), strokeWidth = 1.dp.toPx())
            }
            drawCircle(Color.White, loupeRadius, centre, style = Stroke(width = 3.dp.toPx()))
        }
    }
}
