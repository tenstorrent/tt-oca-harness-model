#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# Build and test the VeeR EL2 CPU TLM wrapper (VeeRISSTlm).
#
# Usage:
#   ./run_tests.sh              # Release build + run tests (default)
#   ./run_tests.sh --debug      # Debug build + run tests
#   ./run_tests.sh --asan       # AddressSanitizer build + run tests
#   ./run_tests.sh --coverage   # Coverage build + lcov report (≥95% on wrapper)
#   ./run_tests.sh --ctest      # Run via CTest with verbose output
#   ./run_tests.sh --clean      # Remove build directory before building
#   ./run_tests.sh --no-smepmp  # Model a core built without Smepmp (RV_SMEPMP=0)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE="Release"
RUN_CTEST=false
CLEAN=false
SMEPMP=ON
COVERAGE_MIN_LINE_PCT="${COVERAGE_MIN_LINE_PCT:-95}"

for arg in "$@"; do
  case "$arg" in
    --debug)    BUILD_TYPE="Debug" ;;
    --asan)     BUILD_TYPE="ASAN" ;;
    --coverage) BUILD_TYPE="Coverage" ;;
    --ctest)    RUN_CTEST=true ;;
    --clean)    CLEAN=true ;;
    --no-smepmp) SMEPMP=OFF ;;
    *) echo "Unknown option: $arg"; exit 1 ;;
  esac
done

BUILD_DIR="${SCRIPT_DIR}/build/$(echo "${BUILD_TYPE}" | tr '[:upper:]' '[:lower:]')"
if [[ "${SMEPMP}" == "OFF" ]]; then
  BUILD_DIR="${BUILD_DIR}-nosmepmp"
fi

if ${CLEAN}; then
  echo "Cleaning ${BUILD_DIR} ..."
  rm -rf "${BUILD_DIR}"
fi

# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../peripherals/setup_build_env.sh"
peripheral_setup_build_env || exit 1

if ! ${CLEAN} && peripheral_cache_stale "${BUILD_DIR}"; then
  echo "Removing stale cmake cache (install paths or C++ standard changed) ..."
  rm -rf "${BUILD_DIR}"
fi

mkdir -p "${BUILD_DIR}"
cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DBUILD_TESTS=ON \
  -DVEERISS_SMEPMP="${SMEPMP}" \
  "${CMAKE_EXTRA_ARGS[@]}"

cmake --build "${BUILD_DIR}" --parallel "$(peripheral_parallel_jobs)"

echo ""
if [ "${BUILD_TYPE}" = "Coverage" ]; then
  cmake --build "${BUILD_DIR}" --target coverage
  cov_log="${BUILD_DIR}/coverage/coverage_filtered.info"
  if [[ -f "${cov_log}" ]] && command -v lcov >/dev/null 2>&1; then
    # lcov --extract / --summary print "  lines.......: 96.2% (...)"
    # lcov --list is a table and does not contain that token.
    pct="$(lcov --summary "${cov_log}" 2>/dev/null \
          | sed -n 's/.*lines\.*:[[:space:]]*\([0-9][0-9]*\.[0-9][0-9]*\)%.*/\1/p' \
          | tail -1)"
    echo ">> Line coverage (VeeR-ISSTlm.cpp): ${pct:-n/a}%  (gate: ≥ ${COVERAGE_MIN_LINE_PCT}%)"
    if [[ -z "${pct}" ]]; then
      echo ">> Coverage gate FAIL (no percentage parsed)"
      exit 1
    fi
    awk -v p="${pct}" -v m="${COVERAGE_MIN_LINE_PCT}" 'BEGIN { exit (p+0 < m+0) }' \
      || { echo ">> Coverage gate FAIL (${pct}% < ${COVERAGE_MIN_LINE_PCT}%)"; exit 1; }
    echo ">> Coverage gate PASS"
  fi
elif ${RUN_CTEST}; then
  ctest --test-dir "${BUILD_DIR}" --output-on-failure -V
else
  "${BUILD_DIR}/bin/veeriss_tb"
fi
