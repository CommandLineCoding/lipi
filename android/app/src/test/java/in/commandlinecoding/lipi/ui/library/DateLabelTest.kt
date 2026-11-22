package `in`.commandlinecoding.lipi.ui.library

import org.junit.Assert.assertEquals
import org.junit.Test
import java.time.LocalDate
import java.time.ZoneId
import java.util.Locale

class DateLabelTest {
    private val zone = ZoneId.of("UTC")
    private val words = DateLabel.Words("Today", "Yesterday")
    private val now = DateLabel.startOf(LocalDate.of(2026, 11, 22), zone) + 15 * 3_600_000L  // Sunday 22 Nov 2026, 15:00

    private fun label(day: LocalDate, hour: Long = 9) =
        DateLabel.of(DateLabel.startOf(day, zone) + hour * 3_600_000L, now, words, zone, Locale.ENGLISH)

    @Test fun sameDayIsToday() = assertEquals("Today", label(LocalDate.of(2026, 11, 22)))

    @Test fun lateInTheDayIsStillToday() = assertEquals("Today", label(LocalDate.of(2026, 11, 22), hour = 14))

    @Test fun previousDayIsYesterday() = assertEquals("Yesterday", label(LocalDate.of(2026, 11, 21)))

    @Test fun lastWeekShowsTheWeekday() {
        assertEquals("Fri", label(LocalDate.of(2026, 11, 20)))
        assertEquals("Mon", label(LocalDate.of(2026, 11, 16)))
    }

    @Test fun olderThisYearShowsMonthAndDay() {
        assertEquals("Nov 15", label(LocalDate.of(2026, 11, 15)))
        assertEquals("Jan 3", label(LocalDate.of(2026, 1, 3)))
    }

    @Test fun earlierYearsShowTheYear() = assertEquals("Dec 31, 2025", label(LocalDate.of(2025, 12, 31)))
}
