cmake_minimum_required(VERSION 3.14)

project(kmac LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")

include(VpSyscBackendFallback)
include(SystemCBackend)

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
message(STATUS "kmac Build Type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

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
  message(STATUS "kmac: Debug build - full logging enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 ${WFLAG}")
  add_compile_definitions(NDEBUG CSML_DEFAULT_VERBOSITY=0)
  message(STATUS "kmac: Release build - errors only, optimized")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g -O0 -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=2)
  message(STATUS "kmac: ASAN build - AddressSanitizer enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g --coverage -fprofile-arcs -ftest-coverage")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=1 __COVERAGE__)
  message(STATUS "kmac: Coverage build enabled")
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

# Find OpenSSL library (required for cryptographic operations)
find_package(OpenSSL REQUIRED)

# -----------------------------------------------------------------------------
# External Dependencies (CCI)
# -----------------------------------------------------------------------------
include(FindCCI)

if(CCI_FOUND)
  include_directories(${CCI_INCLUDE_DIR})
endif()

# -----------------------------------------------------------------------------
# Paths & Includes
# -----------------------------------------------------------------------------
include_directories(
  model/inc
  test/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
)
if(NOT VP_SYSC_BACKEND STREQUAL "synopsys")
  include_directories(${SYSTEMC_INC})
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
# Context-aware BUILD_TESTS option
# -----------------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(BUILD_TESTS "Build KMAC test executables" ON)
else()
    # Built as subdirectory (VP integration) - skip tests by default
    if(NOT DEFINED BUILD_TESTS)
        option(BUILD_TESTS "Build KMAC test executables" OFF)
    endif()
endif()
# -----------------------------------------------------------------------------
# Targets
# -----------------------------------------------------------------------------
# Model library (always built)
add_library(kmac_model STATIC ${MODEL_SRCS})
target_include_directories(kmac_model PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/model/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
)
target_link_libraries(kmac_model PUBLIC systemc Threads::Threads OpenSSL::Crypto)
if(CCI_FOUND)
  target_link_libraries(kmac_model PUBLIC ${CCI_LIBRARIES})
endif()
target_link_libraries(kmac_model PUBLIC vp::sysc_backend)

# Testbench executable (only if BUILD_TESTS=ON)
if(BUILD_TESTS)
    # For standalone builds, we need to build csml_logger ourselves
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml ${CMAKE_CURRENT_BINARY_DIR}/csml)

    add_executable(kmac_testbench ${TEST_SRCS})
    target_link_libraries(kmac_testbench PRIVATE kmac_model systemc OpenSSL::Crypto csml_logger)

# Set RPATH
if(NOT VP_SYSC_BACKEND STREQUAL "synopsys")
  set(_rpath_dirs "${SYSTEMC_LIB_DIR}")
  if(CCI_FOUND AND CCI_CONFIG_LIBRARY)
    get_filename_component(_cci_lib_dir "${CCI_CONFIG_LIBRARY}" DIRECTORY)
    list(APPEND _rpath_dirs "${_cci_lib_dir}")
  endif()
  set_target_properties(kmac_testbench PROPERTIES
      INSTALL_RPATH "${_rpath_dirs}"
      BUILD_WITH_INSTALL_RPATH TRUE
  )
endif()

  # -----------------------------------------------------------------------------
  # Coverage target
  # -----------------------------------------------------------------------------
  if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    # Keep only model/ directory in coverage, exclude everything else
    add_custom_target(coverage
      COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/kmac_testbench ../config/accellera_config.ini
      COMMAND lcov --capture --directory . --output-file coverage.info
      COMMAND lcov --extract coverage.info '${CMAKE_CURRENT_SOURCE_DIR}/model/*' --output-file=coverage_filtered.info
      COMMAND genhtml coverage_filtered.info --output-directory coverage_html
      COMMAND ${CMAKE_COMMAND} -E echo "Coverage report at coverage_html/index.html"
    )
    add_dependencies(coverage kmac_testbench)
  endif()
endif()

# -----------------------------------------------------------------------------
# Print status information
# -----------------------------------------------------------------------------
message(STATUS "Generated Project: kmac")
if(VP_SYSC_BACKEND STREQUAL "synopsys")
  message(STATUS "  - SystemC: Synopsys Virtualizer (SystemC::systemc / SNPS_VP_ROOT)")
else()
  message(STATUS "  - SYSTEMC_HOME: ${SYSTEMC_HOME}")
endif()