cmake_minimum_required(VERSION 3.14)

project(sep_reset_ctrl LANGUAGES CXX)

# -----------------------------------------------------------------------------
# CMake Module Path - Add shared cmake modules from repo root
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
        add_custom_target(sep_reset_ctrl_docs
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
    option(BUILD_TESTS "Build sep_reset_ctrl test executables" ON)
  endif()
endif()

# -----------------------------------------------------------------------------
# Definitions
# -----------------------------------------------------------------------------
add_compile_definitions(USE_CSML_REG_LIB SC_ALLOW_DEPRECATED_IEEE_API)

# -----------------------------------------------------------------------------
# Build type & output dirs
# -----------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
message(STATUS "sep_reset_ctrl Build Type: ${CMAKE_BUILD_TYPE}")

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
  add_compile_definitions(DEBUG CSML_DEFAULT_VERBOSITY=3)
  message(STATUS "sep_reset_ctrl: Debug build - full logging enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 ${WFLAG}")
  add_compile_definitions(NDEBUG CSML_DEFAULT_VERBOSITY=3)
  message(STATUS "sep_reset_ctrl: Release build - errors only, optimized")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g -O0 -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=2)
  message(STATUS "sep_reset_ctrl: ASAN build - AddressSanitizer enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g --coverage -fprofile-arcs -ftest-coverage")
  include(PeripheralCoverage)
  peripheral_coverage_link_flags()
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=1 __COVERAGE__)
  message(STATUS "sep_reset_ctrl: Coverage build enabled")
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
  ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
)
# NOTE: test/inc is deliberately NOT added here — it's added per-target below,
# only for sep_reset_ctrl_testbench. Adding it globally would leak test-only
# headers into sep_reset_ctrl_model's (production) include path.

# Add shared CSML logger library (only if not already built)
if(NOT TARGET csml_logger)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml ${CMAKE_CURRENT_BINARY_DIR}/csml)
endif()

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
set(MODEL_SRCS
    src/sep_reset_ctrl_base.cpp
    src/sep_reset_ctrl.cpp
)

file(GLOB_RECURSE TEST_SRCS "test/src/*.cpp")

# -----------------------------------------------------------------------------
# Targets
# -----------------------------------------------------------------------------
peripheral_add_model_library(sep_reset_ctrl_model ${MODEL_SRCS})
target_include_directories(sep_reset_ctrl_model PUBLIC
  include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
)
target_link_libraries(sep_reset_ctrl_model PUBLIC csml_logger SystemC::systemc Threads::Threads)
if(CCI_FOUND)
  target_link_libraries(sep_reset_ctrl_model PUBLIC ${CCI_LIBRARIES})
endif()

if(BUILD_TESTS)
    enable_testing()

    add_executable(sep_reset_ctrl_testbench ${TEST_SRCS})
    peripheral_link_test_to_model(sep_reset_ctrl_testbench sep_reset_ctrl_model)
    target_include_directories(sep_reset_ctrl_testbench PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/test/inc
    )
    target_link_libraries(sep_reset_ctrl_testbench PRIVATE csml_logger SystemC::systemc)

    add_test(
        NAME sep_reset_ctrl_test
        COMMAND sep_reset_ctrl_testbench
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

# -----------------------------------------------------------------------------
# Print status information
# -----------------------------------------------------------------------------
message(STATUS "Generated Project: sep_reset_ctrl")
message(STATUS "  - SYSTEMC_HOME: ${SYSTEMC_HOME}")

# -----------------------------------------------------------------------------
# Coverage Target
# -----------------------------------------------------------------------------
if(BUILD_TESTS AND CMAKE_BUILD_TYPE STREQUAL "Coverage")
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${CMAKE_BINARY_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/PeripheralCoverageClean.cmake
        COMMAND ${LCOV_EXECUTABLE} ${LCOV_IGNORE_FLAGS} --zerocounters --directory ${CMAKE_BINARY_DIR}
        COMMAND ${CMAKE_BINARY_DIR}/bin/sep_reset_ctrl_testbench
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
        COMMENT "Generating coverage report for sep_reset_ctrl..."
    )
    add_dependencies(coverage sep_reset_ctrl_testbench)
endif()

# -----------------------------------------------------------------------------
# Static Analysis Target (cppcheck)
# -----------------------------------------------------------------------------
find_program(CPPCHECK_EXECUTABLE cppcheck)
if(CPPCHECK_EXECUTABLE)
  add_custom_target(sep_reset_ctrl_cppcheck
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
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running cppcheck on sep_reset_ctrl model..."
  )
  message(STATUS "cppcheck found - 'make sep_reset_ctrl_cppcheck' target available")
else()
  message(STATUS "cppcheck not found - static analysis target not available")
endif()
