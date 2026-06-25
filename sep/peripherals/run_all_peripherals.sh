#!/usr/bin/env bash
# Run debug / asan / coverage / ctest for every peripheral model
# (except sep_memory, cpu, and AVBbus which are excluded by design).
#
# Usage:
#   ./run_all_peripherals.sh              # incremental build, run everything
#   ./run_all_peripherals.sh --clean      # clean build directories first, then run everything
#   ./run_all_peripherals.sh aes hmac     # incremental build, run only listed peripherals
#   ./run_all_peripherals.sh --clean aes  # clean build for specific peripheral(s)
#
# Output
#   logs/<peripheral>/debug_build.log
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
setup_build_env() {
  export VP_CONFIGURE_QUIET=1
  set --
  # shellcheck disable=SC1091
  source "${SCRIPT_DIR}/../../vp/configure_vp.sh"

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
}

setup_build_env || exit 1

set -- "${ARGS[@]+"${ARGS[@]}"}"

# ── Peripherals to skip ────────────────────────────────────────────────────────
SKIP=("sep_memory" "cpu" "AVBbus")

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

# Drop stale cmake cache when install paths or C++ standard change.
_peripheral_cache_stale() {
  local cache="$1/CMakeCache.txt"
  [[ -f "${cache}" ]] || return 1
  local cached_sc cached_ssl cached_cci cached_std
  cached_sc="$(grep -E '^SYSTEMC_HOME:' "${cache}" 2>/dev/null | sed 's/^SYSTEMC_HOME:[^=]*=//' || true)"
  cached_ssl="$(grep -E '^OPENSSL_INCLUDE_DIR:' "${cache}" 2>/dev/null | sed 's/^OPENSSL_INCLUDE_DIR:[^=]*=//' || true)"
  cached_cci="$(grep -E '^CCI_INCLUDE_DIR:' "${cache}" 2>/dev/null | sed 's/^CCI_INCLUDE_DIR:[^=]*=//' || true)"
  cached_std="$(grep -E '^CMAKE_CXX_STANDARD:' "${cache}" 2>/dev/null | sed 's/^CMAKE_CXX_STANDARD:[^=]*=//' || true)"
  [[ -n "${cached_sc}" && "${cached_sc}" != "${SYSTEMC_HOME}" ]] && return 0
  [[ -n "${cached_ssl}" && "${cached_ssl}" != "${OPENSSL_INC}" ]] && return 0
  [[ -n "${cached_cci}" && "${cached_cci}" != "${CCI_HOME}/include" ]] && return 0
  [[ -n "${cached_std}" && "${cached_std}" != "${CMAKE_CXX_STANDARD}" ]] && return 0
  return 1
}

# ── Helper: build one peripheral with one build type ─────────────────────────
# Returns 0 on success, non-zero on failure.
# Writes full output to $LOG_FILE.
build_peripheral() {
  local name="$1"       # e.g. aes
  local build_type="$2" # Debug | ASAN | Coverage
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
    elif _peripheral_cache_stale "${build_dir}"; then
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
    elif _peripheral_cache_stale "${build_dir}"; then
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
  return "${status}"
}

