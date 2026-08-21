cmake_minimum_required(VERSION 3.14)

project(adams_bridge LANGUAGES C CXX)
enable_language(C)

# -----------------------------------------------------------------------------
# CMake Module Path - Add shared cmake modules from repo root
# -----------------------------------------------------------------------------
# Support both standalone and integrated builds
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")

include(PeripheralCommon)

# Doxygen documentation
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOC_INPUT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/README.md ${CMAKE_CURRENT_SOURCE_DIR}/docs")
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
        add_custom_target(adams_bridge_docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
    endif()
else()
    message(WARNING "Doxygen not found - documentation target will not be available")
endif()

add_compile_definitions(SC_ALLOW_DEPRECATED_IEEE_API)

# -----------------------------------------------------------------------------
# Build type & output dirs
# -----------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
message(STATUS "Adams Bridge Build Type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# -----------------------------------------------------------------------------
# Compiler settings
# -----------------------------------------------------------------------------
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/wd4100)  # Unreferenced formal parameter
else()
    add_compile_options(-Wno-unused-parameter)
endif()

set(WFLAG "-Wall -Wextra")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g ${WFLAG}")
  add_compile_definitions(DEBUG CSML_DEFAULT_VERBOSITY=3)
  message(STATUS "Adams Bridge: Debug build - full logging enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 ${WFLAG}")
  add_compile_definitions(NDEBUG CSML_DEFAULT_VERBOSITY=1)
  message(STATUS "Adams Bridge: Release build - errors only, optimized")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=1)
  message(STATUS "Adams Bridge: ASAN build - AddressSanitizer enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g --coverage -fprofile-arcs -ftest-coverage")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WFLAG} -g --coverage -fprofile-arcs -ftest-coverage")
  include(PeripheralCoverage)
  peripheral_coverage_link_flags()
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=1 __COVERAGE__)
  message(STATUS "Adams Bridge: Coverage build enabled")
endif()

# -----------------------------------------------------------------------------
# Paths & Includes
# -----------------------------------------------------------------------------
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/test/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
)

# -----------------------------------------------------------------------------
# External Dependencies (SystemC and CCI)
# -----------------------------------------------------------------------------
include(FindCCI)
include(FindSystemC)

if(CCI_FOUND)
  include_directories(SYSTEM ${CCI_INCLUDE_DIR})
endif()

# -----------------------------------------------------------------------------
# OpenSSL (SHAKE256 for the default crypto backend; available in 1.1.1 and 3.x)
# -----------------------------------------------------------------------------
include(PeripheralOpenSSL)
peripheral_find_openssl()
if(OPENSSL_FOUND)
  message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")
  message(STATUS "  Include: ${OPENSSL_INCLUDE_DIR}")
  message(STATUS "  Libraries: ${OPENSSL_LIBRARIES}")
else()
  message(FATAL_ERROR "OpenSSL not found. Adams Bridge requires OpenSSL for SHAKE256.")
endif()

