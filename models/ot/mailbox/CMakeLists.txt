cmake_minimum_required(VERSION 3.14)

project(mailbox LANGUAGES CXX)

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
message(STATUS "mailbox Build Type: ${CMAKE_BUILD_TYPE}")

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
  message(STATUS "mailbox: Debug build - full logging enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 ${WFLAG}")
  add_compile_definitions(NDEBUG CSML_DEFAULT_VERBOSITY=0)
  message(STATUS "mailbox: Release build - errors only, optimized")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "ASAN")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g -O0 -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=2)
  message(STATUS "mailbox: ASAN build - AddressSanitizer enabled")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g --coverage -fprofile-arcs -ftest-coverage")
  add_compile_definitions(CSML_DEFAULT_VERBOSITY=1)
  message(STATUS "mailbox: Coverage build enabled")
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
  model/src/mailbox.cpp
  model/src/mailbox_base.cpp
)

file(GLOB TEST_SRCS "test/src/*.cpp")

# -----------------------------------------------------------------------------
# Libraries and Executables
# -----------------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if(NOT DEFINED BUILD_TESTS)
      option(BUILD_TESTS "Build mailbox test executables" ON)
    endif()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml ${CMAKE_CURRENT_BINARY_DIR}/csml)
endif()

# mailbox model library (always built)
add_library(mailbox_model STATIC ${MODEL_SRCS})
target_include_directories(mailbox_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/model/inc)
target_link_libraries(mailbox_model PUBLIC csml_logger SystemC::systemc Threads::Threads)
if(CCI_FOUND)
  target_link_libraries(mailbox_model PUBLIC ${CCI_LIBRARIES})
endif()
target_link_libraries(mailbox_model PUBLIC vp::sysc_backend)

# mailbox testbench executable (only if BUILD_TESTS=ON)
if(BUILD_TESTS)
  add_executable(mailbox_testbench ${TEST_SRCS})
  target_link_libraries(mailbox_testbench PRIVATE mailbox_model)

  # -----------------------------------------------------------------------------
  # CTest Integration
  # -----------------------------------------------------------------------------
  enable_testing()

  add_test(NAME func001_system_reset        COMMAND mailbox_testbench)
  add_test(NAME func002_register_access     COMMAND mailbox_testbench)
  add_test(NAME func003_fifo_data_transfer  COMMAND mailbox_testbench)
  add_test(NAME func004_fifo_status_monitoring COMMAND mailbox_testbench)
  add_test(NAME func005_error_detection     COMMAND mailbox_testbench)
  add_test(NAME func006_interrupt_system    COMMAND mailbox_testbench)

  foreach(t func001_system_reset func002_register_access func003_fifo_data_transfer
             func004_fifo_status_monitoring func005_error_detection)
    set_tests_properties(${t} PROPERTIES
      PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
      FAIL_REGULAR_EXPRESSION "SOME TESTS FAILED"
      TIMEOUT 60
    )
  endforeach()

  set_tests_properties(func006_interrupt_system PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[TEST PASSED\\] FUNC-006 TC060: test_config_interrupt_polarity"
    FAIL_REGULAR_EXPRESSION "\\[TEST FAILED\\] FUNC-006"
    TIMEOUT 60
  )
endif()

# -----------------------------------------------------------------------------
# Coverage Target
# -----------------------------------------------------------------------------
if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  add_custom_target(coverage
    COMMAND ./coverage/mailbox_testbench
    COMMAND lcov --capture --directory . --output-file coverage/coverage.info
    COMMAND lcov --extract coverage/coverage.info '${CMAKE_CURRENT_SOURCE_DIR}/model/*' --output-file=coverage/coverage_filtered.info
    COMMAND genhtml coverage/coverage_filtered.info --output-directory coverage/html
    COMMAND ${CMAKE_COMMAND} -E echo "Coverage report at coverage/html/index.html"
  )
  add_dependencies(coverage mailbox_testbench)
endif()

# -----------------------------------------------------------------------------
# Print status information
# -----------------------------------------------------------------------------
message(STATUS "Generated Project: mailbox")