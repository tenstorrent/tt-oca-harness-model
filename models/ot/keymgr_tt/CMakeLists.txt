cmake_minimum_required(VERSION 3.14)

project(keymgr_tt LANGUAGES CXX)

# -----------------------------------------------------------------------------
# CMake Module Path
# -----------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")

# -----------------------------------------------------------------------------
# Build type & output dirs
# -----------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()
message(STATUS "KeyMgr_TT Build Type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG    "${CMAKE_BINARY_DIR}/debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE  "${CMAKE_BINARY_DIR}/release")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_ASAN     "${CMAKE_BINARY_DIR}/asan")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_COVERAGE "${CMAKE_BINARY_DIR}/coverage")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG    "${CMAKE_BINARY_DIR}/debug")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE  "${CMAKE_BINARY_DIR}/release")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_ASAN     "${CMAKE_BINARY_DIR}/asan")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_COVERAGE "${CMAKE_BINARY_DIR}/coverage")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# -----------------------------------------------------------------------------
# Compiler settings
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wno-unused-parameter)
set(WFLAG "-Wall -Wextra")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g ${WFLAG}")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=3)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 ${WFLAG}")
  add_compile_definitions(NDEBUG CSML_DEFAULT_VERBOSITY=1)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g --coverage -fprofile-arcs -ftest-coverage")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=1 __COVERAGE__)
endif()

add_compile_definitions(USE_CSML_REG_LIB SC_ALLOW_DEPRECATED_IEEE_API)

# -----------------------------------------------------------------------------
# External Dependencies
# -----------------------------------------------------------------------------
include(FindSystemC)
include(FindCCI)

# OpenSSL (required for RAND_bytes DRBG fallback in keymgr_tt.cpp)
find_package(OpenSSL REQUIRED)
if(OpenSSL_FOUND)
  message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")
else()
  message(FATAL_ERROR "OpenSSL not found. Install libssl-dev (Ubuntu/Debian) or openssl-devel (RedHat/CentOS)")
endif()

if(CCI_FOUND)
  include_directories(${CCI_INCLUDE_DIR})
endif()

# -----------------------------------------------------------------------------
# Includes
# -----------------------------------------------------------------------------
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/model/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/test/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
)

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
set(MODEL_SRCS
  model/src/keymgr_tt_base.cpp
  model/src/keymgr_tt.cpp
  model/src/km_kpv.cpp
  model/src/km_mailbox.cpp
  model/src/km_firmware_handler.cpp
)

set(TEST_SRCS
  test/src/keymgr_tt_test.cpp
  test/src/keymgr_tt_func001_test.cpp
  test/src/keymgr_tt_func002_test.cpp
  test/src/keymgr_tt_func003_test.cpp
  test/src/keymgr_tt_func004_test.cpp
  test/src/keymgr_tt_func005_test.cpp
  test/src/keymgr_tt_func006_test.cpp
  test/src/keymgr_tt_func007_test.cpp
  test/src/keymgr_tt_func008_test.cpp
  test/src/keymgr_tt_func009_test.cpp
  test/src/keymgr_tt_func010_test.cpp
  test/src/keymgr_tt_func011_test.cpp
  test/src/keymgr_tt_func012_test.cpp
  test/src/keymgr_tt_func013_test.cpp
  test/src/testbench.cpp
)

# -----------------------------------------------------------------------------
# Build targets
# -----------------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  option(BUILD_TESTS "Build keymgr_tt test executable" ON)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml ${CMAKE_CURRENT_BINARY_DIR}/csml)
else()
  option(BUILD_TESTS "Build keymgr_tt test executable" OFF)
endif()

add_library(keymgr_tt_model STATIC ${MODEL_SRCS})
target_include_directories(keymgr_tt_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/model/inc)
target_link_libraries(keymgr_tt_model PUBLIC csml_logger SystemC::systemc Threads::Threads OpenSSL::Crypto)
if(CCI_FOUND)
  target_link_libraries(keymgr_tt_model PUBLIC ${CCI_LIBRARIES})
endif()

if(BUILD_TESTS)
  add_executable(keymgr_tt_testbench ${TEST_SRCS})
  target_link_libraries(keymgr_tt_testbench PRIVATE keymgr_tt_model)
  if(CCI_FOUND)
    target_link_libraries(keymgr_tt_testbench PRIVATE ${CCI_LIBRARIES})
  endif()

  if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    add_custom_target(coverage
      COMMAND ./coverage/keymgr_tt_testbench
      COMMAND lcov --capture --directory . --output-file coverage/coverage.info
      COMMAND lcov --extract coverage/coverage.info '${CMAKE_CURRENT_SOURCE_DIR}/model/*' --output-file coverage/coverage_filtered.info
      COMMAND genhtml coverage/coverage_filtered.info --output-directory coverage/html
      COMMAND ${CMAKE_COMMAND} -E echo "Coverage report: coverage/html/index.html"
    )
    add_dependencies(coverage keymgr_tt_testbench)
  endif()

  enable_testing()
  add_test(NAME keymgr_tt_testbench COMMAND keymgr_tt_testbench)
endif()

# -----------------------------------------------------------------------------
# Status
# -----------------------------------------------------------------------------
message(STATUS "Project: keymgr_tt")
message(STATUS "  SystemC found : ${SystemC_FOUND}")
message(STATUS "  CCI found     : ${CCI_FOUND}")
message(STATUS "  BUILD_TESTS   : ${BUILD_TESTS}")