# Apply sanitizer flags after FindThreads (FindSystemC). Enabling C for PQClean
# makes that check a C try_compile; asan in CMAKE_EXE_LINKER_FLAGS before it
# would link an unsanitized C probe with the sanitizer runtime and fail.
if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
  set(_ABR_ASAN_FLAGS "-g -O0 -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} ${_ABR_ASAN_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
endif()

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
set(MODEL_SRCS
  src/adams_bridge.cpp
  src/abr_base.cpp
  src/abr_crypto.cpp
)

set(PQCLEAN_COMMON_SRCS
  crypto/pqclean/common/fips202.c
  src/abr_randombytes.c
)
set(PQCLEAN_MLDSA_SRCS
  crypto/pqclean/ml-dsa-87/ntt.c
  crypto/pqclean/ml-dsa-87/packing.c
  crypto/pqclean/ml-dsa-87/poly.c
  crypto/pqclean/ml-dsa-87/polyvec.c
  crypto/pqclean/ml-dsa-87/reduce.c
  crypto/pqclean/ml-dsa-87/rounding.c
  crypto/pqclean/ml-dsa-87/sign.c
  crypto/pqclean/ml-dsa-87/symmetric-shake.c
  src/abr_pqc_mldsa.c
)
set(PQCLEAN_MLKEM_SRCS
  crypto/pqclean/ml-kem-1024/cbd.c
  crypto/pqclean/ml-kem-1024/indcpa.c
  crypto/pqclean/ml-kem-1024/kem.c
  crypto/pqclean/ml-kem-1024/ntt.c
  crypto/pqclean/ml-kem-1024/poly.c
  crypto/pqclean/ml-kem-1024/polyvec.c
  crypto/pqclean/ml-kem-1024/reduce.c
  crypto/pqclean/ml-kem-1024/symmetric-shake.c
  crypto/pqclean/ml-kem-1024/verify.c
  src/abr_pqc_mlkem.c
)

add_library(abr_pqclean_common OBJECT ${PQCLEAN_COMMON_SRCS})
target_include_directories(abr_pqclean_common PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pqclean/common
)

add_library(abr_pqclean_mldsa OBJECT ${PQCLEAN_MLDSA_SRCS})
target_include_directories(abr_pqclean_mldsa PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pqclean/common
  ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pqclean/ml-dsa-87
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(abr_pqclean_mlkem OBJECT ${PQCLEAN_MLKEM_SRCS})
target_include_directories(abr_pqclean_mlkem PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pqclean/common
  ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pqclean/ml-kem-1024
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Vendored C is not held to the model's warning bar.
foreach(_abr_pqc abr_pqclean_common abr_pqclean_mldsa abr_pqclean_mlkem)
  target_compile_options(${_abr_pqc} PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Wno-unused-function -Wno-unused-parameter>
  )
  if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
    target_compile_options(${_abr_pqc} PRIVATE
      -g -O0 -fsanitize=address -fsanitize=undefined
      -fno-sanitize-recover=all -fsanitize=float-divide-by-zero
      -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment
    )
    target_link_options(${_abr_pqc} PRIVATE -fsanitize=address -fsanitize=undefined)
  endif()
endforeach()

set(TEST_SRCS
  test/src/testbench.cpp
  test/src/register_tests.cpp
  test/src/mldsa_tests.cpp
  test/src/mlkem_tests.cpp
  test/src/interrupt_kv_tests.cpp
  test/src/nist_kat_tests.cpp
)

# -----------------------------------------------------------------------------
# Libraries and Executables
# -----------------------------------------------------------------------------
# Context-aware BUILD_TESTS option
# Default ON for standalone builds, OFF when included as subproject (VP integration)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if(NOT DEFINED BUILD_TESTS)
      option(BUILD_TESTS "Build Adams Bridge test executables" ON)
    endif()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml ${CMAKE_CURRENT_BINARY_DIR}/csml)
endif()

peripheral_add_model_library(adams_bridge_model ${MODEL_SRCS})
target_include_directories(adams_bridge_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_sources(adams_bridge_model PRIVATE
  $<TARGET_OBJECTS:abr_pqclean_common>
  $<TARGET_OBJECTS:abr_pqclean_mldsa>
  $<TARGET_OBJECTS:abr_pqclean_mlkem>
)
add_dependencies(adams_bridge_model
  abr_pqclean_common abr_pqclean_mldsa abr_pqclean_mlkem
)

target_link_libraries(adams_bridge_model PUBLIC
  csml_logger
  SystemC::systemc
  Threads::Threads
  OpenSSL::SSL
  OpenSSL::Crypto
  ${CCI_LIBRARIES}
)

if(BUILD_TESTS)
  add_executable(adams_bridge_test ${TEST_SRCS})
  peripheral_link_test_to_model(adams_bridge_test adams_bridge_model)
  target_include_directories(adams_bridge_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../sw/sep-vp-tests/fw-tests-from-tt-oca-hw/fw/sep/tests/common
  )

  if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    peripheral_add_coverage_target(PRIMARY_TARGET adams_bridge_test)
  endif()

  enable_testing()
  add_test(NAME adams_bridge_test COMMAND adams_bridge_test)
endif()

# -----------------------------------------------------------------------------
# Static Analysis Target (cppcheck)
# -----------------------------------------------------------------------------
find_program(CPPCHECK_EXECUTABLE cppcheck)
if(CPPCHECK_EXECUTABLE)
  add_custom_target(adams_bridge_cppcheck
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
    COMMENT "Running cppcheck static analysis on Adams Bridge model..."
  )
  message(STATUS "cppcheck found - 'make adams_bridge_cppcheck' target available")
else()
  message(STATUS "cppcheck not found - static analysis target not available")
endif()
