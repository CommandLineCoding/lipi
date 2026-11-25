package `in`.commandlinecoding.lipi.ui.settings

import android.content.ActivityNotFoundException
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import `in`.commandlinecoding.lipi.BuildConfig
import `in`.commandlinecoding.lipi.NativeCore
import `in`.commandlinecoding.lipi.R
import `in`.commandlinecoding.lipi.data.ThemeMode
import `in`.commandlinecoding.lipi.ui.components.LipiIcon
import `in`.commandlinecoding.lipi.ui.components.SegmentedControl
import `in`.commandlinecoding.lipi.ui.theme.LipiDimens
import `in`.commandlinecoding.lipi.ui.theme.LocalLipiColors

private const val MODEL_CARD = "${Licences.REPOSITORY}/blob/main/models/page-net/MODEL_CARD.md"
private const val BUILD_INSTRUCTIONS = "${Licences.REPOSITORY}/blob/main/CONTRIBUTING.md"

/** What the app promises and contains: the privacy statement, the bundled model, the theme, and information about the app. */
@Composable
fun SettingsScreen(theme: ThemeMode, onTheme: (ThemeMode) -> Unit, onOpenLicences: () -> Unit, bottomClearance: Dp) {
    val uri = LocalUriHandler.current
    // Opens a page in the browser. This needs no permission; the app only asks the system to show the link.
    val open = { url: String ->
        try {
            uri.openUri(url)
        } catch (e: ActivityNotFoundException) {
            // no browser installed: nothing to show
        } catch (e: IllegalArgumentException) {
        }
    }
    Column(
        modifier = Modifier
            .fillMaxSize()
            .statusBarsPadding()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = LipiDimens.ScreenPadding)
            .padding(bottom = bottomClearance),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text(
            stringResource(R.string.settings_title),
            style = MaterialTheme.typography.displaySmall,
            color = MaterialTheme.colorScheme.onBackground,
            modifier = Modifier.padding(vertical = 8.dp),
        )
        PrivacyCard(onNoNetwork = { open(BUILD_INSTRUCTIONS) }, onOpenSource = { open(Licences.REPOSITORY) })

        SectionLabel(R.string.settings_models)
        Group {
            SettingsRow(R.string.settings_model_name, stringResource(R.string.settings_model_meta), onClick = { open(MODEL_CARD) }) {
                val colors = LocalLipiColors.current
                Box(Modifier.size(28.dp).background(colors.successTint, CircleShape), contentAlignment = Alignment.Center) {
                    LipiIcon(R.drawable.ic_check, colors.success, size = 16.dp)
                }
            }
        }

        SectionLabel(R.string.settings_appearance)
        Group {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Text(stringResource(R.string.settings_theme), style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.onSurface)
                SegmentedControl(
                    options = listOf(
                        ThemeMode.System to stringResource(R.string.theme_system),
                        ThemeMode.Light to stringResource(R.string.theme_light),
                        ThemeMode.Dark to stringResource(R.string.theme_dark),
                    ),
                    selected = theme,
                    onSelect = onTheme,
                )
            }
        }

        SectionLabel(R.string.settings_about)
        Group {
            SettingsRow(R.string.settings_version, stringResource(R.string.settings_version_value, BuildConfig.VERSION_NAME, NativeCore.version()))
            Divider()
            SettingsRow(R.string.settings_source, null, onClick = { open(Licences.REPOSITORY) }) {
                LipiIcon(R.drawable.ic_external, MaterialTheme.colorScheme.onSurfaceVariant)
            }
            Divider()
            SettingsRow(R.string.settings_licences, null, onClick = onOpenLicences) {
                LipiIcon(R.drawable.ic_chevron_right, MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }
}

@Composable
private fun PrivacyCard(onNoNetwork: () -> Unit, onOpenSource: () -> Unit) {
    val colors = LocalLipiColors.current
    Column(
        modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(20.dp)).background(colors.primaryTint).padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Box(Modifier.size(44.dp).background(MaterialTheme.colorScheme.surface, CircleShape), contentAlignment = Alignment.Center) {
                LipiIcon(R.drawable.ic_shield, MaterialTheme.colorScheme.primary, size = 24.dp)
            }
            Column {
                Text(stringResource(R.string.settings_private_title), style = MaterialTheme.typography.titleMedium, color = MaterialTheme.colorScheme.onSurface)
                Text(stringResource(R.string.settings_private_subtitle), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
        Text(stringResource(R.string.settings_private_body), style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurface)
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Badge(stringResource(R.string.badge_no_network), onNoNetwork)
            Badge(stringResource(R.string.badge_open_source), onOpenSource)
        }
    }
}

@Composable
private fun Badge(text: String, onClick: () -> Unit) {
    val colors = LocalLipiColors.current
    Row(
        modifier = Modifier
            .clip(RoundedCornerShape(LipiDimens.ChipRadius))
            .background(MaterialTheme.colorScheme.surface)
            .clickable(role = Role.Button, onClick = onClick)
            .padding(horizontal = 10.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(5.dp),
    ) {
        LipiIcon(R.drawable.ic_check, colors.success, size = 12.dp)
        Text(text, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurface)
    }
}

@Composable
private fun SectionLabel(res: Int) {
    Text(
        stringResource(res),
        style = MaterialTheme.typography.titleSmall,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
        fontWeight = FontWeight.SemiBold,
        modifier = Modifier.padding(top = 6.dp),
    )
}

@Composable
private fun Group(content: @Composable () -> Unit) {
    val shape = RoundedCornerShape(20.dp)
    Column(modifier = Modifier.fillMaxWidth().clip(shape).background(MaterialTheme.colorScheme.surface).border(1.dp, MaterialTheme.colorScheme.outline, shape)) {
        content()
    }
}

@Composable
private fun Divider() {
    Box(Modifier.fillMaxWidth().size(width = Dp.Unspecified, height = 1.dp).background(MaterialTheme.colorScheme.outline))
}

/** A line of a settings group: a title, an optional second line, and a trailing icon or control. */
@Composable
private fun SettingsRow(title: Int, subtitle: String?, onClick: (() -> Unit)? = null, trailing: @Composable () -> Unit = {}) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .then(if (onClick != null) Modifier.clickable(role = Role.Button, onClick = onClick) else Modifier)
            .padding(horizontal = 16.dp, vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text(stringResource(title), style = MaterialTheme.typography.bodyLarge, color = MaterialTheme.colorScheme.onSurface)
            if (subtitle != null) Text(subtitle, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        trailing()
    }
}
