package `in`.commandlinecoding.lipi.ui.document

import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.unit.dp
import `in`.commandlinecoding.lipi.R
import `in`.commandlinecoding.lipi.scan.ExportFormat
import `in`.commandlinecoding.lipi.ui.components.LipiIcon
import `in`.commandlinecoding.lipi.ui.components.SegmentedControl
import `in`.commandlinecoding.lipi.ui.theme.LocalLipiColors

/** Opens the system share sheet for the PDF at [uri]. The other app gets read access to that one file only. */
fun shareDocument(context: Context, uri: Uri, title: String) {
    val send = Intent(Intent.ACTION_SEND).apply {
        type = ExportFormat.Pdf.mimeType
        putExtra(Intent.EXTRA_STREAM, uri)
        putExtra(Intent.EXTRA_SUBJECT, title)
        addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
    }
    context.startActivity(Intent.createChooser(send, context.getString(R.string.document_share_chooser)))
}

/** The actions of a document in a bottom sheet (long press in the Library, "more" on the document view). */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun DocumentActionsSheet(
    title: String,
    meta: String,
    onShare: () -> Unit,
    onExport: () -> Unit,
    onRename: () -> Unit,
    onDelete: () -> Unit,
    onDismiss: () -> Unit,
) {
    ModalBottomSheet(
        onDismissRequest = onDismiss,
        containerColor = MaterialTheme.colorScheme.surface,
        shape = RoundedCornerShape(topStart = 28.dp, topEnd = 28.dp),
    ) {
        val danger = LocalLipiColors.current.danger
        Column(modifier = Modifier.navigationBarsPadding().padding(horizontal = 20.dp).padding(bottom = 16.dp)) {
            Column(modifier = Modifier.padding(bottom = 8.dp)) {
                Text(title, style = MaterialTheme.typography.titleMedium, color = MaterialTheme.colorScheme.onSurface, maxLines = 1, overflow = androidx.compose.ui.text.style.TextOverflow.Ellipsis)
                Text(meta, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            SheetRow(R.drawable.ic_share, stringResource(R.string.document_share), MaterialTheme.colorScheme.onSurface, onShare)
            SheetRow(R.drawable.ic_download, stringResource(R.string.document_export_again), MaterialTheme.colorScheme.onSurface, onExport)
            SheetRow(R.drawable.ic_pen, stringResource(R.string.document_rename), MaterialTheme.colorScheme.onSurface, onRename)
            SheetRow(R.drawable.ic_trash, stringResource(R.string.delete), danger, onDelete)
        }
    }
}

@Composable
private fun SheetRow(icon: Int, label: String, color: androidx.compose.ui.graphics.Color, onClick: () -> Unit) {
    Row(
        modifier = Modifier.fillMaxWidth().clickable(role = Role.Button, onClick = onClick).padding(vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        LipiIcon(icon, color, size = 22.dp)
        Text(label, style = MaterialTheme.typography.bodyLarge, color = color)
    }
}

/** Asks before a document is deleted, as in the design. */
@Composable
fun DeleteDocumentDialog(title: String, onConfirm: () -> Unit, onDismiss: () -> Unit) {
    val colors = LocalLipiColors.current
    AlertDialog(
        onDismissRequest = onDismiss,
        shape = RoundedCornerShape(28.dp),
        containerColor = MaterialTheme.colorScheme.surface,
        title = { Text(stringResource(R.string.document_delete_title)) },
        text = { Text(stringResource(R.string.document_delete_message, title)) },
        confirmButton = { TextButton(onClick = onConfirm) { Text(stringResource(R.string.delete), color = colors.danger) } },
        dismissButton = { TextButton(onClick = onDismiss) { Text(stringResource(R.string.cancel)) } },
    )
}

@Composable
fun RenameDocumentDialog(initial: String, onConfirm: (String) -> Unit, onDismiss: () -> Unit) {
    var text by remember { mutableStateOf(initial) }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(R.string.document_rename)) },
        text = { OutlinedTextField(value = text, onValueChange = { text = it }, label = { Text(stringResource(R.string.export_title_label)) }, singleLine = true) },
        confirmButton = { TextButton(onClick = { onConfirm(text) }, enabled = text.isNotBlank()) { Text(stringResource(R.string.ok)) } },
        dismissButton = { TextButton(onClick = onDismiss) { Text(stringResource(R.string.cancel)) } },
    )
}

/** Chooses PDF or JPG before the file picker opens for "Export again". */
@Composable
fun ExportAgainDialog(onChoose: (ExportFormat) -> Unit, onDismiss: () -> Unit) {
    var format by remember { mutableStateOf(ExportFormat.Pdf) }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(R.string.document_export_format)) },
        text = {
            SegmentedControl(
                options = listOf(ExportFormat.Pdf to "PDF", ExportFormat.Jpg to "JPG"),
                selected = format,
                onSelect = { format = it },
            )
        },
        confirmButton = { TextButton(onClick = { onChoose(format) }) { Text(stringResource(R.string.document_export_again)) } },
        dismissButton = { TextButton(onClick = onDismiss) { Text(stringResource(R.string.cancel)) } },
    )
}
