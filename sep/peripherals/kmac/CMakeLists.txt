# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
cmake_minimum_required(VERSION 3.14)

project(kmac LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")

include(PeripheralCommon)

# Doxygen documentation
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
        add_custom_target(kmac_docs
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
# Definitions
# -----------------------------------------------------------------------------
add_compile_definitions(SC_ALLOW_DEPRECATED_IEEE_API)

# -----------------------------------------------------------------------------
# Build type & output dirs
# -----------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
message(STATUS "kmac Build Type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# -----------------------------------------------------------------------------
# Compiler settings
# -----------------------------------------------------------------------------
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add warning suppression
if(NOT MSVC)
    add_compile_options(-Wno-unused-parameter)
endif()

set(WFLAG "-Wall -Wextra")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g ${WFLAG}")
  add_compile_definitions(DEBUG REG_DEFAULT_VERBOSITY=3)
  message(STATUS "kmac: Debug build - full logging enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 ${WFLAG}")
  add_compile_definitions(NDEBUG REG_DEFAULT_VERBOSITY=3)
  message(STATUS "kmac: Release build - errors only, optimized")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g -O0 -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  add_compile_definitions(REG_DEFAULT_VERBOSITY=2)
  message(STATUS "kmac: ASAN build - AddressSanitizer enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g --coverage -fprofile-arcs -ftest-coverage")
  include(PeripheralCoverage)
  peripheral_coverage_link_flags()
  # Verbosity 3, not 1. REG_LOG expands to "if (level > max) {} else regLog(...)",
  # so a suppressed multi-line log statement leaves every continuation line of its
  # << chain recorded as unexecuted. At verbosity 1 that reported 192 phantom
  # uncovered lines in kmac.cpp and understated line coverage by roughly ten points,
  # measuring log formatting rather than logic.
  add_compile_definitions(REG_DEFAULT_VERBOSITY=3 __COVERAGE__)
  message(STATUS "kmac: Coverage build enabled")
endif()


# Find threading library (required by SystemC)
find_package(Threads REQUIRED)

# Find OpenSSL library (required for cryptographic operations)
include(PeripheralOpenSSL)
peripheral_find_openssl()

# -----------------------------------------------------------------------------
# External Dependencies (CCI)
# -----------------------------------------------------------------------------
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
)
# NOTE: test/inc is deliberately NOT added here — it's added per-target below,
# only for kmac_testbench. Adding it globally would leak test-only
# headers into kmac_model's (production) include path.



# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
file(GLOB_RECURSE MODEL_SRCS "src/*.cpp")
file(GLOB_RECURSE TEST_SRCS  "test/src/*.cpp")

# -----------------------------------------------------------------------------
# Link Directories (must be set before targets)
# -----------------------------------------------------------------------------



# -----------------------------------------------------------------------------
# Context-aware BUILD_TESTS option
# -----------------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(BUILD_TESTS "Build KMAC test executables" ON)
else()
    # Built as subdirectory (VP integration) - skip tests by default
    if(NOT DEFINED BUILD_TESTS)
        option(BUILD_TESTS "Build KMAC test executables" OFF)
    endif()
endif()
# -----------------------------------------------------------------------------
# Targets
# -----------------------------------------------------------------------------
# Model library (always built)
peripheral_add_model_library(kmac_model ${MODEL_SRCS})
target_include_directories(kmac_model PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
)
target_link_libraries(kmac_model PUBLIC SystemC::systemc Threads::Threads OpenSSL::Crypto)
if(CCI_FOUND)
  target_link_libraries(kmac_model PUBLIC ${CCI_LIBRARIES})
endif()

# Testbench executable (only if BUILD_TESTS=ON)
if(BUILD_TESTS)
    add_executable(kmac_testbench ${TEST_SRCS})
    peripheral_link_test_to_model(kmac_testbench kmac_model)
    target_include_directories(kmac_testbench PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/test/inc
      ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
    )
    target_link_libraries(kmac_testbench PRIVATE SystemC::systemc OpenSSL::Crypto)
    if(CCI_FOUND)
      target_link_libraries(kmac_testbench PRIVATE ${CCI_LIBRARIES})
    endif()

  # Set RPATH so the testbench binary finds SystemC and CCI at runtime.
  set(_rpath_dirs "$<TARGET_FILE_DIR:SystemC::systemc>")
  if(CCI_FOUND AND CCI_CONFIG_LIBRARY)
    get_filename_component(_cci_lib_dir "${CCI_CONFIG_LIBRARY}" DIRECTORY)
    list(APPEND _rpath_dirs "${_cci_lib_dir}")
  endif()
  set_target_properties(kmac_testbench PROPERTIES
      INSTALL_RPATH "${_rpath_dirs}"
      BUILD_WITH_INSTALL_RPATH TRUE
  )


  # -----------------------------------------------------------------------------
  # Coverage target
  # -----------------------------------------------------------------------------
  if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    peripheral_add_coverage_target(
      PRIMARY_TARGET kmac_testbench
      TEST_COMMANDS
        # Absolute path: the relative form resolved against the build directory,
        # so the config never loaded and could not override the build verbosity.
        COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/kmac_testbench
                ${CMAKE_CURRENT_SOURCE_DIR}/config/accellera_config.ini
        COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/kmac_testbench
                ${CMAKE_CURRENT_SOURCE_DIR}/config/accellera_config.ini --unmasked
    )
  endif()

  enable_testing()
  # Both builds of the DUT are registered: EnMasking is a const constructor
  # argument, so the unmasked sideload path needs its own process and would
  # otherwise be skipped by every ctest-driven phase.
  add_test(NAME kmac_testbench
           COMMAND kmac_testbench
                   ${CMAKE_CURRENT_SOURCE_DIR}/config/accellera_config.ini)
  add_test(NAME kmac_testbench_unmasked
           COMMAND kmac_testbench
                   ${CMAKE_CURRENT_SOURCE_DIR}/config/accellera_config.ini --unmasked)
endif()

# -----------------------------------------------------------------------------
# Print status information
# -----------------------------------------------------------------------------
message(STATUS "Generated Project: kmac")
  message(STATUS "  - SYSTEMC_HOME: ${SYSTEMC_HOME}")
# -----------------------------------------------------------------------------
# Static Analysis Target (cppcheck)
# -----------------------------------------------------------------------------
find_program(CPPCHECK_EXECUTABLE cppcheck)
if(CPPCHECK_EXECUTABLE)
  add_custom_target(kmac_cppcheck
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
      -I $ENV{SYSTEMC_HOME}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
      2> ${CMAKE_BINARY_DIR}/cppcheck_report.txt
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    COMMAND ${CMAKE_COMMAND} -E echo "cppcheck report - MODEL ONLY:"
    COMMAND ${CMAKE_COMMAND} -E echo "  ${CMAKE_BINARY_DIR}/cppcheck_report.txt"
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running cppcheck static analysis on KMAC model..."
  )
  message(STATUS "cppcheck found - 'make kmac_cppcheck' target available")
else()
  message(STATUS "cppcheck not found - static analysis target not available")
endif()
