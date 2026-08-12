cmake_minimum_required(VERSION 3.14)

project(lifecycle_ctrl LANGUAGES CXX)

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
        add_custom_target(lifecycle_ctrl_docs
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

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
message(STATUS "lifecycle_ctrl Build Type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/wd4100)
else()
    add_compile_options(-Wno-unused-parameter)
endif()

set(WFLAG "-Wall -Wextra")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g ${WFLAG}")
  add_compile_definitions(DEBUG CSML_DEFAULT_VERBOSITY=3)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 ${WFLAG}")
  add_compile_definitions(NDEBUG CSML_DEFAULT_VERBOSITY=3)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g -O0 -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=2)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g --coverage -fprofile-arcs -ftest-coverage")
  include(PeripheralCoverage)
  peripheral_coverage_link_flags()
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=1 __COVERAGE__)
endif()

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------
include(FindCCI)
include(FindSystemC)

if(CCI_FOUND)
  include_directories(SYSTEM ${CCI_INCLUDE_DIR})
endif()

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/test/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
)

# Default ON for standalone builds, OFF when included as subproject (VP integration)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(BUILD_TESTS "Build lc_ctrl test executables" ON)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml ${CMAKE_CURRENT_BINARY_DIR}/csml)
else()
    option(BUILD_TESTS "Build lc_ctrl test executables" OFF)
endif()

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
set(MODEL_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lifecycle_ctrl_base.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lifecycle_ctrl.cpp
)

set(TEST_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/test/src/lifecycle_ctrl_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/src/lifecycle_ctrl_basetest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/src/testbench.cpp
)

# -----------------------------------------------------------------------------
# Library target
# -----------------------------------------------------------------------------
peripheral_add_model_library(lifecycle_ctrl_model ${MODEL_SRCS})
target_include_directories(lifecycle_ctrl_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(lifecycle_ctrl_model PUBLIC csml_logger SystemC::systemc Threads::Threads ${CCI_LIBRARIES})

# -----------------------------------------------------------------------------
# Test executable
# -----------------------------------------------------------------------------
if(BUILD_TESTS)
  add_executable(lifecycle_ctrl_test ${TEST_SRCS})
  peripheral_link_test_to_model(lifecycle_ctrl_test lifecycle_ctrl_model)
  if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    add_custom_target(coverage
      COMMAND ${LCOV_EXECUTABLE} ${LCOV_IGNORE_FLAGS} --zerocounters --directory ${CMAKE_BINARY_DIR}
      COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${CMAKE_BINARY_DIR}
              -P ${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/PeripheralCoverageClean.cmake
      COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_tests.sh --coverage --no-build
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND ${CMAKE_COMMAND} -E echo "Coverage report at build/coverage/coverage/html/index.html"
    )
    add_dependencies(coverage lifecycle_ctrl_test)
  endif()

  enable_testing()
  add_test(NAME lifecycle_ctrl_test COMMAND lifecycle_ctrl_test)
endif()

# -----------------------------------------------------------------------------
# Static Analysis Target (cppcheck)
# -----------------------------------------------------------------------------
find_program(CPPCHECK_EXECUTABLE cppcheck)
if(CPPCHECK_EXECUTABLE)
  add_custom_target(lifecycle_ctrl_cppcheck
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
    COMMENT "Running cppcheck static analysis on lc_ctrl model..."
  )
  message(STATUS "cppcheck found - 'make lifecycle_ctrl_cppcheck' target available")
else()
  message(STATUS "cppcheck not found - static analysis target not available")
endif()

message(STATUS "Generated Project: lifecycle_ctrl")
