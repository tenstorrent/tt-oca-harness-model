# ============================================================================
# XSPI Target Module CMakeLists.txt
# ============================================================================

# Source files
set(XSPI_TARGET_SOURCES
    xspi_target.cpp
)

set(XSPI_TARGET_HEADERS
    xspi_target.h
)

# Build XSPI Target executable
# Links against sfdp_lib from the sfdp module
add_executable(xspi_target ${XSPI_TARGET_SOURCES} ${XSPI_TARGET_HEADERS})
target_link_libraries(xspi_target PRIVATE sfdp_lib)

# Also need to link sfdp_utils.cpp for utility functions
target_sources(xspi_target PRIVATE ../lib/sfdp_utils.cpp)

set_target_properties(xspi_target PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

