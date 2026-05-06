cmake_minimum_required(VERSION 3.15)
project(MyProject C)

# ==============================
# Options / Variables
# ==============================
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../models/veer-iss/VeeR-ISS/softfloat")  
set(SPECIALIZE_TYPE "RISCV")
#set(SOFTFLOAT_MAKE_DIR "${SOURCE_DIR}/build/RISCV-GCC")  # Adjust if Makefile is elsewhere
set(SOFTFLOAT_MAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../models/veer-iss/VeeR-ISS/softfloat/build/RISCV-GCC")  # Adjust if Makefile is elsewhere

# ==============================
# Step 1: Call existing Makefile
# ==============================
add_custom_target(build_softfloat ALL
    COMMAND make -C ${SOFTFLOAT_MAKE_DIR}
    COMMENT "Building softfloat library using existing Makefile"
    WORKING_DIRECTORY ${SOFTFLOAT_MAKE_DIR}
)

# ==============================
# Step 2: Import the built library
# ==============================
add_library(softfloat STATIC IMPORTED GLOBAL)
set_target_properties(softfloat PROPERTIES
    IMPORTED_LOCATION "${SOFTFLOAT_MAKE_DIR}/softfloat.a"
)

# Make sure the library target depends on the Makefile target
add_dependencies(softfloat build_softfloat)

# ==============================
# Step 3: Include directories
# ==============================
target_include_directories(softfloat INTERFACE
    ${SOURCE_DIR}/source/include
    ${SOURCE_DIR}//source/${SPECIALIZE_TYPE}
)
