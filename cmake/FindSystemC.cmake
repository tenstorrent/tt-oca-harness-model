# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# FindSystemC.cmake
# ------------------
# Locate SystemC library and headers
#
# This module defines:
#   SystemC::systemc       - Imported target for SystemC
#   SYSTEMC_FOUND          - TRUE if SystemC was found
#   SYSTEMC_INCLUDE_DIR    - Include directory for SystemC headers
#   SYSTEMC_LIBRARY        - Path to SystemC library
#
# The module searches for SystemC in the following order:
#   1. CMake package config (find_package(SystemCLanguage))
#   2. SYSTEMC_HOME environment variable
#   3. Standard system paths (/usr/local, /usr)
#
# Users can set:
#   SYSTEMC_HOME - Environment variable or CMake -DSYSTEMC_HOME= for the install prefix
#
# Example usage:
#   include(FindSystemC)
#   target_link_libraries(myapp SystemC::systemc)

# Collect SYSTEMC_HOME hint (cmake var takes priority over env var) so all
# find_package calls below search inside the same install prefix.  This is
# critical when model CMakeLists (e.g. crng, kmac) set SYSTEMC_HOME to a
# versioned prefix like /usr/local/systemc-3.0.1 — the cmake target must link
# the library from that exact installation to avoid header/library ABI mismatches.
set(_sc_pkg_hints "")
if(DEFINED SYSTEMC_HOME AND NOT "${SYSTEMC_HOME}" STREQUAL "")
  list(APPEND _sc_pkg_hints "${SYSTEMC_HOME}")
endif()
if(DEFINED ENV{SYSTEMC_HOME} AND NOT "$ENV{SYSTEMC_HOME}" STREQUAL "")
  list(APPEND _sc_pkg_hints "$ENV{SYSTEMC_HOME}")
endif()

# Try the official "SystemC" cmake config first (cmake-built SystemC 2.3.3+, 3.x).
# CONFIG mode avoids recursing into this Find module.
# HINTS searches inside the SYSTEMC_HOME prefix, e.g. finds
#   /usr/local/systemc-3.0.1/lib/cmake/SystemC/SystemCConfig.cmake
find_package(SystemC CONFIG QUIET HINTS ${_sc_pkg_hints})

# Try "SystemCLanguage" cmake config (alternate export name used by some installs).
if(NOT TARGET SystemC::systemc)
  if(DEFINED SYSTEMC_HOME AND NOT "${SYSTEMC_HOME}" STREQUAL "")
    find_package(SystemCLanguage CONFIG QUIET
      PATHS "${SYSTEMC_HOME}/lib/cmake/SystemCLanguage"
      NO_DEFAULT_PATH)
  endif()
  if(NOT TARGET SystemCLanguage::systemc)
    find_package(SystemCLanguage QUIET HINTS ${_sc_pkg_hints})
  endif()
endif()

unset(_sc_pkg_hints)

if(TARGET SystemCLanguage::systemc AND NOT TARGET SystemC::systemc)
  # Only use this target if it actually links a real library.
  # Some cmake configs export SystemCLanguage::systemc as an INTERFACE target
  # (headers/compile-defs only, no .so) — using that would produce a broken
  # link where symbols compile but the library is missing at link time.
  set(_sc_lang_target SystemCLanguage::systemc)
  while(TRUE)
    get_target_property(_sc_aliased "${_sc_lang_target}" ALIASED_TARGET)
    if(NOT _sc_aliased)
      break()
    endif()
    set(_sc_lang_target "${_sc_aliased}")
  endwhile()
  get_target_property(_sc_lang_type "${_sc_lang_target}" TYPE)
  get_target_property(_sc_lang_loc  "${_sc_lang_target}" IMPORTED_LOCATION)
  if(NOT _sc_lang_type STREQUAL "INTERFACE_LIBRARY"
      AND _sc_lang_loc AND EXISTS "${_sc_lang_loc}")
    # CMake forbids aliasing a non-global ALIAS, so point at the resolved real target.
    add_library(SystemC::systemc ALIAS ${_sc_lang_target})
    set(SYSTEMC_FOUND TRUE)
    message(STATUS "Found SystemC via CMake package config")
  endif()
  # If INTERFACE-only, fall through to manual search below.
  unset(_sc_lang_target)
  unset(_sc_aliased)
  unset(_sc_lang_type)
  unset(_sc_lang_loc)
