cmake_minimum_required(VERSION 3.14)

project(aes LANGUAGES CXX)

# -----------------------------------------------------------------------------
# CMake Module Path - Add shared cmake modules from repo root
# -----------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")

include(VpSyscBackendFallback)
include(SystemCBackend)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  if(NOT DEFINED BUILD_TESTS)
    option(BUILD_TESTS "Build AES test executables" ON)
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
message(STATUS "aes Build Type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
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
  message(STATUS "aes: Debug build - full logging enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 ${WFLAG}")
  add_compile_definitions(NDEBUG CSML_DEFAULT_VERBOSITY=0)
  message(STATUS "aes: Release build - errors only, optimized")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g -O0 -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=2)
  message(STATUS "aes: ASAN build - AddressSanitizer enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g --coverage -fprofile-arcs -ftest-coverage ${WFLAG}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage -lgcov")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=1 __COVERAGE__)
  message(STATUS "aes: Coverage build enabled")
endif()

# -----------------------------------------------------------------------------
# SystemC Detection (Accellera install tree only; Synopsys uses SystemC::systemc / SNPS_VP_ROOT)
# -----------------------------------------------------------------------------
if(VP_SYSC_BACKEND STREQUAL "synopsys")
  set(SYSTEMC_INC "")
  set(SYSTEMC_LIB_DIR "")
else()
  if(NOT DEFINED SYSTEMC_HOME)
      if(DEFINED ENV{SYSTEMC_HOME})
          set(SYSTEMC_HOME $ENV{SYSTEMC_HOME})
      else()
          set(SYSTEMC_HOME "/usr/local/systemc300")
      endif()
  endif()
  if(EXISTS "${SYSTEMC_HOME}/include")
      set(SYSTEMC_INC "${SYSTEMC_HOME}/include")
      if(EXISTS "${SYSTEMC_HOME}/lib-linux64")
          set(SYSTEMC_LIB_DIR "${SYSTEMC_HOME}/lib-linux64")
      else()
          set(SYSTEMC_LIB_DIR "${SYSTEMC_HOME}/lib")
      endif()
  else()
      message(FATAL_ERROR "SystemC NOT found. Please set SYSTEMC_HOME.")
  endif()
endif()

# Find threading library (required by SystemC)
find_package(Threads REQUIRED)

# Find OpenSSL library (required by AES model)
find_package(OpenSSL REQUIRED)

if(OpenSSL_FOUND)
  message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")
  message(STATUS "  Include: ${OPENSSL_INCLUDE_DIR}")
  message(STATUS "  Libraries: ${OPENSSL_LIBRARIES}")
else()
  message(FATAL_ERROR "OpenSSL not found. Install libssl-dev (Ubuntu/Debian) or openssl-devel (RedHat/CentOS)")
endif()

# -----------------------------------------------------------------------------
# External Dependencies (CCI)
# -----------------------------------------------------------------------------
include(FindCCI)

# Add CCI include directory if found
if(CCI_FOUND)
  include_directories(${CCI_INCLUDE_DIR})
endif()

set(CCI_LIB_DIR "")
if(CCI_FOUND AND CCI_LIBRARIES)
  list(GET CCI_LIBRARIES 0 CCI_FIRST_LIB)
  get_filename_component(CCI_LIB_DIR "${CCI_FIRST_LIB}" DIRECTORY)
endif()

# -----------------------------------------------------------------------------
# Paths & Includes
# -----------------------------------------------------------------------------
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/model/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/test/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
)
if(NOT VP_SYSC_BACKEND STREQUAL "synopsys")
  include_directories(${SYSTEMC_INC})
endif()

# Add shared CSML logger library (only if not already built)
if(NOT TARGET csml_logger)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml ${CMAKE_CURRENT_BINARY_DIR}/csml)
endif()

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
file(GLOB_RECURSE MODEL_SRCS "model/src/*.cpp")
file(GLOB_RECURSE TEST_SRCS  "test/src/*.cpp")

# -----------------------------------------------------------------------------
# Link Directories (must be set before targets)
# -----------------------------------------------------------------------------
if(NOT VP_SYSC_BACKEND STREQUAL "synopsys")
  link_directories(${SYSTEMC_LIB_DIR})
endif()

# -----------------------------------------------------------------------------
# Targets
# -----------------------------------------------------------------------------
# Model library
add_library(aes_model STATIC ${MODEL_SRCS})
target_link_libraries(aes_model PUBLIC csml_logger systemc Threads::Threads OpenSSL::SSL OpenSSL::Crypto)
if(CCI_FOUND)
  target_link_libraries(aes_model PUBLIC ${CCI_LIBRARIES})
endif()
target_link_libraries(aes_model PUBLIC vp::sysc_backend)


if(BUILD_TESTS)
  # Testbench executable
  add_executable(aes_testbench ${TEST_SRCS})
  target_link_libraries(aes_testbench PRIVATE aes_model csml_logger systemc)

  # Set RPATH
  if(CCI_LIB_DIR)
    set_target_properties(aes_testbench PROPERTIES 
        INSTALL_RPATH "${SYSTEMC_LIB_DIR};${CCI_LIB_DIR}"
        BUILD_RPATH "${SYSTEMC_LIB_DIR};${CCI_LIB_DIR}"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
  else()
    set_target_properties(aes_testbench PROPERTIES 
        INSTALL_RPATH "${SYSTEMC_LIB_DIR}"
        BUILD_RPATH "${SYSTEMC_LIB_DIR}"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
  endif()
endif()

# -----------------------------------------------------------------------------
# Print status information
# -----------------------------------------------------------------------------
message(STATUS "Generated Project: aes")
if(VP_SYSC_BACKEND STREQUAL "synopsys")
  message(STATUS "  - SystemC: Synopsys Virtualizer (SystemC::systemc / SNPS_VP_ROOT)")
else()
  message(STATUS "  - SYSTEMC_HOME: ${SYSTEMC_HOME}")
endif()

# -----------------------------------------------------------------------------
# Coverage Target
# -----------------------------------------------------------------------------
if(BUILD_TESTS AND CMAKE_BUILD_TYPE STREQUAL "Coverage")
  add_custom_target(coverage
    COMMAND ./coverage/aes_testbench
    COMMAND lcov --capture --directory . --output-file coverage/coverage.info
    COMMAND lcov --extract coverage/coverage.info '${CMAKE_CURRENT_SOURCE_DIR}/model/*' --output-file=coverage/coverage_filtered.info
    COMMAND genhtml coverage/coverage_filtered.info --output-directory coverage/html
    COMMAND ${CMAKE_COMMAND} -E echo "Coverage report at coverage/html/index.html"
  )
  add_dependencies(coverage aes_testbench)
endif()