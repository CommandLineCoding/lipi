package `in`.commandlinecoding.lipi.ui.document

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.itemsIndexed
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.pluralStringResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import `in`.commandlinecoding.lipi.R
import `in`.commandlinecoding.lipi.scan.ExportFormat
import `in`.commandlinecoding.lipi.scan.sanitizeFileName
import `in`.commandlinecoding.lipi.ui.components.LipiIcon
import `in`.commandlinecoding.lipi.ui.components.PagePill
import `in`.commandlinecoding.lipi.ui.components.RoundIconButton
import `in`.commandlinecoding.lipi.ui.library.DateLabel
import `in`.commandlinecoding.lipi.ui.library.rememberThumbnail
import `in`.commandlinecoding.lipi.ui.theme.LipiDimens
import `in`.commandlinecoding.lipi.ui.theme.LocalLipiColors

/** A document of the library: its pages, and the actions share, export again, rename and delete (frame 11 of the design). */
@Composable
fun DocumentScreen(viewModel: DocumentViewModel, startWithExport: Boolean, onBack: () -> Unit) {
    val state by viewModel.state.collectAsState()
    val context = LocalContext.current
    var sheet by rememberSaveable { mutableStateOf(false) }
    var renaming by rememberSaveable { mutableStateOf(false) }
    var deleting by rememberSaveable { mutableStateOf(false) }
    var choosingFormat by rememberSaveable { mutableStateOf(startWithExport) }
    var format by rememberSaveable { mutableStateOf(ExportFormat.Pdf) }

    LaunchedEffect(state.gone) { if (state.gone) onBack() }

    val savePdf = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument(ExportFormat.Pdf.mimeType)) { uri -> uri?.let { viewModel.export(ExportFormat.Pdf, it) } }
    val saveJpg = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument(ExportFormat.Jpg.mimeType)) { uri -> uri?.let { viewModel.export(ExportFormat.Jpg, it) } }
    val saveFolder = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri -> uri?.let { viewModel.export(ExportFormat.Jpg, it) } }
    val startExport = { chosen: ExportFormat ->
        format = chosen
        val name = "${sanitizeFileName(state.title)}.${chosen.extension}"
        when {
            chosen == ExportFormat.Pdf -> savePdf.launch(name)
            state.pages.size == 1 -> saveJpg.launch(name)
            else -> saveFolder.launch(DocumentsFolder)
        }
    }
    val share = { viewModel.share { uri -> shareDocument(context, uri, state.title) } }

    val dateWords = DateLabel.Words(stringResource(R.string.date_today), stringResource(R.string.date_yesterday))
    val meta = listOfNotNull(
        pluralStringResource(R.plurals.page_count, state.pages.size, state.pages.size),
        state.pageSize,
        DateLabel.of(state.createdAt, System.currentTimeMillis(), dateWords),
    ).joinToString(" · ")

    Column(modifier = Modifier.fillMaxSize().background(MaterialTheme.colorScheme.background).statusBarsPadding()) {
        Row(
            modifier = Modifier.fillMaxWidth().padding(horizontal = LipiDimens.ScreenPadding, vertical = 8.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            RoundIconButton(R.drawable.ic_chevron_left, stringResource(R.string.back), onBack)
            Column(modifier = Modifier.weight(1f)) {
                Text(state.title, style = MaterialTheme.typography.titleLarge, color = MaterialTheme.colorScheme.onBackground, maxLines = 1, overflow = androidx.compose.ui.text.style.TextOverflow.Ellipsis)
                Text(meta, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            RoundIconButton(R.drawable.ic_more, stringResource(R.string.document_more), { sheet = true })
        }
        Row(
            modifier = Modifier.fillMaxWidth().padding(horizontal = LipiDimens.ScreenPadding, vertical = 8.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            ActionButton(R.drawable.ic_share, stringResource(R.string.document_share), false, enabled = !state.busy, onClick = share)
            ActionButton(R.drawable.ic_download, stringResource(R.string.document_export), false, onClick = { choosingFormat = true })
            ActionButton(R.drawable.ic_pen, stringResource(R.string.document_rename), false, onClick = { renaming = true })
            ActionButton(R.drawable.ic_trash, stringResource(R.string.delete), true, onClick = { deleting = true })
        }
        LazyVerticalGrid(
            columns = GridCells.Fixed(2),
            modifier = Modifier.weight(1f),
            contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = LipiDimens.ScreenPadding, vertical = 8.dp),
            horizontalArrangement = Arrangement.spacedBy(LipiDimens.Gap),
            verticalArrangement = Arrangement.spacedBy(LipiDimens.Gap),
        ) {
            itemsIndexed(state.pages, key = { _, path -> path }) { index, path -> PageCard(path, index + 1) }
        }
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .background(MaterialTheme.colorScheme.surface, RoundedCornerShape(topStart = 24.dp, topEnd = 24.dp))
                .navigationBarsPadding()
                .padding(horizontal = 20.dp, vertical = 16.dp),
        ) {
            Button(
                onClick = { choosingFormat = true },
                enabled = state.pages.isNotEmpty() && !state.busy,
                modifier = Modifier.fillMaxWidth().height(54.dp),
                shape = RoundedCornerShape(20.dp),
                colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.primary, contentColor = MaterialTheme.colorScheme.onPrimary),
            ) { Text(stringResource(R.string.document_export_again), style = MaterialTheme.typography.titleSmall) }
        }
    }

    if (sheet) {
        DocumentActionsSheet(
            title = state.title,
            meta = meta,
            onShare = { sheet = false; share() },
            onExport = { sheet = false; choosingFormat = true },
            onRename = { sheet = false; renaming = true },
            onDelete = { sheet = false; deleting = true },
            onDismiss = { sheet = false },
        )
    }
    if (choosingFormat) ExportAgainDialog(onChoose = { choosingFormat = false; startExport(it) }, onDismiss = { choosingFormat = false })
    if (renaming) RenameDocumentDialog(state.title, onConfirm = { renaming = false; viewModel.rename(it) }, onDismiss = { renaming = false })
    if (deleting) DeleteDocumentDialog(state.title, onConfirm = { deleting = false; viewModel.delete() }, onDismiss = { deleting = false })
    state.notice?.let { notice ->
        AlertDialog(
            onDismissRequest = viewModel::noticeShown,
            text = {
                Text(
                    stringResource(
                        when (notice) {
                            Notice.Exported -> R.string.document_exported
                            Notice.ExportFailed -> R.string.document_export_failed
                            Notice.ShareFailed -> R.string.document_share_failed
                        },
                    ),
                )
            },
            confirmButton = { TextButton(onClick = viewModel::noticeShown) { Text(stringResource(R.string.ok)) } },
        )
    }
}

