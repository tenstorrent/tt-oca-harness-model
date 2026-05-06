#!/bin/bash

# Build script for SFDP project
# Uses CMake to build both SFDP and XSPI Target modules

set -e  # Exit on error

echo "Building SFDP project..."

# Find CMake - check multiple locations
CMAKE_BIN=""
if command -v cmake &> /dev/null; then
    CMAKE_BIN=$(command -v cmake)
elif [ -f "/localdev/ctr-sshetty/miniconda3/bin/cmake" ]; then
    CMAKE_BIN="/localdev/ctr-sshetty/miniconda3/bin/cmake"
elif [ -f "$HOME/miniconda3/bin/cmake" ]; then
    CMAKE_BIN="$HOME/miniconda3/bin/cmake"
elif [ -f "/usr/bin/cmake" ]; then
    CMAKE_BIN="/usr/bin/cmake"
fi

# Check if CMake was found
if [ -z "$CMAKE_BIN" ] || [ ! -f "$CMAKE_BIN" ]; then
    echo "Error: CMake is required but not found."
    echo "Please install CMake (version 3.10 or later) to build this project."
    echo "Checked locations:"
    echo "  - PATH: $(command -v cmake 2>/dev/null || echo 'not found')"
    echo "  - /localdev/ctr-sshetty/miniconda3/bin/cmake"
    echo "  - $HOME/miniconda3/bin/cmake"
    echo "  - /usr/bin/cmake"
    exit 1
fi

# Verify CMake version
CMAKE_VERSION=$($CMAKE_BIN --version | head -n1 | sed 's/.*version //' | cut -d' ' -f1)
CMAKE_MAJOR=$(echo $CMAKE_VERSION | cut -d'.' -f1)
CMAKE_MINOR=$(echo $CMAKE_VERSION | cut -d'.' -f2)

if [ "$CMAKE_MAJOR" -lt 3 ] || ([ "$CMAKE_MAJOR" -eq 3 ] && [ "$CMAKE_MINOR" -lt 10 ]); then
    echo "Error: CMake version 3.10 or later is required."
    echo "Found version: $CMAKE_VERSION"
    exit 1
fi

echo "Using CMake build system..."
echo "  CMake: $CMAKE_BIN (version $CMAKE_VERSION)"

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Configure and build
$CMAKE_BIN ..
make

echo ""
echo "Build successful!"
echo "  - SFDP Standalone: build/bin/sfdp_standalone"
echo "  - XSPI Target: build/bin/xspi_target"
echo ""
echo "Run with:"
echo "  ./build/bin/sfdp_standalone"
echo "  ./build/bin/xspi_target"
