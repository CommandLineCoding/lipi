add_library(lipi_core
  src/geometry.cpp
  src/gray.cpp
  src/image.cpp
  src/detect.cpp
  src/pdf.cpp
  src/quad.cpp
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
