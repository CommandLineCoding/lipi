package `in`.commandlinecoding.lipi.ui.library

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.pluralStringResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import `in`.commandlinecoding.lipi.R
import `in`.commandlinecoding.lipi.data.DocumentSummary
import `in`.commandlinecoding.lipi.ui.components.LipiSearchField
import `in`.commandlinecoding.lipi.ui.components.PagePill
import `in`.commandlinecoding.lipi.ui.components.RoundIconButton
import `in`.commandlinecoding.lipi.ui.theme.LipiDimens
import `in`.commandlinecoding.lipi.ui.theme.LocalLipiColors

/** The Library frame of the design: title, search, "Recent" and a two-column grid of documents. */
@Composable
fun LibraryScreen(
    state: LibraryState,
    nowMillis: Long,
    onQueryChange: (String) -> Unit,
    onOpenSettings: () -> Unit,
    onOpenDocument: (Long) -> Unit,
    onDeleteDocument: (Long) -> Unit,
    bottomClearance: androidx.compose.ui.unit.Dp,
) {
    Column(modifier = Modifier.fillMaxSize().statusBarsPadding()) {
        Row(
            modifier = Modifier.fillMaxWidth().padding(horizontal = LipiDimens.ScreenPadding, vertical = 8.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(stringResource(R.string.library_title), style = MaterialTheme.typography.displaySmall, color = MaterialTheme.colorScheme.onBackground)
            RoundIconButton(R.drawable.ic_sliders, stringResource(R.string.settings_title), onOpenSettings)
        }
        Box(modifier = Modifier.padding(horizontal = LipiDimens.ScreenPadding, vertical = 8.dp)) {
            LipiSearchField(state.query, onQueryChange, stringResource(R.string.library_search_placeholder))
        }
        Row(
            modifier = Modifier.fillMaxWidth().padding(horizontal = LipiDimens.ScreenPadding, vertical = 10.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(stringResource(R.string.library_recent), style = MaterialTheme.typography.titleMedium, color = MaterialTheme.colorScheme.onBackground)
            Text(
                pluralStringResource(R.plurals.document_count, state.total, state.total),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
        when {
            !state.loaded -> Unit
            state.total == 0 -> EmptyLibrary(stringResource(R.string.library_empty), bottomClearance)
            state.documents.isEmpty() -> EmptyLibrary(stringResource(R.string.library_no_match), bottomClearance)
            else -> {
                val words = DateLabel.Words(stringResource(R.string.date_today), stringResource(R.string.date_yesterday))
                LazyVerticalGrid(
                    columns = GridCells.Fixed(2),
                    contentPadding = PaddingValues(start = LipiDimens.ScreenPadding, end = LipiDimens.ScreenPadding, bottom = bottomClearance),
                    horizontalArrangement = Arrangement.spacedBy(LipiDimens.Gap),
                    verticalArrangement = Arrangement.spacedBy(LipiDimens.Gap),
                    modifier = Modifier.fillMaxSize(),
                ) {
                    items(state.documents, key = { it.document.id }) { summary ->
                        DocumentCard(summary, nowMillis, words, { onOpenDocument(summary.document.id) }, { onDeleteDocument(summary.document.id) })
                    }
                }
            }
        }
    }
}

@Composable
private fun EmptyLibrary(message: String, bottomClearance: androidx.compose.ui.unit.Dp) {
    Box(
        modifier = Modifier.fillMaxSize().padding(start = 32.dp, end = 32.dp, bottom = bottomClearance),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            message,
            style = MaterialTheme.typography.bodyLarge,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            textAlign = androidx.compose.ui.text.style.TextAlign.Center,
        )
    }
}

@OptIn(androidx.compose.foundation.ExperimentalFoundationApi::class)
@Composable
private fun DocumentCard(
    summary: DocumentSummary,
    nowMillis: Long,
    words: DateLabel.Words,
    onOpen: () -> Unit,
    onDelete: () -> Unit,
) {
    val doc = summary.document
    var menuOpen by remember { mutableStateOf(false) }
    val tints = LocalLipiColors.current.thumbTints
    Column(
        modifier = Modifier.fillMaxWidth().combinedClickable(onClick = onOpen, onLongClick = { menuOpen = true }),
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(148.dp)
                .clip(RoundedCornerShape(LipiDimens.CardRadius))
                .background(tints[(doc.id % tints.size).toInt()]),
            contentAlignment = Alignment.Center,
        ) {
            Paper(summary.coverPath)
            PagePill(
                pluralStringResource(R.plurals.page_count, doc.pageCount, doc.pageCount),
                modifier = Modifier.align(Alignment.TopEnd).padding(top = 8.dp, end = 8.dp),
            )
        }
        Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
            Text(doc.title, style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.onBackground, maxLines = 1, overflow = androidx.compose.ui.text.style.TextOverflow.Ellipsis)
            Text(
                stringResource(R.string.document_meta, pluralStringResource(R.plurals.page_count, doc.pageCount, doc.pageCount), DateLabel.of(doc.createdAt, nowMillis, words)),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
        DropdownMenu(expanded = menuOpen, onDismissRequest = { menuOpen = false }) {
            DropdownMenuItem(text = { Text(stringResource(R.string.delete)) }, onClick = { menuOpen = false; onDelete() })
        }
    }
}

private val PaperShape = RoundedCornerShape(6.dp)

/** The page on a thumbnail: the first page of the document, or the paper drawn in the design while there is none. */
@Composable
private fun Paper(coverPath: String?) {
    val cover = rememberThumbnail(coverPath, maxEdge = 480)
    val shadow = Color.Black.copy(alpha = 0.08f)
    if (cover != null) {
        Image(
            bitmap = cover,
            contentDescription = null,
            contentScale = ContentScale.Fit,
            modifier = Modifier.height(124.dp).shadow(5.dp, PaperShape, spotColor = shadow, ambientColor = shadow).clip(PaperShape),
        )
    } else {
        val lines = LocalLipiColors.current
        val widths = listOf(73.6f, 77.6f, 48f, 80f, 70.4f, 76f, 40f, 78.4f, 72f, 56f, 80f)
        Column(
            modifier = Modifier
                .size(width = 100.dp, height = 124.dp)
                .shadow(5.dp, PaperShape, spotColor = shadow, ambientColor = shadow)
                .background(Color.White, PaperShape)
                .padding(10.dp),
            verticalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            Box(Modifier.width(50.dp).height(5.dp).background(lines.paperTitle, RoundedCornerShape(2.dp)))
            widths.forEach { w -> Box(Modifier.width(w.dp).height(2.dp).background(lines.paperLine, RoundedCornerShape(1.dp))) }
        }
    }
}
