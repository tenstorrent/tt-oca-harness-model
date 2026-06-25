cmake_minimum_required(VERSION 3.14)

project(spi_flash LANGUAGES CXX)

# -----------------------------------------------------------------------------
# CMake Module Path - Add shared cmake modules from repo root
# -----------------------------------------------------------------------------
# Support both standalone and integrated builds
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")

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
        add_custom_target(spi_flash_docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
    endif()
else()
    message(WARNING "Doxygen not found - documentation target will not be available")
endif()

# SC_ALLOW_DEPRECATED_IEEE_API suppresses SystemC 3.x compatibility warnings
add_compile_definitions(SC_ALLOW_DEPRECATED_IEEE_API)

# The SPI flash is not memory-mapped to the CPU bus — it has no CSML registers
# and no CCI configuration parameters.

# -----------------------------------------------------------------------------
# Build type & output dirs
# -----------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
message(STATUS "SPI Flash Build Type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# -----------------------------------------------------------------------------
# Compiler settings
# -----------------------------------------------------------------------------
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/wd4100)  # Unreferenced formal parameter
else()
    add_compile_options(-Wno-unused-parameter)
endif()

set(WFLAG "-Wall -Wextra")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g ${WFLAG}")
  add_compile_definitions(DEBUG CSML_DEFAULT_VERBOSITY=3)
  message(STATUS "SPI Flash: Debug build")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 ${WFLAG}")
  add_compile_definitions(NDEBUG CSML_DEFAULT_VERBOSITY=0)
  message(STATUS "SPI Flash: Release build - optimized")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g -O0 -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=2)
  message(STATUS "SPI Flash: ASAN build - AddressSanitizer enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g --coverage -fprofile-arcs -ftest-coverage")
  include(PeripheralCoverage)
  peripheral_coverage_link_flags()
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=1 __COVERAGE__)
  message(STATUS "SPI Flash: Coverage build enabled")
endif()

# -----------------------------------------------------------------------------
# Paths & Includes
# -----------------------------------------------------------------------------
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/test/inc
  # spi_controller_interface.h needed by the SystemC wrapper (spi_flash.h)
  ${CMAKE_CURRENT_SOURCE_DIR}/../spi_controller/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
)

# -----------------------------------------------------------------------------
# External Dependencies (SystemC, CCI, CSML)
# -----------------------------------------------------------------------------
include(FindCCI)
include(FindSystemC)
if(CCI_FOUND)
  include_directories(${CCI_INCLUDE_DIR})
endif()

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
set(MODEL_CORE_SRCS
  # Pure-C++ flash behaviour (no SystemC dependency)
  src/spi_flash_model.cpp
)

set(MODEL_SC_SRCS
  # SystemC wrapper implementing spi_if (depends on SystemC)
  src/spi_flash.cpp
)

# Test utility (parser + pretty-printer for SFDP bytes)
# Compiled into the test executable; not part of the model library.
set(TEST_UTIL_SRCS
  test/src/spi_flash_sfdp_utils.cpp
)

# Unit-test runner sources — added in Step 6.
# Listed here; each file is guarded with EXISTS so missing files don't break
# the build before Step 6 is complete.
set(TEST_SRCS
  # test/src/spi_flash_test.cpp        (Step 6 - pure-C++ unit tests)
  # test/src/spi_flash_sc_test.cpp     (Step 7 - SystemC integration test)
)

# -----------------------------------------------------------------------------
# Context-aware BUILD_TESTS option
# Default ON for standalone builds, OFF when included as subproject
# -----------------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(BUILD_TESTS "Build SPI Flash test executables" ON)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml ${CMAKE_CURRENT_BINARY_DIR}/csml)
else()
    option(BUILD_TESTS "Build SPI Flash test executables" OFF)
endif()

