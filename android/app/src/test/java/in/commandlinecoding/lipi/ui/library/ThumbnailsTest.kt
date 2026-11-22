package `in`.commandlinecoding.lipi.ui.library

import org.junit.Assert.assertEquals
import org.junit.Test

class ThumbnailsTest {
    @Test
    fun smallImagesAreNotShrunk() = assertEquals(1, sampleSizeFor(300, 200, 400))

    @Test
    fun sampleSizeIsAPowerOfTwoThatKeepsAtLeastTheRequestedEdge() {
        assertEquals(8, sampleSizeFor(4000, 3000, 480))
        assertEquals(4, sampleSizeFor(4000, 3000, 1000))
    }

    @Test
    fun usesTheLongerSide() = assertEquals(sampleSizeFor(4000, 100, 500), sampleSizeFor(100, 4000, 500))
}
