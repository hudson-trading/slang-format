# ~~~
# SPDX-FileCopyrightText: Hudson River Trading
# SPDX-License-Identifier: MIT
# ~~~

find_package(Catch2 3 QUIET)
if(NOT TARGET Catch2::Catch2WithMain)
  include(FetchContent)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.10.0
    GIT_SHALLOW ON)
  FetchContent_MakeAvailable(Catch2)
endif()

add_executable(
  formatter_unittests FormatConfigTests.cpp FormatDocumentTests.cpp
                      FormatValidationTests.cpp NormalizedFormatTests.cpp)
target_link_libraries(formatter_unittests PRIVATE slang_format_lib
                                                  Catch2::Catch2WithMain)
add_test(NAME formatter_unittests COMMAND formatter_unittests)

find_package(
  Python3
  COMPONENTS Interpreter
  QUIET)
if(Python3_FOUND)
  if(NOT TARGET slang::driver)
    add_subdirectory(${PROJECT_SOURCE_DIR}/external/slang/tools/driver
                     ${CMAKE_CURRENT_BINARY_DIR}/slang-driver)
  endif()

  add_test(
    NAME format_golden_tests
    COMMAND
      ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/test_format.py
      --slang-format $<TARGET_FILE:slang_format> --slang
      $<TARGET_FILE:slang_driver> --exclude-prop
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
  set_tests_properties(format_golden_tests PROPERTIES LABELS "format")

  add_test(
    NAME format_config_discovery_tests
    COMMAND
      ${Python3_EXECUTABLE}
      ${PROJECT_SOURCE_DIR}/scripts/test_config_discovery.py --slang-format
      $<TARGET_FILE:slang_format>
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
  set_tests_properties(format_config_discovery_tests PROPERTIES LABELS "format")
endif()
