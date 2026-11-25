package `in`.commandlinecoding.lipi.ui.settings

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class LicencesTest {
    @Test
    fun idsAreUnique() = assertEquals(Licences.all.size, Licences.all.map { it.id }.toSet().size)

    @Test
    fun everyEntryNamesItsLicenceHolderAndLink() {
        for (e in Licences.all) {
            assertTrue(e.id, e.licence.isNotBlank() && e.holder.isNotBlank() && e.url.startsWith("https://"))
        }
    }

    @Test
    fun theComponentsThatNeedANoticeShipTheirText() {
        // BSD, MIT and OFL ask for their notice to be shipped with binaries; Apache-2.0 asks for the licence text.
        for (id in listOf("lipi", "ncnn", "inter", "androidx", "kotlin", "google", "checker")) {
            assertTrue("$id has no licence text", Licences.all.first { it.id == id }.textAsset != null)
        }
    }

    @Test
    fun textFilesAreTheOnesTheBuildCopies() {
        val copied = setOf("lipi-MIT.txt", "ncnn-BSD-3-Clause.txt", "inter-OFL-1.1.txt", "Apache-2.0.txt", "checker-qual-MIT.txt")
        assertTrue(Licences.all.mapNotNull { it.textAsset }.all { it in copied })
    }
}
