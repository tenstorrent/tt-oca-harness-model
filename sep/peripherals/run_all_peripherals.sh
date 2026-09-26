#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# Run release / asan / coverage / ctest for every peripheral model
# (except cpu, which is excluded by design).
#
# Prerequisites: valid VP install paths (same as vp/configure_vp.sh).
#   Edit vp/configure_vp.sh defaults or export SYSTEMC_HOME, CCI_HOME,
#   OPENSSL_ROOT, BOOST_ROOT (and CMAKE_CXX_STANDARD if needed) before running.
#   This script sources setup_build_env.sh → vp/configure_vp.sh automatically.
#
# Usage:
#   ./run_all_peripherals.sh              # incremental build, run everything
#   ./run_all_peripherals.sh --clean      # clean build directories first, then run everything
#   ./run_all_peripherals.sh aes hmac     # incremental build, run only listed peripherals
#   ./run_all_peripherals.sh --clean aes  # clean build for specific peripheral(s)
#
# Output
#   logs/<peripheral>/release_build.log
#   logs/<peripheral>/asan_build.log      ← ASAN build + ctest under sanitizers
#   logs/<peripheral>/coverage_build.log
#   logs/<peripheral>/ctest.log
#   logs/<peripheral>/Full_result.log     ← per-peripheral summary
#   logs/Full_result.log                  ← top-level summary across all peripherals

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_ROOT="${SCRIPT_DIR}/logs"

# Linux: nproc; macOS: sysctl; fallback: getconf
if command -v nproc >/dev/null 2>&1; then
  JOBS=$(nproc)
elif [[ "$(uname -s)" == "Darwin" ]]; then
  JOBS=$(sysctl -n hw.ncpu)
else
  JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)
fi
CLEAN=false

# ── Parse --clean flag (must come before peripheral names) ───────────────────
ARGS=()
for arg in "$@"; do
  case "$arg" in
    --clean) CLEAN=true ;;
    *) ARGS+=("$arg") ;;
  esac
done

# ── Resolve build environment (SystemC, OpenSSL, Boost) ─────────────────────
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/setup_build_env.sh"
peripheral_setup_build_env || exit 1

set -- "${ARGS[@]+"${ARGS[@]}"}"

# ── Peripherals to skip ────────────────────────────────────────────────────────
SKIP=("cpu")

