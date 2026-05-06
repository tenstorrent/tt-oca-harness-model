cmake_minimum_required(VERSION 3.14)

project(avbbus LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")

add_compile_definitions(USE_CSML_REG_LIB SC_ALLOW_DEPRECATED_IEEE_API)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
message(STATUS "AVBbus Build Type: ${CMAKE_BUILD_TYPE}")

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
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 ${WFLAG}")
  add_compile_definitions(NDEBUG CSML_DEFAULT_VERBOSITY=1)
endif()

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------
include(FindSystemC)
include(FindCCI)

if(CCI_FOUND)
  include_directories(${CCI_INCLUDE_DIR})
endif()

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/model/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/test/inc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
)

# Default ON for standalone builds, OFF when included as subproject (VP integration)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(BUILD_TESTS "Build AVBbus test executables" ON)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml ${CMAKE_CURRENT_BINARY_DIR}/csml)
else()
    option(BUILD_TESTS "Build AVBbus test executables" OFF)
endif()

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
set(MODEL_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/model/src/AVBBus_base.cpp
)

# -----------------------------------------------------------------------------
# Library target
# -----------------------------------------------------------------------------
add_library(avbbus_model STATIC ${MODEL_SRCS})
target_include_directories(avbbus_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/model/inc)
target_link_libraries(avbbus_model PUBLIC csml_logger SystemC::systemc Threads::Threads ${CCI_LIBRARIES})

# -----------------------------------------------------------------------------
# Test executable
# -----------------------------------------------------------------------------
if(BUILD_TESTS)
  set(TEST_SRCS
      ${CMAKE_CURRENT_SOURCE_DIR}/test/src/AVBBus_basetest.cpp
  )
  add_executable(avbbus_test ${TEST_SRCS})
  target_link_libraries(avbbus_test PRIVATE avbbus_model)

  enable_testing()
  add_test(NAME avbbus_test COMMAND avbbus_test)
endif()

message(STATUS "Generated Project: AVBbus")
