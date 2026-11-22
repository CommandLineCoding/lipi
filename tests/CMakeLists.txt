add_executable(lipi_tests
  main.cpp
  test_detect.cpp
  test_detector.cpp
  test_image.cpp
  test_warp.cpp
)
target_include_directories(lipi_tests PRIVATE ${PROJECT_SOURCE_DIR}/third_party/doctest ${PROJECT_SOURCE_DIR}/core/src)
target_link_libraries(lipi_tests PRIVATE lipi::core lipi_options)

include(CTest)
add_test(NAME lipi_tests COMMAND lipi_tests)
