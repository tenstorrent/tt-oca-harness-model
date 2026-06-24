cmake_minimum_required(VERSION 3.14)

project(sep_virt_console LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wno-unused-parameter)
add_compile_definitions(SC_ALLOW_DEPRECATED_IEEE_API)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Doxygen documentation (optional)
find_package(Doxygen)
if(DOXYGEN_FOUND AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(DOC_INPUT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    set(DOC_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(sep_virt_console_docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen" VERBATIM)
endif()

include(FindCCI)
include(FindSystemC)
if(CCI_FOUND)
  include_directories(${CCI_INCLUDE_DIR})
endif()

if(NOT TARGET csml_logger)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml ${CMAKE_CURRENT_BINARY_DIR}/csml)
endif()

set(MODEL_SRCS
    src/sep_virt_console.cpp
)
set(MODEL_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
)

add_library(sep_virt_console_model STATIC ${MODEL_SRCS})
target_include_directories(sep_virt_console_model PUBLIC ${MODEL_INCLUDES})
target_link_libraries(sep_virt_console_model PUBLIC csml_logger SystemC::systemc)
if(CCI_FOUND)
  target_link_libraries(sep_virt_console_model PUBLIC ${CCI_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Tests (self-checking, registered with CTest)
# -----------------------------------------------------------------------------
if(NOT DEFINED BUILD_TESTS)
  option(BUILD_TESTS "Build sep_virt_console test executables" ON)
endif()

if(BUILD_TESTS)
  enable_testing()
  # The decode logic is pure C++ (no SystemC), so its test builds and runs without
  # the SystemC/CCI stack — it is the correctness oracle for the whole feature.
  add_executable(sep_virt_console_decoder_test test/src/virt_console_decoder_test.cpp)
  target_include_directories(sep_virt_console_decoder_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  add_test(NAME sep_virt_console_decoder_test COMMAND sep_virt_console_decoder_test)
  set_tests_properties(sep_virt_console_decoder_test PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "TESTS FAILED")
endif()
