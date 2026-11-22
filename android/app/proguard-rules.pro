# JNI entry points are looked up by name from the native library.
-keepclasseswithmembernames class * {
    native <methods>;
}
