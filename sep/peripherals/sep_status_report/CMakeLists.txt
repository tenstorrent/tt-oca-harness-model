cmake_minimum_required(VERSION 3.14)

project(sep_status_report LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")

# Inherit the C++ standard from the parent VP build (which may select 17 or 20). Only
# default it when this component is configured standalone. Forcing a fixed standard here
# would mismatch the SystemC API-version guard (sc_api_version_*_cxxNNNNNNL) against the
# SystemC library the VP links, causing an undefined-reference link error.
if(NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wno-unused-parameter)
add_compile_definitions(SC_ALLOW_DEPRECATED_IEEE_API)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

include(FindCCI)
include(FindSystemC)
if(CCI_FOUND)
  include_directories(${CCI_INCLUDE_DIR})
endif()

if(NOT TARGET csml_logger)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml ${CMAKE_CURRENT_BINARY_DIR}/csml)
endif()

set(MODEL_SRCS
    src/sep_status_report.cpp
)
set(MODEL_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
)

add_library(sep_status_report_model STATIC ${MODEL_SRCS})
target_include_directories(sep_status_report_model PUBLIC ${MODEL_INCLUDES})
target_link_libraries(sep_status_report_model PUBLIC csml_logger SystemC::systemc)
if(CCI_FOUND)
  target_link_libraries(sep_status_report_model PUBLIC ${CCI_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Tests (self-checking, registered with CTest)
# -----------------------------------------------------------------------------
if(NOT DEFINED BUILD_TESTS)
  option(BUILD_TESTS "Build sep_status_report test executables" ON)
endif()

if(BUILD_TESTS)
  enable_testing()
  # The decode + TSV-parse logic is pure C++ (no SystemC), so its test builds and runs
  # without the SystemC/CCI stack — it is the correctness oracle for the whole feature.
  add_executable(sep_status_decoder_test test/src/sep_status_decoder_test.cpp)
  target_include_directories(sep_status_decoder_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  add_test(NAME sep_status_decoder_test COMMAND sep_status_decoder_test)
  set_tests_properties(sep_status_decoder_test PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "TESTS FAILED")
endif()