# ── Colour helpers (disabled when not a TTY) ──────────────────────────────────
if [ -t 1 ]; then
  GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[0;33m'; NC='\033[0m'
else
  GREEN=''; RED=''; YELLOW=''; NC=''
fi

pass() { printf "${GREEN}PASS${NC}"; }
fail() { printf "${RED}FAIL${NC}"; }
skip() { printf "${YELLOW}SKIP${NC}"; }

LCOV_AVAILABLE=false
if command -v lcov >/dev/null 2>&1; then
  LCOV_AVAILABLE=true
fi

# ── Helper: build one peripheral with one build type ─────────────────────────
# Returns 0 on success, non-zero on failure.
# Writes full output to $LOG_FILE.
build_peripheral() {
  local name="$1"       # e.g. aes
  local build_type="$2" # Release | ASAN | Coverage
  local src_dir="${SCRIPT_DIR}/${name}"
  local build_dir="${src_dir}/build/$(echo "${build_type}" | tr '[:upper:]' '[:lower:]')"
  local log_file="$3"

  {
    echo "=== ${name} ${build_type} build ==="
    echo "    src : ${src_dir}"
    echo "    bld : ${build_dir}"
    echo "    clean: ${CLEAN}"
    echo "    date: $(date)"
    echo ""

    if $CLEAN; then
      rm -rf "${build_dir}"
    elif peripheral_cache_stale "${build_dir}"; then
      echo "    (removing stale cmake cache — install paths changed)"
      rm -rf "${build_dir}"
    fi
    mkdir -p "${build_dir}"

    cmake -S "${src_dir}" -B "${build_dir}" \
      -DCMAKE_BUILD_TYPE="${build_type}" \
      -DBUILD_TESTS=ON \
      "${CMAKE_EXTRA_ARGS[@]}" 2>&1

    cmake --build "${build_dir}" --parallel "${JOBS}" 2>&1

  } > "${log_file}" 2>&1

  return $?
}

# ── Helper: ASAN build + run ctest under AddressSanitizer ─────────────────────
run_asan() {
  local name="$1"
  local src_dir="${SCRIPT_DIR}/${name}"
  local build_dir="${src_dir}/build/asan"
  local log_file="$2"

  {
    echo "=== ${name} ASAN build + test ==="
    echo "    src : ${src_dir}"
    echo "    bld : ${build_dir}"
    echo "    clean: ${CLEAN}"
    echo "    date: $(date)"
    echo ""

    if $CLEAN; then
      rm -rf "${build_dir}"
    elif peripheral_cache_stale "${build_dir}"; then
      echo "    (removing stale cmake cache — install paths changed)"
      rm -rf "${build_dir}"
    fi
    mkdir -p "${build_dir}"

    cmake -S "${src_dir}" -B "${build_dir}" \
      -DCMAKE_BUILD_TYPE=ASAN \
      -DBUILD_TESTS=ON \
      "${CMAKE_EXTRA_ARGS[@]}" 2>&1

    cmake --build "${build_dir}" --parallel "${JOBS}" 2>&1

    echo ""
    echo "=== ${name} ASAN ctest ==="
    echo "    bld : ${build_dir}"
    echo "    date: $(date)"
    echo ""
    ctest --test-dir "${build_dir}" --output-on-failure -V 2>&1

  } > "${log_file}" 2>&1
  local status=$?
  local asan_gate="${SCRIPT_DIR}/../../smc/scripts/enforce_asan_clean.sh"
  if [[ -f "${asan_gate}" ]] && ! "${asan_gate}" "${build_dir}" "${log_file}"; then
    return 1
  fi
  return "${status}"
}

# ── Helper: run ctest for one peripheral ─────────────────────────────────────
run_ctest() {
  local name="$1"
  local build_dir="${SCRIPT_DIR}/${name}/build/release"
  local log_file="$2"

  {
    echo "=== ${name} ctest ==="
    echo "    bld : ${build_dir}"
    echo "    date: $(date)"
    echo ""
    ctest --test-dir "${build_dir}" --output-on-failure -V 2>&1
  } > "${log_file}" 2>&1

  return $?
}

# ── Helper: extract coverage % from coverage log ─────────────────────────────
extract_coverage() {
  local log="$1"
  # lcov summary line: "  lines......: 72.3% (1234 of 1706 lines)"
  # sed works on BSD/macOS and GNU; grep -oP is GNU-only.
  sed -n 's/.*lines\.*:[[:space:]]*\([0-9][0-9]*\.[0-9][0-9]*\)%.*/\1/p' "$log" | tail -1
}

# True if $1 is a number >= $2 (default 95).  "n/a" / empty fails the gate.
COVERAGE_MIN_LINE_PCT="${COVERAGE_MIN_LINE_PCT:-95}"
coverage_meets_min() {
  local pct="$1" min="${2:-${COVERAGE_MIN_LINE_PCT}}"
  [[ -z "${pct}" || "${pct}" == "n/a" ]] && return 1
  awk -v p="${pct}" -v m="${min}" 'BEGIN { exit (p+0 < m+0) }'
}

# ── Helper: run coverage build + generate report, return status ──────────────
run_coverage() {
  local name="$1"
  local src_dir="${SCRIPT_DIR}/${name}"
  local build_dir="${src_dir}/build/coverage"
  local log_file="$2"

  # On macOS, pthreads are always in libSystem; pre-cache the result so that
  # FindThreads succeeds even when --coverage is in CMAKE_CXX_FLAGS during the
  # cmake configure phase (the Coverage build type sets coverage flags before
  # find_package(Threads) runs, which confuses CMake's -pthread probe on macOS).
  local thread_cache_arg=""
  local linker_extra_flags=""
  local lcov_wrapper_dir=""

  if [ "$LCOV_AVAILABLE" != true ]; then
    {
      echo "=== ${name} Coverage build ==="
      echo "SKIP: lcov not found (install with: brew install lcov)"
      echo "    date: $(date)"
    } > "${log_file}"
    return 3
  fi

  if [[ "$(uname -s)" == "Darwin" ]]; then
    thread_cache_arg="-DCMAKE_HAVE_LIBC_PTHREAD=1"
    # The peripheral CMakeLists.txt passes --coverage -lgcov to the linker;
    # libgcov doesn't exist on macOS — shim it with libclang_rt.profile_osx.
    local clang_rt_dir
    clang_rt_dir=$(clang -print-runtime-dir 2>/dev/null || true)
    if [[ -f "${clang_rt_dir}/libclang_rt.profile_osx.a" ]]; then
      local gcov_shim_dir="${TMPDIR:-/tmp}/gcov-shim-$$"
      mkdir -p "${gcov_shim_dir}"
      ln -sf "${clang_rt_dir}/libclang_rt.profile_osx.a" "${gcov_shim_dir}/libgcov.a"
      linker_extra_flags="-DCMAKE_EXE_LINKER_FLAGS=-L${gcov_shim_dir}"
    fi
    # lcov 2.x + Apple LLVM gcov emits inconsistent line-number data for system
    # headers; use a thin wrapper that injects --ignore-errors to suppress the
    # fatal error without modifying the peripheral CMakeLists.txt files.
    local real_lcov
    real_lcov=$(command -v lcov 2>/dev/null || true)
    if [[ -n "${real_lcov}" ]]; then
      lcov_wrapper_dir="${TMPDIR:-/tmp}/lcov-wrap-$$"
      mkdir -p "${lcov_wrapper_dir}"
      cat > "${lcov_wrapper_dir}/lcov" <<LCOV_WRAP
#!/usr/bin/env bash
exec "${real_lcov}" --ignore-errors unsupported,unsupported,inconsistent,format,mismatch "\$@"
LCOV_WRAP
      chmod +x "${lcov_wrapper_dir}/lcov"
    fi
  fi

  local status=0
  {
    echo "=== ${name} Coverage build ==="
    echo "    clean: ${CLEAN}"
    echo "    date: $(date)"
    echo ""

    if $CLEAN; then
      rm -rf "${build_dir}"
    elif peripheral_cache_stale "${build_dir}"; then
      echo "    (removing stale cmake cache — install paths changed)"
      rm -rf "${build_dir}"
    fi
    mkdir -p "${build_dir}"

    # Prepend lcov wrapper dir to PATH so cmake's custom coverage target picks it up
    local PATH_ORIG="${PATH}"
    [[ -n "${lcov_wrapper_dir}" ]] && export PATH="${lcov_wrapper_dir}:${PATH}"

    # Track the real exit status of configure + build. (The coverage report
    # target is best-effort: lcov/gcov quirks must not fail the build stage.)
    if cmake -S "${src_dir}" -B "${build_dir}" \
         -DCMAKE_BUILD_TYPE=Coverage \
         -DBUILD_TESTS=ON \
         ${thread_cache_arg:+"${thread_cache_arg}"} \
         ${linker_extra_flags:+"${linker_extra_flags}"} \
         "${CMAKE_EXTRA_ARGS[@]}" 2>&1 \
       && cmake --build "${build_dir}" --parallel "${JOBS}" 2>&1; then
      # Coverage target now includes the ≥95% line-coverage gate.
      cmake --build "${build_dir}" --target coverage 2>&1 || status=$?
    else
      status=1
    fi

    export PATH="${PATH_ORIG}"

  } > "${log_file}" 2>&1
  return "${status}"
}

# ── Build peripheral list (remaining args after --clean was stripped) ─────────
if [ $# -gt 0 ]; then
  PERIPHERALS=("$@")
else
  PERIPHERALS=()
  for d in "${SCRIPT_DIR}"/*/; do
    name=$(basename "$d")
    skip=false
    for s in "${SKIP[@]}"; do [ "$s" = "$name" ] && skip=true && break; done
    $skip && continue
    [ -f "${d}/run_tests.sh" ] || continue   # must have a build script
    PERIPHERALS+=("$name")
  done
fi

echo ""
echo "Peripherals to test (${#PERIPHERALS[@]}): ${PERIPHERALS[*]}"
echo "C++ standard     : ${CMAKE_CXX_STANDARD}"
echo "SYSTEMC_HOME     : ${SYSTEMC_HOME}"
echo "Build mode       : $($CLEAN && echo 'clean' || echo 'incremental')"
echo "Logs directory   : ${LOG_ROOT}"
echo ""

mkdir -p "${LOG_ROOT}"

# Top-level summary header
TOP_SUMMARY="${LOG_ROOT}/Full_result.log"
{
  echo "============================================================"
  echo "  run_all_peripherals.sh — Full Result Summary"
  echo "  $(date)"
  echo "============================================================"
  printf "%-4s  %-32s  %-12s  %-12s  %-12s  %-12s  %s\n" \
    "Sr." "Peripheral" "Release" "ASAN" "Coverage" "CTest" "Coverage%"
  echo "------------------------------------------------------------------------"
} > "${TOP_SUMMARY}"

# ── Per-peripheral run ────────────────────────────────────────────────────────
OVERALL_PASS=true
sr_no=0

for name in "${PERIPHERALS[@]}"; do
  sr_no=$((sr_no + 1))
  plog="${LOG_ROOT}/${name}"
  mkdir -p "${plog}"

  echo "──────────────────────────────────────────────"
  echo "  ${name}"
  echo "──────────────────────────────────────────────"

  # 1. Release build
  printf "  Release build ... "
  build_peripheral "${name}" "Release" "${plog}/release_build.log"
  release_status=$?
  [ $release_status -eq 0 ] && { pass; echo; release_label="PASS"; } \
                             || { fail; echo; release_label="FAIL"; OVERALL_PASS=false; }

  # 2. ASAN build + ctest (sanitizer-instrumented binary)
  if [[ "${SKIP_ASAN:-0}" == "1" ]]; then
    printf "  ASAN          ... SKIP (SKIP_ASAN=1)\n"
    asan_label="SKIP"
  else
    printf "  ASAN          ... "
    run_asan "${name}" "${plog}/asan_build.log"
    asan_status=$?
    [ $asan_status -eq 0 ] && { pass; echo; asan_label="PASS"; } \
                            || { fail; echo; asan_label="FAIL"; OVERALL_PASS=false; }
  fi

  # 3. Coverage build + report
  printf "  Coverage      ... "
  run_coverage "${name}" "${plog}/coverage_build.log"
  cov_status=$?
  cov_pct=$(extract_coverage "${plog}/coverage_build.log")
  [ -z "$cov_pct" ] && cov_pct="n/a"
  if [ $cov_status -eq 3 ]; then
    skip; printf " (lcov not installed)\n"; cov_label="SKIP"
  elif [ $cov_status -eq 0 ] && coverage_meets_min "${cov_pct}"; then
    pass; printf " (${cov_pct}%%)\n"; cov_label="PASS"
  elif [ $cov_status -eq 0 ]; then
    fail; printf " (${cov_pct}%% < ${COVERAGE_MIN_LINE_PCT}%%)\n"
    cov_label="FAIL"; OVERALL_PASS=false
  else
    fail
    if [ $cov_status -eq 0 ]; then
      printf " (${cov_pct}%% < %s%%)\n" "${COVERAGE_MIN_LINE_PCT}"
    else
      echo
    fi
    cov_label="FAIL"
    OVERALL_PASS=false
  fi

  # 4. CTest (uses the Release build that was built in step 1)
  printf "  CTest         ... "
  run_ctest "${name}" "${plog}/ctest.log"
  ctest_status=$?
  [ $ctest_status -eq 0 ] && { pass; echo; ctest_label="PASS"; } \
                           || { fail; echo; ctest_label="FAIL"; OVERALL_PASS=false; }

  # Per-peripheral Full_result.log
  {
    echo "============================================================"
    echo "  ${name} — Full Result"
    echo "  $(date)"
    echo "============================================================"
    printf "  %-16s : %s\n" "Release build" "${release_label}"
    printf "  %-16s : %s\n" "ASAN build"    "${asan_label}"
    printf "  %-16s : %s  (${cov_pct}%%)\n" "Coverage"      "${cov_label}"
    printf "  %-16s : %s\n" "CTest"         "${ctest_label}"
    echo "------------------------------------------------------------"
    echo "  Log files:"
    echo "    release_build.log"
    echo "    asan_build.log"
    echo "    coverage_build.log"
    echo "    ctest.log"
  } > "${plog}/Full_result.log"

  # Append row to top-level summary
  printf "%-4s  %-32s  %-12s  %-12s  %-12s  %-12s  %s\n" \
    "${sr_no}." "${name}" "${release_label}" "${asan_label}" "${cov_label}" "${ctest_label}" "${cov_pct}%" \
    >> "${TOP_SUMMARY}"

  echo ""
done

# Top-level footer
{
  echo "------------------------------------------------------------------------"
  if $OVERALL_PASS; then
    echo "  Overall: PASS — all peripherals passed all stages"
  else
    echo "  Overall: FAIL — one or more peripherals/stages failed"
  fi
  echo "============================================================"
  echo "Notes:"
  echo "  - sep_filter_ctrl covers both the inbound and outbound filter control instances."
  echo "  - sep_output_remap_ctrl covers both the AP and STEE output remap instances."
  echo "  - sep_scratch_warm is store-only (no behavioral logic); its suite checks"
  echo "    reset values, read/write and reserved-bit masking."
} >> "${TOP_SUMMARY}"

echo ""
echo "──────────────────────────────────────────────"
echo "  Top-level summary written to:"
echo "    ${TOP_SUMMARY}"
echo ""
cat "${TOP_SUMMARY}"

$OVERALL_PASS && exit 0 || exit 1
