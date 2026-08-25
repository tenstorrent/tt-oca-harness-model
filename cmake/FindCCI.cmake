# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# FindCCI.cmake
# -------------
# Locate CCI (Configuration, Control and Inspection) library and headers
#
# This module defines:
#   CCI::cci              - Imported target for CCI (if found)
#   CCI_FOUND             - TRUE if CCI was found
#   CCI_INCLUDE_DIR       - Include directory for CCI headers
#   CCI_LIBRARIES         - List of CCI libraries (cci-config, cci-inspection)
#   CCI_CONFIG_LIBRARY    - Path to cci-config library
#   CCI_INSPECTION_LIBRARY - Path to cci-inspection library
#
# The module searches for CCI in the following order:
#   1. CCI_HOME environment variable (separate CCI installation)
#   2. SYSTEMC_HOME environment variable (CCI bundled with SystemC)
#   3. Standard system paths (/usr/local, /usr)
#
# Users can set:
#   CCI_HOME      - Environment variable, or CMake -DCCI_HOME=, for the CCI prefix
#   SYSTEMC_HOME  - Environment variable, or CMake -DSYSTEMC_HOME= (CCI may live under SystemC)
#
# Example usage:
#   include(FindCCI)
#   if(CCI_FOUND)
#     target_link_libraries(myapp CCI::cci)
#   endif()

# Early-return guard: skip re-running if CCI::cci already has valid, real include dirs.
# We do NOT simply check CCI_FOUND+TARGET because SystemC 3.x cmake configs call
# find_dependency(CCI) internally, which creates CCI::cci with INSTALL_INTERFACE
# generator-expression include dirs that don't expand for downstream consumers.
# Only return early when cci_configuration is actually reachable on disk.
if(TARGET CCI::cci)
  get_target_property(_cci_early_incs CCI::cci INTERFACE_INCLUDE_DIRECTORIES)
  set(_cci_early_valid FALSE)
  if(_cci_early_incs)
    foreach(_d ${_cci_early_incs})
      if(NOT "${_d}" MATCHES "\\$<")
        if(EXISTS "${_d}/cci_configuration" OR EXISTS "${_d}/cci_configuration.h")
          set(_cci_early_valid TRUE)
          break()
        endif()
      endif()
    endforeach()
  endif()
  unset(_cci_early_incs)
  if(_cci_early_valid)
    unset(_cci_early_valid)
    set(CCI_FOUND TRUE)
    return()
  endif()
  unset(_cci_early_valid)
  # CCI::cci exists but include dirs are genex-only or missing — fall through to repair.
endif()

# Root hints: use env vars, or CMake -DCCI_HOME / -DSYSTEMC_HOME (same idea as FindSystemC)
set(CCI_ROOT_HINT "")
if(DEFINED ENV{CCI_HOME} AND NOT "$ENV{CCI_HOME}" STREQUAL "")
  set(CCI_ROOT_HINT "$ENV{CCI_HOME}")
elseif(DEFINED CCI_HOME AND NOT "${CCI_HOME}" STREQUAL "")
  set(CCI_ROOT_HINT "${CCI_HOME}")
endif()
set(SYSTEMC_ROOT_HINT "")
if(DEFINED ENV{SYSTEMC_HOME} AND NOT "$ENV{SYSTEMC_HOME}" STREQUAL "")
  set(SYSTEMC_ROOT_HINT "$ENV{SYSTEMC_HOME}")
elseif(DEFINED SYSTEMC_HOME AND NOT "${SYSTEMC_HOME}" STREQUAL "")
  set(SYSTEMC_ROOT_HINT "${SYSTEMC_HOME}")
endif()

# Find CCI configuration library (CCI 1.0+)
find_library(CCI_CONFIG_LIBRARY
  NAMES cci-config libcci-config
  HINTS ${CCI_ROOT_HINT}/lib
        ${CCI_ROOT_HINT}/lib-linux64
        ${CCI_ROOT_HINT}/lib64
        ${SYSTEMC_ROOT_HINT}/lib
        ${SYSTEMC_ROOT_HINT}/lib-linux64
        ${SYSTEMC_ROOT_HINT}/lib64
        /usr/local/lib
        /usr/local/lib64
        /usr/lib
        /usr/lib64
  DOC "CCI configuration library"
)

# Find CCI inspection library (CCI 1.0+)
find_library(CCI_INSPECTION_LIBRARY
  NAMES cci-inspection libcci-inspection
  HINTS ${CCI_ROOT_HINT}/lib
        ${CCI_ROOT_HINT}/lib-linux64
        ${CCI_ROOT_HINT}/lib64
        ${SYSTEMC_ROOT_HINT}/lib
        ${SYSTEMC_ROOT_HINT}/lib-linux64
        ${SYSTEMC_ROOT_HINT}/lib64
        /usr/local/lib
        /usr/local/lib64
        /usr/lib
        /usr/lib64
  DOC "CCI inspection library"
)

