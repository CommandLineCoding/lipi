package `in`.commandlinecoding.lipi.ui.components

import androidx.annotation.DrawableRes
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import `in`.commandlinecoding.lipi.ui.theme.LipiDimens
import `in`.commandlinecoding.lipi.ui.theme.LocalLipiColors

/** A line icon from the design, tinted in code. */
@Composable
fun LipiIcon(@DrawableRes id: Int, tint: Color, modifier: Modifier = Modifier, size: androidx.compose.ui.unit.Dp = 20.dp) {
    Icon(painter = painterResource(id), contentDescription = null, tint = tint, modifier = modifier.size(size))
}

/** The 40 dp round button in the screen header ("btn/sliders" in the design). */
@Composable
fun RoundIconButton(@DrawableRes icon: Int, description: String, onClick: () -> Unit, modifier: Modifier = Modifier) {
    Box(
        modifier = modifier
            .size(40.dp)
            .background(MaterialTheme.colorScheme.surface, CircleShape)
            .border(1.dp, MaterialTheme.colorScheme.outline, CircleShape)
            .clickable(role = Role.Button, onClick = onClick)
            .semantics { contentDescription = description },
        contentAlignment = Alignment.Center,
    ) {
        LipiIcon(icon, MaterialTheme.colorScheme.onSurface)
    }
}

/** The dark rounded label on a thumbnail ("6 pages"). */
@Composable
fun PagePill(text: String, modifier: Modifier = Modifier) {
    Text(
        text = text,
        style = MaterialTheme.typography.labelSmall,
        color = Color.White,
        modifier = modifier
            .background(LocalLipiColors.current.scrim, RoundedCornerShape(LipiDimens.ChipRadius))
            .padding(horizontal = 8.dp, vertical = 4.dp),
    )
}

/** The 48 dp search field of the Library. */
@Composable
fun LipiSearchField(value: String, onValueChange: (String) -> Unit, placeholder: String, modifier: Modifier = Modifier) {
    val shape = RoundedCornerShape(16.dp)
    Row(
        modifier = modifier
            .fillMaxWidth()
            .height(48.dp)
            .background(MaterialTheme.colorScheme.surface, shape)
            .border(BorderStroke(1.dp, MaterialTheme.colorScheme.outline), shape)
            .padding(horizontal = 16.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        LipiIcon(`in`.commandlinecoding.lipi.R.drawable.ic_search, MaterialTheme.colorScheme.onSurface)
        Box(modifier = Modifier.weight(1f), contentAlignment = Alignment.CenterStart) {
            if (value.isEmpty()) {
                Text(placeholder, style = MaterialTheme.typography.bodyLarge, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            BasicTextField(
                value = value,
                onValueChange = onValueChange,
                singleLine = true,
                textStyle = MaterialTheme.typography.bodyLarge.copy(color = MaterialTheme.colorScheme.onSurface),
                cursorBrush = SolidColor(MaterialTheme.colorScheme.primary),
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}

enum class HomeTab { Library, Settings }

/**
 * The bottom bar with the raised scan button, as in the Library and Settings frames: an 84 dp white bar with two
 * tabs, and a 64 dp round primary button that sits across its top edge.
 */
@Composable
fun HomeBottomBar(
    selected: HomeTab,
    libraryLabel: String,
    settingsLabel: String,
    scanDescription: String,
    onLibrary: () -> Unit,
    onSettings: () -> Unit,
    onScan: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Box(modifier = modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .fillMaxWidth()
                .shadow(8.dp, spotColor = Color.Black.copy(alpha = 0.06f), ambientColor = Color.Black.copy(alpha = 0.06f))
                .background(MaterialTheme.colorScheme.surface)
                .navigationBarsPadding()
                .height(LipiDimens.BottomBarHeight)
                .padding(horizontal = 56.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            BottomTab(`in`.commandlinecoding.lipi.R.drawable.ic_folder, libraryLabel, selected == HomeTab.Library, onLibrary)
            Spacer(Modifier.width(64.dp))
            BottomTab(`in`.commandlinecoding.lipi.R.drawable.ic_sliders, settingsLabel, selected == HomeTab.Settings, onSettings)
        }
        Box(
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .navigationBarsPadding()
                .padding(bottom = 48.dp)
                .shadow(8.dp, CircleShape, spotColor = MaterialTheme.colorScheme.primary.copy(alpha = 0.35f), ambientColor = MaterialTheme.colorScheme.primary.copy(alpha = 0.35f))
                .size(LipiDimens.FabSize)
                .background(MaterialTheme.colorScheme.primary, CircleShape)
                .clickable(role = Role.Button, onClick = onScan)
                .semantics { contentDescription = scanDescription },
            contentAlignment = Alignment.Center,
        ) {
            LipiIcon(`in`.commandlinecoding.lipi.R.drawable.ic_scan, MaterialTheme.colorScheme.onPrimary, size = 28.dp)
        }
    }
}

@Composable
private fun BottomTab(@DrawableRes icon: Int, label: String, selected: Boolean, onClick: () -> Unit) {
    val color = if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant
    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(4.dp),
        modifier = Modifier.clickable(role = Role.Tab, onClick = onClick),
    ) {
        LipiIcon(icon, color, size = 24.dp)
        Text(
            label,
            style = MaterialTheme.typography.labelSmall.copy(
                fontWeight = if (selected) androidx.compose.ui.text.font.FontWeight.SemiBold else androidx.compose.ui.text.font.FontWeight.Medium,
            ),
            color = color,
        )
    }
}
