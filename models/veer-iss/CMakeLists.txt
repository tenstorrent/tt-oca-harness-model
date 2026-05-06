cmake_minimum_required(VERSION 3.14)

project(riscv LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")

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
message(STATUS "RISCV Build Type: ${CMAKE_BUILD_TYPE}")

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
else()
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 ${WFLAG}")
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

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
#List of all CPP sources needed for librvcore.a
file(GLOB_RECURSE VEERISS_MODEL_SRCS 
	"VeeR-ISS/IntRegs.cpp"
	"VeeR-ISS/CsRegs.cpp"
	"VeeR-ISS/FpRegs.cpp"
	"VeeR-ISS/instforms.cpp"
	"VeeR-ISS/Memory.cpp"
	"VeeR-ISS/Hart.cpp"
	"VeeR-ISS/InstEntry.cpp"
	"VeeR-ISS/Triggers.cpp"
	"VeeR-ISS/PerfRegs.cpp"
	"VeeR-ISS/gdb.cpp"
	"VeeR-ISS/HartConfig.cpp"
	"VeeR-ISS/Server.cpp"
	"VeeR-ISS/Interactive.cpp"
	"VeeR-ISS/decode.cpp"
	"VeeR-ISS/disas.cpp"
	"VeeR-ISS/Syscall.cpp"
	"VeeR-ISS/PmaManager.cpp"
	"VeeR-ISS/DecodedInst.cpp"
	"VeeR-ISS/snapshot.cpp"
	"VeeR-ISS/PmpManager.cpp"
	"VeeR-ISS/VirtMem.cpp"
	"VeeR-ISS/Core.cpp"
	"VeeR-ISS/System.cpp"
	"VeeR-ISS/Cache.cpp"
	"VeeR-ISS/Tlb.cpp"
	"VeeR-ISS/VecRegs.cpp"
	"VeeR-ISS/vector.cpp"
	"VeeR-ISS/wideint.cpp"
	"VeeR-ISS/float.cpp"
	"VeeR-ISS/bitmanip.cpp"
	"VeeR-ISS/amo.cpp"
	"VeeR-ISS/SparseMem.cpp"
	"VeeR-ISS/InstProfile.cpp"
	"VeeR-ISSTlm/model/src/VeeR-ISSTlm.cpp"
	)

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
add_library(veeriss_model STATIC ${VEERISS_MODEL_SRCS})

# -----------------------------------------------------------------------------
# Paths & Includes
# -----------------------------------------------------------------------------
target_include_directories(veeriss_model
  PRIVATE
	VeeR-ISS
	VeeR-ISS/softfloat/source/include
	VeeR-ISSTlm/model/inc
	${CMAKE_CURRENT_SOURCE_DIR}/../utils/csml/inc
	)
if(NOT VP_SYSC_BACKEND STREQUAL "synopsys")
  target_include_directories(veeriss_model PRIVATE ${SYSTEMC_INC})
endif()
# -----------------------------------------------------------------------------
# External Dependencies (SystemC and CCI)
# -----------------------------------------------------------------------------
include(FindCCI)

# Add CCI include directory if found
if(CCI_FOUND)
  include_directories(${CCI_INCLUDE_DIR})
endif()

target_compile_options(veeriss_model PRIVATE
  -MMD
  -MP
  -mtune=native
  -Wall
  -Wextra
  -fPIC
)

target_compile_definitions(veeriss_model PUBLIC
  SOFT_FLOAT
  MEM_CALLBACKS
  ISA=rv32imc
)

target_link_libraries(veeriss_model PRIVATE softfloat PUBLIC systemc pthread z pthread stdc++fs)
target_link_libraries(veeriss_model PUBLIC vp::sysc_backend)
# -----------------------------------------------------------------------------
# Print status information
# -----------------------------------------------------------------------------
message(STATUS "Generated Project: veeriss")
if(VP_SYSC_BACKEND STREQUAL "synopsys")
  message(STATUS "  - SystemC: Synopsys Virtualizer (SystemC::systemc / SNPS_VP_ROOT)")
else()
  message(STATUS "  - SYSTEMC_HOME: ${SYSTEMC_HOME}")
endif()
