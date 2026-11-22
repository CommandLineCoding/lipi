add_library(lipi_cli_support STATIC
  exif.cpp
  imageio.cpp
)
target_include_directories(lipi_cli_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(lipi_cli_support SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party/stb)
target_link_libraries(lipi_cli_support PUBLIC lipi::core PRIVATE lipi_options)

add_executable(lipi-cli main.cpp)
target_link_libraries(lipi-cli PRIVATE lipi_cli_support lipi_options)

if(LIPI_BUILD_TESTS)
  set(PAGE ${PROJECT_SOURCE_DIR}/tests/data/page.png)
  add_test(NAME cli_version COMMAND lipi-cli --version)
  set_tests_properties(cli_version PROPERTIES PASS_REGULAR_EXPRESSION "lipi-cli 0\\.0\\.1")
  add_test(NAME cli_detect_json COMMAND lipi-cli detect ${PAGE} --json)
  set_tests_properties(cli_detect_json PROPERTIES PASS_REGULAR_EXPRESSION "\"quad\":\\[\\[")
  add_test(NAME cli_scan COMMAND lipi-cli scan ${PAGE} -o ${CMAKE_CURRENT_BINARY_DIR}/cli_scan_out.png)
  set_tests_properties(cli_scan PROPERTIES PASS_REGULAR_EXPRESSION "wrote .*cli_scan_out\\.png")
  add_test(NAME cli_missing_file COMMAND lipi-cli detect does-not-exist.png)
  set_tests_properties(cli_missing_file PROPERTIES PASS_REGULAR_EXPRESSION "cannot read does-not-exist\\.png")
  add_test(NAME cli_bad_command COMMAND lipi-cli bogus)
  set_tests_properties(cli_bad_command PROPERTIES PASS_REGULAR_EXPRESSION "unknown command 'bogus'")
endif()
