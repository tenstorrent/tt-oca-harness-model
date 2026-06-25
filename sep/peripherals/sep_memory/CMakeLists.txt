cmake_minimum_required(VERSION 3.14)

project(sep_memory LANGUAGES CXX)

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

find_package(Boost REQUIRED COMPONENTS iostreams)

if(CCI_FOUND)
  include_directories(${CCI_INCLUDE_DIR})
endif()

set(SRAM_SRCS
    model/src/sep_memory.cpp
)

set(SRAM_ROM_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/paged-memory  # PagedMemory header
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../vp/platform/infra
)

if(NOT TARGET csml_logger)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml ${CMAKE_CURRENT_BINARY_DIR}/csml)
endif()

add_compile_definitions(SC_ALLOW_DEPRECATED_IEEE_API)

add_library(sep_memory_model STATIC ${SRAM_SRCS})

target_include_directories(sep_memory_model PUBLIC ${SRAM_ROM_INCLUDES})

target_link_libraries(sep_memory_model PUBLIC csml_logger SystemC::systemc Boost::iostreams)
if(CCI_FOUND)
  target_link_libraries(sep_memory_model PUBLIC ${CCI_LIBRARIES})
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
      -I ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
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
