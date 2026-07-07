cmake_minimum_required(VERSION 3.14)
project(softfloat C)

# Berkeley SoftFloat — RISCV specialisation, built natively for the host.
# The original CMakeLists delegated to an external Makefile (build/RISCV-GCC/Makefile)
# that is not tracked in git.  This replacement compiles the C sources directly
# using CMake so the library builds on any host without extra tooling.

set(SOFTFLOAT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/source")

file(GLOB SOFTFLOAT_SRCS "${SOFTFLOAT_SOURCE_DIR}/*.c")

# SOFTFLOAT_FAST_INT64 — exclude ALL multi-word (M-suffix) variants.
# Every M-variant file ends with "M.c" (e.g. s_addF128M.c, extF80M_to_f128M.c,
# s_shiftRightM.c).  These reference symbols only available in non-FAST_INT64
# builds and also require specialisation headers that define those symbols.
list(FILTER SOFTFLOAT_SRCS EXCLUDE REGEX ".*M\\.c$")

# SOFTFLOAT_FAST_INT64 build uses only the non-M (non-multi-word) RISCV
# specialisation files.  The *ExtF80M* and *F128M* files reference symbols
# that are only defined in non-FAST_INT64 builds and must be excluded.
set(RISCV_SRCS
    ${SOFTFLOAT_SOURCE_DIR}/RISCV/s_commonNaNToExtF80UI.c
    ${SOFTFLOAT_SOURCE_DIR}/RISCV/s_commonNaNToF128UI.c
    ${SOFTFLOAT_SOURCE_DIR}/RISCV/s_commonNaNToF16UI.c
    ${SOFTFLOAT_SOURCE_DIR}/RISCV/s_commonNaNToF32UI.c
    ${SOFTFLOAT_SOURCE_DIR}/RISCV/s_commonNaNToF64UI.c
    ${SOFTFLOAT_SOURCE_DIR}/RISCV/s_extF80UIToCommonNaN.c
    ${SOFTFLOAT_SOURCE_DIR}/RISCV/s_f128UIToCommonNaN.c
    ${SOFTFLOAT_SOURCE_DIR}/RISCV/s_f16UIToCommonNaN.c
    ${SOFTFLOAT_SOURCE_DIR}/RISCV/s_f32UIToCommonNaN.c
    ${SOFTFLOAT_SOURCE_DIR}/RISCV/s_f64UIToCommonNaN.c
    ${SOFTFLOAT_SOURCE_DIR}/RISCV/s_propagateNaNExtF80UI.c
    ${SOFTFLOAT_SOURCE_DIR}/RISCV/s_propagateNaNF128UI.c
    ${SOFTFLOAT_SOURCE_DIR}/RISCV/s_propagateNaNF16UI.c
    ${SOFTFLOAT_SOURCE_DIR}/RISCV/s_propagateNaNF32UI.c
    ${SOFTFLOAT_SOURCE_DIR}/RISCV/s_propagateNaNF64UI.c
    ${SOFTFLOAT_SOURCE_DIR}/RISCV/softfloat_raiseFlags.c
)

# Register in-tree softfloat artifacts for CMake "make clean" (built outside vp/build).
file(GLOB _softfloat_clean_artifacts CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/build/RISCV-GCC/*.o"
    "${CMAKE_CURRENT_SOURCE_DIR}/build/RISCV-GCC/*.a"
    "${CMAKE_CURRENT_SOURCE_DIR}/build/Darwin-GCC/*.o"
    "${CMAKE_CURRENT_SOURCE_DIR}/build/Darwin-GCC/*.a"
)

add_library(softfloat STATIC ${SOFTFLOAT_SRCS} ${RISCV_SRCS})

target_include_directories(softfloat PUBLIC
    "${SOFTFLOAT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/build/RISCV-GCC"
    "${SOFTFLOAT_SOURCE_DIR}/RISCV"
)

target_compile_options(softfloat PRIVATE
    -DSOFTFLOAT_ROUND_ODD
    -DSOFTFLOAT_FAST_INT64
    -w
)

if(APPLE OR NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  # Neither AppleClang nor GCC at -O0 reliably inline away the plain-"inline"
  # softfloat_countLeadingZeros{16,32,64} helpers (see build/RISCV-GCC/platform.h),
  # which leaves them with no out-of-line body and fails to link — most visibly
  # in Debug (-O0) builds on either toolchain. Fall back to the portable
  # generic implementations in that case; GCC Release (-O2/-O3) inlines every
  # call site away and never needs this fallback.
  target_compile_definitions(softfloat PRIVATE SOFTFLOAT_NO_BUILTIN_CLZ)
endif()

set_target_properties(softfloat PROPERTIES
    C_STANDARD 99
    ADDITIONAL_CLEAN_FILES "${_softfloat_clean_artifacts}"
)
