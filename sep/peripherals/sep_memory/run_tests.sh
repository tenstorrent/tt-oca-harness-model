#!/usr/bin/env bash
# Build-only check for the sep_memory model.
# No standalone unit tests exist; functional testing is done at VP level.
#
# Usage:
#   ./run_tests.sh              # Release build
#   ./run_tests.sh --debug      # Debug build
#   ./run_tests.sh --docs       # Build Doxygen documentation
#   ./run_tests.sh --cppcheck  # Run cppcheck static analysis
#   ./run_tests.sh --clean      # Remove build directory first

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE="Release"
RUN_DOCS=false
RUN_CPPCHECK=false
CLEAN=false

for arg in "$@"; do
  case "$arg" in
    --debug)  BUILD_TYPE="Debug" ;;
    --docs)   RUN_DOCS=true ;;
    --cppcheck) RUN_CPPCHECK=true ;;
    --clean)  CLEAN=true ;;
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

mkdir -p "${BUILD_DIR}"
cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

echo ""
if ${RUN_DOCS}; then
  cmake --build "${BUILD_DIR}" --target sep_memory_docs
elif ${RUN_CPPCHECK}; then
  cmake --build "${BUILD_DIR}" --target sep_memory_cppcheck
else
  echo "Build complete (${BUILD_TYPE}): ${BUILD_DIR}/libsep_memory_model.a"
fi