# Fallback: Find single CCI library (older CCI versions pre-1.0)
if(NOT CCI_CONFIG_LIBRARY OR NOT CCI_INSPECTION_LIBRARY)
  find_library(CCI_LEGACY_LIBRARY
    NAMES cci libcci
    HINTS ${CCI_ROOT_HINT}/lib
          ${CCI_ROOT_HINT}/lib-linux64
          ${CCI_ROOT_HINT}/lib64
          ${SYSTEMC_ROOT_HINT}/lib
          ${SYSTEMC_ROOT_HINT}/lib-linux64
          ${SYSTEMC_ROOT_HINT}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
    DOC "CCI library (legacy single-library version)"
  )

  if(CCI_LEGACY_LIBRARY)
    message(STATUS "FindCCI: Found legacy CCI library (pre-1.0): ${CCI_LEGACY_LIBRARY}")
    # Use the single library for both config and inspection
    set(CCI_CONFIG_LIBRARY ${CCI_LEGACY_LIBRARY})
    set(CCI_INSPECTION_LIBRARY ${CCI_LEGACY_LIBRARY})
  endif()
endif()

# Prefer include dir next to the resolved library (matches lib + headers from one prefix)
set(_CCI_LIB_FOR_INCLUDE "")
if(CCI_LEGACY_LIBRARY)
  set(_CCI_LIB_FOR_INCLUDE "${CCI_LEGACY_LIBRARY}")
elseif(CCI_CONFIG_LIBRARY)
  set(_CCI_LIB_FOR_INCLUDE "${CCI_CONFIG_LIBRARY}")
endif()
set(CCI_INCLUDE_DIR "")
if(_CCI_LIB_FOR_INCLUDE)
  get_filename_component(_cci_lib_parent "${_CCI_LIB_FOR_INCLUDE}" DIRECTORY)
  get_filename_component(_cci_prefix "${_cci_lib_parent}" DIRECTORY)
  set(_cci_inc_candidate "${_cci_prefix}/include")
  if(EXISTS "${_cci_inc_candidate}/cci_configuration"
      OR EXISTS "${_cci_inc_candidate}/cci_configuration.h")
    set(CCI_INCLUDE_DIR "${_cci_inc_candidate}")
  endif()
endif()

# Find CCI include directory (if not derived from library path)
if(NOT CCI_INCLUDE_DIR)
  find_path(CCI_INCLUDE_DIR NAMES cci_configuration cci_configuration.h
    HINTS ${CCI_ROOT_HINT}/include
          ${SYSTEMC_ROOT_HINT}/include
          /usr/local/include
          /usr/include
    DOC "CCI include directory"
  )
endif()

# Set CCI_LIBRARIES if both libraries found
if(CCI_CONFIG_LIBRARY AND CCI_INSPECTION_LIBRARY)
  set(CCI_LIBRARIES ${CCI_CONFIG_LIBRARY} ${CCI_INSPECTION_LIBRARY})
endif()

# Handle standard find_package arguments (REQUIRED, QUIET)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CCI
  REQUIRED_VARS CCI_CONFIG_LIBRARY CCI_INSPECTION_LIBRARY CCI_INCLUDE_DIR
  FAIL_MESSAGE "Could not find CCI. Set CCI_HOME or SYSTEMC_HOME environment variable. CCI may be bundled with SystemC or installed separately."
)

# Create or repair imported target (use GLOBAL so subprojects can see it).
# If CCI::cci was already created by a SystemC cmake config with genex-only include dirs,
# we fall through here and overwrite the properties with the real paths we just found.
if(CCI_FOUND)
  if(NOT TARGET CCI::cci)
    add_library(CCI::cci INTERFACE IMPORTED GLOBAL)
  endif()
  set_target_properties(CCI::cci PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CCI_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "${CCI_LIBRARIES}"
  )

  message(STATUS "Found CCI libraries: ${CCI_LIBRARIES}")
  message(STATUS "CCI include directory: ${CCI_INCLUDE_DIR}")

  # Cache variables so subprojects can see them
  set(CCI_FOUND TRUE CACHE BOOL "CCI found" FORCE)
  set(CCI_INCLUDE_DIR "${CCI_INCLUDE_DIR}" CACHE PATH "CCI include directory" FORCE)
  set(CCI_LIBRARIES "${CCI_LIBRARIES}" CACHE STRING "CCI libraries" FORCE)
else()
  # CCI is optional in most cases - provide informational message
  if(NOT CCI_FIND_REQUIRED)
    message(STATUS "CCI not found - building without CCI support")
    message(STATUS "  To enable CCI: Set CCI_HOME")
  endif()
endif()

mark_as_advanced(CCI_INCLUDE_DIR CCI_CONFIG_LIBRARY CCI_INSPECTION_LIBRARY CCI_LIBRARIES)
