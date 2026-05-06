cmake_minimum_required(VERSION 3.14)

project(dma LANGUAGES CXX)

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
message(STATUS "dma Build Type: ${CMAKE_BUILD_TYPE}")

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
  message(STATUS "dma: Debug build - full logging enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 ${WFLAG}")
  add_compile_definitions(NDEBUG CSML_DEFAULT_VERBOSITY=0)
  message(STATUS "dma: Release build - errors only, optimized")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g -O0 -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=2)
  message(STATUS "dma: ASAN build - AddressSanitizer enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g --coverage -fprofile-arcs -ftest-coverage")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage -lgcov")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=1 __COVERAGE__)
  message(STATUS "dma: Coverage build enabled")
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
# External Dependencies (SystemC, CCI, OpenSSL)
# -----------------------------------------------------------------------------
include(SystemCBackend)
include(FindCCI)

if(CCI_FOUND)
  include_directories(${CCI_INCLUDE_DIR})
endif()

# OpenSSL (required for SHA-2 hash computation in FUNC-010)
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
set(MODEL_SRCS
  model/src/dma.cpp
  model/src/dma_base.cpp
)

set(TEST_BASE_SRCS
  test/src/dma_basetest.cpp
  test/src/dma_test.cpp
)

# -----------------------------------------------------------------------------
# Libraries and Executables
# -----------------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if(NOT DEFINED BUILD_TESTS)
      option(BUILD_TESTS "Build dma test executables" ON)
    endif()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml ${CMAKE_CURRENT_BINARY_DIR}/csml)
endif()

# dma model library (always built)
add_library(dma_model STATIC ${MODEL_SRCS})
target_include_directories(dma_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/model/inc)
target_link_libraries(dma_model PUBLIC csml_logger SystemC::systemc Threads::Threads OpenSSL::Crypto ${CCI_LIBRARIES})
target_link_libraries(dma_model PUBLIC vp::sysc_backend)

# dma testbench executable (only if BUILD_TESTS=ON)
if(BUILD_TESTS)
  add_executable(dma_testbench
    test/src/testbench.cpp
    test/src/test_dma_func_001.cpp
    test/src/test_dma_func_002.cpp
    test/src/test_dma_func_003.cpp
    test/src/test_dma_func_004.cpp
    test/src/test_dma_func_005.cpp
    test/src/test_dma_func_006.cpp
    test/src/test_dma_func_007.cpp
    test/src/test_dma_func_008.cpp
    test/src/test_dma_func_009.cpp
    test/src/test_dma_func_010.cpp
    test/src/test_dma_func_011.cpp
    test/src/test_dma_func_012.cpp
    ${TEST_BASE_SRCS}
  )
  target_link_libraries(dma_testbench PRIVATE dma_model)

  # -----------------------------------------------------------------------------
  # Coverage target
  # -----------------------------------------------------------------------------
  if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    # Keep only model/ directory in coverage, exclude everything else
    add_custom_target(coverage
      COMMAND ./coverage/dma_testbench
      COMMAND lcov --capture --directory . --output-file coverage/coverage.info
      COMMAND lcov --extract coverage/coverage.info '${CMAKE_CURRENT_SOURCE_DIR}/model/*' --output-file=coverage/coverage_filtered.info
      COMMAND genhtml coverage/coverage_filtered.info --output-directory coverage/html
      COMMAND ${CMAKE_COMMAND} -E echo "Coverage report at coverage/html/index.html"
    )
    add_dependencies(coverage dma_testbench)
  endif()

  enable_testing()
  add_test(NAME dma_test COMMAND dma_testbench)
endif()

# -----------------------------------------------------------------------------
# Print status information
# -----------------------------------------------------------------------------
message(STATUS "Generated Project: dma")