endif()

if(TARGET SystemC::systemc)
  # Validate: must link a real library, not an INTERFACE-only target.
  # SystemC 3.x cmake configs sometimes export SystemC::systemc as INTERFACE
  # (headers + compile-defs only, no .so path), which causes undefined references
  # at link time even though compilation succeeds.
  set(_sc_check_target SystemC::systemc)
  while(TRUE)
    get_target_property(_sc_check_aliased "${_sc_check_target}" ALIASED_TARGET)
    if(NOT _sc_check_aliased)
      break()
    endif()
    set(_sc_check_target "${_sc_check_aliased}")
  endwhile()
  get_target_property(_sc_check_type "${_sc_check_target}" TYPE)
  get_target_property(_sc_check_loc  "${_sc_check_target}" IMPORTED_LOCATION)
  if(NOT _sc_check_type STREQUAL "INTERFACE_LIBRARY"
      AND _sc_check_loc AND EXISTS "${_sc_check_loc}")
    set(SYSTEMC_FOUND TRUE)
    message(STATUS "Found SystemC via CMake package config")
    unset(_sc_check_target)
    unset(_sc_check_aliased)
    unset(_sc_check_type)
    unset(_sc_check_loc)
    return()
  endif()
  # INTERFACE-only or library path missing/invalid — fall through to manual search.
  unset(_sc_check_target)
  unset(_sc_check_aliased)
  unset(_sc_check_type)
  unset(_sc_check_loc)
endif()

# Earlier include() in this configure already created SystemC::systemc (manual path).
# Refresh INTERFACE_INCLUDE_DIRECTORIES from the cached library so includes match libsystemc.
# Skip this block if the existing target is INTERFACE-only (came from a cmake package config
# with no real library) — in that case fall through to the manual search to repair it.
if(TARGET SystemC::systemc)
  get_target_property(_sc_existing_type SystemC::systemc TYPE)
  if(NOT _sc_existing_type STREQUAL "INTERFACE_LIBRARY")
    if(SYSTEMC_FOUND AND SYSTEMC_LIBRARY AND EXISTS "${SYSTEMC_LIBRARY}")
      get_filename_component(_sc_lib_dir "${SYSTEMC_LIBRARY}" DIRECTORY)
      get_filename_component(_sc_prefix "${_sc_lib_dir}" DIRECTORY)
      set(_sc_inc "${_sc_prefix}/include")
      if(EXISTS "${_sc_inc}/systemc.h"
          OR EXISTS "${_sc_inc}/systemc/systemc.h")
        set_target_properties(SystemC::systemc PROPERTIES
          IMPORTED_LOCATION "${SYSTEMC_LIBRARY}"
          INTERFACE_INCLUDE_DIRECTORIES "${_sc_inc}")
        set(SYSTEMC_INCLUDE_DIR "${_sc_inc}" CACHE PATH "SystemC include directory" FORCE)
      endif()
    endif()
    unset(_sc_existing_type)
    return()
  endif()
  unset(_sc_existing_type)
  # INTERFACE-only — fall through to manual search to repair.
endif()

# Manual search - env or -DSYSTEMC_HOME= (must match CCI / FindCCI root hint pattern)
set(SYSTEMC_ROOT_HINT "")
if(DEFINED ENV{SYSTEMC_HOME} AND NOT "$ENV{SYSTEMC_HOME}" STREQUAL "")
  set(SYSTEMC_ROOT_HINT "$ENV{SYSTEMC_HOME}")
elseif(DEFINED SYSTEMC_HOME AND NOT "${SYSTEMC_HOME}" STREQUAL "")
  set(SYSTEMC_ROOT_HINT "${SYSTEMC_HOME}")
endif()

# Drop stale find_path cache so includes stay aligned with the library install
unset(SYSTEMC_INCLUDE_DIR CACHE)

