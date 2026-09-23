# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
cmake_minimum_required(VERSION 3.14)

project(riscv LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH
  "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")
include(PeripheralCommon)

# Standalone `./run_tests.sh` builds this directory as the CMake source root.
# When we are a subdirectory of sep/ or vp/, those parents already add
# softfloat / el2_pic.
set(VEERISS_STANDALONE OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(VEERISS_STANDALONE ON)
  if(NOT DEFINED BUILD_TESTS)
    option(BUILD_TESTS "Build VeeR-ISS TLM standalone tests" ON)
  endif()
endif()

if(VEERISS_STANDALONE)
  find_package(Threads REQUIRED)
  include(FindCCI)
  include(FindSystemC)
  # el2_pic must not inherit BUILD_TESTS=ON — its test model stubs VeeR-ISSTlm.hpp.
  set(_veeriss_build_tests "${BUILD_TESTS}")
  set(BUILD_TESTS OFF)
  if(NOT TARGET softfloat)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/VeeR-ISS/softfloat
                     ${CMAKE_CURRENT_BINARY_DIR}/softfloat)
  endif()
  if(NOT TARGET el2_pic_model)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../peripherals/el2_pic
                     ${CMAKE_CURRENT_BINARY_DIR}/el2_pic)
  endif()
  set(BUILD_TESTS "${_veeriss_build_tests}")
endif()


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
elseif(CMAKE_BUILD_TYPE STREQUAL "ASAN")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g -O0 -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
elseif(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WFLAG} -g --coverage -fprofile-arcs -ftest-coverage")
  include(PeripheralCoverage)
  peripheral_coverage_link_flags()
  add_compile_definitions(__COVERAGE__)
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
  PUBLIC
	VeeR-ISSTlm/model/inc
	${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
	${CMAKE_CURRENT_SOURCE_DIR}/../utils/tlm_extensions
	${CMAKE_CURRENT_SOURCE_DIR}/../../../vp/platform/infra
	${CMAKE_CURRENT_SOURCE_DIR}/../peripherals/el2_pic/include
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
if(CCI_FOUND)
  target_link_libraries(veeriss_model PUBLIC ${CCI_LIBRARIES})
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_libraries(veeriss_model PRIVATE stdc++fs)
endif()
# -----------------------------------------------------------------------------
# Print status information
# -----------------------------------------------------------------------------
if(BUILD_TESTS)
  add_executable(veeriss_tb test/veeriss_tb.cpp)
  peripheral_link_test_to_model(veeriss_tb veeriss_model)
  target_link_libraries(veeriss_tb PRIVATE
    SystemC::systemc
    Threads::Threads
    Boost::headers
    Boost::program_options
    el2_pic_model
  )
  if(CCI_FOUND)
    target_link_libraries(veeriss_tb PRIVATE ${CCI_LIBRARIES})
  endif()
  target_include_directories(veeriss_tb PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/VeeR-ISSTlm/model/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/tlm_extensions
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../vp/platform/infra
    ${CMAKE_CURRENT_SOURCE_DIR}/../peripherals/el2_pic/include
  )
  target_include_directories(veeriss_tb SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/VeeR-ISS
    ${CMAKE_CURRENT_SOURCE_DIR}/VeeR-ISS/softfloat/source/include
  )
  target_compile_definitions(veeriss_tb PRIVATE
    FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures"
  )
  target_compile_options(veeriss_tb PRIVATE -UNDEBUG)

  enable_testing()
  add_test(NAME veeriss_tb COMMAND veeriss_tb)

  if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    find_program(LCOV_EXECUTABLE lcov)
    find_program(GENHTML_EXECUTABLE genhtml)
    if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
      # --filter arrived in lcov 2. Ubuntu 22.04 and RHEL 8 ship lcov 1.x.
      set(_lcov_capture_flags "")
      if(LCOV_VERSION VERSION_GREATER_EQUAL "2.0")
        set(_lcov_capture_flags --filter range)
      endif()
      add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${CMAKE_BINARY_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/PeripheralCoverageClean.cmake
        COMMAND ${LCOV_EXECUTABLE} ${LCOV_IGNORE_FLAGS} --zerocounters --directory ${CMAKE_BINARY_DIR}
        COMMAND ${CMAKE_BINARY_DIR}/bin/veeriss_tb
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
        COMMAND ${LCOV_EXECUTABLE} ${LCOV_IGNORE_FLAGS} ${_lcov_capture_flags}
                --capture --directory ${CMAKE_BINARY_DIR}
                --output-file ${CMAKE_BINARY_DIR}/coverage/coverage.info
        COMMAND ${LCOV_EXECUTABLE} ${LCOV_IGNORE_FLAGS}
                --extract ${CMAKE_BINARY_DIR}/coverage/coverage.info
                "${CMAKE_CURRENT_SOURCE_DIR}/VeeR-ISSTlm/model/src/VeeR-ISSTlm.cpp"
                --output-file ${CMAKE_BINARY_DIR}/coverage/coverage_filtered.info
        COMMAND ${GENHTML_EXECUTABLE} ${GENHTML_IGNORE_FLAGS}
                ${CMAKE_BINARY_DIR}/coverage/coverage_filtered.info
                --output-directory ${CMAKE_BINARY_DIR}/coverage/html
        COMMAND ${CMAKE_COMMAND} -E echo "Coverage report at coverage/html/index.html"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating coverage report for VeeR-ISS TLM wrapper..."
      )
      add_dependencies(coverage veeriss_tb)
    endif()
  endif()
endif()

message(STATUS "Generated Project: veeriss")
message(STATUS "  - SYSTEMC_HOME: ${SYSTEMC_HOME}")
