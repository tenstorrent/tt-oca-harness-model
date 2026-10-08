#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# Build and test the drbg_edn_adapter model.
#
# Usage:
#   ./run_tests.sh              # Release build + run tests (default)
#   ./run_tests.sh --debug      # Debug build + run tests
#   ./run_tests.sh --asan       # AddressSanitizer build + run tests
#   ./run_tests.sh --coverage   # Coverage build + lcov report
#   ./run_tests.sh --ctest      # Run via CTest with verbose output
#   ./run_tests.sh --clean      # Remove build directory before building
#
# --asan and --coverage are mutually exclusive.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE="Release"
RUN_CTEST=false
CLEAN=false

BUILD_TYPE_SET=""

# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../setup_build_env.sh"

for arg in "$@"; do
  case "$arg" in
    --debug)    peripheral_set_build_type "$arg" "Debug" ;;
    --asan)     peripheral_set_build_type "$arg" "ASAN" ;;
    --coverage) peripheral_set_build_type "$arg" "Coverage" ;;
    --ctest)    RUN_CTEST=true ;;
    --clean)    CLEAN=true ;;
    *) echo "Unknown option: $arg"; exit 1 ;;
  esac
done

BUILD_DIR="${SCRIPT_DIR}/build/$(echo "${BUILD_TYPE}" | tr '[:upper:]' '[:lower:]')"

if ${CLEAN}; then
  echo "Cleaning ${BUILD_DIR} ..."
  rm -rf "${BUILD_DIR}"
fi

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

TB="${BUILD_DIR}/bin/drbg_edn_adapter_testbench"

# num_endpoints outside 1..32 must abort elaboration with a diagnostic.
expect_fatal() {
  local n="$1" out rc=0
  out="$("${TB}" --bad-endpoints "${n}" 2>&1)" || rc=$?
  if [ "${rc}" -ne 0 ] || ! grep -q "PASS: num_endpoints=${n} fatal" <<<"${out}"; then
    echo "${out}"
    echo "FAIL: num_endpoints=${n} did not abort elaboration (rc=${rc})"
    exit 1
  fi
  echo "  [PASS] num_endpoints=${n} is fatal"
}

echo ""
expect_fatal 0
expect_fatal 33
if [ "${BUILD_TYPE}" = "Coverage" ]; then
  cmake --build "${BUILD_DIR}" --target coverage
  peripheral_enforce_coverage_gate "${BUILD_DIR}"
elif [ "${BUILD_TYPE}" = "ASAN" ]; then
  peripheral_enforce_asan_clean "${TB}" "${BUILD_DIR}"
elif ${RUN_CTEST}; then
  ctest --test-dir "${BUILD_DIR}" --output-on-failure -V
else
  "${TB}"
fi
