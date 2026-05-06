cmake_minimum_required(VERSION 3.14)

project(sep_efuse LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")

add_compile_definitions(USE_CSML_REG_LIB SC_ALLOW_DEPRECATED_IEEE_API)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
message(STATUS "sep_efuse Build Type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG    "${CMAKE_BINARY_DIR}/debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE  "${CMAKE_BINARY_DIR}/release")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_ASAN     "${CMAKE_BINARY_DIR}/asan")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_COVERAGE "${CMAKE_BINARY_DIR}/coverage")

set(CMAKE_CXX_STANDARD 17)
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
  add_compile_definitions(NDEBUG CSML_DEFAULT_VERBOSITY=1)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g -O0 -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=2)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g --coverage -fprofile-arcs -ftest-coverage")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage -lgcov")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=1 __COVERAGE__)
endif()

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------
include(FindSystemC)
include(FindCCI)

if(CCI_FOUND)
  include_directories(${CCI_INCLUDE_DIR})
endif()

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/model/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/test/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
)

# Default ON for standalone builds, OFF when included as subproject (VP integration)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(BUILD_TESTS "Build sep_efuse test executables" ON)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml ${CMAKE_CURRENT_BINARY_DIR}/csml)
else()
    option(BUILD_TESTS "Build sep_efuse test executables" OFF)
endif()

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
set(MODEL_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/model/src/sep_efuse_base.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/model/src/sep_efuse.cpp
)

set(TEST_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/test/src/sep_efuse_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/src/sep_efuse_basetest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/src/testbench.cpp
)

# -----------------------------------------------------------------------------
# Library target
# -----------------------------------------------------------------------------
add_library(sep_efuse_model STATIC ${MODEL_SRCS})
target_include_directories(sep_efuse_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/model/inc)
target_link_libraries(sep_efuse_model PUBLIC csml_logger SystemC::systemc Threads::Threads ${CCI_LIBRARIES})

# -----------------------------------------------------------------------------
# Test executable
# -----------------------------------------------------------------------------
if(BUILD_TESTS)
  add_executable(sep_efuse_test ${TEST_SRCS})
  target_link_libraries(sep_efuse_test PRIVATE sep_efuse_model)

  if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    add_custom_target(coverage
      COMMAND ./coverage/sep_efuse_test
      COMMAND lcov --capture --directory . --output-file coverage/coverage.info
      COMMAND lcov --extract coverage/coverage.info '${CMAKE_CURRENT_SOURCE_DIR}/model/*' --output-file=coverage/coverage_filtered.info
      COMMAND genhtml coverage/coverage_filtered.info --output-directory coverage/html
      COMMAND ${CMAKE_COMMAND} -E echo "Coverage report at coverage/html/index.html"
    )
    add_dependencies(coverage sep_efuse_test)
  endif()

  enable_testing()
  add_test(NAME sep_efuse_test COMMAND sep_efuse_test)
endif()

# -----------------------------------------------------------------------------
# Static Analysis Target (cppcheck)
# -----------------------------------------------------------------------------
find_program(CPPCHECK_EXECUTABLE cppcheck)
if(CPPCHECK_EXECUTABLE)
  add_custom_target(sep_efuse_cppcheck
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
      -I ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
      -I $ENV{SYSTEMC_HOME}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/model/src/*.cpp
      2> ${CMAKE_BINARY_DIR}/cppcheck_report.txt
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    COMMAND ${CMAKE_COMMAND} -E echo "cppcheck report - MODEL ONLY:"
    COMMAND ${CMAKE_COMMAND} -E echo "  ${CMAKE_BINARY_DIR}/cppcheck_report.txt"
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running cppcheck static analysis on sep_efuse model..."
  )
  message(STATUS "cppcheck found - 'make sep_efuse_cppcheck' target available")
else()
  message(STATUS "cppcheck not found - static analysis target not available")
endif()

message(STATUS "Generated Project: sep_efuse")
