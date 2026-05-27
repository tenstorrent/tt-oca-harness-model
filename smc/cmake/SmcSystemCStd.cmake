# =============================================================================
# SmcSystemCStd.cmake
#
# Auto-detect the C++ language standard that the linked SystemC library was
# compiled with, and set CMAKE_CXX_STANDARD to match.
#
# WHY THIS EXISTS
# ---------------
# Accellera SystemC emits a unique guard symbol per language standard,
# e.g.  `sc_api_version_3_0_2_cxx201703::sc_api_version_3_0_2_cxx201703(...)`
#  or   `sc_api_version_3_0_2_cxx202002L::sc_api_version_3_0_2_cxx202002L(...)`.
# Every TU that includes <systemc> emits an `static` instance of the class
# named for *its* compile-time `__cplusplus` value.  If the consumer's
# standard does not match the library's, the link fails with:
#   "Undefined symbols ... sc_api_version_3_0_2_cxx2020XXL::..."
# This is by design: SystemC's ABI changes per language standard, so the
# language-version mismatch is forbidden at link time.
#
# In practice this means hard-coding `CMAKE_CXX_STANDARD 20` in an IP's
# CMakeLists silently breaks anyone whose SystemC was built with C++17
# (e.g. Homebrew SystemC 3.0.2 on macOS), and vice versa.  We avoid that
# by probing the SystemC library's symbols at configure time and picking
# the matching standard automatically.
#
# USAGE
# -----
#   list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
#   include(SmcSystemCStd)
#   smc_detect_systemc_cxx_std()           # AFTER SystemC::systemc is defined
#
# Sets in the caller scope:
#   CMAKE_CXX_STANDARD            (17 / 20 / 23, detected or default)
#   CMAKE_CXX_STANDARD_REQUIRED   ON
#   CMAKE_CXX_EXTENSIONS          OFF
#
# OVERRIDE
# --------
#   cmake ... -DSMC_CXX_STANDARD=17       # force; skips auto-detect
#
# FALL-BACK
# ---------
# If detection fails (no SystemC library found, no nm available, or no
# `sc_api_version_*_cxxNNNNNNL?` symbol present), the module emits a
# WARNING and falls back to C++17 (the most permissive choice; will link
# against any SystemC built with C++17 or later).
# =============================================================================

cmake_minimum_required(VERSION 3.16)

