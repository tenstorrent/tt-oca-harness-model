#!/usr/bin/env bash
# Build and test the sep_virt_console model.
#
# Usage:
#   ./run_tests.sh              # Release build + run CTest (default)
#   ./run_tests.sh --debug      # Debug build + run CTest
#   ./run_tests.sh --asan       # AddressSanitizer build + run CTest
#   ./run_tests.sh --coverage   # Coverage build
#   ./run_tests.sh --clean      # Remove build directory before building
#
# Flags may be combined, e.g.: ./run_tests.sh --asan --clean

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE="Release"
CLEAN=false

for arg in "$@"; do
  case "$arg" in
    --debug)    BUILD_TYPE="Debug" ;;
    --asan)     BUILD_TYPE="ASAN" ;;
    --coverage) BUILD_TYPE="Coverage" ;;
    --clean)    CLEAN=true ;;
    *) echo "Unknown option: $arg"; exit 1 ;;
  esac
done

BUILD_DIR="${SCRIPT_DIR}/build/$(echo "${BUILD_TYPE}" | tr '[:upper:]' '[:lower:]')"

if ${CLEAN}; then
  echo "Cleaning ${BUILD_DIR} ..."
  rm -rf "${BUILD_DIR}"
fi

# Resolve SYSTEMC_HOME (probe common locations, including this repo's local install).
if [ -z "${SYSTEMC_HOME:-}" ]; then
  for d in \
    "${SCRIPT_DIR}/../../../../local/systemc-3.0.1" \
    /usr/local/systemc300 /usr/local/systemc; do
    if [ -d "$d" ]; then export SYSTEMC_HOME="$(cd "$d" && pwd)"; break; fi
  done
  [ -z "${SYSTEMC_HOME:-}" ] && echo "WARNING: SYSTEMC_HOME not set and no default path found."
fi

# Resolve CCI_HOME similarly.
if [ -z "${CCI_HOME:-}" ]; then
  for d in \
    "${SCRIPT_DIR}/../../../../local/cci-1.0.1" \
    /usr/local/cci; do
    if [ -d "$d" ]; then export CCI_HOME="$(cd "$d" && pwd)"; break; fi
  done
fi

mkdir -p "${BUILD_DIR}"
cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DBUILD_TESTS=ON

cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

echo ""
if [ "${BUILD_TYPE}" = "Coverage" ]; then
  cmake --build "${BUILD_DIR}" --target coverage || true
else
  ctest --test-dir "${BUILD_DIR}" --output-on-failure
fi
