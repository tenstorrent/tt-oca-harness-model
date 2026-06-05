cmake_minimum_required(VERSION 3.14)

project(riscv LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")


# -----------------------------------------------------------------------------
# Definitions
# -----------------------------------------------------------------------------
add_compile_definitions(SC_ALLOW_DEPRECATED_IEEE_API)

# -----------------------------------------------------------------------------
# Build type & output dirs
# -----------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
message(STATUS "RISCV Build Type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# -----------------------------------------------------------------------------
# Compiler settings (CMAKE_CXX_STANDARD from top-level vp / configure_vp.sh)
# -----------------------------------------------------------------------------
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
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
# -----------------------------------------------------------------------------
# External Dependencies (SystemC and CCI)
# -----------------------------------------------------------------------------
include(FindCCI)
include(FindSystemC)

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

target_link_libraries(veeriss_model PRIVATE softfloat PUBLIC SystemC::systemc pthread z pthread stdc++fs)
# -----------------------------------------------------------------------------
# Print status information
# -----------------------------------------------------------------------------
message(STATUS "Generated Project: veeriss")
  message(STATUS "  - SYSTEMC_HOME: ${SYSTEMC_HOME}")