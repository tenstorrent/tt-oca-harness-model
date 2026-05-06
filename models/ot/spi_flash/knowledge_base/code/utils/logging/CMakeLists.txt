cmake_minimum_required(VERSION 3.22)

# Set project name
project(utils_logging VERSION 1.0.0 LANGUAGES CXX)

# Set compiler flags for shared library
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

# Include directories
include_directories(include)

# Source files
set(LOGGING_SOURCES
    src/Logger.cpp
    src/LoggerConfig.cpp
)

# Header files
set(LOGGING_HEADERS
    include/Logger.h
    include/LoggerConfig.h
)

# Create shared library
add_library(utils_logging SHARED ${LOGGING_SOURCES} ${LOGGING_HEADERS})

# Set library properties
set_target_properties(utils_logging PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "${LOGGING_HEADERS}"
)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(utils_logging PRIVATE -Wall -Wextra -Wpedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(utils_logging PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Threading support
find_package(Threads REQUIRED)
target_link_libraries(utils_logging Threads::Threads)

# Install targets
install(TARGETS utils_logging
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/utils/logging
)

# Install headers
install(FILES ${LOGGING_HEADERS}
    DESTINATION include/utils/logging
)

# Create pkg-config file
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/utils_logging.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/utils_logging.pc"
    @ONLY
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/utils_logging.pc"
    DESTINATION lib/pkgconfig
)
