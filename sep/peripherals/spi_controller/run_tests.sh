#!/usr/bin/env bash
# Build and test the spi_controller model.
#
# Usage:
#   ./run_tests.sh              # Release build + run tests (default)
#   ./run_tests.sh --debug      # Debug build + run tests
#   ./run_tests.sh --asan       # AddressSanitizer build + run tests
#   ./run_tests.sh --coverage   # Coverage build + lcov report
#   ./run_tests.sh --ctest      # Run via CTest with verbose output
#   ./run_tests.sh --docs      # Build Doxygen documentation
#   ./run_tests.sh --cppcheck  # Run cppcheck static analysis
#   ./run_tests.sh --clean      # Remove build directory before building
#
# Flags may be combined, e.g.: ./run_tests.sh --debug --clean

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE="Release"
RUN_CTEST=false
RUN_DOCS=false
RUN_CPPCHECK=false
CLEAN=false

for arg in "$@"; do
  case "$arg" in
    --debug)    BUILD_TYPE="Debug" ;;
    --asan)     BUILD_TYPE="ASAN" ;;
    --coverage) BUILD_TYPE="Coverage" ;;
    --ctest)    RUN_CTEST=true ;;
    --docs)     RUN_DOCS=true ;;
    --cppcheck) RUN_CPPCHECK=true ;;
    --clean)    CLEAN=true ;;
    *) echo "Unknown option: $arg"; exit 1 ;;
  esac
done

BUILD_DIR="${SCRIPT_DIR}/build/$(echo "${BUILD_TYPE}" | tr '[:upper:]' '[:lower:]')"

if ${CLEAN}; then
  echo "Cleaning ${BUILD_DIR} ..."
  rm -rf "${BUILD_DIR}"
fi

# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../setup_build_env.sh"
peripheral_setup_build_env || exit 1

if ! ${CLEAN} && peripheral_cache_stale "${BUILD_DIR}"; then
  echo "Removing stale cmake cache (install paths or C++ standard changed) ..."
  rm -rf "${BUILD_DIR}"
fi

mkdir -p "${BUILD_DIR}"
cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DBUILD_TESTS=ON \
  "${CMAKE_EXTRA_ARGS[@]}"

cmake --build "${BUILD_DIR}" --parallel "$(peripheral_parallel_jobs)"

echo ""
if ${RUN_DOCS}; then
  cmake --build "${BUILD_DIR}" --target spi_controller_docs
elif ${RUN_CPPCHECK}; then
  cmake --build "${BUILD_DIR}" --target spi_controller_cppcheck
elif [ "${BUILD_TYPE}" = "Coverage" ]; then
  cmake --build "${BUILD_DIR}" --target coverage
elif ${RUN_CTEST}; then
  ctest --test-dir "${BUILD_DIR}" --output-on-failure -V
else
  "${BUILD_DIR}/bin/spi_controller_test"
fi
