package `in`.commandlinecoding.lipi.ui.export

import android.net.Uri
import android.provider.DocumentsContract
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
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.pluralStringResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import `in`.commandlinecoding.lipi.R
import `in`.commandlinecoding.lipi.scan.Compression
import `in`.commandlinecoding.lipi.scan.ExportFormat
import `in`.commandlinecoding.lipi.ui.components.LipiIcon
import `in`.commandlinecoding.lipi.ui.components.RoundIconButton
import `in`.commandlinecoding.lipi.ui.components.SegmentedControl

/** Where the folder picker for several JPEGs starts. Android does not let an app write into the top of the storage or into Downloads. */
private val DocumentsFolder: Uri = DocumentsContract.buildDocumentUri("com.android.externalstorage.documents", "primary:Documents")

/** Name, format and compression of the scan, and Save, which asks the system where to put the file. */
@Composable
fun ExportScreen(viewModel: ExportViewModel, onBack: () -> Unit, onSaved: () -> Unit) {
    val state by viewModel.state.collectAsState()
    var renaming by remember { mutableStateOf(false) }

    val savePdf = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument(ExportFormat.Pdf.mimeType)) { uri -> uri?.let(viewModel::save) }
    val saveJpg = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument(ExportFormat.Jpg.mimeType)) { uri -> uri?.let(viewModel::save) }
    val saveFolder = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri -> uri?.let(viewModel::save) }
    val saving = state.phase is SavePhase.Saving

    Column(modifier = Modifier.fillMaxSize().background(MaterialTheme.colorScheme.background)) {
        Row(
            modifier = Modifier.fillMaxWidth().statusBarsPadding().padding(horizontal = 20.dp, vertical = 8.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            RoundIconButton(R.drawable.ic_chevron_left, stringResource(R.string.back), onBack)
            Text(stringResource(R.string.export_title), style = MaterialTheme.typography.titleLarge, color = MaterialTheme.colorScheme.onSurface)
        }

        Column(
            modifier = Modifier.weight(1f).verticalScroll(rememberScrollState()).padding(horizontal = 20.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp),
        ) {
            Spacer(Modifier.height(2.dp))
            SummaryCard(state, onRename = { renaming = true })
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(stringResource(R.string.export_format), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.padding(start = 2.dp))
                SegmentedControl(
                    options = listOf(ExportFormat.Pdf to "PDF", ExportFormat.Jpg to "JPG"),
                    selected = state.format,
                    onSelect = viewModel::setFormat,
                )
            }
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(20.dp))
                    .background(MaterialTheme.colorScheme.surface)
                    .border(1.dp, MaterialTheme.colorScheme.outline, RoundedCornerShape(20.dp))
                    .padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                Column {
                    Text(stringResource(R.string.export_compression), style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.onSurface)
                    Text(stringResource(R.string.export_compression_hint), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
                SegmentedControl(
                    options = listOf(
                        Compression.Small to stringResource(R.string.compression_small),
                        Compression.Balanced to stringResource(R.string.compression_balanced),
                        Compression.High to stringResource(R.string.compression_high),
                    ),
                    selected = state.compression,
                    onSelect = viewModel::setCompression,
                )
            }
            Spacer(Modifier.height(8.dp))
        }

        // The action bar of the design, without the share button (later).
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .background(MaterialTheme.colorScheme.surface, RoundedCornerShape(topStart = 24.dp, topEnd = 24.dp))
                .navigationBarsPadding()
                .padding(horizontal = 20.dp, vertical = 16.dp),
        ) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(54.dp)
                    .background(MaterialTheme.colorScheme.primary.copy(alpha = if (saving || state.pageCount == 0) 0.6f else 1f), RoundedCornerShape(20.dp))
                    .clickable(enabled = !saving && state.pageCount > 0, role = Role.Button) {
                        when {
                            state.format == ExportFormat.Pdf -> savePdf.launch(viewModel.suggestedFileName())
                            state.pageCount == 1 -> saveJpg.launch(viewModel.suggestedFileName())
                            else -> saveFolder.launch(DocumentsFolder)
                        }
                    },
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.Center,
            ) {
                val phase = state.phase
                if (phase is SavePhase.Saving) {
                    Text(stringResource(R.string.export_saving, (phase.done + 1).coerceAtMost(phase.total), phase.total), style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.onPrimary)
                } else {
                    LipiIcon(R.drawable.ic_download, MaterialTheme.colorScheme.onPrimary)
                    Text(
                        stringResource(if (state.format == ExportFormat.Pdf) R.string.export_save_pdf else R.string.export_save_jpg),
                        style = MaterialTheme.typography.titleSmall,
                        color = MaterialTheme.colorScheme.onPrimary,
                        modifier = Modifier.padding(start = 8.dp),
                    )
                }
            }
        }
    }

    if (renaming) {
        var text by remember { mutableStateOf(state.title) }
        AlertDialog(
            onDismissRequest = { renaming = false },
            title = { Text(stringResource(R.string.export_rename)) },
            text = { OutlinedTextField(value = text, onValueChange = { text = it }, label = { Text(stringResource(R.string.export_title_label)) }, singleLine = true) },
            confirmButton = { TextButton(onClick = { viewModel.setTitle(text); renaming = false }) { Text(stringResource(R.string.ok)) } },
            dismissButton = { TextButton(onClick = { renaming = false }) { Text(stringResource(R.string.cancel)) } },
        )
    }
    when (val phase = state.phase) {
        is SavePhase.Saved -> AlertDialog(
            onDismissRequest = onSaved,
            title = { Text(stringResource(R.string.export_saved_title)) },
            text = { Text(stringResource(R.string.export_saved_message, phase.name)) },
            confirmButton = { TextButton(onClick = onSaved) { Text(stringResource(R.string.ok)) } },
        )
        SavePhase.Failed -> AlertDialog(
            onDismissRequest = viewModel::dismissResult,
            text = { Text(stringResource(R.string.export_failed)) },
            confirmButton = { TextButton(onClick = viewModel::dismissResult) { Text(stringResource(R.string.ok)) } },
        )
        else -> Unit
    }
}

