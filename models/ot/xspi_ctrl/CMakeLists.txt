cmake_minimum_required(VERSION 3.14)

project(xspi_ctrl LANGUAGES CXX)

# -----------------------------------------------------------------------------
# CMake Module Path - Add shared cmake modules from repo root
# -----------------------------------------------------------------------------
# Support both standalone and integrated builds
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")

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
message(STATUS "xspi_ctrl Build Type: ${CMAKE_BUILD_TYPE}")

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

# Add warning suppression
if(NOT MSVC)
    add_compile_options(-Wno-unused-parameter)
endif()

set(WFLAG "-Wall -Wextra")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g ${WFLAG}")
  add_compile_definitions(DEBUG CSML_DEFAULT_VERBOSITY=3)
  message(STATUS "xspi_ctrl: Debug build - full logging enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 ${WFLAG}")
  add_compile_definitions(NDEBUG CSML_DEFAULT_VERBOSITY=1)
  message(STATUS "xspi_ctrl: Release build - errors only, optimized")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g -O0 -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=2)
  message(STATUS "xspi_ctrl: ASAN build - AddressSanitizer enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g --coverage -fprofile-arcs -ftest-coverage ${WFLAG}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage -lgcov")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=1 __COVERAGE__)
  message(STATUS "xspi_ctrl: Coverage build enabled")
endif()

# -----------------------------------------------------------------------------
# External Dependencies (SystemC, CCI)
# -----------------------------------------------------------------------------
include(FindSystemC)
include(FindCCI)

if(CCI_FOUND)
  include_directories(${CCI_INCLUDE_DIR})
endif()

# Find threading library (required by SystemC)
find_package(Threads REQUIRED)

# -----------------------------------------------------------------------------
# Paths & Includes
# -----------------------------------------------------------------------------
include_directories(
  model/inc
  test/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
)

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
file(GLOB_RECURSE MODEL_SRCS "model/src/*.cpp")
file(GLOB_RECURSE TEST_SRCS  "test/src/*.cpp")

# -----------------------------------------------------------------------------
# Targets
# -----------------------------------------------------------------------------
# Context-aware BUILD_TESTS option
# Default ON for standalone builds, OFF when included as subproject (VP integration)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(BUILD_TESTS "Build xspi_ctrl test executables" ON)
    # For standalone builds, we need to build csml_logger ourselves
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml ${CMAKE_CURRENT_BINARY_DIR}/csml)
else()
    option(BUILD_TESTS "Build xspi_ctrl test executables" OFF)
    # csml_logger library is provided by parent models/CMakeLists.txt when built as subproject
endif()

# Model library (always built)
add_library(xspi_ctrl_model STATIC ${MODEL_SRCS})
target_include_directories(xspi_ctrl_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/model/inc)
target_link_libraries(xspi_ctrl_model PUBLIC csml_logger SystemC::systemc Threads::Threads ${CCI_LIBRARIES})

  add_library(xspi_flash_target STATIC
    target_model/src/xspi_target.cpp
    target_model/src/xspi_target_sc_wrapper.cpp
  )
  target_include_directories(xspi_flash_target PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/target_model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/model/inc
  )
  target_link_libraries(xspi_flash_target PUBLIC SystemC::systemc)

											   
if(BUILD_TESTS)			   
  add_executable(xspi_ctrl_testbench ${TEST_SRCS})
  target_link_libraries(xspi_ctrl_testbench PRIVATE xspi_ctrl_model xspi_flash_target)

  enable_testing()
  add_test(NAME xspi_ctrl_test COMMAND xspi_ctrl_testbench)

  # -----------------------------------------------------------------------------
  # Coverage target (model-only: excludes target_model/flash and tests)
  # -----------------------------------------------------------------------------
  if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    find_program(LCOV_PATH lcov)
    find_program(GENHTML_PATH genhtml)

    if(LCOV_PATH AND GENHTML_PATH)
      add_custom_target(coverage
        COMMAND ${LCOV_PATH} --zerocounters --directory .
        COMMAND ./coverage/xspi_ctrl_testbench
        COMMAND ${LCOV_PATH} --capture --directory . --output-file coverage/coverage.info
        COMMAND ${LCOV_PATH} --extract coverage/coverage.info '${CMAKE_CURRENT_SOURCE_DIR}/model/*' --output-file=coverage/coverage_filtered.info
        COMMAND ${GENHTML_PATH} coverage/coverage_filtered.info --output-directory coverage/html
        COMMAND ${CMAKE_COMMAND} -E echo "Coverage report at coverage/html/index.html"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating xspi_ctrl model coverage report..."
      )
      add_dependencies(coverage xspi_ctrl_testbench)
    else()
      message(WARNING "lcov or genhtml not found; coverage target will not be available")
    endif()
  endif()
endif()

# -----------------------------------------------------------------------------
# Print status information
# -----------------------------------------------------------------------------
message(STATUS "Generated Project: xspi_ctrl")
message(STATUS "  - SystemC library: ${SYSTEMC_LIBRARY}")