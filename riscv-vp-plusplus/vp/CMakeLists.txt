cmake_minimum_required(VERSION 3.24)

# Module path + backend must be known before project() so Synopsys can set CMAKE_CXX_COMPILER
# (tenstorrent_sep/Makefile: CXX := scc +cxx .../common/bin/g++)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")

# Same flow as vayavyalabs Makefiles: Accellera (SystemC + CCI) or Synopsys Virtualizer (SCML / Snps VP).
set(VP_SYSC_BACKEND "accellera" CACHE STRING "SystemC backend: accellera | synopsys")
set_property(CACHE VP_SYSC_BACKEND PROPERTY STRINGS accellera synopsys)

if(VP_SYSC_BACKEND STREQUAL "synopsys")
  include(SynopsysVirtualizerCompiler)
endif()

project(riscv-vp
	VERSION 1.2.2
	LANGUAGES CXX
)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "-g3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR})

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

find_package(Boost REQUIRED COMPONENTS iostreams program_options log)

add_library(vp::sysc_backend INTERFACE IMPORTED GLOBAL)

if(VP_SYSC_BACKEND STREQUAL "accellera")
  set(ACCELLERA_CCI_STD 1 CACHE BOOL "Build with Accellera CCI" FORCE)
  set_property(TARGET vp::sysc_backend PROPERTY INTERFACE_COMPILE_DEFINITIONS
    ACCELLERA_CCI_STD
    USE_CCI_CONFIG
  )

elseif(VP_SYSC_BACKEND STREQUAL "synopsys")
  set(ACCELLERA_CCI_STD 0 CACHE BOOL "Build with Accellera CCI" FORCE)
  include(SynopsysVirtualizer)
  set_property(TARGET vp::sysc_backend PROPERTY INTERFACE_LINK_LIBRARIES Snps::VirtualizerRuntime)
  # When -DOCH_SEP_SS_SNPS_ROOT is set, cmake/OchSepSnps.cmake (from src/platform/sep) also builds
  # libOCH_SEP_SS_SNPS.so (Makefile MODEL_LIB) and sep-vp (Makefile TARGET).

else()
  message(FATAL_ERROR "Invalid VP_SYSC_BACKEND='${VP_SYSC_BACKEND}'. Use accellera or synopsys.")
endif()

subdirs(src)
