# ===========================================================================
# whisper-cmake/CMakeLists.txt
#
# Import shim for the Tenstorrent Whisper ISS.
#
# Whisper itself is built with its own GNUmakefile (not CMake).  This file
# wraps the pre-built archive `${WHISPER_HOME}/${WHISPER_BUILD_DIR}/librvcore.a`
# and its companion sub-archives (libvirtual_memory.a, libpci.a, softfloat.a)
# as CMake IMPORTED targets so the rest of the SMC build can link against
# `whisper::whisper` without knowing where Whisper lives on disk.
#
# Whisper MUST have been compiled with MEM_CALLBACKS=1 -- the SMC memory
# bridge depends on Whisper's defineRead/WriteMemoryCallback API.
#
# Cross-platform: Whisper's GNUmakefile names its build directory after
# `uname -s`, so the archives live in:
#     Linux  -> ${WHISPER_HOME}/build-Linux/librvcore.a
#     macOS  -> ${WHISPER_HOME}/build-Darwin/librvcore.a
# The parent CMakeLists.txt resolves WHISPER_BUILD_DIR (default
# build-${CMAKE_HOST_SYSTEM_NAME}, overridable via -D or env).
# ===========================================================================

if(NOT DEFINED WHISPER_BUILD_DIR)
    # Standalone use of this directory.  Mirror the parent's default.
    if(DEFINED ENV{WHISPER_BUILD_DIR})
        set(WHISPER_BUILD_DIR "$ENV{WHISPER_BUILD_DIR}")
    else()
        set(WHISPER_BUILD_DIR "build-${CMAKE_HOST_SYSTEM_NAME}")
    endif()
endif()

# ----- Locate the four static archives produced by Whisper's makefile -----
set(_whisper_rvcore         "${WHISPER_HOME}/${WHISPER_BUILD_DIR}/librvcore.a")
set(_whisper_virtual_memory "${WHISPER_HOME}/virtual_memory/libvirtual_memory.a")
set(_whisper_pci            "${WHISPER_HOME}/pci/libpci.a")
set(_whisper_softfloat      "${WHISPER_HOME}/third_party/softfloat/build/RISCV-GCC/softfloat.a")

foreach(_lib IN ITEMS _whisper_rvcore _whisper_virtual_memory _whisper_pci _whisper_softfloat)
    if(NOT EXISTS "${${_lib}}")
        message(FATAL_ERROR "Whisper artifact not found: ${${_lib}}\n"
                            "Build Whisper first:\n"
                            "    cd ${WHISPER_HOME} && make MEM_CALLBACKS=1")
    endif()
endforeach()

# ----- Sub-library imported targets -----
add_library(whisper::virtual_memory STATIC IMPORTED GLOBAL)
set_target_properties(whisper::virtual_memory PROPERTIES
    IMPORTED_LOCATION "${_whisper_virtual_memory}")

add_library(whisper::pci STATIC IMPORTED GLOBAL)
set_target_properties(whisper::pci PROPERTIES
    IMPORTED_LOCATION "${_whisper_pci}")

add_library(whisper::softfloat STATIC IMPORTED GLOBAL)
set_target_properties(whisper::softfloat PROPERTIES
    IMPORTED_LOCATION "${_whisper_softfloat}")

