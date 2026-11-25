package `in`.commandlinecoding.lipi.data

import org.junit.Assert.assertEquals
import org.junit.Test

class ThemeModeTest {
    @Test
    fun storedNamesComeBack() {
        for (mode in ThemeMode.entries) assertEquals(mode, ThemeMode.fromStored(mode.name))
    }

    @Test
    fun missingOrUnknownMeansFollowTheSystem() {
        assertEquals(ThemeMode.System, ThemeMode.fromStored(null))
        assertEquals(ThemeMode.System, ThemeMode.fromStored(""))
        assertEquals(ThemeMode.System, ThemeMode.fromStored("sepia"))
    }
}
