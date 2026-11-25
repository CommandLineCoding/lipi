plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
    alias(libs.plugins.ksp)
}

// What the app bundles besides code, copied from the repository so that there is one copy of each: the page detector's
// model (only PageNet, trained without the CC BY-SA ID-card frames; see models/page-net-small/MODEL_CARD.md) and the
// licence texts that the open-source licences ask us to ship with the app.
val bundledAssets = layout.buildDirectory.dir("generated/assets")
val copyBundledAssets = tasks.register<Copy>("copyBundledAssets") {
    from(rootProject.file("../models/page-net")) {
        include("page-net.ncnn.param", "page-net.ncnn.bin")
        into("models")
    }
    from(rootProject.file("../LICENSE")) { rename { "lipi-MIT.txt" }; into("licenses") }
    from(rootProject.file("../third_party/ncnn/LICENSE.txt")) { rename { "ncnn-BSD-3-Clause.txt" }; into("licenses") }
    from(rootProject.file("../third_party/fonts/inter/OFL.txt")) { rename { "inter-OFL-1.1.txt" }; into("licenses") }
    from(rootProject.file("../third_party/licenses")) { include("Apache-2.0.txt", "checker-qual-MIT.txt"); into("licenses") }
    into(bundledAssets)
}
tasks.named("preBuild") { dependsOn(copyBundledAssets) }

android {
    namespace = "in.commandlinecoding.lipi"
    compileSdk = 36
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "in.commandlinecoding.lipi"
        minSdk = 26
        targetSdk = 36
        versionCode = 1
        versionName = "0.1.0"

        ndk {
            // arm64-v8a first; x86_64 is only for the emulator.
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                arguments += listOf("-DANDROID_STL=c++_static")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildFeatures {
        compose = true
        buildConfig = true
    }

    packaging {
        jniLibs {
            // Native libraries are stored uncompressed and page-aligned (16 KB), which Android 15+ requires.
            useLegacyPackaging = false
        }
    }

    sourceSets {
        getByName("main") { assets.srcDir(bundledAssets) }
    }

    // Reproducible builds: no timestamps or VCS info in the output.
    dependenciesInfo {
        includeInApk = false
        includeInBundle = false
    }
}

ksp {
    arg("room.schemaLocation", "$projectDir/schemas")
}

kotlin {
    compilerOptions {
        jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17)
    }
}

dependencies {
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(libs.androidx.navigation.compose)
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(libs.androidx.lifecycle.viewmodel.compose)
    implementation(libs.androidx.compose.ui)
    implementation(libs.androidx.compose.ui.graphics)
    implementation(libs.androidx.compose.foundation)
    implementation(libs.androidx.compose.material3)
    implementation(libs.androidx.compose.ui.tooling.preview)
    implementation(libs.androidx.camera.core)
    implementation(libs.androidx.camera.camera2)
    implementation(libs.androidx.camera.lifecycle)
    implementation(libs.androidx.camera.view)
    implementation(libs.androidx.exifinterface)
    implementation(libs.androidx.room.runtime)
    implementation(libs.androidx.room.ktx)
    ksp(libs.androidx.room.compiler)
    debugImplementation(libs.androidx.compose.ui.tooling)

    testImplementation(libs.junit)
    testImplementation(libs.kotlinx.coroutines.test)
}
