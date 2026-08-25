# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
cmake_minimum_required(VERSION 3.14)

project(otbn LANGUAGES CXX)

# -----------------------------------------------------------------------------
# CMake Module Path - Add shared cmake modules from repo root
# -----------------------------------------------------------------------------
# Support both standalone and integrated builds
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")

include(PeripheralCommon)


# Doxygen documentation
find_package(Doxygen)
if(DOXYGEN_FOUND)
    # Set variables expected by Doxyfile.in template
    # Use space-separated format for Doxygen INPUT
    set(DOC_INPUT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/README.md")

    set(DOC_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs)

    # Check for Graphviz/dot for diagram generation
    if(DOXYGEN_DOT_FOUND)
        set(DOXY_HAVE_DOT "YES")
    else()
        set(DOXY_HAVE_DOT "NO")
    endif()

    # Configure the Doxyfile (if Doxyfile.in exists)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
            ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            @ONLY
        )

        # Add custom target for documentation
        add_custom_target(otbn_docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
    endif()
else()
    message(WARNING "Doxygen not found - documentation target will not be available")
endif()

# -----------------------------------------------------------------------------
# Build type & output dirs
# -----------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
message(STATUS "OTBN Build Type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# -----------------------------------------------------------------------------
# Compiler settings
# -----------------------------------------------------------------------------
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add warning suppression and SystemC deprecation flag
if(MSVC)
    add_compile_options(/wd4100)  # Unreferenced formal parameter
else()
    add_compile_options(-Wno-unused-parameter -Wno-sign-compare)
endif()

# SystemC allows deprecated IEEE API
add_compile_definitions(SC_ALLOW_DEPRECATED_IEEE_API)

set(WFLAG "-Wall -Wextra")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g ${WFLAG}")
  add_compile_definitions(DEBUG CSML_DEFAULT_VERBOSITY=3)
  message(STATUS "OTBN: Debug build - full logging enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  # -Wno-overloaded-virtual: Clang's -Wall (unlike GCC's) enables
  # -Woverloaded-virtual, which fires on CCI's cci_param_typed<T>
  # register_*_callback() overload set (typed overloads intentionally
  # coexist with the untyped virtuals from cci_param_untyped) - upstream
  # CCI API design, not an application bug.
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 ${WFLAG} -Werror -Wno-unused-function -Wno-overloaded-virtual")
  add_compile_definitions(NDEBUG CSML_DEFAULT_VERBOSITY=3)
  message(STATUS "OTBN: Release build - errors only, optimized")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g -O0 -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=2)
  message(STATUS "OTBN: ASAN build - AddressSanitizer enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g --coverage -fprofile-arcs -ftest-coverage")
  include(PeripheralCoverage)
  peripheral_coverage_link_flags()
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=1 __COVERAGE__)
  message(STATUS "OTBN: Coverage build enabled")
endif()

# -----------------------------------------------------------------------------
# Paths & Includes
# -----------------------------------------------------------------------------
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/algo
  ${CMAKE_CURRENT_SOURCE_DIR}/test/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
)

# -----------------------------------------------------------------------------
# External Dependencies (SystemC and CCI)
# -----------------------------------------------------------------------------
include(FindCCI)
include(FindSystemC)

# Add CCI include directory if found
if(CCI_FOUND)
  include_directories(SYSTEM ${CCI_INCLUDE_DIR})
endif()

# -----------------------------------------------------------------------------
# OpenSSL Library (Required for RSA-2048 cryptographic operations)
# -----------------------------------------------------------------------------
include(PeripheralOpenSSL)
peripheral_find_openssl()

if(OpenSSL_FOUND)
  message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")
  message(STATUS "  Include: ${OPENSSL_INCLUDE_DIR}")
  message(STATUS "  Libraries: ${OPENSSL_LIBRARIES}")
else()
  message(FATAL_ERROR "OpenSSL not found. Install libssl-dev (Ubuntu/Debian) or openssl-devel (RedHat/CentOS)")
endif()

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
set(MODEL_SRCS
  src/otbn.cpp
  src/otbn_base.cpp
  algo/otbn_algorithm_rsa_2048.cpp
  algo/otbn_algorithm_rsa_2048_key_enabled.cpp
  algo/otbn_algorithm_summation.cpp
  algo/otbn_algorithm_otbn_loop.cpp
  algo/otbn_algorithm_rnd_test.cpp
  algo/otbn_algorithm_p256_ecdsa.cpp
  algo/otbn_algorithm_smoke.cpp
  algo/otbn_algorithm_rsa_3072.cpp
  algo/otbn_algorithm_callback_cov.cpp
)

set(TEST_SRCS
  test/src/testbench.cpp
  test/src/test_coverage.cpp
  test/src/otbn_test.cpp
  test/src/otbn_basetest.cpp
)

# -----------------------------------------------------------------------------
# Libraries and Executables
# -----------------------------------------------------------------------------
# Context-aware BUILD_TESTS option
# Default ON for standalone builds, OFF when included as subproject (VP integration)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if(NOT DEFINED BUILD_TESTS)
      option(BUILD_TESTS "Build OTBN test executables" ON)
    endif()
    # For standalone builds, we need to build csml_logger ourselves
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml ${CMAKE_CURRENT_BINARY_DIR}/csml)
endif()

# OTBN model library (always built)
# Use STATIC library to match HMAC configuration
peripheral_add_model_library(otbn_model ${MODEL_SRCS})
target_include_directories(otbn_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(otbn_model PUBLIC
  csml_logger
  SystemC::systemc
  Threads::Threads
  OpenSSL::SSL
  OpenSSL::Crypto
  ${CCI_LIBRARIES}
)

# OTBN test executable (only if BUILD_TESTS=ON)
if(BUILD_TESTS)
  add_executable(otbn_test ${TEST_SRCS})
  peripheral_link_test_to_model(otbn_test otbn_model)
  # -----------------------------------------------------------------------------
  # Coverage target
  # -----------------------------------------------------------------------------
  if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    add_custom_target(coverage
      COMMAND ${LCOV_EXECUTABLE} ${LCOV_IGNORE_FLAGS} --zerocounters --directory ${CMAKE_BINARY_DIR}
      COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${CMAKE_BINARY_DIR}
              -P ${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/PeripheralCoverageClean.cmake
      COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_tests.sh --coverage --no-build
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND ${CMAKE_COMMAND} -E echo "Coverage report at build/coverage/coverage/html/index.html"
    )
    add_dependencies(coverage otbn_test)
  endif()

  # -----------------------------------------------------------------------------
  # CTest
  # -----------------------------------------------------------------------------
  enable_testing()
  add_test(NAME otbn_test COMMAND otbn_test)
endif()

# -----------------------------------------------------------------------------
# Static Analysis Target (cppcheck)
# -----------------------------------------------------------------------------
find_program(CPPCHECK_EXECUTABLE cppcheck)
if(CPPCHECK_EXECUTABLE)
  add_custom_target(otbn_cppcheck
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
      -I ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
      -I $ENV{SYSTEMC_HOME}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
      2> ${CMAKE_BINARY_DIR}/cppcheck_report.txt
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    COMMAND ${CMAKE_COMMAND} -E echo "cppcheck report - MODEL ONLY:"
    COMMAND ${CMAKE_COMMAND} -E echo "  ${CMAKE_BINARY_DIR}/cppcheck_report.txt"
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running cppcheck static analysis on OTBN model..."
  )
  message(STATUS "cppcheck found - 'make otbn_cppcheck' target available")
else()
  message(STATUS "cppcheck not found - static analysis target not available")
endif()
