cmake_minimum_required(VERSION 3.14)

project(el2_pic LANGUAGES CXX)

# -----------------------------------------------------------------------------
# CMake Module Path
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
        add_custom_target(el2_pic_docs
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
    option(BUILD_TESTS "Build el2_pic test executables" ON)
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
message(STATUS "el2_pic Build Type: ${CMAKE_BUILD_TYPE}")

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
  message(STATUS "el2_pic: Debug build - full logging enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 ${WFLAG}")
  add_compile_definitions(NDEBUG CSML_DEFAULT_VERBOSITY=3)
  message(STATUS "el2_pic: Release build - errors only, optimized")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g -O0 -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=2)
  message(STATUS "el2_pic: ASAN build - AddressSanitizer enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g --coverage -fprofile-arcs -ftest-coverage")
  include(PeripheralCoverage)
  peripheral_coverage_link_flags()
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=1 __COVERAGE__)
  message(STATUS "el2_pic: Coverage build enabled")
endif()

# -----------------------------------------------------------------------------
# External Dependencies
# -----------------------------------------------------------------------------
find_package(Threads REQUIRED)
include(FindCCI)
include(FindSystemC)

# el2_pic_model compiles against the real VeeR-ISSTlm.hpp, which includes
# boost/program_options.hpp.  The test model uses the stub header and does not
# need Boost, but the production target does.
if(NOT TARGET Boost::headers)
  find_package(Boost REQUIRED)
endif()

if(CCI_FOUND)
  include_directories(SYSTEM ${CCI_INCLUDE_DIR})
endif()

# -----------------------------------------------------------------------------
# Paths & Includes
# -----------------------------------------------------------------------------
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cpu/VeeR-ISSTlm/model/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/tlm_extensions
)
# Vendored ISS: SYSTEM so -Wall -Wextra does not report on its headers.
include_directories(SYSTEM
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cpu/VeeR-ISS
)
# NOTE: test/inc is deliberately NOT added here — it's added per-target
# (target_include_directories) only for el2_pic_testmodel_lib/el2_pic_testbench
# below. Adding it globally would let test/inc/VeeR-ISSTlm.hpp (a stub for
# MockHart) shadow the real cpu/VeeR-ISSTlm/model/inc/VeeR-ISSTlm.hpp for
# el2_pic_model too, silently breaking the production build.

# Add shared CSML logger library (only if not already built)
if(NOT TARGET csml_logger)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml ${CMAKE_CURRENT_BINARY_DIR}/csml)
endif()

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
file(GLOB_RECURSE MODEL_SRCS "src/*.cpp")

# Test model: compiles the REAL el2_pic.cpp against a stub VeeR-ISSTlm.hpp
# (test/inc/VeeR-ISSTlm.hpp, aliasing VeeRISSTlm to MockHart) so the actual
# production PIC logic is exercised directly, without the full ISS/Boost
# dependency of the real VeeR-ISSTlm.hpp.
set(TEST_MODEL_SRCS
  src/el2_pic_base.cpp
  src/el2_pic.cpp
)

set(TEST_SRCS
  test/src/el2_pic_basetest.cpp
  test/src/el2_pic_test.cpp
  test/src/testbench.cpp
)

# -----------------------------------------------------------------------------
# Targets
# -----------------------------------------------------------------------------
peripheral_add_model_library(el2_pic_model ${MODEL_SRCS})
target_include_directories(el2_pic_model PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cpu/VeeR-ISSTlm/model/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/tlm_extensions
)
target_include_directories(el2_pic_model SYSTEM PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cpu/VeeR-ISS
)
target_link_libraries(el2_pic_model PUBLIC csml_logger SystemC::systemc Threads::Threads Boost::headers)
if(CCI_FOUND)
  target_link_libraries(el2_pic_model PUBLIC ${CCI_LIBRARIES})
endif()

if(BUILD_TESTS)
  # Test model library: the real el2_pic.cpp, compiled against the stub
  # VeeR-ISSTlm.hpp in test/inc (no real ISS deps) instead of the productio
  # header — see TEST_MODEL_SRCS above.
  peripheral_add_model_library(el2_pic_testmodel_lib ${TEST_MODEL_SRCS})
  # BEFORE guarantees test/inc (and its VeeR-ISSTlm.hpp stub) is searched
  # ahead of the directory-scoped cpu/VeeR-ISSTlm/model/inc above, regardless
  # of how CMake orders directory-scope vs target-scope include flags.
  target_include_directories(el2_pic_testmodel_lib BEFORE PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/test/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
  )
  target_link_libraries(el2_pic_testmodel_lib PUBLIC csml_logger SystemC::systemc Threads::Threads)
  if(CCI_FOUND)
    target_link_libraries(el2_pic_testmodel_lib PUBLIC ${CCI_LIBRARIES})
  endif()

  # Testbench executable
  add_executable(el2_pic_testbench ${TEST_SRCS})
  peripheral_link_test_to_model(el2_pic_testbench el2_pic_testmodel_lib)
  target_link_libraries(el2_pic_testbench PRIVATE csml_logger SystemC::systemc)
  target_include_directories(el2_pic_testbench BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/test/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
  )

  enable_testing()
  add_test(NAME el2_pic_testbench COMMAND el2_pic_testbench)
endif()

# -----------------------------------------------------------------------------
# Print status information
# -----------------------------------------------------------------------------
message(STATUS "Generated Project: el2_pic")
message(STATUS "  - SYSTEMC_HOME: ${SYSTEMC_HOME}")

# -----------------------------------------------------------------------------
# Coverage Target
# -----------------------------------------------------------------------------
if(BUILD_TESTS AND CMAKE_BUILD_TYPE STREQUAL "Coverage")
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${CMAKE_BINARY_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/PeripheralCoverageClean.cmake
        COMMAND ${LCOV_EXECUTABLE} ${LCOV_IGNORE_FLAGS} --zerocounters --directory ${CMAKE_BINARY_DIR}
        COMMAND ${CMAKE_BINARY_DIR}/bin/el2_pic_testbench
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
        COMMAND ${LCOV_EXECUTABLE} ${LCOV_IGNORE_FLAGS}
                --capture --directory ${CMAKE_BINARY_DIR}
                --output-file ${CMAKE_BINARY_DIR}/coverage/coverage.info
        COMMAND ${LCOV_EXECUTABLE} ${LCOV_IGNORE_FLAGS}
                # Hand-written sources only. include/el2_pic_register.h is
                # generated from the RDL, and its csml template instantiations
                # dominate the totals without saying anything about this model.
                --extract ${CMAKE_BINARY_DIR}/coverage/coverage.info
                "${CMAKE_CURRENT_SOURCE_DIR}/src/el2_pic.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/el2_pic_base.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/include/el2_pic.h"
                "${CMAKE_CURRENT_SOURCE_DIR}/include/el2_pic_base.h"
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
        COMMENT "Generating coverage report for el2_pic..."
    )
    add_dependencies(coverage el2_pic_testbench)
endif()

# -----------------------------------------------------------------------------
# Static Analysis Target (cppcheck)
# -----------------------------------------------------------------------------
find_program(CPPCHECK_EXECUTABLE cppcheck)
if(CPPCHECK_EXECUTABLE)
  add_custom_target(el2_pic_cppcheck
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
    COMMENT "Running cppcheck on el2_pic model..."
  )
  message(STATUS "cppcheck found - 'make el2_pic_cppcheck' target available")
else()
  message(STATUS "cppcheck not found - static analysis target not available")
endif()
