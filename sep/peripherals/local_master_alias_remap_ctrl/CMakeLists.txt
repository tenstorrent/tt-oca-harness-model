cmake_minimum_required(VERSION 3.14)

project(local_master_alias_remap_ctrl LANGUAGES CXX)

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
        add_custom_target(local_master_alias_remap_ctrl_docs
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
add_compile_definitions(USE_CSML_REG_LIB SC_ALLOW_DEPRECATED_IEEE_API)

# -----------------------------------------------------------------------------
# Build type & output dirs
# -----------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)

message(STATUS "local_master_alias_remap_ctrl Build Type: ${CMAKE_BUILD_TYPE}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "local_master_alias_remap_ctrl: Debug build - full logging enabled")
    add_compile_definitions(SC_ENABLE_SIMULATION_PHASE_CALLBACKS)
elseif(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    message(STATUS "local_master_alias_remap_ctrl: Coverage build - gcov enabled")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
    include(PeripheralCoverage)
    peripheral_coverage_link_flags()
    add_compile_definitions(__COVERAGE__)
elseif(CMAKE_BUILD_TYPE STREQUAL "ASAN")
    message(STATUS "local_master_alias_remap_ctrl: AddressSanitizer build - ASAN enabled")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
endif()

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------
include(FindCCI)
include(FindSystemC)
find_package(Threads REQUIRED)

if(CCI_FOUND)
    include_directories(SYSTEM ${CCI_INCLUDE_DIR})
endif()

# -----------------------------------------------------------------------------
# csml (Core SystemC Model Library) — only when built standalone
# -----------------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(../../utils/csml csml)
endif()

# -----------------------------------------------------------------------------
# Model library
# -----------------------------------------------------------------------------
peripheral_add_model_library(local_master_alias_remap_ctrl_model
    src/local_alias_remap_base.cpp
    src/local_alias_remap.cpp
)

target_include_directories(local_master_alias_remap_ctrl_model
    PUBLIC
        include
        ../../utils/tlm_extensions
    PRIVATE
        ../../utils/csml/inc
)

target_link_libraries(local_master_alias_remap_ctrl_model
    PUBLIC
        SystemC::systemc
        Threads::Threads
        csml_logger
)

if(TARGET CCI::cci)
    target_link_libraries(local_master_alias_remap_ctrl_model PUBLIC CCI::cci)
endif()

# -----------------------------------------------------------------------------
# Tests (enabled by BUILD_TESTS option)
# -----------------------------------------------------------------------------
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()
    
    add_executable(local_master_alias_remap_ctrl_testbench
        test/src/local_alias_remap_basetest.cpp
        test/src/local_alias_remap_test.cpp
        test/src/local_master_alias_remap_ctrl_testbench.cpp
    )
    
    target_include_directories(local_master_alias_remap_ctrl_testbench PRIVATE
        test/inc
        include
        ../../utils/csml/inc
        ../../utils/tlm_extensions
    )

    peripheral_link_test_to_model(local_master_alias_remap_ctrl_testbench local_master_alias_remap_ctrl_model)
    target_link_libraries(local_master_alias_remap_ctrl_testbench PRIVATE
        csml_logger
        SystemC::systemc
    )
    
    add_test(
        NAME local_master_alias_remap_ctrl_unit_tests
        COMMAND local_master_alias_remap_ctrl_testbench
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    
    add_dependencies(local_master_alias_remap_ctrl_testbench csml_logger)

    if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
        add_custom_target(coverage
            COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${CMAKE_BINARY_DIR}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/PeripheralCoverageClean.cmake
            COMMAND ${LCOV_EXECUTABLE} ${LCOV_IGNORE_FLAGS} --zerocounters --directory ${CMAKE_BINARY_DIR}
            COMMAND ${CMAKE_BINARY_DIR}/bin/local_master_alias_remap_ctrl_testbench
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
            COMMAND ${LCOV_EXECUTABLE} ${LCOV_IGNORE_FLAGS}
                    --capture --directory ${CMAKE_BINARY_DIR}
                    --output-file ${CMAKE_BINARY_DIR}/coverage/coverage.info
            COMMAND ${LCOV_EXECUTABLE} ${LCOV_IGNORE_FLAGS}
                    # Hand-written sources only. include/local_alias_remap_register.h
                    # is generated from the RDL, and its csml template instantiations
                    # dominate the totals without saying anything about this model.
                    --extract ${CMAKE_BINARY_DIR}/coverage/coverage.info
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/local_alias_remap.cpp"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/local_alias_remap_base.cpp"
                    "${CMAKE_CURRENT_SOURCE_DIR}/include/local_alias_remap.h"
                    "${CMAKE_CURRENT_SOURCE_DIR}/include/local_alias_remap_base.h"
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
            COMMENT "Generating coverage report for local_master_alias_remap_ctrl..."
        )
        add_dependencies(coverage local_master_alias_remap_ctrl_testbench)
    endif()
endif()

# -----------------------------------------------------------------------------
# CPPCheck static analysis
# -----------------------------------------------------------------------------
find_program(CPPCHECK_BIN NAMES cppcheck)
if(CPPCHECK_BIN)
    add_custom_target(local_master_alias_remap_ctrl_cppcheck
        COMMAND ${CPPCHECK_BIN}
            --enable=warning,portability,performance,style
            --std=c++17
            --verbose
            --quiet
            --force
            --inline-suppr
            --suppressions-list=${CMAKE_CURRENT_SOURCE_DIR}/../../../.cppcheck_suppressions
            --includes-file=${CMAKE_CURRENT_SOURCE_DIR}/include
            --includes-file=${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/test/src
        COMMENT "Running cppcheck static analysis on local_master_alias_remap_ctrl"
    )
    message(STATUS "cppcheck found - 'make local_master_alias_remap_ctrl_cppcheck' target available")
endif()

message(STATUS "Generated Project: local_master_alias_remap_ctrl")