#!/usr/bin/env bash
# Build-only check for the VeeR EL2 CPU model.
# No standalone unit tests exist; functional testing is done at VP level.
#
# Usage:
#   ./run_tests.sh              # Release build
#   ./run_tests.sh --debug      # Debug build
#   ./run_tests.sh --clean      # Remove build directory first

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
BUILD_TYPE="Release"

for arg in "$@"; do
  case "$arg" in
    --debug)  BUILD_TYPE="Debug" ;;
    --clean)  rm -rf "${BUILD_DIR}" ;;
    *) echo "Unknown option: $arg"; exit 1 ;;
  esac
done

# Resolve SYSTEMC_HOME
if [ -z "${SYSTEMC_HOME:-}" ] && [ -n "${SYSTEMC_HOME:-}" ]; then
  export SYSTEMC_HOME
elif [ -d "/usr/local/systemc300" ]; then
  export SYSTEMC_HOME="/usr/local/systemc300"
fi

mkdir -p "${BUILD_DIR}"
cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

echo ""
echo "Build complete: ${BUILD_DIR}/libveeriss_model.a"
echo "For functional testing, run firmware tests under sw/sep-vp-tests/"
