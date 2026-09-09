# ~~~
# SPDX-FileCopyrightText: Hudson River Trading
# SPDX-License-Identifier: MIT
# ~~~

cmake_minimum_required(VERSION 3.23...3.29)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds are not supported. Remove CMakeCache.txt and CMakeFiles/ first."
  )
endif()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" SLANG_FORMAT_VERSION)
string(STRIP "${SLANG_FORMAT_VERSION}" SLANG_FORMAT_VERSION)

project(
  slang-format
  VERSION ${SLANG_FORMAT_VERSION}
  LANGUAGES CXX
  HOMEPAGE_URL https://github.com/hudson-trading/slang-format
  DESCRIPTION "An opinionated SystemVerilog formatter based on slang")

if(PROJECT_IS_TOP_LEVEL)
  message(STATUS "CMake version: ${CMAKE_VERSION}")
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

option(SLANG_FORMAT_BUILD_CLI "Build the slang-format command-line tool"
       ${PROJECT_IS_TOP_LEVEL})
option(SLANG_FORMAT_INCLUDE_TESTS "Build formatter tests"
       ${PROJECT_IS_TOP_LEVEL})
option(SLANG_FORMAT_INCLUDE_INSTALL "Add installation targets"
       ${PROJECT_IS_TOP_LEVEL})
option(SLANG_FORMAT_STATIC_LINK_STDLIB
       "Statically link the C++ runtime where supported" OFF)
option(SLANG_FORMAT_CI_BUILD "Enable stricter warnings for CI builds" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

if(PROJECT_IS_TOP_LEVEL)
  get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  if(NOT is_multi_config AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE
        Debug
        CACHE STRING
              "Choose the build type: Debug Release RelWithDebInfo MinSizeRel."
              FORCE)
  endif()

  foreach(
    var
    CMAKE_RUNTIME_OUTPUT_DIRECTORY
    CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE
    CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO
    CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL
    CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG)
    if(NOT ${var})
      set(${var} ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
    endif()
  endforeach()
  foreach(
    var
    CMAKE_ARCHIVE_OUTPUT_DIRECTORY
    CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE
    CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO
    CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL
    CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG
    CMAKE_LIBRARY_OUTPUT_DIRECTORY
    CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE
    CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO
    CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL
    CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG)
    if(NOT ${var})
      set(${var} ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
    endif()
  endforeach()
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  add_compile_definitions(WIN32 _WINDOWS NTDDI_VERSION=0x06010000
                          _WIN32_WINNT=0x0601)
  add_compile_definitions(_SCL_SECURE_NO_WARNINGS _CRT_SECURE_NO_WARNINGS)
  add_compile_definitions(_CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_WARNINGS)
endif()

if(MSVC)
  add_compile_options(/utf-8 /bigobj /permissive- /Zc:preprocessor)
endif()

if(SLANG_FORMAT_STATIC_LINK_STDLIB)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT CMAKE_SYSTEM_NAME
                                                   MATCHES "Darwin")
    add_link_options(-static-libstdc++ -static-libgcc)
  elseif(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
endif()

if(PROJECT_IS_TOP_LEVEL AND NOT DEFINED CMAKE_DISABLE_FIND_PACKAGE_fmt)
  set(CMAKE_DISABLE_FIND_PACKAGE_fmt TRUE)
endif()

if(NOT TARGET slang::slang)
  set(SLANG_INCLUDE_TOOLS
      OFF
      CACHE BOOL "Do not build slang tools" FORCE)
  set(SLANG_INCLUDE_DOCS
      OFF
      CACHE BOOL "Do not build slang docs" FORCE)
  set(SLANG_INCLUDE_PYLIB
      OFF
      CACHE BOOL "Do not build slang Python bindings" FORCE)
  set(SLANG_INCLUDE_INSTALL
      OFF
      CACHE BOOL "Do not install slang separately" FORCE)
  set(SLANG_INCLUDE_TESTS
      OFF
      CACHE BOOL "Do not build slang tests" FORCE)
  add_subdirectory(external/slang EXCLUDE_FROM_ALL)
endif()

if(NOT TARGET fmt::fmt)
  find_package(fmt 12 REQUIRED)
endif()

if(NOT TARGET reflectcpp::reflectcpp)
  set(REFLECTCPP_INSTALL
      OFF
      CACHE BOOL "Do not install reflect-cpp separately" FORCE)
  add_subdirectory(external/reflect-cpp EXCLUDE_FROM_ALL)
endif()

find_package(Git QUIET)
if(Git_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE SLANG_FORMAT_VERSION_HASH
    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
if(SLANG_FORMAT_VERSION_HASH)
  set(SLANG_FORMAT_VERSION_FULL
      "${PROJECT_VERSION}+${SLANG_FORMAT_VERSION_HASH}")
else()
  set(SLANG_FORMAT_VERSION_FULL "${PROJECT_VERSION}")
endif()
message(STATUS "slang-format version: ${SLANG_FORMAT_VERSION_FULL}")

configure_file(include/format/Version.h.in
               "${CMAKE_CURRENT_BINARY_DIR}/include/format/Version.h" @ONLY)

add_library(
  slang_format_lib STATIC
  src/Formatter.cpp
  src/FormatConfig.cpp
  src/NormalizedFormat.cpp
  src/FormatDocument.cpp
  src/Layout.cpp
  src/CstValidation.cpp
  src/FormatStyle.cpp
  src/FormatValidation.cpp)
add_library(slang::format ALIAS slang_format_lib)
target_include_directories(
  slang_format_lib
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>)
target_compile_features(slang_format_lib PUBLIC cxx_std_20)
target_link_libraries(slang_format_lib PUBLIC slang::slang fmt::fmt
                                              reflectcpp::reflectcpp)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(
    slang_format_lib
    PRIVATE -Wall
            -Wextra
            -Wpedantic
            -Wpessimizing-move
            -Wno-nonnull
            -Wno-unused-parameter
            -Werror)
  if(SLANG_FORMAT_CI_BUILD)
    target_compile_options(
      slang_format_lib PRIVATE -Wunused-variable -Wunused-but-set-variable
                               -Wunused-parameter)
  endif()
endif()

if(SLANG_FORMAT_BUILD_CLI OR SLANG_FORMAT_INCLUDE_TESTS)
  add_executable(slang_format src/formatter_main.cpp)
  target_include_directories(slang_format PRIVATE external)
  target_link_libraries(slang_format PRIVATE slang_format_lib)
  set_target_properties(slang_format PROPERTIES OUTPUT_NAME "slang-format")

  if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_sources(slang_format PRIVATE scripts/win32.manifest)
  endif()
endif()

if(PROJECT_IS_TOP_LEVEL)
  add_executable(gen_format_config_schema src/gen_config_main.cpp)
  target_include_directories(gen_format_config_schema PRIVATE include)
  target_link_libraries(gen_format_config_schema PRIVATE reflectcpp::reflectcpp)
endif()

if(SLANG_FORMAT_INCLUDE_INSTALL)
  if(NOT TARGET slang_format)
    message(
      FATAL_ERROR "SLANG_FORMAT_INCLUDE_INSTALL requires SLANG_FORMAT_BUILD_CLI"
    )
  endif()
  install(TARGETS slang_format RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(SLANG_FORMAT_INCLUDE_TESTS)
  include(CTest)
  add_subdirectory(tests/cpp)
endif()
