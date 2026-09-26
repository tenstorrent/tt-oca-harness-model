# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# Shared coverage support for peripheral standalone builds.
#
# macOS/clang: no libgcov; deferred linker flags avoid FindThreads failures.
# macOS Coverage: OBJECT libraries merge model code into the test binary so
# llvm-prof writes .gcda reliably (STATIC libs often produce corrupt merges).

set(PERIPHERAL_COVERAGE_MODULE_DIR "${CMAKE_CURRENT_LIST_DIR}")

find_program(LCOV_EXECUTABLE lcov)
find_program(GENHTML_EXECUTABLE genhtml)

if(LCOV_EXECUTABLE)
  execute_process(
    COMMAND ${LCOV_EXECUTABLE} --version
    OUTPUT_VARIABLE _lcov_ver_out
    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
  string(REGEX MATCH "[0-9]+\\.[0-9]+" LCOV_VERSION "${_lcov_ver_out}")
endif()
unset(_lcov_ver_out)

if(LCOV_VERSION VERSION_GREATER_EQUAL "1.15")
  # 'unused' is required because --extract already narrows the trace to src/ and
  # include/, so any later --remove pattern matches nothing and lcov 2.x makes
  # that a fatal error rather than a warning.
  set(LCOV_IGNORE_FLAGS   --ignore-errors inconsistent,unsupported,format,mismatch,unused)
  set(GENHTML_IGNORE_FLAGS --ignore-errors inconsistent,unsupported,format,corrupt,category)
else()
  set(LCOV_IGNORE_FLAGS   "")
  set(GENHTML_IGNORE_FLAGS "")
endif()

function(_peripheral_coverage_apply_link_flags)
  if(APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
  else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage -lgcov")
  endif()
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}" PARENT_SCOPE)
endfunction()

function(peripheral_coverage_link_flags)
  cmake_language(DEFER CALL _peripheral_coverage_apply_link_flags)
endfunction()

include(PeripheralCommon)

# Create the standard coverage target: zerocounters -> run tests -> lcov -> genhtml.
#   PRIMARY_TARGET  - CMake test executable target (required)
#   TEST_COMMANDS   - optional list of COMMAND ... steps (default: run PRIMARY_TARGET)
#   EXTRA_DEPENDS   - optional extra build dependencies
#   EXCLUDE_SRC     - optional lcov --remove patterns after --extract (e.g. optional modules)
#   SOURCE_GLOBS    - optional lcov extract globs for nonstandard source layouts
function(peripheral_add_coverage_target)
  cmake_parse_arguments(
    PCOV "" "PRIMARY_TARGET"
    "TEST_COMMANDS;EXTRA_DEPENDS;EXCLUDE_SRC;SOURCE_GLOBS" ${ARGN})

  if(NOT PCOV_PRIMARY_TARGET)
    message(FATAL_ERROR "peripheral_add_coverage_target: PRIMARY_TARGET is required")
  endif()

  if(NOT LCOV_EXECUTABLE OR NOT GENHTML_EXECUTABLE)
    message(WARNING "lcov/genhtml not found - coverage target unavailable for ${PCOV_PRIMARY_TARGET}")
    return()
  endif()

  if(NOT PCOV_TEST_COMMANDS)
    if(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
      set(_exe "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PCOV_PRIMARY_TARGET}")
    else()
      set(_exe "${CMAKE_BINARY_DIR}/bin/${PCOV_PRIMARY_TARGET}")
    endif()
    set(PCOV_TEST_COMMANDS COMMAND "${_exe}")
  endif()
  if(NOT PCOV_SOURCE_GLOBS)
    set(PCOV_SOURCE_GLOBS
      "${CMAKE_CURRENT_SOURCE_DIR}/src/*"
      "${CMAKE_CURRENT_SOURCE_DIR}/include/*")
  endif()

  set(_cov_cmds
    COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${CMAKE_BINARY_DIR}
            -P ${PERIPHERAL_COVERAGE_MODULE_DIR}/PeripheralCoverageClean.cmake
    COMMAND ${LCOV_EXECUTABLE} ${LCOV_IGNORE_FLAGS} --zerocounters --directory .
    ${PCOV_TEST_COMMANDS}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
    COMMAND ${LCOV_EXECUTABLE} ${LCOV_IGNORE_FLAGS} --capture --directory . --output-file coverage/coverage.info
    COMMAND ${LCOV_EXECUTABLE} ${LCOV_IGNORE_FLAGS}
            --extract coverage/coverage.info ${PCOV_SOURCE_GLOBS}
            --output-file=coverage/coverage_filtered.info
  )
  if(PCOV_EXCLUDE_SRC)
    list(APPEND _cov_cmds
      COMMAND ${LCOV_EXECUTABLE} ${LCOV_IGNORE_FLAGS}
              --remove coverage/coverage_filtered.info ${PCOV_EXCLUDE_SRC}
              --output-file coverage/coverage_filtered.info
    )
  endif()
  list(APPEND _cov_cmds
    COMMAND ${GENHTML_EXECUTABLE} ${GENHTML_IGNORE_FLAGS} coverage/coverage_filtered.info --output-directory coverage/html
    COMMAND ${CMAKE_COMMAND} -E echo "Coverage report at coverage/html/index.html"
    COMMAND /bin/bash ${PERIPHERAL_COVERAGE_MODULE_DIR}/coverage_gate.sh
            --lcov-info ${CMAKE_BINARY_DIR}/coverage/coverage_filtered.info
  )

  add_custom_target(coverage
    ${_cov_cmds}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating coverage report for ${PCOV_PRIMARY_TARGET}..."
  )
  add_dependencies(coverage ${PCOV_PRIMARY_TARGET})
  if(PCOV_EXTRA_DEPENDS)
    add_dependencies(coverage ${PCOV_EXTRA_DEPENDS})
  endif()
endfunction()
