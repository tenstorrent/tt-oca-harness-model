# Only one platform target exists; list it explicitly so CMake
# re-evaluates when files change (GLOB does not trigger re-configure).
add_subdirectory(sep)
add_subdirectory(smc)