# Find library first so we can pin headers to the same prefix as libsystemc
find_library(SYSTEMC_LIBRARY
  NAMES systemc libsystemc
  HINTS ${SYSTEMC_ROOT_HINT}/lib-linux64
        ${SYSTEMC_ROOT_HINT}/lib64
        ${SYSTEMC_ROOT_HINT}/lib
        /usr/local/lib
        /usr/local/lib64
        /usr/lib
        /usr/lib64
  DOC "SystemC library"
)

set(SYSTEMC_INCLUDE_DIR "")
if(SYSTEMC_LIBRARY)
  get_filename_component(_systemc_lib_dir "${SYSTEMC_LIBRARY}" DIRECTORY)
  get_filename_component(_systemc_prefix "${_systemc_lib_dir}" DIRECTORY)
  set(_systemc_inc_candidate "${_systemc_prefix}/include")
  if(EXISTS "${_systemc_inc_candidate}/systemc.h"
      OR EXISTS "${_systemc_inc_candidate}/systemc/systemc.h")
    set(SYSTEMC_INCLUDE_DIR "${_systemc_inc_candidate}")
  endif()
endif()

if(NOT SYSTEMC_INCLUDE_DIR)
  find_path(SYSTEMC_INCLUDE_DIR NAMES systemc.h systemc/systemc.h
    HINTS ${SYSTEMC_ROOT_HINT}/include
          /usr/local/include
          /usr/include
    DOC "SystemC include directory"
  )
endif()

# Handle standard find_package arguments (REQUIRED, QUIET)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SystemC
  REQUIRED_VARS SYSTEMC_LIBRARY SYSTEMC_INCLUDE_DIR
  FAIL_MESSAGE "Could not find SystemC. Set SYSTEMC_HOME environment variable or install SystemC with CMake package config."
)

# Create or repair imported target (use GLOBAL so subprojects can see it).
# If SystemC::systemc was already created as INTERFACE-only by a cmake package config,
# we cannot change its type, so we create a new real target and alias to it instead.
if(SYSTEMC_FOUND)
  if(TARGET SystemC::systemc)
    # Existing target is INTERFACE-only (we fell through to here) — create the real
    # library target under a private name and retarget the alias.
    if(NOT TARGET _SystemC_real)
      add_library(_SystemC_real UNKNOWN IMPORTED GLOBAL)
    endif()
    set_target_properties(_SystemC_real PROPERTIES
      IMPORTED_LOCATION "${SYSTEMC_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${SYSTEMC_INCLUDE_DIR}"
    )
    # Overwrite the INTERFACE target's link libs to point at the real target.
    set_target_properties(SystemC::systemc PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${SYSTEMC_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES "_SystemC_real"
    )
  else()
    add_library(SystemC::systemc UNKNOWN IMPORTED GLOBAL)
    set_target_properties(SystemC::systemc PROPERTIES
      IMPORTED_LOCATION "${SYSTEMC_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${SYSTEMC_INCLUDE_DIR}"
    )
  endif()

  # Find and link pthread (required by SystemC)
  find_package(Threads REQUIRED)
  if(TARGET _SystemC_real)
    # Workaround path: add Threads to the real library target, not the INTERFACE wrapper.
    set_target_properties(_SystemC_real PROPERTIES
      INTERFACE_LINK_LIBRARIES Threads::Threads
    )
  else()
    set_target_properties(SystemC::systemc PROPERTIES
      INTERFACE_LINK_LIBRARIES Threads::Threads
    )
  endif()

  message(STATUS "Found SystemC: ${SYSTEMC_LIBRARY}")
  message(STATUS "SystemC include directory: ${SYSTEMC_INCLUDE_DIR}")

  # Cache variables so subprojects can see them
  set(SYSTEMC_FOUND TRUE CACHE BOOL "SystemC found" FORCE)
  set(SYSTEMC_INCLUDE_DIR "${SYSTEMC_INCLUDE_DIR}" CACHE PATH "SystemC include directory" FORCE)
  set(SYSTEMC_LIBRARY "${SYSTEMC_LIBRARY}" CACHE FILEPATH "SystemC library" FORCE)
endif()

mark_as_advanced(SYSTEMC_INCLUDE_DIR SYSTEMC_LIBRARY)
