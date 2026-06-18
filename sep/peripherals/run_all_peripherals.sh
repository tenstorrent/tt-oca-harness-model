#!/usr/bin/env bash
# Run release / asan / coverage / ctest for every peripheral model
# (except sep_memory, cpu, and AVBbus which are excluded by design).
#
# Usage:
#   ./run_all_peripherals.sh              # incremental build, run everything
#   ./run_all_peripherals.sh --clean      # clean build directories first, then run everything
#   ./run_all_peripherals.sh aes hmac     # incremental build, run only listed peripherals
#   ./run_all_peripherals.sh --clean aes  # clean build for specific peripheral(s)
#
# Output
#   logs/<peripheral>/release_build.log
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
  for candidate in \
    "${HOME}/local/systemc-3.0.2-cxx20" \
    "${HOME}/local/systemc-3.0.2-cxx17" \
    "${HOME}/local/systemc-3.0.2" \
    "${HOME}/local/systemc-3.0.1" \
    "${HOME}/local/systemc" \
    "${HOME}/systemc-3.0.2" \
    "${HOME}/systemc-3.0.1" \
    "${HOME}/systemc" \
    /usr/local/systemc-3.0.2 \
    /usr/local/systemc301 \
    /usr/local/systemc-3.0.1 \
    /usr/local/systemc300 \
    /usr/local/systemc \
    /usr/local \
    /usr/lib/systemc-3.0.2 \
    /usr/lib/systemc-3.0.1 \
    /usr/lib/systemc \
    /opt/homebrew/opt/systemc \
    /opt/homebrew/opt/libsystemc \
    /usr/local/opt/systemc; do
    [ -f "${candidate}/include/systemc.h" ] && { export SYSTEMC_HOME="$candidate"; break; }
  done
fi
if [ -z "${CCI_HOME:-}" ]; then
  for candidate in \
    "${HOME}/local/cci-cxx20" \
    "${HOME}/local/cci-1.0.1" \
    "${HOME}/local/cci" \
    "${HOME}/cci-1.0.1" \
    "${HOME}/cci" \
    /usr/local/cci-1.0.1 \
    /usr/local/cci \
    /usr/lib/cci-1.0.1 \
    /usr/lib/cci \
    /opt/homebrew/opt/systemc-cci \
    /usr/local/opt/systemc-cci; do
    [ -d "${candidate}/include/cci_configuration" ] && { export CCI_HOME="$candidate"; break; }
  done
fi

# Default C++ standard
: "${CMAKE_CXX_STANDARD:=20}"

if [ -n "${SYSTEMC_HOME:-}" ]; then
  echo "  SYSTEMC_HOME = ${SYSTEMC_HOME}"
else
  echo "warning: SYSTEMC_HOME not found — builds may fail" >&2
fi
if [ -n "${CCI_HOME:-}" ]; then
  echo "  CCI_HOME     = ${CCI_HOME}"
fi
echo "  CXX_STANDARD = ${CMAKE_CXX_STANDARD}"
echo ""

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
    fi
    mkdir -p "${build_dir}"

    cmake -S "${src_dir}" -B "${build_dir}" \
      -DCMAKE_BUILD_TYPE="${build_type}" \
      -DCMAKE_CXX_STANDARD="${CMAKE_CXX_STANDARD}" \
      -DSYSTEMC_HOME="${SYSTEMC_HOME:-}" \
      ${CCI_HOME:+-DCCI_HOME="${CCI_HOME}"} \
      -DBUILD_TESTS=ON 2>&1

    cmake --build "${build_dir}" --parallel "${JOBS}" 2>&1

  } > "${log_file}" 2>&1

  return $?
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
  # Use perl instead of grep -P (BSD grep on macOS does not support -P)
  perl -ne 'print "$1\n" if /lines\.+:\s*([0-9]+\.[0-9]+)%/' "$log" | tail -1
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

  {
    echo "=== ${name} Coverage build ==="
    echo "    clean: ${CLEAN}"
    echo "    date: $(date)"
    echo ""

    if $CLEAN; then
      rm -rf "${build_dir}"
    fi
    mkdir -p "${build_dir}"

    # Prepend lcov wrapper dir to PATH so cmake's custom coverage target picks it up
    local PATH_ORIG="${PATH}"
    [[ -n "${lcov_wrapper_dir}" ]] && export PATH="${lcov_wrapper_dir}:${PATH}"

    cmake -S "${src_dir}" -B "${build_dir}" \
      -DCMAKE_BUILD_TYPE=Coverage \
      -DCMAKE_CXX_STANDARD="${CMAKE_CXX_STANDARD}" \
      -DSYSTEMC_HOME="${SYSTEMC_HOME:-}" \
      ${CCI_HOME:+-DCCI_HOME="${CCI_HOME}"} \
      ${thread_cache_arg:+"${thread_cache_arg}"} \
      ${linker_extra_flags:+"${linker_extra_flags}"} \
      -DBUILD_TESTS=ON 2>&1

    cmake --build "${build_dir}" --parallel "${JOBS}" 2>&1

    # Run the coverage target (generates lcov report)
    cmake --build "${build_dir}" --target coverage 2>&1

    export PATH="${PATH_ORIG}"

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
    "Peripheral" "Release" "ASAN" "Coverage" "CTest" "Coverage%"
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

  # 1. Release build
  printf "  Release build ... "
  build_peripheral "${name}" "Release" "${plog}/release_build.log"
  release_status=$?
  [ $release_status -eq 0 ] && { pass; echo; release_label="PASS"; } \
                             || { fail; echo; release_label="FAIL"; OVERALL_PASS=false; }

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
  printf "%-20s  %-12s  %-12s  %-12s  %-12s  %s\n" \
    "${name}" "${release_label}" "${asan_label}" "${cov_label}" "${ctest_label}" "${cov_pct}%" \
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