@Composable
private fun SummaryCard(state: ExportState, onRename: () -> Unit) {
    val shape = RoundedCornerShape(20.dp)
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(shape)
            .background(MaterialTheme.colorScheme.surface)
            .border(1.dp, MaterialTheme.colorScheme.outline, shape)
            .padding(15.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        Box(
            modifier = Modifier.size(width = 56.dp, height = 74.dp).clip(RoundedCornerShape(8.dp)).background(MaterialTheme.colorScheme.background),
            contentAlignment = Alignment.Center,
        ) {
            val cover = state.cover
            if (cover != null) {
                val image = remember(cover) { cover.asImageBitmap() }
                Image(image, contentDescription = null, contentScale = ContentScale.Fit, modifier = Modifier.fillMaxSize())
            }
        }
        Column(modifier = Modifier.weight(1f)) {
            Text(state.title, style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.onSurface, maxLines = 2)
            val parts = buildList {
                add(pluralStringResource(R.plurals.page_count, state.pageCount, state.pageCount))
                state.pageSize?.let(::add)
                if (state.autoNamed) add(stringResource(R.string.export_auto_named))
            }
            Text(parts.joinToString(" · "), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        val renameLabel = stringResource(R.string.export_rename)
        Box(
            modifier = Modifier
                .size(36.dp)
                .background(MaterialTheme.colorScheme.background, CircleShape)
                .clickable(role = Role.Button, onClick = onRename)
                .semantics { contentDescription = renameLabel },
            contentAlignment = Alignment.Center,
        ) { LipiIcon(R.drawable.ic_pen, MaterialTheme.colorScheme.onSurface, size = 16.dp) }
    }
}
