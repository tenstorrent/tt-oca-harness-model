# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
cmake_minimum_required(VERSION 3.14)

project(sep_output_remap_ctrl LANGUAGES CXX)

# -----------------------------------------------------------------------------
# CMake Module Path
# -----------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")

include(PeripheralCommon)

# -----------------------------------------------------------------------------
# Doxygen documentation
# -----------------------------------------------------------------------------
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOC_INPUT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    set(DOC_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs)
    if(DOXYGEN_DOT_FOUND)
        set(DOXY_HAVE_DOT "YES")
    else()
        set(DOXY_HAVE_DOT "NO")
    endif()
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
            ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            @ONLY
        )
        add_custom_target(sep_output_remap_ctrl_docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
    endif()
else()
    message(WARNING "Doxygen not found - documentation target will not be available")
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  if(NOT DEFINED BUILD_TESTS)
    option(BUILD_TESTS "Build sep_output_remap_ctrl test executables" ON)
  endif()
endif()

# -----------------------------------------------------------------------------
# Definitions
# -----------------------------------------------------------------------------
add_compile_definitions(SC_ALLOW_DEPRECATED_IEEE_API)

# -----------------------------------------------------------------------------
# Build type & output dirs
# -----------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
message(STATUS "sep_output_remap_ctrl Build Type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# -----------------------------------------------------------------------------
# Compiler settings
# -----------------------------------------------------------------------------
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
  add_compile_definitions(DEBUG REG_DEFAULT_VERBOSITY=3)
  message(STATUS "sep_output_remap_ctrl: Debug build - full logging enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 ${WFLAG}")
  add_compile_definitions(NDEBUG REG_DEFAULT_VERBOSITY=0)
  message(STATUS "sep_output_remap_ctrl: Release build - errors only, optimized")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g -O0 -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  add_compile_definitions(REG_DEFAULT_VERBOSITY=2)
  message(STATUS "sep_output_remap_ctrl: ASAN build - AddressSanitizer enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g --coverage -fprofile-arcs -ftest-coverage")
  include(PeripheralCoverage)
  peripheral_coverage_link_flags()
  add_compile_definitions(REG_DEFAULT_VERBOSITY=1 __COVERAGE__)
  message(STATUS "sep_output_remap_ctrl: Coverage build enabled")
endif()

# -----------------------------------------------------------------------------
# External Dependencies
# -----------------------------------------------------------------------------
find_package(Threads REQUIRED)
include(FindCCI)
include(FindSystemC)

if(CCI_FOUND)
  include_directories(SYSTEM ${CCI_INCLUDE_DIR})
endif()

# -----------------------------------------------------------------------------
# Paths & Includes
# -----------------------------------------------------------------------------
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/tlm_extensions
)
# NOTE: test/inc is deliberately NOT added here — it's added per-target below,
# only for sep_output_remap_ctrl_testbench. Adding it globally would leak
# test-only headers into sep_output_remap_ctrl_model's (production) include path.

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
set(MODEL_SRCS
  src/sep_output_remap_ctrl_base.cpp
  src/sep_output_remap_ctrl.cpp
)

file(GLOB TEST_SRCS "test/src/*.cpp")

# -----------------------------------------------------------------------------
# Targets
# -----------------------------------------------------------------------------
peripheral_add_model_library(sep_output_remap_ctrl_model ${MODEL_SRCS})
target_include_directories(sep_output_remap_ctrl_model PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
)
target_link_libraries(sep_output_remap_ctrl_model PUBLIC SystemC::systemc Threads::Threads)
if(CCI_FOUND)
  target_link_libraries(sep_output_remap_ctrl_model PUBLIC ${CCI_LIBRARIES})
endif()

if(BUILD_TESTS)
  add_executable(sep_output_remap_ctrl_testbench ${TEST_SRCS})
  peripheral_link_test_to_model(sep_output_remap_ctrl_testbench sep_output_remap_ctrl_model)
  target_include_directories(sep_output_remap_ctrl_testbench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/test/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/tlm_extensions
  )
  target_link_libraries(sep_output_remap_ctrl_testbench PRIVATE SystemC::systemc)
  if(CCI_FOUND)
    target_link_libraries(sep_output_remap_ctrl_testbench PRIVATE ${CCI_LIBRARIES})
  endif()

  # The suite checks with assert(), which the Release NDEBUG above would compile
  # out — leaving a testbench that passes without verifying anything. Release is
  # what ./run_tests.sh builds by default, so keep asserts live for this target.
  target_compile_options(sep_output_remap_ctrl_testbench PRIVATE -UNDEBUG)

  enable_testing()
  add_test(NAME sep_output_remap_ctrl_testbench COMMAND sep_output_remap_ctrl_testbench)
endif()

# -----------------------------------------------------------------------------
# Print status information
# -----------------------------------------------------------------------------
message(STATUS "Generated Project: sep_output_remap_ctrl")
message(STATUS "  - SYSTEMC_HOME: ${SYSTEMC_HOME}")

# -----------------------------------------------------------------------------
# Coverage Target
# -----------------------------------------------------------------------------
if(BUILD_TESTS AND CMAKE_BUILD_TYPE STREQUAL "Coverage")
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${CMAKE_BINARY_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/PeripheralCoverageClean.cmake
        COMMAND ${LCOV_EXECUTABLE} ${LCOV_IGNORE_FLAGS} --zerocounters --directory ${CMAKE_BINARY_DIR}
        COMMAND ${CMAKE_BINARY_DIR}/bin/sep_output_remap_ctrl_testbench
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
        COMMAND ${CMAKE_COMMAND} -E echo "Coverage report at coverage/html/index.html"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating coverage report for sep_output_remap_ctrl..."
    )
    add_dependencies(coverage sep_output_remap_ctrl_testbench)
endif()

# -----------------------------------------------------------------------------
# Static Analysis Target (cppcheck)
# -----------------------------------------------------------------------------
find_program(CPPCHECK_EXECUTABLE cppcheck)
if(CPPCHECK_EXECUTABLE)
  add_custom_target(sep_output_remap_ctrl_cppcheck
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/cppcheck_results
    COMMAND ${CPPCHECK_EXECUTABLE}
      --enable=all
      --suppress=missingIncludeSystem
      --suppress=unmatchedSuppression
      --inline-suppr
      --inconclusive
      --force
      --template=gcc
      --cppcheck-build-dir=${CMAKE_BINARY_DIR}/cppcheck_results
      -I ${CMAKE_CURRENT_SOURCE_DIR}/include
      -I ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
      -I ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/tlm_extensions
      -I $ENV{SYSTEMC_HOME}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
      2> ${CMAKE_BINARY_DIR}/cppcheck_report.txt
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running cppcheck on sep_output_remap_ctrl model..."
  )
  message(STATUS "cppcheck found - 'make sep_output_remap_ctrl_cppcheck' target available")
else()
  message(STATUS "cppcheck not found - static analysis target not available")
endif()