# -----------------------------------------------------------------------------
# Internal: walk the SystemC::systemc target's properties to find the on-disk
# library file we can run nm against.  Handles both manually-imported targets
# (IMPORTED_LOCATION) and CMake-installed packages (IMPORTED_LOCATION_*).
# -----------------------------------------------------------------------------
function(_smc_extract_systemc_libpath OUT_VAR)
    set(${OUT_VAR} "" PARENT_SCOPE)
    if(NOT TARGET SystemC::systemc)
        return()
    endif()

    # Try common imported-location properties first.
    foreach(_prop
            IMPORTED_LOCATION
            IMPORTED_LOCATION_RELEASE
            IMPORTED_LOCATION_DEBUG
            IMPORTED_LOCATION_RELWITHDEBINFO
            IMPORTED_LOCATION_MINSIZEREL
            IMPORTED_LOCATION_NOCONFIG)
        get_target_property(_loc SystemC::systemc ${_prop})
        if(_loc AND EXISTS "${_loc}")
            set(${OUT_VAR} "${_loc}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    # Fall back to scanning INTERFACE_INCLUDE_DIRECTORIES/../<libdir>/<libname>.
    get_target_property(_incs SystemC::systemc INTERFACE_INCLUDE_DIRECTORIES)
    if(NOT _incs)
        return()
    endif()
    foreach(_inc ${_incs})
        get_filename_component(_root "${_inc}" DIRECTORY)
        foreach(_libdir lib lib64 lib-macosx lib-macosarm64 lib-linux64)
            foreach(_libname
                    libsystemc.a
                    libsystemc.dylib
                    libsystemc.so)
                set(_candidate "${_root}/${_libdir}/${_libname}")
                if(EXISTS "${_candidate}")
                    set(${OUT_VAR} "${_candidate}" PARENT_SCOPE)
                    return()
                endif()
            endforeach()
        endforeach()
    endforeach()
endfunction()

# -----------------------------------------------------------------------------
# Public: detect & apply the right CMAKE_CXX_STANDARD.
# -----------------------------------------------------------------------------
function(smc_detect_systemc_cxx_std)
    set(CMAKE_CXX_STANDARD_REQUIRED ON  PARENT_SCOPE)
    set(CMAKE_CXX_EXTENSIONS        OFF PARENT_SCOPE)

    # Highest priority: explicit user override.
    if(DEFINED SMC_CXX_STANDARD)
        message(STATUS
            "smc: CMAKE_CXX_STANDARD = ${SMC_CXX_STANDARD} "
            "(user override via -DSMC_CXX_STANDARD)")
        set(CMAKE_CXX_STANDARD ${SMC_CXX_STANDARD} PARENT_SCOPE)
        return()
    endif()

    _smc_extract_systemc_libpath(_sc_lib)
    if(NOT _sc_lib)
        message(WARNING
            "smc: could not locate the SystemC library on disk to probe its "
            "language standard; defaulting CMAKE_CXX_STANDARD to 17. Pass "
            "-DSMC_CXX_STANDARD=<17|20|23> to override.")
        set(CMAKE_CXX_STANDARD 17 PARENT_SCOPE)
        return()
    endif()

    find_program(_nm_exe NAMES nm)
    if(NOT _nm_exe)
        message(WARNING
            "smc: `nm` not found in PATH; cannot probe ${_sc_lib} for "
            "sc_api_version symbols; defaulting CMAKE_CXX_STANDARD to 17. "
            "Pass -DSMC_CXX_STANDARD=<17|20|23> to override.")
        set(CMAKE_CXX_STANDARD 17 PARENT_SCOPE)
        return()
    endif()

    execute_process(
        COMMAND "${_nm_exe}" "${_sc_lib}"
        OUTPUT_VARIABLE _sc_syms
        ERROR_QUIET
        RESULT_VARIABLE _nm_rc
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _nm_rc EQUAL 0)
        message(WARNING
            "smc: `nm ${_sc_lib}` failed (rc=${_nm_rc}); defaulting "
            "CMAKE_CXX_STANDARD to 17.  Pass -DSMC_CXX_STANDARD=<17|20|23> to override.")
        set(CMAKE_CXX_STANDARD 17 PARENT_SCOPE)
        return()
    endif()

    # Find the first sc_api_version_*_cxxNNNNNN[L]? symbol.
    string(REGEX MATCH
        "sc_api_version[A-Za-z0-9_]*_cxx([0-9]+)L?"
        _match "${_sc_syms}")
    if(NOT _match)
        message(WARNING
            "smc: no sc_api_version_*_cxxNNNNNN symbol found in ${_sc_lib}; "
            "defaulting CMAKE_CXX_STANDARD to 17. Pass -DSMC_CXX_STANDARD=<17|20|23> to override.")
        set(CMAKE_CXX_STANDARD 17 PARENT_SCOPE)
        return()
    endif()

    set(_cplusplus "${CMAKE_MATCH_1}")
    if(_cplusplus STREQUAL "201103")
        set(_std 11)
    elseif(_cplusplus STREQUAL "201402")
        set(_std 14)
    elseif(_cplusplus STREQUAL "201703")
        set(_std 17)
    elseif(_cplusplus STREQUAL "202002")
        set(_std 20)
    elseif(_cplusplus STREQUAL "202302")
        set(_std 23)
    else()
        message(WARNING
            "smc: unrecognised __cplusplus value '${_cplusplus}' in SystemC "
            "guard symbol; defaulting CMAKE_CXX_STANDARD to 17.")
        set(_std 17)
    endif()

    message(STATUS
        "smc: detected SystemC built with C++${_std} "
        "(probed ${_sc_lib}, __cplusplus=${_cplusplus}L)")
    message(STATUS
        "smc: setting CMAKE_CXX_STANDARD = ${_std} "
        "(override with -DSMC_CXX_STANDARD=<17|20|23>)")
    set(CMAKE_CXX_STANDARD ${_std} PARENT_SCOPE)
endfunction()
