# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# Shared helpers for peripheral standalone builds (all build types).
#
# macOS Coverage uses OBJECT model libraries so llvm-prof writes .gcda reliably;
# other build types use STATIC libraries.

function(peripheral_add_model_library target)
  if(APPLE AND CMAKE_BUILD_TYPE STREQUAL "Coverage")
    add_library(${target} OBJECT ${ARGN})
  else()
    add_library(${target} STATIC ${ARGN})
  endif()
endfunction()

# On macOS Coverage, also link PUBLIC OBJECT dependencies (e.g. spi_flash_core
# behind spi_flash_model). IMPORTED targets still propagate; chained OBJECT
# libs do not on CMake 3.14+.
function(_peripheral_link_object_deps test_target model_target)
  get_target_property(_link_libs ${model_target} LINK_LIBRARIES)
  if(NOT _link_libs)
    return()
  endif()
  foreach(_dep IN LISTS _link_libs)
    if(NOT TARGET "${_dep}")
      continue()
    endif()
    get_target_property(_dep_type "${_dep}" TYPE)
    if(_dep_type STREQUAL "OBJECT_LIBRARY")
      target_link_libraries(${test_target} PRIVATE "${_dep}")
      _peripheral_link_object_deps(${test_target} "${_dep}")
    endif()
  endforeach()
endfunction()

function(peripheral_link_test_to_model test_target model_target)
  target_link_libraries(${test_target} PRIVATE ${model_target})
  if(APPLE AND CMAKE_BUILD_TYPE STREQUAL "Coverage")
    _peripheral_link_object_deps(${test_target} ${model_target})
  endif()
endfunction()
