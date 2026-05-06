cmake_minimum_required(VERSION 3.10)

project(SFDP_PROJECT VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ============================================================================
# Code Coverage Support (gcov/lcov)
# ============================================================================
option(ENABLE_COVERAGE "Enable code coverage reporting (requires gcov/lcov)" OFF)

if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        # Check for gcov
        find_program(GCOV_PATH gcov)
        if(NOT GCOV_PATH)
            message(WARNING "gcov not found. Coverage reporting may not work correctly.")
        endif()
        
        # Check for lcov
        find_program(LCOV_PATH lcov)
        if(NOT LCOV_PATH)
            message(WARNING "lcov not found. HTML coverage reports will not be generated.")
            message(WARNING "Install lcov: sudo apt-get install lcov (Ubuntu/Debian)")
        endif()
        
        # Check for genhtml
        find_program(GENHTML_PATH genhtml)
        if(NOT GENHTML_PATH)
            message(WARNING "genhtml not found. HTML coverage reports will not be generated.")
        endif()
        
        # Set coverage flags
        set(CMAKE_CXX_FLAGS_COVERAGE "-g -O0 --coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_C_FLAGS_COVERAGE "-g -O0 --coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_EXE_LINKER_FLAGS_COVERAGE "--coverage")
        
        # Override build type to Coverage
        set(CMAKE_BUILD_TYPE Coverage CACHE STRING "Build type" FORCE)
        
        message(STATUS "Code coverage enabled")
        message(STATUS "  gcov: ${GCOV_PATH}")
        message(STATUS "  lcov: ${LCOV_PATH}")
        message(STATUS "  genhtml: ${GENHTML_PATH}")
    else()
        message(WARNING "Coverage is only supported with GCC compiler. Current: ${CMAKE_CXX_COMPILER_ID}")
        set(ENABLE_COVERAGE OFF)
    endif()
endif()

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Coverage")
        # Coverage flags are set above
    else()
        add_compile_options(-O2)
    endif()
endif()

# ============================================================================
# Add SFDP library subdirectory
# ============================================================================
add_subdirectory(lib)

# ============================================================================
# Add XSPI Target subdirectory
# ============================================================================
add_subdirectory(xspi_target)

# ============================================================================
# Coverage Report Generation
# ============================================================================
if(ENABLE_COVERAGE AND LCOV_PATH AND GENHTML_PATH)
    # Coverage output directory
    set(COVERAGE_DIR "${CMAKE_BINARY_DIR}/coverage")
    set(COVERAGE_HTML_DIR "${COVERAGE_DIR}/html")
    set(COVERAGE_INFO_FILE "${COVERAGE_DIR}/coverage.info")
    
    # Custom target to generate coverage report
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_HTML_DIR}
        # Run tests first to generate .gcda files
        COMMAND ${CMAKE_COMMAND} -E echo "Running tests to generate coverage data..."
        COMMAND ${CMAKE_BINARY_DIR}/bin/sfdp_tests || true
        # Capture baseline (zero out counters)
        COMMAND ${LCOV_PATH} --directory ${CMAKE_BINARY_DIR} --zerocounters -q
        # Run tests
        COMMAND ${CMAKE_COMMAND} -E echo "Running tests..."
        COMMAND ${CMAKE_BINARY_DIR}/bin/sfdp_tests || true
        # Capture coverage data
        COMMAND ${CMAKE_COMMAND} -E echo "Capturing coverage data..."
        COMMAND ${LCOV_PATH} --directory ${CMAKE_BINARY_DIR} --capture --output-file ${COVERAGE_INFO_FILE} -q
        # Remove system headers and external files from coverage
        COMMAND ${LCOV_PATH} --remove ${COVERAGE_INFO_FILE} '/usr/*' '*/test/*' '*/build/*' --output-file ${COVERAGE_INFO_FILE} -q
        # Generate HTML report
        COMMAND ${CMAKE_COMMAND} -E echo "Generating HTML report..."
        COMMAND ${GENHTML_PATH} ${COVERAGE_INFO_FILE} --output-directory ${COVERAGE_HTML_DIR} -q
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "Coverage report generated:"
        COMMAND ${CMAKE_COMMAND} -E echo "  HTML: ${COVERAGE_HTML_DIR}/index.html"
        COMMAND ${CMAKE_COMMAND} -E echo "  Info: ${COVERAGE_INFO_FILE}"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating code coverage report"
    )
    
    # Custom target to show coverage summary
    add_custom_target(coverage-summary
        COMMAND ${CMAKE_COMMAND} -E echo "Code Coverage Summary:"
        COMMAND ${LCOV_PATH} --summary ${COVERAGE_INFO_FILE} 2>/dev/null || ${CMAKE_COMMAND} -E echo "Run 'make coverage' first to generate coverage data"
        DEPENDS coverage
        COMMENT "Displaying coverage summary"
    )
    
    message(STATUS "Coverage targets available:")
    message(STATUS "  make coverage       - Generate HTML coverage report")
    message(STATUS "  make coverage-summary - Show coverage summary")
endif()

# Print configuration summary
message(STATUS "SFDP Project Build Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  Coverage: ${ENABLE_COVERAGE}")
message(STATUS "")
message(STATUS "  Targets:")
message(STATUS "    - sfdp_standalone: Standalone SFDP executable")
message(STATUS "    - sfdp_tests: SFDP unit tests")
message(STATUS "    - xspi_target: XSPI Target executable (depends on sfdp)")
if(ENABLE_COVERAGE AND LCOV_PATH AND GENHTML_PATH)
    message(STATUS "    - coverage: Generate HTML coverage report")
    message(STATUS "    - coverage-summary: Show coverage summary")
endif()
message(STATUS "")
message(STATUS "  Output directory: ${CMAKE_BINARY_DIR}/bin")
if(ENABLE_COVERAGE)
    message(STATUS "  Coverage directory: ${CMAKE_BINARY_DIR}/coverage")
endif()