# -----------------------------------------------------------------------------
# Model libraries (always built)
# Two-target design:
#   spi_flash_core  — pure C++ flash behaviour, no SystemC dependency
#   spi_flash_model — SystemC TLM wrapper; links spi_flash_core + SystemC/csml
# spi_flash_test  links spi_flash_core only (no sc_main required)
# spi_flash_sc_test links spi_flash_model (sc_main provided)
# -----------------------------------------------------------------------------
add_library(spi_flash_core STATIC ${MODEL_CORE_SRCS})
target_include_directories(spi_flash_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(spi_flash_model STATIC ${MODEL_SC_SRCS})
target_include_directories(spi_flash_model PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../spi_controller/include
)
target_link_libraries(spi_flash_model
  PUBLIC  spi_flash_core
  PRIVATE csml_logger SystemC::systemc Threads::Threads ${CCI_LIBRARIES})

# -----------------------------------------------------------------------------
# Test executable (only if BUILD_TESTS=ON and runner source exists)
# -----------------------------------------------------------------------------
if(BUILD_TESTS)
  set(TEST_RUNNER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/test/src/spi_flash_test.cpp")

  if(EXISTS ${TEST_RUNNER_SRC})
    add_executable(spi_flash_test
      ${TEST_RUNNER_SRC}
      ${TEST_UTIL_SRCS}
      ${TEST_SRCS}
    )
    target_include_directories(spi_flash_test PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/test/inc
      ${CMAKE_CURRENT_SOURCE_DIR}/../spi_controller/include
    )
    target_link_libraries(spi_flash_test PRIVATE spi_flash_core)

    # -------------------------------------------------------------------------
    # CTest
    # -------------------------------------------------------------------------
    enable_testing()
    add_test(NAME spi_flash_test COMMAND spi_flash_test)
  else()
    message(STATUS "SPI Flash: test runner not yet created (Step 6) - skipping test executable")
  endif()

  # ---------------------------------------------------------------------------
  # SC wrapper test (Step 7) — sc_main testbench via spi_master_stub
  # ---------------------------------------------------------------------------
  set(SC_TEST_SRC "${CMAKE_CURRENT_SOURCE_DIR}/test/src/spi_flash_sc_test.cpp")

  if(EXISTS ${SC_TEST_SRC})
    add_executable(spi_flash_sc_test
      ${SC_TEST_SRC}
      ${TEST_UTIL_SRCS}
    )
    target_include_directories(spi_flash_sc_test PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/test/inc
      ${CMAKE_CURRENT_SOURCE_DIR}/../spi_controller/include
    )
    # spi_flash_model propagates spi_flash_core (PUBLIC) but keeps csml/SystemC PRIVATE.
    # Explicitly link csml_logger and SystemC for the static lib symbol resolution + sc_main.
    target_link_libraries(spi_flash_sc_test PRIVATE spi_flash_model csml_logger SystemC::systemc Threads::Threads)

    enable_testing()
    add_test(NAME spi_flash_sc_test COMMAND spi_flash_sc_test)
  else()
    message(STATUS "SPI Flash: SC test not yet created (Step 7) - skipping")
  endif()

  # ---------------------------------------------------------------------------
  # Coverage target (runs both tests when coverage build)
  # ---------------------------------------------------------------------------
  if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    set(COVERAGE_CMDS
      COMMAND ./bin/spi_flash_test
    )
    if(EXISTS ${SC_TEST_SRC})
      list(APPEND COVERAGE_CMDS COMMAND ./bin/spi_flash_sc_test)
    endif()

    add_custom_target(coverage
      ${COVERAGE_CMDS}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
      COMMAND lcov --ignore-errors inconsistent,unsupported,format,mismatch --capture --directory . --output-file coverage/coverage.info
      COMMAND lcov --ignore-errors inconsistent,unsupported,format,mismatch --extract coverage/coverage.info '${CMAKE_CURRENT_SOURCE_DIR}/src/*' '${CMAKE_CURRENT_SOURCE_DIR}/include/*' --output-file=coverage/coverage_filtered.info
      COMMAND genhtml --ignore-errors inconsistent,unsupported,format,corrupt,category coverage/coverage_filtered.info --output-directory coverage/html
      COMMAND ${CMAKE_COMMAND} -E echo "Coverage report at coverage/html/index.html"
    )
    if(TARGET spi_flash_test)
      add_dependencies(coverage spi_flash_test)
    endif()
    if(TARGET spi_flash_sc_test)
      add_dependencies(coverage spi_flash_sc_test)
    endif()
  endif()

endif()

# -----------------------------------------------------------------------------
# Static Analysis Target (cppcheck)
# -----------------------------------------------------------------------------
find_program(CPPCHECK_EXECUTABLE cppcheck)
if(CPPCHECK_EXECUTABLE)
  add_custom_target(spi_flash_cppcheck
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
      -I ${CMAKE_CURRENT_SOURCE_DIR}/../spi_controller/include
      -I $ENV{SYSTEMC_HOME}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
      2> ${CMAKE_BINARY_DIR}/cppcheck_report.txt
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    COMMAND ${CMAKE_COMMAND} -E echo "cppcheck report - MODEL ONLY:"
    COMMAND ${CMAKE_COMMAND} -E echo "  ${CMAKE_BINARY_DIR}/cppcheck_report.txt"
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running cppcheck static analysis on SPI Flash model..."
  )
  message(STATUS "cppcheck found - 'make spi_flash_cppcheck' target available")
else()
  message(STATUS "cppcheck not found - static analysis target not available")
endif()