# ----- Probe librvcore.a for optional features -----
# Whisper's GNUmakefile turns features on/off via knobs that change which
# external libs it actually drags in:
#     LZ4_COMPRESS=1   ->  -DLZ4_COMPRESS  -llz4    (snapshot compression)
# A pre-built archive that wasn't compiled with the knob does NOT carry
# the corresponding undefined references, so linking the lib is dead
# weight (and on macOS, the bare -llz4 even fails since /opt/homebrew/lib
# is not on Apple ld's default search path).  We auto-detect what's
# actually inside librvcore.a and only require the lib when needed.
find_program(_nm_exe NAMES nm)
set(_whisper_needs_lz4 OFF)
if(_nm_exe)
    execute_process(
        COMMAND "${_nm_exe}" "${_whisper_rvcore}"
        OUTPUT_VARIABLE _rvcore_syms
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    # Match both legacy (LZ4_*) and frame (LZ4F_*) APIs from liblz4.
    # Avoid \b -- CMake's regex engine does not treat it as a word boundary.
    if(_rvcore_syms MATCHES "LZ4F_|LZ4_")
        set(_whisper_needs_lz4 ON)
    endif()
endif()

# Build the set of compile definitions to propagate to SMC consumers.
# MEM_CALLBACKS is always set (we require Whisper to be built with it -- see
# top-of-file comment), LZ4_COMPRESS only if librvcore.a actually wants it.
set(_whisper_defs "MEM_CALLBACKS")
if(_whisper_needs_lz4)
    list(APPEND _whisper_defs "LZ4_COMPRESS")
endif()

# ----- Main rvcore archive -----
# INTERFACE_INCLUDE_DIRECTORIES exposes Whisper's headers (Hart.hpp, System.hpp,
# CsRegs.hpp, DecodedInst.hpp, ...). The third_party/ dir contains ELFIO and
# the softfloat headers Whisper includes internally.
add_library(whisper::rvcore STATIC IMPORTED GLOBAL)
set_target_properties(whisper::rvcore PROPERTIES
    IMPORTED_LOCATION "${_whisper_rvcore}"
    INTERFACE_INCLUDE_DIRECTORIES "${WHISPER_HOME};${WHISPER_HOME}/third_party"
    INTERFACE_COMPILE_DEFINITIONS "${_whisper_defs}"
)

# ----- Composite target: link this and you get the whole Whisper stack -----
#
# librvcore.a contains template instantiations (e.g. System<uint64_t>::
# loadElfFiles) that show up as WEAK symbols.  ld will NOT pull an
# archive member into the link just to satisfy a reference whose only
# available definition is weak -- so without forcing the whole archive in,
# the SMC code fails to link with "undefined reference to
# System<...>::loadElfFiles".
#
# The mechanism to force-include every object differs per linker:
#     GNU ld / lld  ->  -Wl,--whole-archive  <archive>  -Wl,--no-whole-archive
#     Apple ld64    ->  -Wl,-force_load,<archive>     (no closing flag)
#
# The size cost is small (~25 MB static, mostly debug info) and is acceptable
# for the cycle-accurate ISS.
#
# Non-system runtime deps that Whisper *may* drag in -- only resolved when
# the archive's symbols actually need them (see LZ4_* probe above).  On
# Linux these live in /usr/lib and the toolchain finds them via the
# default search path; on macOS they live under Homebrew (/opt/homebrew/lib
# or /usr/local/lib) and Apple ld does NOT search those by default, so we
# resolve them explicitly with find_library() and pass the full path.
set(_whisper_extra_libs "")
if(_whisper_needs_lz4)
    find_library(WHISPER_LZ4_LIB
        NAMES lz4
        HINTS /opt/homebrew/lib
              /usr/local/lib
              /usr/lib64
              /usr/lib
              /usr/lib/x86_64-linux-gnu
              /usr/lib/aarch64-linux-gnu
        REQUIRED)
    message(STATUS "    lz4            = ${WHISPER_LZ4_LIB}")
    list(APPEND _whisper_extra_libs "${WHISPER_LZ4_LIB}")
else()
    message(STATUS "    lz4            = (not needed - librvcore.a has no LZ4* symbols)")
endif()

add_library(whisper::whisper INTERFACE IMPORTED GLOBAL)

if(APPLE)
    # macOS / Mach-O linker: -force_load wants the archive path inline.
    # We therefore do NOT list whisper::rvcore as a regular link library;
    # -force_load brings every object in (and the archive's
    # INTERFACE_INCLUDE_DIRECTORIES are already propagated via the
    # composite target below).
    set_property(TARGET whisper::whisper APPEND PROPERTY
        INTERFACE_LINK_OPTIONS "-Wl,-force_load,${_whisper_rvcore}")
    set_property(TARGET whisper::whisper APPEND PROPERTY
        INTERFACE_INCLUDE_DIRECTORIES "${WHISPER_HOME};${WHISPER_HOME}/third_party")
    set_property(TARGET whisper::whisper APPEND PROPERTY
        INTERFACE_COMPILE_DEFINITIONS "${_whisper_defs}")
    set_property(TARGET whisper::whisper APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES
            whisper::virtual_memory
            whisper::pci
            whisper::softfloat
            boost::headers
            # System libs Whisper drags in (see its GNUmakefile EXTRA_LIBS).
            # NOTE: librt does not exist on macOS -- POSIX realtime calls are
            # in libSystem.  We also drop libdl because dlopen() lives in
            # libSystem too.
            pthread m z util
            ${_whisper_extra_libs})
else()
    # GNU / GNU-compatible linker: bracket the archive with --whole-archive /
    # --no-whole-archive so the per-object inclusion can be turned off again
    # for everything that follows on the link line.
    set_property(TARGET whisper::whisper PROPERTY
        INTERFACE_LINK_LIBRARIES
            "-Wl,--whole-archive"
            whisper::rvcore
            "-Wl,--no-whole-archive"
            whisper::virtual_memory
            whisper::pci
            whisper::softfloat
            boost::headers
            # System libs Whisper drags in (see its GNUmakefile EXTRA_LIBS).
            pthread m z dl rt util
            ${_whisper_extra_libs})
endif()

# Boost program_options is needed if Whisper's Args.cpp made it into librvcore.
# Appending it HERE is what places it after the archive on the link line; a
# consumer listing Boost itself (as vp/platform/smc does) is not enough, since
# that copy precedes librvcore.a and --as-needed then discards it.
#
# Accept either the target the parent CMakeLists locates by hand
# (boost::program_options) or the find_package one a consumer may already have
# (Boost::program_options), so a Boost layout the manual search misses is not
# fatal.
if(TARGET boost::program_options)
    set_property(TARGET whisper::whisper APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES boost::program_options)
elseif(TARGET Boost::program_options)
    set_property(TARGET whisper::whisper APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES Boost::program_options)
else()
    message(WARNING
        "No Boost program_options target found; linking whisper::whisper may "
        "fail with undefined boost::program_options symbols from "
        "librvcore.a(Args.cpp.o).  Point BOOST_DIR at an install whose lib "
        "directory contains libboost_program_options.")
endif()

message(STATUS "Whisper imported (${WHISPER_BUILD_DIR}):")
message(STATUS "    rvcore         = ${_whisper_rvcore}")
message(STATUS "    virtual_memory = ${_whisper_virtual_memory}")
message(STATUS "    pci            = ${_whisper_pci}")
message(STATUS "    softfloat      = ${_whisper_softfloat}")
