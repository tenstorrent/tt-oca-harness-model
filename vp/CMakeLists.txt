cmake_minimum_required(VERSION 3.20)

# (tenstorrent_sep/Makefile: CXX := scc +cxx .../common/bin/g++)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

project(riscv-vp
	VERSION 1.2.2
	LANGUAGES CXX
)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# Default only when not passed via -DCMAKE_CXX_STANDARD=… on the cmake command line.
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "-g3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# SC_HAS_PROCESS is still required for Synopsys Virtualizer; SystemC 3.x deprecates it
# per IEEE 1666-2023 unless this compatibility define is set.
add_compile_definitions(SC_ALLOW_DEPRECATED_IEEE_API)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR})

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

find_package(Boost REQUIRED COMPONENTS iostreams program_options)

include(FindCCI)
if(CCI_FOUND)
  
endif()
  if(DEFINED SYSTEMC_LIB_CXX_STANDARD AND SYSTEMC_LIB_CXX_STANDARD STREQUAL "202002L")
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_QUIET TRUE)
    check_cxx_source_compiles(
      "#if __cplusplus < 202002L\n#error\n#endif\nint main(){return 0;}\n"
      OCH_VP_COMPILER_SUPPORTS_CXX20
    )
    unset(CMAKE_REQUIRED_QUIET)
    if(NOT OCH_VP_COMPILER_SUPPORTS_CXX20)
      message(FATAL_ERROR
        "libsystemc was built with C++20 (SC_CPLUSPLUS=202002L) but the active compiler "
        "does not report __cplusplus >= 202002L under CMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}.\n"
        "  scl enable gcc-toolset-11 bash   # then re-run vp/configure_vp.sh\n"
        "Or use CMAKE_CXX_STANDARD=17 with install_c17, or rebuild SystemC for your compiler.")
    endif()
  endif()
  include(FindSystemC)
  if(NOT TARGET systemc)
    set(_sc_target SystemC::systemc)
    while(TRUE)
      get_target_property(_sc_aliased "${_sc_target}" ALIASED_TARGET)
      if(NOT _sc_aliased)
        break()
      endif()
      set(_sc_target "${_sc_aliased}")
    endwhile()
    add_library(systemc ALIAS ${_sc_target})
    unset(_sc_target)
    unset(_sc_aliased)
  endif()

include(PeripheralOpenSSL)
peripheral_find_openssl()

subdirs(platform)

# Safe clean from the CMake build directory (vp/build).
#   make clean          — CMake built-in (object files for targets)
#   make clean-workdir  — built-in clean + remove bin/platform/models artifacts;
#                         keeps Makefile/CMakeCache so you can rebuild in-place
set(_sep_root "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(_softfloat_riscv "${_sep_root}/sep/cpu/VeeR-ISS/softfloat/build/RISCV-GCC")
set(_softfloat_darwin "${_sep_root}/sep/cpu/VeeR-ISS/softfloat/build/Darwin-GCC")

add_custom_target(clean-workdir
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean
  COMMAND ${CMAKE_COMMAND}
          -DCMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/SafeCleanBuildDir.cmake
  COMMAND /bin/sh -c "[ -f '${_softfloat_riscv}/Makefile' ] && $(MAKE) -C '${_softfloat_riscv}' clean || true"
  COMMAND /bin/sh -c "[ -f '${_softfloat_darwin}/Makefile' ] && $(MAKE) -C '${_softfloat_darwin}' clean || true"
  COMMENT "Clean VP build artifacts (keep CMake configure files) and softfloat"
  VERBATIM
)
unset(_sep_root)
unset(_softfloat_riscv)
unset(_softfloat_darwin)
