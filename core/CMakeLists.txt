add_library(lipi_core
  src/deskew.cpp
  src/detect.cpp
  src/detector.cpp
  src/enhance.cpp
  src/geometry.cpp
  src/gray.cpp
  src/image.cpp
  src/pdf.cpp
  src/quad.cpp
  src/resize.cpp
  src/smooth.cpp
  src/status.cpp
  src/version.cpp
  src/warp.cpp
)
target_include_directories(lipi_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(lipi_core PRIVATE lipi_options)
target_compile_definitions(lipi_core PRIVATE LIPI_BUILDING_CORE)
if(BUILD_SHARED_LIBS)
  target_compile_definitions(lipi_core PUBLIC LIPI_SHARED)
endif()
add_library(lipi::core ALIAS lipi_core)

# Optional learned detector. Point CMAKE_PREFIX_PATH at an ncnn install
# (built with NCNN_VULKAN=OFF), e.g. -DLIPI_WITH_NCNN=ON -DCMAKE_PREFIX_PATH=/opt/ncnn.
option(LIPI_WITH_NCNN "Build the learned page detector on top of ncnn" OFF)
if(LIPI_WITH_NCNN)
  find_package(ncnn REQUIRED)
  target_link_libraries(lipi_core PRIVATE ncnn)
  # Some ncnn builds export -fno-exceptions -fno-rtti to their users. The core reports allocation failures as
  # status codes with try/catch, so it must not inherit them. ncnn only needs the headers and the libraries.
  set_property(TARGET ncnn PROPERTY INTERFACE_COMPILE_OPTIONS "")
  target_compile_definitions(lipi_core PRIVATE LIPI_HAVE_NCNN)
endif()
