cmake_minimum_required(VERSION 3.14)

project(sep_memory LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")

include(VpSyscBackendFallback)
include(SystemCBackend)

include(FindCCI)

if(CCI_FOUND)
  include_directories(${CCI_INCLUDE_DIR})
endif()

set(SRAM_SRCS
    model/src/sep_memory.cpp
)

set(SRAM_ROM_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utils  # Shared paged memory from models/utils/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../riscv-vp-plusplus/vp/src/platform/common
)

if(NOT TARGET csml_logger)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../utils/csml ${CMAKE_CURRENT_BINARY_DIR}/csml)
endif()

add_library(sep_memory_model STATIC ${SRAM_SRCS})

target_include_directories(sep_memory_model PUBLIC ${SRAM_ROM_INCLUDES})

target_link_libraries(sep_memory_model PUBLIC csml_logger SystemC::systemc)
target_link_libraries(sep_memory_model PUBLIC vp::sysc_backend)
if(CCI_FOUND)
  target_link_libraries(sep_memory_model PUBLIC ${CCI_LIBRARIES})
endif()
