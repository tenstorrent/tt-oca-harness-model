cmake_minimum_required(VERSION 3.14)

project(aon_timer LANGUAGES CXX)

# -----------------------------------------------------------------------------
# CMake Module Path - Add shared cmake modules from repo root
# -----------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")

include(VpSyscBackendFallback)

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
message(STATUS "aon_timer Build Type: ${CMAKE_BUILD_TYPE}")

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
    add_compile_options(/wd4100)
else()
    add_compile_options(-Wno-unused-parameter)
endif()

set(WFLAG "-Wall -Wextra")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g ${WFLAG}")
  add_compile_definitions(DEBUG CSML_DEFAULT_VERBOSITY=3)
  message(STATUS "aon_timer: Debug build - full logging enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 ${WFLAG}")
  add_compile_definitions(NDEBUG CSML_DEFAULT_VERBOSITY=0)
  message(STATUS "aon_timer: Release build - errors only, optimized")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g -O0 -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=2)
  message(STATUS "aon_timer: ASAN build - AddressSanitizer enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g --coverage -fprofile-arcs -ftest-coverage")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage -lgcov")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=1 __COVERAGE__)
  message(STATUS "aon_timer: Coverage build enabled")
endif()

# -----------------------------------------------------------------------------
# Paths & Includes
# -----------------------------------------------------------------------------
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/model/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/test/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
)

# -----------------------------------------------------------------------------
# External Dependencies (SystemC and CCI)
# -----------------------------------------------------------------------------
include(SystemCBackend)
include(FindCCI)

if(CCI_FOUND)
  include_directories(${CCI_INCLUDE_DIR})
endif()

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
set(MODEL_SRCS
  model/src/aon_timer.cpp
  model/src/aon_timer_base.cpp
)

set(TEST_SRCS
  test/src/aon_timer_test.cpp
  test/src/aon_timer_basetest.cpp
  test/src/testbench.cpp
)

# -----------------------------------------------------------------------------
# Libraries and Executables
# -----------------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if(NOT DEFINED BUILD_TESTS)
      option(BUILD_TESTS "Build aon_timer test executables" ON)
    endif()
    # For standalone builds, build csml_logger from common location
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml ${CMAKE_CURRENT_BINARY_DIR}/csml)
endif()

# aon_timer model library (always built)
add_library(aon_timer_model STATIC ${MODEL_SRCS})
target_include_directories(aon_timer_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/model/inc)
target_link_libraries(aon_timer_model PUBLIC csml_logger SystemC::systemc Threads::Threads)
if(CCI_FOUND)
  target_link_libraries(aon_timer_model PUBLIC ${CCI_LIBRARIES})
endif()
target_link_libraries(aon_timer_model PUBLIC vp::sysc_backend)

# aon_timer testbench executable (only if BUILD_TESTS=ON)
if(BUILD_TESTS)
  add_executable(aon_timer_testbench ${TEST_SRCS})
  target_link_libraries(aon_timer_testbench PRIVATE aon_timer_model)

  # Set RPATH so the binary finds SystemC and CCI libraries without LD_LIBRARY_PATH
  if(NOT VP_SYSC_BACKEND STREQUAL "synopsys")
    set(_rpath_dirs "")
    if(DEFINED SYSTEMC_LIB_DIR AND SYSTEMC_LIB_DIR)
      list(APPEND _rpath_dirs "${SYSTEMC_LIB_DIR}")
    endif()
    if(CCI_FOUND AND CCI_CONFIG_LIBRARY)
      get_filename_component(_cci_lib_dir "${CCI_CONFIG_LIBRARY}" DIRECTORY)
      list(APPEND _rpath_dirs "${_cci_lib_dir}")
    endif()
    if(_rpath_dirs)
      set_target_properties(aon_timer_testbench PROPERTIES
          INSTALL_RPATH "${_rpath_dirs}"
          BUILD_WITH_INSTALL_RPATH TRUE
      )
    endif()
  endif()

  # -----------------------------------------------------------------------------
  # Coverage target
  # -----------------------------------------------------------------------------
  if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    # Keep only model/ directory in coverage, exclude everything else
    add_custom_target(coverage
      COMMAND ./coverage/aon_timer_testbench
      COMMAND lcov --capture --directory . --output-file coverage/coverage.info
      COMMAND lcov --extract coverage/coverage.info '${CMAKE_CURRENT_SOURCE_DIR}/model/*' --output-file=coverage/coverage_filtered.info
      COMMAND genhtml coverage/coverage_filtered.info --output-directory coverage/html
      COMMAND ${CMAKE_COMMAND} -E echo "Coverage report at coverage/html/index.html"
    )
    add_dependencies(coverage aon_timer_testbench)
  endif()

  enable_testing()
  add_test(NAME aon_timer_test COMMAND aon_timer_testbench)
endif()

# -----------------------------------------------------------------------------
# Print status information
# -----------------------------------------------------------------------------
message(STATUS "Generated Project: aon_timer")