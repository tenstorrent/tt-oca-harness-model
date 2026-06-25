cmake_minimum_required(VERSION 3.14)

project(hmac LANGUAGES CXX)

# -----------------------------------------------------------------------------
# CMake Module Path - Add shared cmake modules from repo root
# -----------------------------------------------------------------------------
# Support both standalone and integrated builds
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")


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
        add_custom_target(hmac_docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
    endif()
else()
    message(WARNING "Doxygen not found - documentation target will not be available")
endif()

add_compile_definitions(SC_ALLOW_DEPRECATED_IEEE_API)

# -----------------------------------------------------------------------------
# Build type & output dirs
# -----------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
message(STATUS "HMAC Build Type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# -----------------------------------------------------------------------------
# Compiler settings
# -----------------------------------------------------------------------------
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add warning suppression here
if(MSVC)
    add_compile_options(/wd4100)  # Unreferenced formal parameter
else()
    add_compile_options(-Wno-unused-parameter)
endif()

set(WFLAG "-Wall -Wextra")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g ${WFLAG}")
  add_compile_definitions(DEBUG CSML_DEFAULT_VERBOSITY=3)
  message(STATUS "HMAC: Debug build - full logging enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 ${WFLAG}")
  add_compile_definitions(NDEBUG CSML_DEFAULT_VERBOSITY=3)
  message(STATUS "HMAC: Release build - errors only, optimized")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g -O0 -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=2)
  message(STATUS "HMAC: ASAN build - AddressSanitizer enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g --coverage -fprofile-arcs -ftest-coverage")
  include(PeripheralCoverage)
  peripheral_coverage_link_flags()
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=1 __COVERAGE__)
  message(STATUS "HMAC: Coverage build enabled")
endif()

# -----------------------------------------------------------------------------
# Paths & Includes
# -----------------------------------------------------------------------------
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
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
  include_directories(${CCI_INCLUDE_DIR})
endif()

# -----------------------------------------------------------------------------
# OpenSSL Library (required for HMAC cryptographic operations)
# -----------------------------------------------------------------------------
include(PeripheralOpenSSL)
peripheral_find_openssl()
if(OPENSSL_FOUND)
  message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")
  message(STATUS "  Include: ${OPENSSL_INCLUDE_DIR}")
  message(STATUS "  Libraries: ${OPENSSL_LIBRARIES}")
else()
  message(FATAL_ERROR "OpenSSL not found. HMAC requires OpenSSL for cryptographic operations.")
endif()

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
set(MODEL_SRCS
  src/hmac.cpp
  src/hmac_base.cpp
)

set(TEST_SRCS
  test/src/hmac_test.cpp
  test/src/hmac_basetest.cpp
  test/src/basic_tests.cpp
  test/src/testbench.cpp
)

# -----------------------------------------------------------------------------
# Libraries and Executables
# -----------------------------------------------------------------------------
# Context-aware BUILD_TESTS option
# Default ON for standalone builds, OFF when included as subproject (VP integration)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if(NOT DEFINED BUILD_TESTS)
      option(BUILD_TESTS "Build HMAC test executables" ON)
    endif()
    # For standalone builds, we need to build csml_logger ourselves
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml ${CMAKE_CURRENT_BINARY_DIR}/csml)
endif()

# HMAC model library (always built)
# Use STATIC library to match old working CMake configuration
add_library(hmac_model STATIC ${MODEL_SRCS})
target_include_directories(hmac_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Increase template depth for HMAC's large register arrays (1024 MSG_FIFO registers)
target_compile_options(hmac_model PRIVATE -ftemplate-depth=2000)

target_link_libraries(hmac_model PUBLIC
  csml_logger
  SystemC::systemc
  Threads::Threads
  OpenSSL::SSL
  OpenSSL::Crypto
  ${CCI_LIBRARIES}
)

# HMAC test executable (only if BUILD_TESTS=ON)
if(BUILD_TESTS)
  add_executable(hmac_test ${TEST_SRCS})
  target_compile_options(hmac_test PRIVATE -ftemplate-depth=2000)
  target_link_libraries(hmac_test PRIVATE hmac_model)

  # -----------------------------------------------------------------------------
  # Coverage target
  # -----------------------------------------------------------------------------
  if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    # Keep only model/ directory in coverage, exclude everything else
    add_custom_target(coverage
      COMMAND ./bin/hmac_test
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
      COMMAND lcov --ignore-errors inconsistent,unsupported,format,mismatch --capture --directory . --output-file coverage/coverage.info
      COMMAND lcov --ignore-errors inconsistent,unsupported,format,mismatch --extract coverage/coverage.info '${CMAKE_CURRENT_SOURCE_DIR}/src/*' '${CMAKE_CURRENT_SOURCE_DIR}/include/*' --output-file=coverage/coverage_filtered.info
      COMMAND genhtml --ignore-errors inconsistent,unsupported,format,corrupt,category coverage/coverage_filtered.info --output-directory coverage/html
      COMMAND ${CMAKE_COMMAND} -E echo "Coverage report at coverage/html/index.html"
    )
    add_dependencies(coverage hmac_test)
  endif()

  # -----------------------------------------------------------------------------
  # CTest
  # -----------------------------------------------------------------------------
  enable_testing()
  add_test(NAME hmac_test COMMAND hmac_test)
endif()

# -----------------------------------------------------------------------------
# Static Analysis Target (cppcheck)
# -----------------------------------------------------------------------------
find_program(CPPCHECK_EXECUTABLE cppcheck)
if(CPPCHECK_EXECUTABLE)
  add_custom_target(hmac_cppcheck
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
    COMMENT "Running cppcheck static analysis on HMAC model..."
  )
  message(STATUS "cppcheck found - 'make hmac_cppcheck' target available")
else()
  message(STATUS "cppcheck not found - static analysis target not available")
endif()
