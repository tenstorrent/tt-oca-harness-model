# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
cmake_minimum_required(VERSION 3.14)
project(sep_entropy_pool LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")
include(PeripheralCommon)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND NOT DEFINED BUILD_TESTS)
  option(BUILD_TESTS "Build SEP entropy pool tests" ON)
endif()

add_compile_definitions(SC_ALLOW_DEPRECATED_IEEE_API)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_compile_options(-O2 -Wall -Wextra)
elseif(CMAKE_BUILD_TYPE STREQUAL "ASAN")
  add_compile_options(-g -O0 -Wall -Wextra -fsanitize=address,undefined
                      -fno-sanitize-recover=all -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
elseif(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  add_compile_options(-g -O0 -Wall -Wextra --coverage)
  include(PeripheralCoverage)
  peripheral_coverage_link_flags()
endif()

find_package(Threads REQUIRED)
include(FindCCI)
include(FindSystemC)

peripheral_add_model_library(sep_entropy_pool_model src/sep_entropy_pool.cpp)
target_include_directories(sep_entropy_pool_model PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../edn/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
)
target_link_libraries(sep_entropy_pool_model PUBLIC SystemC::systemc Threads::Threads)
if(CCI_FOUND)
  target_include_directories(sep_entropy_pool_model SYSTEM PUBLIC ${CCI_INCLUDE_DIR})
  target_link_libraries(sep_entropy_pool_model PUBLIC ${CCI_LIBRARIES})
endif()

if(BUILD_TESTS)
  add_executable(sep_entropy_pool_testbench test/sep_entropy_pool_tb.cpp)
  peripheral_link_test_to_model(sep_entropy_pool_testbench sep_entropy_pool_model)
  if(CCI_FOUND)
    target_link_libraries(sep_entropy_pool_testbench PRIVATE ${CCI_LIBRARIES})
  endif()
  enable_testing()
  add_test(NAME sep_entropy_pool_testbench COMMAND sep_entropy_pool_testbench)

  if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    peripheral_add_coverage_target(PRIMARY_TARGET sep_entropy_pool_testbench)
  endif()
endif()
