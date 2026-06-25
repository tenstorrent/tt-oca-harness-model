# Shared coverage linker flags for peripheral standalone builds.
# macOS/clang has no libgcov; -lgcov in CMAKE_EXE_LINKER_FLAGS breaks FindThreads
# try_compile when set before find_package(Threads).
#
# Linker flags are applied via cmake_language(DEFER) so find_package(Threads) runs
# first during configure, then --coverage is appended for the actual link step.

# Detect lcov version; --ignore-errors <tokens> only exists in lcov >= 1.15.
# LCOV_IGNORE_FLAGS / GENHTML_IGNORE_FLAGS are empty lists on older installs so
# peripheral coverage targets can just do:  lcov ${LCOV_IGNORE_FLAGS} --capture ...
find_program(_LCOV_EXE lcov)
if(_LCOV_EXE)
  execute_process(
    COMMAND ${_LCOV_EXE} --version
    OUTPUT_VARIABLE _lcov_ver_out
    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
  string(REGEX MATCH "[0-9]+\\.[0-9]+" LCOV_VERSION "${_lcov_ver_out}")
endif()
unset(_LCOV_EXE)
unset(_lcov_ver_out)

if(LCOV_VERSION VERSION_GREATER_EQUAL "1.15")
  set(LCOV_IGNORE_FLAGS   --ignore-errors inconsistent,unsupported,format,mismatch)
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
