package `in`.commandlinecoding.lipi.ui.library

import java.time.Instant
import java.time.LocalDate
import java.time.ZoneId
import java.time.format.TextStyle
import java.util.Locale

/**
 * The short date under a document title, as in the design: "Today", "Yesterday", a weekday for the last
 * week ("Mon"), and otherwise the day and month ("Nov 18"), with the year added for earlier years.
 */
object DateLabel {
    data class Words(val today: String, val yesterday: String)

    fun of(
        createdAtMillis: Long,
        nowMillis: Long,
        words: Words,
        zone: ZoneId = ZoneId.systemDefault(),
        locale: Locale = Locale.getDefault(),
    ): String {
        val day = Instant.ofEpochMilli(createdAtMillis).atZone(zone).toLocalDate()
        val today = Instant.ofEpochMilli(nowMillis).atZone(zone).toLocalDate()
        val daysAgo = java.time.temporal.ChronoUnit.DAYS.between(day, today)
        return when {
            daysAgo == 0L -> words.today
            daysAgo == 1L -> words.yesterday
            daysAgo in 2..6 -> day.dayOfWeek.getDisplayName(TextStyle.SHORT, locale)
            day.year == today.year -> "${day.month.getDisplayName(TextStyle.SHORT, locale)} ${day.dayOfMonth}"
            else -> "${day.month.getDisplayName(TextStyle.SHORT, locale)} ${day.dayOfMonth}, ${day.year}"
        }
    }

    /** Whether [day] is a valid local date; used by tests to build timestamps. */
    internal fun startOf(day: LocalDate, zone: ZoneId): Long = day.atStartOfDay(zone).toInstant().toEpochMilli()
}
