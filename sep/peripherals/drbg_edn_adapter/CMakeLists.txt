# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
cmake_minimum_required(VERSION 3.14)

project(drbg_edn_adapter LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")
include(PeripheralCommon)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  if(NOT DEFINED BUILD_TESTS)
    option(BUILD_TESTS "Build drbg_edn_adapter test executables" ON)
  endif()
endif()

add_compile_definitions(SC_ALLOW_DEPRECATED_IEEE_API)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT MSVC)
  add_compile_options(-Wno-unused-parameter)
endif()

set(WFLAG "-Wall -Wextra")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g ${WFLAG}")
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 ${WFLAG}")
  add_compile_definitions(NDEBUG)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g -O0 -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g --coverage -fprofile-arcs -ftest-coverage")
  include(PeripheralCoverage)
  peripheral_coverage_link_flags()
  add_compile_definitions(__COVERAGE__)
endif()

find_package(Threads REQUIRED)
include(FindCCI)
include(FindSystemC)
if(CCI_FOUND)
  include_directories(SYSTEM ${CCI_INCLUDE_DIR})
endif()

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
)

set(MODEL_SRCS src/drbg_edn_adapter.cpp)

peripheral_add_model_library(drbg_edn_adapter_model ${MODEL_SRCS})
target_include_directories(drbg_edn_adapter_model PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
)
target_link_libraries(drbg_edn_adapter_model PUBLIC SystemC::systemc Threads::Threads)
if(CCI_FOUND)
  target_link_libraries(drbg_edn_adapter_model PUBLIC ${CCI_LIBRARIES})
endif()

if(BUILD_TESTS)
  enable_testing()
  add_executable(drbg_edn_adapter_testbench test/drbg_edn_adapter_tb.cpp)
  peripheral_link_test_to_model(drbg_edn_adapter_testbench drbg_edn_adapter_model)
  target_include_directories(drbg_edn_adapter_testbench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
  )
  target_link_libraries(drbg_edn_adapter_testbench PRIVATE SystemC::systemc)
  if(CCI_FOUND)
    target_link_libraries(drbg_edn_adapter_testbench PRIVATE ${CCI_LIBRARIES})
  endif()
  target_compile_options(drbg_edn_adapter_testbench PRIVATE -UNDEBUG)
  add_test(
    NAME drbg_edn_adapter_test
    COMMAND drbg_edn_adapter_testbench
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
  foreach(n 0 33)
    add_test(NAME drbg_edn_adapter_bad_endpoints_${n}
             COMMAND drbg_edn_adapter_testbench --bad-endpoints ${n})
    set_tests_properties(drbg_edn_adapter_bad_endpoints_${n} PROPERTIES
      PASS_REGULAR_EXPRESSION "PASS: num_endpoints=${n} fatal: .*num_endpoints must be in 1..32")
  endforeach()
endif()

if(BUILD_TESTS AND CMAKE_BUILD_TYPE STREQUAL "Coverage")
  add_custom_target(coverage
    COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${CMAKE_BINARY_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/PeripheralCoverageClean.cmake
    COMMAND ${LCOV_EXECUTABLE} ${LCOV_IGNORE_FLAGS} --zerocounters --directory ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_BINARY_DIR}/bin/drbg_edn_adapter_testbench
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
    COMMAND ${LCOV_EXECUTABLE} ${LCOV_IGNORE_FLAGS}
            --capture --directory ${CMAKE_BINARY_DIR}
            --output-file ${CMAKE_BINARY_DIR}/coverage/coverage.info
    COMMAND ${LCOV_EXECUTABLE} ${LCOV_IGNORE_FLAGS}
            --extract ${CMAKE_BINARY_DIR}/coverage/coverage.info
            "${CMAKE_CURRENT_SOURCE_DIR}/src/*"
            "${CMAKE_CURRENT_SOURCE_DIR}/include/*"
            --output-file ${CMAKE_BINARY_DIR}/coverage/coverage_filtered.info
    COMMAND ${LCOV_EXECUTABLE} ${LCOV_IGNORE_FLAGS}
            --remove ${CMAKE_BINARY_DIR}/coverage/coverage_filtered.info
            '/opt/*' '/usr/*'
            --output-file ${CMAKE_BINARY_DIR}/coverage/coverage_filtered.info
    COMMAND ${GENHTML_EXECUTABLE} ${GENHTML_IGNORE_FLAGS}
            ${CMAKE_BINARY_DIR}/coverage/coverage_filtered.info
            --output-directory ${CMAKE_BINARY_DIR}/coverage/html
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating coverage report for drbg_edn_adapter..."
  )
  add_dependencies(coverage drbg_edn_adapter_testbench)
endif()
