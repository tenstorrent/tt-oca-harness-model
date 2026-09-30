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

# --debug / --asan / --coverage select one build type. The caller initialises
# BUILD_TYPE (default Release) and BUILD_TYPE_SET (empty) before parsing argv.
# A second, different selector is an error: last-flag-wins used to build ASAN
# and then skip the sanitizer gate because the type was no longer ASAN.
peripheral_set_build_type() {
  local flag="$1"
  local chosen="$2"
  if [ -n "${BUILD_TYPE_SET}" ] && [ "${BUILD_TYPE_SET}" != "${chosen}" ]; then
    echo "Error: ${flag} conflicts with --$(echo "${BUILD_TYPE_SET}" | tr '[:upper:]' '[:lower:]');" \
         "build types are mutually exclusive." >&2
    # Exit 2 for a usage error, matching every SMC run_tests.sh and the
    # "exits 2 if both are passed" contract in test-coverage-asan.mdc.
    exit 2
  fi
  BUILD_TYPE="${chosen}"
  BUILD_TYPE_SET="${chosen}"
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
  pct="$(awk -F: '
    /^LF:/ { found += $2 }
    /^LH:/ { hit += $2 }
    END {
      if (found <= 0) exit 1
      printf "%.1f\n", 100.0 * hit / found
    }
  ' "${info}")"
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

# Run an ASan/UBSan binary and fail on any sanitizer diagnosis.
#
# Usage: peripheral_enforce_asan_clean <binary> <build_dir> [binary_args...]
# Any argument after <build_dir> is forwarded to the testbench, so a target
# whose binary takes a CCI config path (spi_controller) uses this helper
# rather than reimplementing the gate.
#
# Sanitizers report on stderr and, by default, do not change the process exit
# code for leaks or for recoverable UBSan findings. A runner that only checks
# the exit status therefore reports PASS on a dirty run. This routes the
# diagnostics to a log and greps it, which is the same contract
# smc/scripts/enforce_asan_clean.sh enforces on the SMC side.
peripheral_enforce_asan_clean() {
  local binary="$1"
  local build_dir="$2"
  shift 2
  local log="${build_dir}/asan.log"

  # Sanitizers append .<pid>, so logs accumulate across runs and a stale failure
  # would keep failing the gate long after it was fixed.
  rm -f "${log}" "${log}".*

  # detect_leaks=1 is LeakSanitizer, which exists on Linux only; Apple Clang's
  # ASan aborts at startup if it is set. Same split the SMC runners use.
  local opts="halt_on_error=0:log_path=${log}"
  if [[ "$(uname -s)" == "Linux" ]]; then
    opts="halt_on_error=0:detect_leaks=1:log_path=${log}"
  fi

  local rc=0
  ASAN_OPTIONS="${opts}" \
  UBSAN_OPTIONS="log_path=${log}:print_stacktrace=1" \
    "${binary}" "$@" || rc=$?

  # Concatenate first: grep -c over several files prints one count per file.
  # A leak-only report says "ERROR: LeakSanitizer", and a recoverable UBSan
  # finding says only "runtime error:", so neither can be found by looking for
  # AddressSanitizer alone.
  local findings
  findings=$(cat "${log}" "${log}".* 2>/dev/null |
             grep -cE 'ERROR: (Address|Leak|Memory)Sanitizer|ERROR: UndefinedBehaviorSanitizer|runtime error:' || true)

  if [[ "${findings}" -gt 0 ]]; then
    echo ">> Sanitizer gate FAIL (${findings} finding(s))" >&2
    cat "${log}" "${log}".* 2>/dev/null >&2
    return 1
  fi
  if [[ "${rc}" -ne 0 ]]; then
    echo ">> Sanitizer gate FAIL (binary exited ${rc})" >&2
    return "${rc}"
  fi
  echo ">> Sanitizer gate: clean (no ASan/UBSan findings)"
  return 0
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
