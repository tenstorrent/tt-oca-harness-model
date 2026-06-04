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
#   logs/<peripheral>/asan_build.log
#   logs/<peripheral>/coverage_build.log
#   logs/<peripheral>/ctest.log
#   logs/<peripheral>/Full_result.log     ← per-peripheral summary
#   logs/Full_result.log                  ← top-level summary across all peripherals

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_ROOT="${SCRIPT_DIR}/logs"
JOBS=$(nproc)
CLEAN=false

# ── Parse --clean flag (must come before peripheral names) ───────────────────
ARGS=()
for arg in "$@"; do
  case "$arg" in
    --clean) CLEAN=true ;;
    *) ARGS+=("$arg") ;;
  esac
done
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

# ── Resolve SystemC / CCI paths once ─────────────────────────────────────────
if [ -z "${SYSTEMC_HOME:-}" ]; then
  for candidate in /usr/local/systemc300 /usr/local/systemc; do
    [ -d "$candidate" ] && { export SYSTEMC_HOME="$candidate"; break; }
  done
fi
if [ -z "${CCI_HOME:-}" ] && [ -d "/usr/local/cci" ]; then
  export CCI_HOME="/usr/local/cci"
fi

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
    fi
    mkdir -p "${build_dir}"

    cmake -S "${src_dir}" -B "${build_dir}" \
      -DCMAKE_BUILD_TYPE="${build_type}" \
      -DBUILD_TESTS=ON 2>&1

    cmake --build "${build_dir}" --parallel "${JOBS}" 2>&1

  } > "${log_file}" 2>&1

  return $?
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
  grep -oP 'lines\.*:\s*\K[0-9]+\.[0-9]+(?=%)' "$log" | tail -1
}

# ── Helper: run coverage build + generate report, return status ──────────────
run_coverage() {
  local name="$1"
  local src_dir="${SCRIPT_DIR}/${name}"
  local build_dir="${src_dir}/build/coverage"
  local log_file="$2"

  {
    echo "=== ${name} Coverage build ==="
    echo "    clean: ${CLEAN}"
    echo "    date: $(date)"
    echo ""

    if $CLEAN; then
      rm -rf "${build_dir}"
    fi
    mkdir -p "${build_dir}"

    cmake -S "${src_dir}" -B "${build_dir}" \
      -DCMAKE_BUILD_TYPE=Coverage \
      -DBUILD_TESTS=ON 2>&1

    cmake --build "${build_dir}" --parallel "${JOBS}" 2>&1

    # Run the coverage target (generates lcov report)
    cmake --build "${build_dir}" --target coverage 2>&1

  } > "${log_file}" 2>&1

  return $?
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

  # 2. ASAN build
  printf "  ASAN build    ... "
  build_peripheral "${name}" "ASAN" "${plog}/asan_build.log"
  asan_status=$?
  [ $asan_status -eq 0 ] && { pass; echo; asan_label="PASS"; } \
                          || { fail; echo; asan_label="FAIL"; OVERALL_PASS=false; }

  # 3. Coverage build + report
  printf "  Coverage      ... "
  run_coverage "${name}" "${plog}/coverage_build.log"
  cov_status=$?
  cov_pct=$(extract_coverage "${plog}/coverage_build.log")
  [ -z "$cov_pct" ] && cov_pct="n/a"
  [ $cov_status -eq 0 ] && { pass; printf " (${cov_pct}%%)\n"; cov_label="PASS"; } \
                         || { fail; echo; cov_label="FAIL"; OVERALL_PASS=false; }

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
    printf "  %-16s : %s\n" "ASAN build"    "${asan_label}"
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
