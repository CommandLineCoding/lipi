package `in`.commandlinecoding.lipi.ui

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class RoutesTest {
    @Test
    fun documentRouteCarriesTheIdAndTheExportFlag() {
        assertEquals("document/42?export=false", Routes.document(42))
        assertEquals("document/7?export=true", Routes.document(7, export = true))
    }

    @Test
    fun documentRouteMatchesItsPattern() {
        val pattern = Routes.DocumentPattern.substringBefore("/{")
        assertTrue(Routes.document(1).startsWith("$pattern/"))
    }

    @Test
    fun adjustRouteStillCarriesItsArguments() = assertEquals("adjust?page=2&single=true", Routes.adjust(page = 2, single = true))
}
