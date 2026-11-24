cmake_minimum_required(VERSION 3.22)
project(lipi_android LANGUAGES C CXX)

# The portable core lives in the repository root; the app only adds the JNI glue.
set(LIPI_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LIPI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
get_filename_component(LIPI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)

# Native code is optimised in debug builds too. Unoptimised ncnn is about twice as slow, which makes the camera preview
# stutter and the timings in the log meaningless. Symbols are still kept (and stripped from the APK).
# (The NDK toolchain sets these as plain variables, so they are appended to here and not set as cache entries.)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O2")

# Use ccache when it is installed. It only saves compile time (ncnn is large) and does not change the output.
find_program(LIPI_CCACHE ccache)
if(LIPI_CCACHE)
  set(CMAKE_C_COMPILER_LAUNCHER "${LIPI_CCACHE}")
  set(CMAKE_CXX_COMPILER_LAUNCHER "${LIPI_CCACHE}")
endif()

# ncnn, the inference engine of the page detector, is built from source (the git submodule in third_party/ncnn) so that
# the whole app, native code included, can be rebuilt from this repository. CPU only: no Vulkan.
set(NCNN_VULKAN OFF CACHE BOOL "" FORCE)
set(NCNN_SHARED_LIB OFF CACHE BOOL "" FORCE)
set(NCNN_OPENMP ON CACHE BOOL "" FORCE)
set(NCNN_SIMPLEOCV ON CACHE BOOL "" FORCE)
set(NCNN_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(NCNN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(NCNN_BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
set(NCNN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NCNN_PYTHON OFF CACHE BOOL "" FORCE)
set(NCNN_INSTALL_SDK OFF CACHE BOOL "" FORCE)
# The detector is a float network: leave out the int8 and bf16 kernels and the newest CPU variants. This keeps the build
# time and the library size down; the CPU features ncnn picks at run time stay (NEON, fp16 on Armv8.2, AVX2).
set(NCNN_INT8 OFF CACHE BOOL "" FORCE)
set(NCNN_BF16 OFF CACHE BOOL "" FORCE)
set(NCNN_ARM82DOT OFF CACHE BOOL "" FORCE)
set(NCNN_ARM82FP16FML OFF CACHE BOOL "" FORCE)
set(NCNN_ARM84BF16 OFF CACHE BOOL "" FORCE)
set(NCNN_ARM84I8MM OFF CACHE BOOL "" FORCE)
set(NCNN_ARM86SVE OFF CACHE BOOL "" FORCE)
set(NCNN_AVX512 OFF CACHE BOOL "" FORCE)
set(NCNN_AVX512VNNI OFF CACHE BOOL "" FORCE)
set(NCNN_AVX512BF16 OFF CACHE BOOL "" FORCE)
set(NCNN_AVX512FP16 OFF CACHE BOOL "" FORCE)
set(NCNN_AVXVNNI OFF CACHE BOOL "" FORCE)
set(NCNN_AVXVNNIINT8 OFF CACHE BOOL "" FORCE)
set(NCNN_AVXVNNIINT16 OFF CACHE BOOL "" FORCE)
set(NCNN_AVXNECONVERT OFF CACHE BOOL "" FORCE)
add_subdirectory("${LIPI_ROOT}/third_party/ncnn" ncnn)
set(LIPI_WITH_NCNN ON CACHE BOOL "" FORCE)
add_subdirectory("${LIPI_ROOT}" lipi)

add_library(lipi_jni SHARED lipi_jni.cpp lipi_jni_pages.cpp)
target_link_libraries(lipi_jni PRIVATE lipi::core lipi_options log jnigraphics)
# 16 KB page alignment, as Android 15+ and the Play Store require.
target_link_options(lipi_jni PRIVATE -Wl,-z,max-page-size=16384)
