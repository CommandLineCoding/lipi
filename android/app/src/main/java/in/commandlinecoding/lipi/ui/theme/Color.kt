package `in`.commandlinecoding.lipi.ui.theme

import androidx.compose.ui.graphics.Color

// Light palette: values read from the Figma file "FOSS Document Scanner - App Design" (frame 01 Library and the
// other five screens). The design has no dark theme, so the dark palette below is derived from it; see docs/DESIGN.md.

object LightPalette {
    val Background = Color(0xFFF5F6FA)
    val Surface = Color(0xFFFFFFFF)
    val Outline = Color(0xFFE6E8EF)
    val OnSurface = Color(0xFF10131A)
    val OnSurfaceVariant = Color(0xFF6B7280)
    val Primary = Color(0xFF3D5AFE)
    val OnPrimary = Color(0xFFFFFFFF)
    val Scrim = Color(0xB810131A) // 72% of the text colour, used by page-count pills
    val PaperLine = Color(0xFFE3E6EE)
    val PaperTitle = Color(0xFFC3C9D8)
    val ThumbTints = listOf(
        Color(0xFFE6EAF7), Color(0xFFE5F0E8), Color(0xFFF3EADF),
        Color(0xFFECE6F5), Color(0xFFE4F0F3), Color(0xFFF2E7E7),
    )
}

object DarkPalette {
    val Background = Color(0xFF0E1117)
    val Surface = Color(0xFF171B24)
    val Outline = Color(0xFF2A303C)
    val OnSurface = Color(0xFFE8EAF0)
    val OnSurfaceVariant = Color(0xFF9AA3B2)
    val Primary = Color(0xFF8C9EFF)
    val OnPrimary = Color(0xFF0B1030)
    val Scrim = Color(0xB8000000)
    val PaperLine = Color(0xFF2B3140)
    val PaperTitle = Color(0xFF3A4152)
    val ThumbTints = listOf(
        Color(0xFF1B2133), Color(0xFF1B2A22), Color(0xFF2E271D),
        Color(0xFF262033), Color(0xFF1B2A2F), Color(0xFF2E2323),
    )
}

/** The camera screens are dark in both themes, over the live picture. Values from the Camera frame of the design. */
object CameraPalette {
    val Background = Color(0xFF0B0D12)
    val Control = Color(0x24FFFFFF) // white at 14%
    val ModeInactive = Color(0xFF9AA0AE)
    val Accent = Color(0xFF2DD4A0)
    val OnAccent = Color(0xFF04261B)
    val FlashOn = Color(0xFFFFD54A)
    val Indigo = Color(0xFF3D5AFE) // handles, outline and the main button over the dark picture
}
