add_executable(lipi_tests
  main.cpp
  test_detect.cpp
  test_detector.cpp
  test_enhance.cpp
  test_image.cpp
  test_pdf.cpp
  test_smooth.cpp
  test_warp.cpp
)
target_include_directories(lipi_tests PRIVATE ${PROJECT_SOURCE_DIR}/third_party/doctest ${PROJECT_SOURCE_DIR}/core/src)
target_link_libraries(lipi_tests PRIVATE lipi::core lipi_options)
if(TARGET lipi_cli_support)
  target_sources(lipi_tests PRIVATE test_cli_support.cpp)
  target_link_libraries(lipi_tests PRIVATE lipi_cli_support)
endif()

include(CTest)
add_test(NAME lipi_tests COMMAND lipi_tests)