# ── Helper: run ctest for one peripheral ─────────────────────────────────────
run_ctest() {
  local name="$1"
  local build_dir="${SCRIPT_DIR}/${name}/build/debug"
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

# ── Helper: run coverage build + generate report, return status ──────────────
run_coverage() {
  local name="$1"
  local src_dir="${SCRIPT_DIR}/${name}"
  local build_dir="${src_dir}/build/coverage"
  local log_file="$2"

  if [ "$LCOV_AVAILABLE" != true ]; then
    {
      echo "=== ${name} Coverage build ==="
      echo "SKIP: lcov not found (install with: brew install lcov)"
      echo "    date: $(date)"
    } > "${log_file}"
    return 3
  fi

  {
    echo "=== ${name} Coverage build ==="
    echo "    clean: ${CLEAN}"
    echo "    date: $(date)"
    echo ""

    if $CLEAN; then
      rm -rf "${build_dir}"
    elif _peripheral_cache_stale "${build_dir}"; then
      echo "    (removing stale cmake cache — install paths changed)"
      rm -rf "${build_dir}"
    fi
    mkdir -p "${build_dir}"

    cmake -S "${src_dir}" -B "${build_dir}" \
      -DCMAKE_BUILD_TYPE=Coverage \
      -DBUILD_TESTS=ON \
      "${CMAKE_EXTRA_ARGS[@]}" 2>&1

    cmake --build "${build_dir}" --parallel "${JOBS}" 2>&1

    # Run the coverage target (generates lcov report)
    cmake --build "${build_dir}" --target coverage 2>&1

  } > "${log_file}" 2>&1
  local status=$?
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
  printf "%-20s  %-12s  %-12s  %-12s  %-12s  %s\n" \
    "Peripheral" "Debug" "ASAN" "Coverage" "CTest" "Coverage%"
  echo "------------------------------------------------------------"
} > "${TOP_SUMMARY}"

# ── Per-peripheral run ────────────────────────────────────────────────────────
OVERALL_PASS=true

for name in "${PERIPHERALS[@]}"; do
  plog="${LOG_ROOT}/${name}"
  mkdir -p "${plog}"

  echo "──────────────────────────────────────────────"
  echo "  ${name}"
  echo "──────────────────────────────────────────────"

  # 1. Debug build
  printf "  Debug build   ... "
  build_peripheral "${name}" "Debug" "${plog}/debug_build.log"
  debug_status=$?
  [ $debug_status -eq 0 ] && { pass; echo; debug_label="PASS"; } \
                           || { fail; echo; debug_label="FAIL"; OVERALL_PASS=false; }

  # 2. ASAN build + ctest (sanitizer-instrumented binary)
  printf "  ASAN          ... "
  run_asan "${name}" "${plog}/asan_build.log"
  asan_status=$?
  [ $asan_status -eq 0 ] && { pass; echo; asan_label="PASS"; } \
                          || { fail; echo; asan_label="FAIL"; OVERALL_PASS=false; }

  # 3. Coverage build + report
  printf "  Coverage      ... "
  run_coverage "${name}" "${plog}/coverage_build.log"
  cov_status=$?
  cov_pct=$(extract_coverage "${plog}/coverage_build.log")
  [ -z "$cov_pct" ] && cov_pct="n/a"
  if [ $cov_status -eq 3 ]; then
    skip; printf " (lcov not installed)\n"; cov_label="SKIP"
  elif [ $cov_status -eq 0 ]; then
    pass; printf " (${cov_pct}%%)\n"; cov_label="PASS"
  else
    fail; echo; cov_label="FAIL"; OVERALL_PASS=false
  fi

  # 4. CTest (uses the Debug build that was built in step 1)
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
    printf "  %-16s : %s\n" "Debug build"   "${debug_label}"
    printf "  %-16s : %s\n" "ASAN"          "${asan_label}"
    printf "  %-16s : %s  (${cov_pct}%%)\n" "Coverage"      "${cov_label}"
    printf "  %-16s : %s\n" "CTest"         "${ctest_label}"
    echo "------------------------------------------------------------"
    echo "  Log files:"
    echo "    debug_build.log"
    echo "    asan_build.log"
    echo "    coverage_build.log"
    echo "    ctest.log"
  } > "${plog}/Full_result.log"

  # Append row to top-level summary
  printf "%-20s  %-12s  %-12s  %-12s  %-12s  %s\n" \
    "${name}" "${debug_label}" "${asan_label}" "${cov_label}" "${ctest_label}" "${cov_pct}%" \
    >> "${TOP_SUMMARY}"

  echo ""
done

# Top-level footer
{
  echo "------------------------------------------------------------"
  if $OVERALL_PASS; then
    echo "  Overall: PASS — all peripherals passed all stages"
  else
    echo "  Overall: FAIL — one or more peripherals/stages failed"
  fi
  echo "============================================================"
} >> "${TOP_SUMMARY}"

echo ""
echo "──────────────────────────────────────────────"
echo "  Top-level summary written to:"
echo "    ${TOP_SUMMARY}"
echo ""
cat "${TOP_SUMMARY}"

$OVERALL_PASS && exit 0 || exit 1
