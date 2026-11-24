package `in`.commandlinecoding.lipi.ui.enhance

import android.graphics.Bitmap
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.compose.LifecycleEventEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.input.pointer.positionChange
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import `in`.commandlinecoding.lipi.R
import `in`.commandlinecoding.lipi.scan.EnhanceFilter
import `in`.commandlinecoding.lipi.scan.signed
import `in`.commandlinecoding.lipi.ui.components.LipiIcon
import `in`.commandlinecoding.lipi.ui.components.RoundIconButton
import `in`.commandlinecoding.lipi.ui.theme.LocalLipiColors
import kotlin.math.abs
import kotlin.math.roundToInt

/** Look and feel of one page: the cleaned-up preview, the filter row and the brightness and contrast sliders. */
@Composable
fun EnhanceScreen(viewModel: EnhanceViewModel, onBack: () -> Unit, onDone: () -> Unit, onCrop: (Int) -> Unit, onAddPage: () -> Unit) {
    val state by viewModel.state.collectAsState()
    var comparing by remember { mutableStateOf(false) }
    // The crop tool leaves this screen; when it comes back the page may have a new outline.
    LifecycleEventEffect(Lifecycle.Event.ON_RESUME) { viewModel.reloadIfCropped() }

    Column(modifier = Modifier.fillMaxSize().background(MaterialTheme.colorScheme.background)) {
        TopBar(state.pageIndex, state.pageCount, onBack = onBack, onDone = onDone)
        Column(
            modifier = Modifier.weight(1f).verticalScroll(rememberScrollState()).padding(bottom = 16.dp),
        ) {
            PreviewCard(
                shown = if (comparing) state.original else state.preview,
                comparing = comparing,
                filter = state.edit.filter,
                loading = state.loading,
                failed = state.failed,
                onCompare = { comparing = it },
                onSwipe = { step -> if (step > 0) viewModel.showNext() else viewModel.showPrevious() },
            )
            Text(
                stringResource(R.string.enhance_filters),
                style = MaterialTheme.typography.titleSmall,
                color = MaterialTheme.colorScheme.onSurface,
                modifier = Modifier.padding(start = 20.dp, top = 16.dp, bottom = 10.dp),
            )
            FilterRow(selected = state.edit.filter, thumbnails = state.thumbnails, onSelect = viewModel::setFilter)
            Spacer(Modifier.height(16.dp))
            LabeledSlider(stringResource(R.string.enhance_brightness), state.edit.brightness, viewModel::setBrightness)
            Spacer(Modifier.height(12.dp))
            LabeledSlider(stringResource(R.string.enhance_contrast), state.edit.contrast, viewModel::setContrast)
        }
        ToolsBar(
            enabled = !state.loading && !state.failed,
            onRotate = viewModel::rotate,
            onCrop = { onCrop(state.pageIndex) },
            onAddPage = onAddPage,
        )
    }
}

/** Rotate, crop and add page, on a raised bar along the bottom as in the design. */
@Composable
private fun ToolsBar(enabled: Boolean, onRotate: () -> Unit, onCrop: () -> Unit, onAddPage: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .background(MaterialTheme.colorScheme.surface, RoundedCornerShape(topStart = 28.dp, topEnd = 28.dp))
            .navigationBarsPadding()
            .padding(horizontal = 20.dp, vertical = 14.dp),
        horizontalArrangement = Arrangement.SpaceEvenly,
    ) {
        Tool(R.drawable.ic_rotate, stringResource(R.string.tool_rotate), enabled, onRotate)
        Tool(R.drawable.ic_crop, stringResource(R.string.tool_crop), enabled, onCrop)
        Tool(R.drawable.ic_plus, stringResource(R.string.tool_add_page), enabled, onAddPage)
    }
}

@Composable
private fun Tool(icon: Int, label: String, enabled: Boolean, onClick: () -> Unit) {
    Column(
        modifier = Modifier.width(64.dp).clickable(enabled = enabled, role = Role.Button, onClick = onClick),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Box(
            modifier = Modifier.size(44.dp).background(MaterialTheme.colorScheme.background, CircleShape),
            contentAlignment = Alignment.Center,
        ) {
            LipiIcon(icon, MaterialTheme.colorScheme.onSurface.copy(alpha = if (enabled) 1f else 0.4f), size = 22.dp)
        }
        Text(label, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.padding(top = 6.dp))
    }
}