/** Where the folder picker for several JPEGs starts; Android does not let an app write into the top of the storage or into Downloads. */
private val DocumentsFolder = android.provider.DocumentsContract.buildDocumentUri("com.android.externalstorage.documents", "primary:Documents")

@Composable
private fun ActionButton(icon: Int, label: String, danger: Boolean, enabled: Boolean = true, onClick: () -> Unit) {
    val colors = LocalLipiColors.current
    val tint = if (danger) colors.danger else MaterialTheme.colorScheme.onSurface
    Column(
        modifier = Modifier.size(width = 76.dp, height = 78.dp).clickable(enabled = enabled, role = Role.Button, onClick = onClick),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Box(
            modifier = Modifier
                .size(48.dp)
                .background(MaterialTheme.colorScheme.surface, CircleShape)
                .border(1.dp, MaterialTheme.colorScheme.outline, CircleShape),
            contentAlignment = Alignment.Center,
        ) { LipiIcon(icon, tint.copy(alpha = if (enabled) 1f else 0.4f), size = 22.dp) }
        Text(label, style = MaterialTheme.typography.labelSmall, color = if (danger) colors.danger else MaterialTheme.colorScheme.onSurfaceVariant)
    }
}

@Composable
private fun PageCard(path: String, number: Int) {
    val image = rememberThumbnail(path, maxEdge = 640)
    val shape = RoundedCornerShape(14.dp)
    val description = stringResource(R.string.document_page_number, number)
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .aspectRatio(168f / 224f)
            .clip(shape)
            .background(MaterialTheme.colorScheme.surface)
            .border(1.dp, MaterialTheme.colorScheme.outline, shape)
            .semantics { contentDescription = description },
    ) {
        if (image != null) Image(bitmap = image, contentDescription = null, contentScale = ContentScale.Fit, modifier = Modifier.fillMaxSize().padding(6.dp))
        PagePill(number.toString(), modifier = Modifier.align(Alignment.BottomStart).padding(10.dp))
    }
}
