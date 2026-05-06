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
    set(DOC_INPUT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/model/inc ${CMAKE_CURRENT_SOURCE_DIR}/README.md")
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

# NOTE: No USE_CSML_REG_LIB / USE_CCI_CONFIG here.
# The SPI flash is not memory-mapped to the CPU bus — it has no CSML registers
# and no CCI configuration parameters.

# -----------------------------------------------------------------------------
# Build type & output dirs
# -----------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
message(STATUS "SPI Flash Build Type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG    "${CMAKE_BINARY_DIR}/debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE  "${CMAKE_BINARY_DIR}/release")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_ASAN     "${CMAKE_BINARY_DIR}/asan")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_COVERAGE "${CMAKE_BINARY_DIR}/coverage")

# -----------------------------------------------------------------------------
# Compiler settings
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
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
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage -lgcov")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=1 __COVERAGE__)
  message(STATUS "SPI Flash: Coverage build enabled")
endif()

# -----------------------------------------------------------------------------
# Paths & Includes
# -----------------------------------------------------------------------------
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/model/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/test/inc
  # spi_controller_interface.h needed by the SystemC wrapper (spi_flash.h)
  ${CMAKE_CURRENT_SOURCE_DIR}/../spi_controller/model/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
)

# -----------------------------------------------------------------------------
# External Dependencies (SystemC, CCI, CSML)
# -----------------------------------------------------------------------------
include(FindSystemC)
include(FindCCI)
if(CCI_FOUND)
  include_directories(${CCI_INCLUDE_DIR})
endif()

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
set(MODEL_SRCS
  # Pure-C++ flash behaviour (no SystemC dependency)
  model/src/spi_flash_model.cpp
  # SystemC wrapper implementing spi_if (depends on SystemC)
  model/src/spi_flash.cpp
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
# Model library (always built)
# -----------------------------------------------------------------------------
add_library(spi_flash_model STATIC ${MODEL_SRCS})
target_include_directories(spi_flash_model PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/model/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/../spi_controller/model/inc
)
# Link SystemC (needed by the spi_flash.cpp wrapper)
target_link_libraries(spi_flash_model PUBLIC csml_logger SystemC::systemc Threads::Threads ${CCI_LIBRARIES})

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
      ${CMAKE_CURRENT_SOURCE_DIR}/../spi_controller/model/inc
    )
    target_link_libraries(spi_flash_test PRIVATE spi_flash_model)

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
      ${CMAKE_CURRENT_SOURCE_DIR}/../spi_controller/model/inc
    )
    target_link_libraries(spi_flash_sc_test PRIVATE spi_flash_model)

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
      COMMAND ./coverage/spi_flash_test
    )
    if(EXISTS ${SC_TEST_SRC})
      list(APPEND COVERAGE_CMDS COMMAND ./coverage/spi_flash_sc_test)
    endif()

    add_custom_target(coverage
      ${COVERAGE_CMDS}
      COMMAND lcov --capture --directory . --output-file coverage/coverage.info
      COMMAND lcov --extract coverage/coverage.info '${CMAKE_CURRENT_SOURCE_DIR}/model/*' --output-file=coverage/coverage_filtered.info
      COMMAND genhtml coverage/coverage_filtered.info --output-directory coverage/html
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
      -I ${CMAKE_CURRENT_SOURCE_DIR}/model/inc
      -I ${CMAKE_CURRENT_SOURCE_DIR}/../spi_controller/model/inc
      -I $ENV{SYSTEMC_HOME}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/model/src/*.cpp
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
