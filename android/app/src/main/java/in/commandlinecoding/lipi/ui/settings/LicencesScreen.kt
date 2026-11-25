package `in`.commandlinecoding.lipi.ui.settings

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.produceState
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import `in`.commandlinecoding.lipi.R
import `in`.commandlinecoding.lipi.ui.components.RoundIconButton
import `in`.commandlinecoding.lipi.ui.theme.LipiDimens
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/** The open-source components of the app. Tapping one shows its licence text, which ships with the app. */
@Composable
fun LicencesScreen(onBack: () -> Unit) {
    Column(modifier = Modifier.fillMaxSize().statusBarsPadding()) {
        Row(
            modifier = Modifier.fillMaxWidth().padding(horizontal = LipiDimens.ScreenPadding, vertical = 8.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            RoundIconButton(R.drawable.ic_chevron_left, stringResource(R.string.back), onBack)
            Text(stringResource(R.string.licences_title), style = MaterialTheme.typography.titleLarge, color = MaterialTheme.colorScheme.onBackground)
        }
        LazyColumn(
            modifier = Modifier.navigationBarsPadding(),
            contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = LipiDimens.ScreenPadding, vertical = 8.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            item {
                Text(stringResource(R.string.licences_intro), style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            items(Licences.all, key = { it.id }) { LicenceCard(it) }
        }
    }
}

@Composable
private fun LicenceCard(entry: LicenceEntry) {
    var expanded by rememberSaveable(entry.id) { androidx.compose.runtime.mutableStateOf(false) }
    val context = LocalContext.current
    val text by produceState<String?>(initialValue = null, expanded) {
        value = if (expanded && entry.textAsset != null) withContext(Dispatchers.IO) {
            try {
                context.assets.open("licenses/${entry.textAsset}").bufferedReader().use { it.readText() }
            } catch (e: java.io.IOException) {
                null
            }
        } else null
    }
    val shape = RoundedCornerShape(20.dp)
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(shape)
            .background(MaterialTheme.colorScheme.surface)
            .border(1.dp, MaterialTheme.colorScheme.outline, shape)
            .clickable(enabled = entry.textAsset != null, role = Role.Button) { expanded = !expanded }
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        Text(entry.name, style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.onSurface)
        Text(entry.use, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(entry.licence, style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.primary)
        Text(entry.holder, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(entry.url, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        val shown = text
        if (expanded && shown != null) {
            SelectionContainer {
                Text(
                    shown,
                    style = MaterialTheme.typography.bodySmall.copy(fontFamily = FontFamily.Monospace, fontSize = 11.sp, lineHeight = 15.sp),
                    color = MaterialTheme.colorScheme.onSurface,
                    modifier = Modifier.padding(top = 10.dp),
                )
            }
        }
    }
}
