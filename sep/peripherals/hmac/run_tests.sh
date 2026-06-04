#!/usr/bin/env bash
# Build and test the hmac model.
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

# Resolve SYSTEMC_HOME
if [ -z "${SYSTEMC_HOME:-}" ]; then
  if [ -d "/usr/local/systemc300" ]; then
    export SYSTEMC_HOME="/usr/local/systemc300"
  elif [ -d "/usr/local/systemc" ]; then
    export SYSTEMC_HOME="/usr/local/systemc"
  else
    echo "WARNING: SYSTEMC_HOME not set and no default path found."
  fi
fi

# Resolve CCI_HOME
if [ -z "${CCI_HOME:-}" ]; then
  if [ -d "/usr/local/cci" ]; then
    export CCI_HOME="/usr/local/cci"
  fi
fi

mkdir -p "${BUILD_DIR}"
cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DBUILD_TESTS=ON

cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

echo ""
if ${RUN_DOCS}; then
  cmake --build "${BUILD_DIR}" --target hmac_docs
elif ${RUN_CPPCHECK}; then
  cmake --build "${BUILD_DIR}" --target hmac_cppcheck
elif [ "${BUILD_TYPE}" = "Coverage" ]; then
  cmake --build "${BUILD_DIR}" --target coverage
elif ${RUN_CTEST}; then
  ctest --test-dir "${BUILD_DIR}" --output-on-failure -V
else
  "${BUILD_DIR}/bin/hmac_test"
fi
