# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
cmake_minimum_required(VERSION 3.14)

project(sep_memory LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")
include(PeripheralCommon)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
  add_compile_options(-g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
elseif(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  add_compile_options(-g --coverage)
  include(PeripheralCoverage)
  peripheral_coverage_link_flags()
endif()

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
        add_custom_target(sep_memory_docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
    endif()
else()
    message(WARNING "Doxygen not found - documentation target will not be available")
endif()

include(FindCCI)
include(FindSystemC)

if(NOT TARGET Boost::iostreams)
  find_package(Boost REQUIRED COMPONENTS iostreams)
endif()

if(CCI_FOUND)
  include_directories(SYSTEM ${CCI_INCLUDE_DIR})
endif()

set(SRAM_SRCS
    model/src/sep_memory.cpp
)

set(SRAM_ROM_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/paged-memory  # PagedMemory header
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../vp/platform/infra
)

add_compile_definitions(SC_ALLOW_DEPRECATED_IEEE_API)

add_library(sep_memory_model STATIC ${SRAM_SRCS})

target_include_directories(sep_memory_model PUBLIC ${SRAM_ROM_INCLUDES})

target_link_libraries(sep_memory_model PUBLIC SystemC::systemc Boost::iostreams)
if(CCI_FOUND)
  target_link_libraries(sep_memory_model PUBLIC ${CCI_LIBRARIES})
endif()

option(BUILD_TESTS "Build standalone tests" ON)
if(BUILD_TESTS)
  add_executable(sep_memory_tb test/sep_memory_tb.cpp)
  target_link_libraries(sep_memory_tb PRIVATE sep_memory_model SystemC::systemc)
  if(CCI_FOUND)
    target_link_libraries(sep_memory_tb PRIVATE ${CCI_LIBRARIES})
  endif()
  enable_testing()
  add_test(NAME sep_memory_tb COMMAND sep_memory_tb)
  if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    peripheral_add_coverage_target(
      PRIMARY_TARGET sep_memory_tb
      SOURCE_GLOBS
        "${CMAKE_CURRENT_SOURCE_DIR}/model/src/*"
        "${CMAKE_CURRENT_SOURCE_DIR}/model/inc/*"
    )
  endif()
endif()

# -----------------------------------------------------------------------------
# Static Analysis Target (cppcheck)
# -----------------------------------------------------------------------------
find_program(CPPCHECK_EXECUTABLE cppcheck)
if(CPPCHECK_EXECUTABLE)
  add_custom_target(sep_memory_cppcheck
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
      -I ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
      -I $ENV{SYSTEMC_HOME}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/model/src/*.cpp
      2> ${CMAKE_BINARY_DIR}/cppcheck_report.txt
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    COMMAND ${CMAKE_COMMAND} -E echo "cppcheck report - MODEL ONLY:"
    COMMAND ${CMAKE_COMMAND} -E echo "  ${CMAKE_BINARY_DIR}/cppcheck_report.txt"
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running cppcheck static analysis on SEP Memory model..."
  )
  message(STATUS "cppcheck found - 'make sep_memory_cppcheck' target available")
else()
  message(STATUS "cppcheck not found - static analysis target not available")
endif()
