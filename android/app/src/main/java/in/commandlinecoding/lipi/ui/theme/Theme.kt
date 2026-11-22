package `in`.commandlinecoding.lipi.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Shapes
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.Immutable
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp

/** Design values that Material 3 has no slot for. */
@Immutable
data class LipiColors(
    val scrim: Color,
    val paperLine: Color,
    val paperTitle: Color,
    val thumbTints: List<Color>,
)

val LocalLipiColors = staticCompositionLocalOf {
    LipiColors(LightPalette.Scrim, LightPalette.PaperLine, LightPalette.PaperTitle, LightPalette.ThumbTints)
}

/** Spacing and sizes used across screens (dp). */
object LipiDimens {
    val ScreenPadding = 20.dp
    val Gap = 14.dp
    val CardRadius = 16.dp
    val ChipRadius = 99.dp
    val FabSize = 64.dp
    val BottomBarHeight = 84.dp
}

private val LightScheme = lightColorScheme(
    primary = LightPalette.Primary,
    onPrimary = LightPalette.OnPrimary,
    background = LightPalette.Background,
    onBackground = LightPalette.OnSurface,
    surface = LightPalette.Surface,
    onSurface = LightPalette.OnSurface,
    surfaceVariant = LightPalette.Surface,
    onSurfaceVariant = LightPalette.OnSurfaceVariant,
    outline = LightPalette.Outline,
    outlineVariant = LightPalette.Outline,
)

private val DarkScheme = darkColorScheme(
    primary = DarkPalette.Primary,
    onPrimary = DarkPalette.OnPrimary,
    background = DarkPalette.Background,
    onBackground = DarkPalette.OnSurface,
    surface = DarkPalette.Surface,
    onSurface = DarkPalette.OnSurface,
    surfaceVariant = DarkPalette.Surface,
    onSurfaceVariant = DarkPalette.OnSurfaceVariant,
    outline = DarkPalette.Outline,
    outlineVariant = DarkPalette.Outline,
)

private val LipiShapes = Shapes(
    small = RoundedCornerShape(LipiDimens.CardRadius),
    medium = RoundedCornerShape(LipiDimens.CardRadius),
    large = RoundedCornerShape(28.dp),
)

@Composable
fun LipiTheme(darkTheme: Boolean = isSystemInDarkTheme(), content: @Composable () -> Unit) {
    val lipiColors = if (darkTheme) {
        LipiColors(DarkPalette.Scrim, DarkPalette.PaperLine, DarkPalette.PaperTitle, DarkPalette.ThumbTints)
    } else {
        LipiColors(LightPalette.Scrim, LightPalette.PaperLine, LightPalette.PaperTitle, LightPalette.ThumbTints)
    }
    CompositionLocalProvider(LocalLipiColors provides lipiColors) {
        MaterialTheme(
            colorScheme = if (darkTheme) DarkScheme else LightScheme,
            typography = LipiTypography,
            shapes = LipiShapes,
            content = content,
        )
    }
}