@Composable
private fun TopBar(pageIndex: Int, pageCount: Int, onBack: () -> Unit, onDone: () -> Unit) {
    Box(modifier = Modifier.fillMaxWidth().statusBarsPadding().padding(horizontal = 20.dp, vertical = 8.dp).height(40.dp)) {
        RoundIconButton(R.drawable.ic_chevron_left, stringResource(R.string.back), onBack, Modifier.align(Alignment.CenterStart))
        Column(modifier = Modifier.align(Alignment.Center), horizontalAlignment = Alignment.CenterHorizontally) {
            Text(stringResource(R.string.enhance_title), style = MaterialTheme.typography.titleMedium, color = MaterialTheme.colorScheme.onSurface)
            if (pageCount > 0) {
                Text(
                    stringResource(R.string.page_of, pageIndex + 1, pageCount),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
        Box(
            modifier = Modifier
                .align(Alignment.CenterEnd)
                .height(35.dp)
                .background(MaterialTheme.colorScheme.primary, RoundedCornerShape(50))
                .clickable(role = Role.Button, onClick = onDone)
                .padding(horizontal = 16.dp),
            contentAlignment = Alignment.Center,
        ) {
            Text(
                stringResource(R.string.enhance_done),
                style = MaterialTheme.typography.titleSmall,
                color = MaterialTheme.colorScheme.onPrimary,
            )
        }
    }
}

/**
 * The page. Holding a finger on it shows the original; a swipe to the side goes to the next or previous page.
 * [onSwipe] gets +1 for the next page and -1 for the previous one.
 */
@Composable
private fun PreviewCard(
    shown: Bitmap?,
    comparing: Boolean,
    filter: EnhanceFilter,
    loading: Boolean,
    failed: Boolean,
    onCompare: (Boolean) -> Unit,
    onSwipe: (Int) -> Unit,
) {
    val currentOnCompare by rememberUpdatedState(onCompare)
    val currentOnSwipe by rememberUpdatedState(onSwipe)
    val shape = RoundedCornerShape(20.dp)
    Box(
        modifier = Modifier
            .padding(horizontal = 20.dp, vertical = 8.dp)
            .fillMaxWidth()
            .height(344.dp)
            .clip(shape)
            .background(MaterialTheme.colorScheme.outline)
            .pointerInput(Unit) {
                val swipe = 72.dp.toPx()
                awaitEachGesture {
                    awaitFirstDown()
                    currentOnCompare(true)
                    var moved = 0f
                    var swiped = false
                    while (true) {
                        val change = awaitPointerEvent().changes.firstOrNull() ?: break
                        if (!change.pressed) break
                        moved += change.positionChange().x
                        if (!swiped && abs(moved) > swipe) {
                            swiped = true
                            currentOnCompare(false)
                            currentOnSwipe(if (moved < 0) 1 else -1)
                        }
                    }
                    currentOnCompare(false)
                }
            },
        contentAlignment = Alignment.Center,
    ) {
        if (shown != null) {
            val image = remember(shown) { shown.asImageBitmap() }
            Image(
                bitmap = image,
                contentDescription = null,
                contentScale = ContentScale.Fit,
                modifier = Modifier.fillMaxSize().padding(horizontal = 16.dp, vertical = 44.dp),
            )
        }
        when {
            failed -> Text(stringResource(R.string.enhance_failed), color = MaterialTheme.colorScheme.onSurfaceVariant, style = MaterialTheme.typography.bodyMedium)
            loading && shown == null -> Text(stringResource(R.string.enhance_loading), color = MaterialTheme.colorScheme.onSurfaceVariant, style = MaterialTheme.typography.bodyMedium)
        }
        StatusPill(
            text = if (filter == EnhanceFilter.Original) stringResource(R.string.enhance_status_original) else stringResource(R.string.enhance_status, filterLabel(filter)),
            modifier = Modifier.align(Alignment.TopStart).padding(12.dp),
        )
        Text(
            text = stringResource(if (comparing) R.string.enhance_comparing else R.string.enhance_compare),
            style = MaterialTheme.typography.labelSmall,
            color = Color(0xFF10131A),
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .padding(bottom = 12.dp)
                .background(Color.White.copy(alpha = 0.9f), RoundedCornerShape(50))
                .padding(horizontal = 10.dp, vertical = 5.dp),
        )
    }
}

@Composable
private fun StatusPill(text: String, modifier: Modifier = Modifier) {
    Text(
        text = text,
        style = MaterialTheme.typography.labelSmall,
        color = Color.White,
        modifier = modifier.background(LocalLipiColors.current.scrim, RoundedCornerShape(50)).padding(horizontal = 10.dp, vertical = 5.dp),
    )
}

@Composable
private fun filterLabel(filter: EnhanceFilter): String = stringResource(
    when (filter) {
        EnhanceFilter.Original -> R.string.filter_original
        EnhanceFilter.Auto -> R.string.filter_auto
        EnhanceFilter.BlackWhite -> R.string.filter_bw
        EnhanceFilter.Gray -> R.string.filter_gray
        EnhanceFilter.Color -> R.string.filter_color
    },
)

@Composable
private fun FilterRow(selected: EnhanceFilter, thumbnails: Map<EnhanceFilter, Bitmap>, onSelect: (EnhanceFilter) -> Unit) {
    LazyRow(
        contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 20.dp),
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        items(EnhanceFilter.values().toList()) { filter ->
            val isSelected = filter == selected
            val shape = RoundedCornerShape(14.dp)
            Column(
                modifier = Modifier.width(64.dp).clickable(role = Role.RadioButton) { onSelect(filter) },
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Box(
                    modifier = Modifier
                        .size(width = 64.dp, height = 80.dp)
                        .clip(shape)
                        .background(MaterialTheme.colorScheme.surface)
                        .border(if (isSelected) 2.dp else 1.dp, if (isSelected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline, shape),
                    contentAlignment = Alignment.Center,
                ) {
                    val thumb = thumbnails[filter]
                    if (thumb != null) {
                        val image = remember(thumb) { thumb.asImageBitmap() }
                        Image(image, contentDescription = null, contentScale = ContentScale.Fit, modifier = Modifier.fillMaxSize().padding(6.dp))
                    }
                }
                Text(
                    filterLabel(filter),
                    style = MaterialTheme.typography.labelSmall,
                    color = if (isSelected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant,
                    fontWeight = if (isSelected) FontWeight.SemiBold else FontWeight.Medium,
                    textAlign = TextAlign.Center,
                    modifier = Modifier.padding(top = 6.dp),
                )
            }
        }
    }
}

@OptIn(androidx.compose.material3.ExperimentalMaterial3Api::class)
@Composable
private fun LabeledSlider(label: String, value: Int, onChange: (Int) -> Unit) {
    Column(modifier = Modifier.padding(horizontal = 20.dp)) {
        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text(label, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurface, fontWeight = FontWeight.Medium)
            Text(signed(value), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.primary, fontWeight = FontWeight.SemiBold)
        }
        val colors = SliderDefaults.colors(
            thumbColor = MaterialTheme.colorScheme.primary,
            activeTrackColor = MaterialTheme.colorScheme.primary,
            inactiveTrackColor = MaterialTheme.colorScheme.outline,
        )
        Slider(
            value = value.toFloat(),
            onValueChange = { onChange(it.roundToInt()) },
            valueRange = -100f..100f,
            colors = colors,
            // A round thumb on a thin track, as in the design.
            thumb = { Box(Modifier.size(20.dp).background(MaterialTheme.colorScheme.primary, CircleShape)) },
            track = { state ->
                SliderDefaults.Track(
                    sliderState = state,
                    modifier = Modifier.height(4.dp),
                    colors = colors,
                    drawStopIndicator = null,
                    thumbTrackGapSize = 0.dp,
                    trackInsideCornerSize = 0.dp,
                )
            },
            modifier = Modifier.semantics { contentDescription = label },
        )
    }
}
