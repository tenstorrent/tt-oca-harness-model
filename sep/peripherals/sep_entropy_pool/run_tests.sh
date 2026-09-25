#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE=Release
CLEAN=false
INSTRUMENTED_MODES=0

for arg in "$@"; do
  case "$arg" in
    --asan) BUILD_TYPE=ASAN; INSTRUMENTED_MODES=$((INSTRUMENTED_MODES + 1)) ;;
    --coverage) BUILD_TYPE=Coverage; INSTRUMENTED_MODES=$((INSTRUMENTED_MODES + 1)) ;;
    --clean) CLEAN=true ;;
    *) echo "Unknown option: $arg" >&2; exit 2 ;;
  esac
done

if (( INSTRUMENTED_MODES > 1 )); then
  echo "--asan and --coverage are mutually exclusive" >&2
  exit 2
fi

BUILD_DIR="${SCRIPT_DIR}/build/$(echo "${BUILD_TYPE}" | tr '[:upper:]' '[:lower:]')"
${CLEAN} && rm -rf "${BUILD_DIR}"

# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../setup_build_env.sh"
peripheral_setup_build_env || exit 1
if ! ${CLEAN} && peripheral_cache_stale "${BUILD_DIR}"; then
  rm -rf "${BUILD_DIR}"
fi

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DBUILD_TESTS=ON \
  "${CMAKE_EXTRA_ARGS[@]}"
cmake --build "${BUILD_DIR}" --parallel "$(peripheral_parallel_jobs)"

if [[ "${BUILD_TYPE}" == Coverage ]]; then
  cmake --build "${BUILD_DIR}" --target coverage
  peripheral_enforce_coverage_gate "${BUILD_DIR}"
elif [[ "${BUILD_TYPE}" == ASAN ]]; then
  ASAN_LEAKS=1
  if [[ "$(uname -s)" == Darwin ]]; then
    ASAN_LEAKS=0
  fi
  ASAN_OPTIONS="halt_on_error=0:detect_leaks=${ASAN_LEAKS}:log_path=${BUILD_DIR}/asan.log" \
    ctest --test-dir "${BUILD_DIR}" --output-on-failure -V
  "${SCRIPT_DIR}/../../../smc/scripts/enforce_asan_clean.sh" "${BUILD_DIR}"
else
  ctest --test-dir "${BUILD_DIR}" --output-on-failure -V
fi
