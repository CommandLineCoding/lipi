package `in`.commandlinecoding.lipi.ui.settings

/** One open-source component the app ships, with the licence text that goes with it (an asset under licenses/). */
data class LicenceEntry(
    val id: String,
    val name: String,
    val use: String,
    val licence: String,
    val holder: String,
    val url: String,
    val textAsset: String?,
)

/** What the app is built on. BSD, MIT and OFL ask for their notice to be shipped with binaries; Apache-2.0 asks for the licence text. */
object Licences {
    const val REPOSITORY = "https://github.com/CommandLineCoding/lipi"

    val all = listOf(
        LicenceEntry("lipi", "Lipi", "This app, the core library and the PageNet model files", "MIT",
            "Copyright (c) 2026 Suman Biswas and Lipi contributors", REPOSITORY, "lipi-MIT.txt"),
        LicenceEntry("ncnn", "ncnn", "Runs the page detector on the phone", "BSD 3-Clause",
            "Copyright (C) 2017 Tencent", "https://github.com/Tencent/ncnn", "ncnn-BSD-3-Clause.txt"),
        LicenceEntry("inter", "Inter", "The typeface of the app", "SIL Open Font License 1.1",
            "Copyright 2020 The Inter Project Authors", "https://github.com/rsms/inter", "inter-OFL-1.1.txt"),
        LicenceEntry("androidx", "AndroidX: Jetpack Compose, CameraX, Room, Navigation, Lifecycle, Activity, Core and others",
            "The app's framework libraries", "Apache License 2.0", "The Android Open Source Project",
            "https://developer.android.com/jetpack", "Apache-2.0.txt"),
        LicenceEntry("kotlin", "Kotlin and kotlinx.coroutines", "The language and its libraries", "Apache License 2.0",
            "JetBrains s.r.o. and Kotlin Programming Language contributors", "https://kotlinlang.org", "Apache-2.0.txt"),
        LicenceEntry("google", "Guava, Dagger, AutoValue and JSpecify", "Used by CameraX and Room", "Apache License 2.0",
            "Google LLC and The JSpecify Authors", "https://github.com/google/guava", "Apache-2.0.txt"),
        LicenceEntry("checker", "Checker Framework qualifiers", "Annotations used by Guava", "MIT",
            "Copyright 2004-present by the Checker Framework developers", "https://checkerframework.org", "checker-qual-MIT.txt"),
        LicenceEntry("pagenet-data", "Photos used to train the page detector", "Real photos of pages, with synthetic images",
            "Creative Commons Attribution 4.0", "Soheili, Mansouri and Baniadam Dizaj (2020)",
            "https://doi.org/10.17632/x3nm4cxr83.3", null),
    )
}
