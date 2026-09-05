#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# Shared build environment for sep/peripherals/* run_tests.sh and run_all_peripherals.sh.
#
# REQUIRED: vp/configure_vp.sh must resolve valid install paths before peripheral tests run.
#   - Edit defaults in vp/configure_vp.sh (SYSTEMC_HOME, CCI_HOME, OPENSSL_ROOT, BOOST_ROOT), or
#   - Export those variables (and CMAKE_CXX_STANDARD if not C++20) in the shell / CI job.
#
# This script quietly sources vp/configure_vp.sh and exposes CMAKE_EXTRA_ARGS for cmake configure.
# Individual run_tests.sh scripts source this file automatically; you do not need to
# `source vp/configure_vp.sh` separately unless running cmake by hand.

if [ -n "${BASH_VERSION:-}" ]; then
  _PERIPH_SETUP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
else
  _PERIPH_SETUP_DIR="$(cd "$(dirname "${(%):-%N}")" && pwd)"
fi

# Resolve install paths (SystemC, CCI, OpenSSL, Boost) from vp/configure_vp.sh.
peripheral_setup_build_env() {
  export VP_CONFIGURE_QUIET=1
  # shellcheck disable=SC1091
  source "${_PERIPH_SETUP_DIR}/../../vp/configure_vp.sh"

  CMAKE_EXTRA_ARGS=(
    -DCMAKE_CXX_STANDARD="${CMAKE_CXX_STANDARD}"
    -DCMAKE_CXX_STANDARD_REQUIRED=ON
    -DSYSTEMC_HOME="${SYSTEMC_HOME}"
    -DCCI_HOME="${CCI_HOME}"
    -DSystemCLanguage_DIR="${SYSTEMC_HOME}/lib/cmake/SystemCLanguage"
    -DSystemCCCI_DIR="${CCI_HOME}/lib/cmake/SystemCCCI"
    -DBOOST_ROOT="${BOOST_ROOT}" -DBoost_ROOT="${BOOST_ROOT}"
    -DOPENSSL_ROOT_DIR="${OPENSSL_ROOT}"
    -DBoost_NO_SYSTEM_PATHS=ON -DBoost_NO_BOOST_CMAKE=ON
  )
  export CMAKE_EXTRA_ARGS
}

# Drop stale cmake cache when install paths or C++ standard change.
peripheral_cache_stale() {
  local cache="$1/CMakeCache.txt"
  [[ -f "${cache}" ]] || return 1
  local cached_sc cached_ssl cached_cci cached_std cached_lcov
  cached_sc="$(grep -E '^SYSTEMC_HOME:' "${cache}" 2>/dev/null | sed 's/^SYSTEMC_HOME:[^=]*=//' || true)"
  cached_ssl="$(grep -E '^OPENSSL_INCLUDE_DIR:' "${cache}" 2>/dev/null | sed 's/^OPENSSL_INCLUDE_DIR:[^=]*=//' || true)"
  cached_cci="$(grep -E '^CCI_INCLUDE_DIR:' "${cache}" 2>/dev/null | sed 's/^CCI_INCLUDE_DIR:[^=]*=//' || true)"
  cached_std="$(grep -E '^CMAKE_CXX_STANDARD:' "${cache}" 2>/dev/null | sed 's/^CMAKE_CXX_STANDARD:[^=]*=//' || true)"
  cached_lcov="$(grep -E '^LCOV_EXECUTABLE:' "${cache}" 2>/dev/null | sed 's/^LCOV_EXECUTABLE:[^=]*=//' || true)"
  [[ -n "${cached_sc}" && "${cached_sc}" != "${SYSTEMC_HOME}" ]] && return 0
  [[ -n "${cached_ssl}" && "${cached_ssl}" != "${OPENSSL_INC}" ]] && return 0
  [[ -n "${cached_cci}" && "${cached_cci}" != "${CCI_HOME}/include" ]] && return 0
  [[ -n "${cached_std}" && "${cached_std}" != "${CMAKE_CXX_STANDARD}" ]] && return 0
  # run_all_peripherals.sh points LCOV_EXECUTABLE at a per-PID wrapper under
  # TMPDIR; once that is reaped the cached path breaks every later coverage run.
  [[ -n "${cached_lcov}" && ! -x "${cached_lcov}" ]] && return 0
  return 1
}

# Fail if filtered model line coverage is below COVERAGE_MIN_LINE_PCT (default 95).
peripheral_enforce_coverage_gate() {
  local build_dir="$1"
  local info="${2:-${build_dir}/coverage/coverage_filtered.info}"
  local min="${COVERAGE_MIN_LINE_PCT:-95}"
  local pct
  if [[ ! -f "${info}" ]]; then
    echo ">> Coverage gate FAIL (missing lcov info: ${info})" >&2
    return 1
  fi
  if ! command -v lcov >/dev/null 2>&1; then
    echo ">> Coverage gate FAIL (lcov not found)" >&2
    return 1
  fi
  pct="$(lcov --list "${info}" 2>/dev/null | sed -n 's/.*lines\.*:[[:space:]]*\([0-9][0-9]*\.[0-9][0-9]*\)%.*/\1/p' | tail -1)"
  if [[ -z "${pct}" ]]; then
    echo ">> Coverage gate FAIL (could not parse ${info})" >&2
    return 1
  fi
  echo ">> Line coverage: ${pct}%  (gate: ≥ ${min}%)"
  if awk -v p="${pct}" -v m="${min}" 'BEGIN { exit (p+0 < m+0) }'; then
    echo ">> Coverage gate PASS"
    return 0
  fi
  echo "ERROR: line coverage ${pct}% is below the ${min}% gate." >&2
  return 1
}

peripheral_parallel_jobs() {
  if [[ -n "${MAX_JOBS:-}" ]]; then
    echo "${MAX_JOBS}"
    return
  fi
  if command -v nproc >/dev/null 2>&1; then
    nproc
  elif command -v sysctl >/dev/null 2>&1; then
    sysctl -n hw.ncpu 2>/dev/null || echo 4
  else
    echo 4
  fi
}
