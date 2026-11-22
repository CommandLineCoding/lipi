cmake_minimum_required(VERSION 3.22)
project(lipi_android LANGUAGES C CXX)

# The portable core lives in the repository root; the app only adds the JNI glue.
set(LIPI_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LIPI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
get_filename_component(LIPI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
add_subdirectory("${LIPI_ROOT}" lipi)

add_library(lipi_jni SHARED lipi_jni.cpp)
target_link_libraries(lipi_jni PRIVATE lipi::core lipi_options log)
# 16 KB page alignment, as Android 15+ and the Play Store require.
target_link_options(lipi_jni PRIVATE -Wl,-z,max-page-size=16384)
