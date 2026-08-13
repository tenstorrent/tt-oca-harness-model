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
  set(CMAKE_CXX_STANDARD 20)
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
if(NOT TARGET Boost::headers)
  find_package(Boost REQUIRED)
endif()

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
# Vendored VeeR-ISS sources for librvcore.a.  Listed separately from our own
# code so they can be built without -Wall/-Wextra: warnings in upstream's ISS
# are not ours to fix, and they drown out the ones that are.
file(GLOB_RECURSE VEERISS_ISS_SRCS
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
	)

# Our TLM wrapper around the ISS — built with the full warning set.
file(GLOB_RECURSE VEERISS_TLM_SRCS
	"VeeR-ISSTlm/model/src/VeeR-ISSTlm.cpp"
	)

set(VEERISS_MODEL_SRCS ${VEERISS_ISS_SRCS} ${VEERISS_TLM_SRCS})

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
	VeeR-ISSTlm/model/inc
	${CMAKE_CURRENT_SOURCE_DIR}/../utils/csml/inc
	${CMAKE_CURRENT_SOURCE_DIR}/../utils/tlm_extensions
	)

# The ISS and softfloat are vendored third party, so their headers go in as
# SYSTEM includes: -Wall -Wextra below applies to our code, not theirs.
target_include_directories(veeriss_model SYSTEM
  PRIVATE
	VeeR-ISS
	VeeR-ISS/softfloat/source/include
	)
# -----------------------------------------------------------------------------
# External Dependencies (SystemC and CCI)
# -----------------------------------------------------------------------------
include(FindCCI)
include(FindSystemC)

find_package(Boost REQUIRED COMPONENTS program_options)

# Add CCI include directory if found
if(CCI_FOUND)
  include_directories(SYSTEM ${CCI_INCLUDE_DIR})
endif()

target_compile_options(veeriss_model PRIVATE
  -MMD
  -MP
  -Wall
  -Wextra
  -fPIC
)

# The SYSTEM include above only quiets the ISS headers; its .cpp files are
# sources of this target, so silence them here.  Source options come after the
# target's on the command line, so -w wins over the -Wall -Wextra above.
set_source_files_properties(${VEERISS_ISS_SRCS} PROPERTIES COMPILE_OPTIONS "-w")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(veeriss_model PRIVATE -mtune=native)
endif()

target_compile_definitions(veeriss_model PUBLIC
  SOFT_FLOAT
  MEM_CALLBACKS
  ISA=rv32imc
)

target_link_libraries(veeriss_model PRIVATE softfloat Boost::headers Boost::program_options PUBLIC SystemC::systemc Threads::Threads z el2_pic_model)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_libraries(veeriss_model PRIVATE stdc++fs)
endif()
# -----------------------------------------------------------------------------
# Print status information
# -----------------------------------------------------------------------------
message(STATUS "Generated Project: veeriss")
  message(STATUS "  - SYSTEMC_HOME: ${SYSTEMC_HOME}")