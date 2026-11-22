add_executable(lipi_tests
  main.cpp
  test_image.cpp
)
target_include_directories(lipi_tests PRIVATE ${PROJECT_SOURCE_DIR}/third_party/doctest)
target_link_libraries(lipi_tests PRIVATE lipi::core lipi_options)

include(CTest)
add_test(NAME lipi_tests COMMAND lipi_tests)
