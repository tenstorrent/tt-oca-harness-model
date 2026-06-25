cmake_minimum_required(VERSION 3.14)

project(key_manager LANGUAGES CXX)

# -----------------------------------------------------------------------------
# CMake Module Path
# -----------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")

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
        add_custom_target(key_manager_docs
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
# Build type & output dirs
# -----------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
message(STATUS "KeyMgr_TT Build Type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# -----------------------------------------------------------------------------
# Compiler settings
# -----------------------------------------------------------------------------
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
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
  add_compile_definitions(NDEBUG CSML_DEFAULT_VERBOSITY=0)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g -O0 -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=2)
  message(STATUS "key_manager: ASAN build - AddressSanitizer enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g --coverage -fprofile-arcs -ftest-coverage")
  include(PeripheralCoverage)
  peripheral_coverage_link_flags()
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=1 __COVERAGE__)
endif()

add_compile_definitions(SC_ALLOW_DEPRECATED_IEEE_API)

# -----------------------------------------------------------------------------
# External Dependencies
# -----------------------------------------------------------------------------
include(FindCCI)
include(FindSystemC)

# OpenSSL (required for RAND_bytes DRBG fallback in keymgr_tt.cpp)
include(PeripheralOpenSSL)
peripheral_find_openssl()
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
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/test/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
)

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
set(MODEL_SRCS
  src/key_manager_base.cpp
  src/key_manager.cpp
  src/km_kpv.cpp
  src/km_mailbox.cpp
  src/km_firmware_handler.cpp
)

set(TEST_SRCS
  test/src/key_manager_test.cpp
  test/src/key_manager_func001_test.cpp
  test/src/key_manager_func002_test.cpp
  test/src/key_manager_func003_test.cpp
  test/src/key_manager_func004_test.cpp
  test/src/key_manager_func005_test.cpp
  test/src/key_manager_func006_test.cpp
  test/src/key_manager_func007_test.cpp
  test/src/key_manager_func008_test.cpp
  test/src/key_manager_func009_test.cpp
  test/src/key_manager_func010_test.cpp
  test/src/key_manager_func011_test.cpp
  test/src/key_manager_func012_test.cpp
  test/src/key_manager_func013_test.cpp
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

add_library(key_manager_model STATIC ${MODEL_SRCS})
target_include_directories(key_manager_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(key_manager_model PUBLIC csml_logger SystemC::systemc Threads::Threads OpenSSL::Crypto)
if(CCI_FOUND)
  target_link_libraries(key_manager_model PUBLIC ${CCI_LIBRARIES})
endif()

if(BUILD_TESTS)
  add_executable(key_manager_testbench ${TEST_SRCS})
  target_link_libraries(key_manager_testbench PRIVATE key_manager_model)
  if(CCI_FOUND)
    target_link_libraries(key_manager_testbench PRIVATE ${CCI_LIBRARIES})
  endif()

  if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    add_custom_target(coverage
      COMMAND ./bin/key_manager_testbench
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
      COMMAND lcov --ignore-errors inconsistent,unsupported,format,mismatch --capture --directory . --output-file coverage/coverage.info
      COMMAND lcov --ignore-errors inconsistent,unsupported,format,mismatch --extract coverage/coverage.info '${CMAKE_CURRENT_SOURCE_DIR}/src/*' '${CMAKE_CURRENT_SOURCE_DIR}/include/*' --output-file coverage/coverage_filtered.info
      COMMAND genhtml --ignore-errors inconsistent,unsupported,format,corrupt,category coverage/coverage_filtered.info --output-directory coverage/html
      COMMAND ${CMAKE_COMMAND} -E echo "Coverage report: coverage/html/index.html"
    )
    add_dependencies(coverage key_manager_testbench)
  endif()

  enable_testing()
  add_test(NAME key_manager_testbench COMMAND key_manager_testbench)
endif()

# -----------------------------------------------------------------------------
# Status
# -----------------------------------------------------------------------------
message(STATUS "Project: keymgr_tt")
message(STATUS "  SystemC found : ${SystemC_FOUND}")
message(STATUS "  CCI found     : ${CCI_FOUND}")
message(STATUS "  BUILD_TESTS   : ${BUILD_TESTS}")

# -----------------------------------------------------------------------------
# Static Analysis Target (cppcheck)
# -----------------------------------------------------------------------------
find_program(CPPCHECK_EXECUTABLE cppcheck)
if(CPPCHECK_EXECUTABLE)
  add_custom_target(key_manager_cppcheck
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
    COMMENT "Running cppcheck static analysis on KeyMgr TT model..."
  )
  message(STATUS "cppcheck found - 'make key_manager_cppcheck' target available")
else()
  message(STATUS "cppcheck not found - static analysis target not available")
endif()
