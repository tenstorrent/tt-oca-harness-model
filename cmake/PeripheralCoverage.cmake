# Shared coverage linker flags for peripheral standalone builds.
# macOS/clang has no libgcov; -lgcov in CMAKE_EXE_LINKER_FLAGS breaks FindThreads
# try_compile when set before find_package(Threads).
#
# Linker flags are applied via cmake_language(DEFER) so find_package(Threads) runs
# first during configure, then --coverage is appended for the actual link step.

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
