# Remove stale .gcda files before a coverage run.
# Usage: cmake -DBUILD_DIR=/path/to/build -P PeripheralCoverageClean.cmake
if(NOT DEFINED BUILD_DIR)
  message(FATAL_ERROR "PeripheralCoverageClean.cmake: BUILD_DIR is required")
endif()

file(GLOB_RECURSE _peripheral_gcda LIST_DIRECTORIES false "${BUILD_DIR}/*.gcda")
foreach(_gcda_file IN LISTS _peripheral_gcda)
  file(REMOVE "${_gcda_file}")
endforeach()
