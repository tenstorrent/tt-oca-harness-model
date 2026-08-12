#!/usr/bin/env bash
# Build and test the lc_ctrl model.
#
# Usage:
#   ./run_tests.sh              # Release build + run tests (default)
#   ./run_tests.sh --debug      # Debug build + run tests
#   ./run_tests.sh --asan       # AddressSanitizer build + run tests
#   ./run_tests.sh --coverage   # Coverage build + multi-config lcov HTML report
#   ./run_tests.sh --ctest      # Run via CTest with verbose output
#   ./run_tests.sh --docs       # Build Doxygen documentation
#   ./run_tests.sh --cppcheck   # Run cppcheck static analysis
#   ./run_tests.sh --clean      # Remove build directory before building
#   ./run_tests.sh --no-build   # Skip cmake/make, re-run tests only
#
# Flags may be combined, e.g.: ./run_tests.sh --coverage --clean --jobs 4

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE="Release"
RUN_CTEST=false
RUN_DOCS=false
RUN_CPPCHECK=false
CLEAN=false
NO_BUILD=false
# Linux: nproc; macOS: sysctl; fallback: getconf
if command -v nproc >/dev/null 2>&1; then
  MAX_JOBS=$(nproc)
elif [[ "$(uname -s)" == "Darwin" ]]; then
  MAX_JOBS=$(sysctl -n hw.ncpu)
else
  MAX_JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)
fi

# All lifecycle state combinations that exercise distinct code paths.
# Each entry becomes one isolated parallel run with its own GCOV_PREFIX.
ALL_COMBOS=(
    combo_test_dev_secure
    combo_test_dev_no_secure_tm
    combo_prod
    combo_invalid
    combo_rma_chiplet
    combo_prod_end
    combo_rma_sip
    combo_security_disable
)
COMBOS=("${ALL_COMBOS[@]}")

# ---------------------------------------------------------------------------
# Argument parsing  (while/shift so --jobs can take a value)
# ---------------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
  case "$1" in
    --debug)      BUILD_TYPE="Debug";     shift ;;
    --asan)       BUILD_TYPE="ASAN";      shift ;;
    --coverage)   BUILD_TYPE="Coverage";  shift ;;
    --ctest)      RUN_CTEST=true;         shift ;;
    --docs)       RUN_DOCS=true;          shift ;;
    --cppcheck)   RUN_CPPCHECK=true;      shift ;;
    --clean)      CLEAN=true;             shift ;;
    --no-build)   NO_BUILD=true;          shift ;;
    --jobs)       MAX_JOBS="$2";          shift 2 ;;
    *) echo "Unknown option: $1"; echo "Run '$0 --help' to see usage."; exit 1 ;;
  esac
done

BUILD_DIR="${SCRIPT_DIR}/build/$(echo "${BUILD_TYPE}" | tr '[:upper:]' '[:lower:]')"

if ${CLEAN}; then
  echo "==> Cleaning ${BUILD_DIR} ..."
  rm -rf "${BUILD_DIR}"
fi

# ---------------------------------------------------------------------------
# Build environment (SystemC, CCI, OpenSSL, Boost — same as run_all_peripherals.sh)
# ---------------------------------------------------------------------------
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../setup_build_env.sh"
peripheral_setup_build_env || exit 1

if ! ${NO_BUILD} && ! ${CLEAN} && peripheral_cache_stale "${BUILD_DIR}"; then
  echo "Removing stale cmake cache (install paths or C++ standard changed) ..."
  rm -rf "${BUILD_DIR}"
fi

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
if ! ${NO_BUILD}; then
  echo "==> Configuring (${BUILD_TYPE}) ..."
  mkdir -p "${BUILD_DIR}"
  cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DBUILD_TESTS=ON \
    "${CMAKE_EXTRA_ARGS[@]}"

  echo "==> Building ..."
  cmake --build "${BUILD_DIR}" --parallel "$(peripheral_parallel_jobs)"
fi

# ---------------------------------------------------------------------------
# Run
# ---------------------------------------------------------------------------
echo ""
if ${RUN_DOCS}; then
  cmake --build "${BUILD_DIR}" --target lifecycle_ctrl_docs

elif ${RUN_CPPCHECK}; then
  cmake --build "${BUILD_DIR}" --target lifecycle_ctrl_cppcheck

elif ${RUN_CTEST}; then
  ctest --test-dir "${BUILD_DIR}" --output-on-failure -V

elif [ "${BUILD_TYPE}" = "Coverage" ]; then
  # -------------------------------------------------------------------------
  # Multi-config parallel coverage
  #
  # Each lifecycle state combination runs in a separate sub-process with
  # GCOV_PREFIX pointing to a private work directory, so parallel runs never
  # overwrite each other's .gcda files.  Per-combo .info files are merged.
  # -------------------------------------------------------------------------
  BINARY="${BUILD_DIR}/bin/lifecycle_ctrl_test"
  if [ ! -x "${BINARY}" ]; then
    echo "ERROR: binary not found at ${BINARY}"
    exit 1
  fi

  CONFIG_DIR="${BUILD_DIR}/configs"
  INFO_DIR="${BUILD_DIR}/coverage"
  mkdir -p "${CONFIG_DIR}" "${INFO_DIR}"

  # Detect lcov version: --ignore-errors flags only exist in lcov 2.x+
  LCOV_IGNORE=""
  GENHTML_IGNORE=""
  LCOV_MAJOR=$(lcov --version 2>/dev/null | grep -oE "LCOV version ([0-9]+)" | grep -oE "[0-9]+$" || echo "0")
  if [ "${LCOV_MAJOR}" -ge 2 ] 2>/dev/null; then
    LCOV_IGNORE="--ignore-errors inconsistent,unsupported,format,mismatch"
    GENHTML_IGNORE="--ignore-errors inconsistent,unsupported,format,corrupt,category"
  fi

  find "${BUILD_DIR}" -name '*.gcda' -delete 2>/dev/null || true
  lcov ${LCOV_IGNORE} --zerocounters --directory "${BUILD_DIR}" 2>/dev/null || true

  # GCOV_PREFIX_STRIP = number of '/' in BUILD_DIR so stripped relative paths
  # look like  CMakeFiles/lifecycle_ctrl_model.dir/src/...gcda  under WORK_DIR.
  GCOV_STRIP=$(echo "${BUILD_DIR}" | tr -dc '/' | wc -c)

  # ---------------------------------------------------------------------------
  # Emit a .ini file for each lifecycle state combination.
  # All parameters use the accellera_config.ini format:
  #   [string] instance_prefix, [int] verbosity + lc_state,
  #   [uint] sip/sys_dis, [bool] security_disable / secure_tm
  # ---------------------------------------------------------------------------
  emit_config() {
    local COMBO="$1"
    local CFG="$2"
    case "${COMBO}" in
      combo_test_dev_secure)
        cat > "${CFG}" <<INIEOF
# Auto-generated: TEST_DEV state, secure_tm=true (default; runs Tests 1-3, 5-7, 10)
[string]
instance_prefix: testbench.lifecycle_ctrl_dut

[int]
verbosity: 2
lc_state: 0

[uint]
sip_dis_lo: 0
sip_dis_hi: 0
sys_dis_lo: 0
sys_dis_hi: 0

[bool]
security_disable: false
secure_tm: true
INIEOF
        ;;
      combo_test_dev_no_secure_tm)
        cat > "${CFG}" <<INIEOF
# Auto-generated: TEST_DEV state, secure_tm=false (covers Test 10 false branch)
[string]
instance_prefix: testbench.lifecycle_ctrl_dut

[int]
verbosity: 2
lc_state: 0

[uint]
sip_dis_lo: 0
sip_dis_hi: 0
sys_dis_lo: 0
sys_dis_hi: 0

[bool]
security_disable: false
secure_tm: false
INIEOF
        ;;
      combo_prod)
        cat > "${CFG}" <<INIEOF
# Auto-generated: PROD state (runs Test 4)
[string]
instance_prefix: testbench.lifecycle_ctrl_dut

[int]
verbosity: 2
lc_state: 1

[uint]
sip_dis_lo: 0
sip_dis_hi: 0
sys_dis_lo: 0
sys_dis_hi: 0

[bool]
security_disable: false
secure_tm: true
INIEOF
        ;;
      combo_invalid)
        cat > "${CFG}" <<INIEOF
# Auto-generated: INVALID state (runs Test 8; FEAT_CTRL=0)
[string]
instance_prefix: testbench.lifecycle_ctrl_dut

[int]
verbosity: 2
lc_state: 4

[uint]
sip_dis_lo: 0
sip_dis_hi: 0
sys_dis_lo: 0
sys_dis_hi: 0

[bool]
security_disable: false
secure_tm: true
INIEOF
        ;;
      combo_rma_chiplet)
        cat > "${CFG}" <<INIEOF
# Auto-generated: RMA_CHIPLET state (runs Test 9; all features forced on)
[string]
instance_prefix: testbench.lifecycle_ctrl_dut

[int]
verbosity: 2
lc_state: 6

[uint]
sip_dis_lo: 0
sip_dis_hi: 0
sys_dis_lo: 0
sys_dis_hi: 0

[bool]
security_disable: false
secure_tm: true
INIEOF
        ;;
      combo_prod_end)
        cat > "${CFG}" <<INIEOF
# Auto-generated: PROD_END state (func-bits only, same base as PROD)
[string]
instance_prefix: testbench.lifecycle_ctrl_dut

[int]
verbosity: 2
lc_state: 8

[uint]
sip_dis_lo: 0
sip_dis_hi: 0
sys_dis_lo: 0
sys_dis_hi: 0

[bool]
security_disable: false
secure_tm: true
INIEOF
        ;;
      combo_rma_sip)
        cat > "${CFG}" <<INIEOF
# Auto-generated: RMA_SIP state with sip_dis_lo=0xFF (all SIP features enabled minus lower 8)
[string]
instance_prefix: testbench.lifecycle_ctrl_dut

[int]
verbosity: 2
lc_state: 2

[uint]
sip_dis_lo: 255
sip_dis_hi: 0
sys_dis_lo: 0
sys_dis_hi: 0

[bool]
security_disable: false
secure_tm: true
INIEOF
        ;;
      combo_security_disable)
        cat > "${CFG}" <<INIEOF
# Auto-generated: security_disable override (runs Test 12; FEAT_CTRL=all-ones)
[string]
instance_prefix: testbench.lifecycle_ctrl_dut

[int]
verbosity: 2
lc_state: 1

[uint]
sip_dis_lo: 0
sip_dis_hi: 0
sys_dis_lo: 0
sys_dis_hi: 0

[bool]
security_disable: true
secure_tm: true
INIEOF
        ;;
      *)
        echo "ERROR: unknown combo '${COMBO}'" >&2
        return 1
        ;;
    esac
  }

  # Per-combo worker — runs in a subshell so it can be backgrounded
  run_one_combo() {
    local COMBO="$1"
    local LOG="${INFO_DIR}/log_${COMBO}.txt"
    {
    echo "[${COMBO}] Starting at $(date +%T)"

    local CFG="${CONFIG_DIR}/config_${COMBO}.ini"
    local WORK_DIR="${INFO_DIR}/work_${COMBO}"
    local RAW_INFO="${INFO_DIR}/raw_${COMBO}.info"
    local FILTERED_INFO="${INFO_DIR}/filtered_${COMBO}.info"

    emit_config "${COMBO}" "${CFG}"

    # Private work dir: holds .gcno symlinks + redirected .gcda files.
    # INFO_DIR is pruned from the find so sibling workers' rm-rf activity
    # does not cause find to exit non-zero (which would kill this subshell
    # via pipefail).
    rm -rf "${WORK_DIR}" && mkdir -p "${WORK_DIR}"
    find "${BUILD_DIR}" -path "${INFO_DIR}" -prune -o -name "*.gcno" -print \
      | while read -r gcno; do
          rel="${gcno#${BUILD_DIR}/}"
          link="${WORK_DIR}/${rel}"
          mkdir -p "$(dirname "${link}")"
          ln -sf "${gcno}" "${link}"
        done

    set +e
    GCOV_PREFIX="${WORK_DIR}" GCOV_PREFIX_STRIP="${GCOV_STRIP}" "${BINARY}" "${CFG}"
    local RUN_EXIT=$?
    set -e
    [ "${RUN_EXIT}" -ne 0 ] && \
      echo "[${COMBO}] WARNING: binary exited ${RUN_EXIT} — coverage still captured"

    lcov ${LCOV_IGNORE} \
      --capture --directory "${WORK_DIR}" \
      --output-file "${RAW_INFO}" 2>&1

    lcov ${LCOV_IGNORE} \
      --extract "${RAW_INFO}" \
        "${SCRIPT_DIR}/src/*" \
        "${SCRIPT_DIR}/include/*" \
      --output-file "${FILTERED_INFO}" 2>&1

    echo "[${COMBO}] Done at $(date +%T) — ${FILTERED_INFO}"
    } > "${LOG}" 2>&1
  }

  echo "============================================================"
  echo "  Launching ${#COMBOS[@]} lifecycle config(s) in parallel (max ${MAX_JOBS} jobs)"
  echo "  Binary : ${BINARY}"
  echo "============================================================"

  PIDS=()
  RUNNING=0
  for COMBO in "${COMBOS[@]}"; do
    # Throttle to MAX_JOBS concurrent workers
    while [ "${RUNNING}" -ge "${MAX_JOBS}" ]; do
      for i in "${!PIDS[@]}"; do
        if ! kill -0 "${PIDS[$i]}" 2>/dev/null; then
          wait "${PIDS[$i]}" 2>/dev/null || true
          unset 'PIDS[$i]'
          RUNNING=$((RUNNING - 1))
        fi
      done
      sleep 0.5
    done
    echo "  Launching: ${COMBO}"
    run_one_combo "${COMBO}" &
    PIDS+=($!)
    RUNNING=$((RUNNING + 1))
  done

  echo "  Waiting for all jobs to finish..."
  for PID in "${PIDS[@]+"${PIDS[@]}"}"; do
    wait "${PID}" 2>/dev/null || true
  done

  # Print per-combo summary lines
  echo ""
  echo "============================================================"
  echo "  Per-config results:"
  echo "============================================================"
  for COMBO in "${COMBOS[@]}"; do
    [ -f "${INFO_DIR}/log_${COMBO}.txt" ] && \
      tail -2 "${INFO_DIR}/log_${COMBO}.txt" | sed "s/^/  /"
  done

  # Merge all filtered .info files into one
  echo ""
  echo "============================================================"
  echo "  Merging coverage from ${#COMBOS[@]} lifecycle config(s)"
  echo "============================================================"
  MERGED_ARGS=()
  for COMBO in "${COMBOS[@]}"; do
    FILTERED="${INFO_DIR}/filtered_${COMBO}.info"
    if [ -f "${FILTERED}" ]; then
      MERGED_ARGS+=(-a "${FILTERED}")
    else
      echo "  WARNING: missing ${FILTERED} — skipping ${COMBO}"
    fi
  done

  MERGED_INFO="${INFO_DIR}/coverage_merged.info"
  lcov ${LCOV_IGNORE} "${MERGED_ARGS[@]}" --output-file "${MERGED_INFO}"

  HTML_DIR="${INFO_DIR}/html"
  genhtml ${GENHTML_IGNORE} \
    "${MERGED_INFO}" \
    --output-directory "${HTML_DIR}" \
    --title "lifecycle_ctrl Multi-Config Coverage (${COMBOS[*]})"

  echo ""
  echo "============================================================"
  echo "  Done."
  echo "  Configs    : ${COMBOS[*]}"
  echo "  Merged info: ${MERGED_INFO}"
  echo "  HTML report: ${HTML_DIR}/index.html"
  echo "============================================================"

else
  # Default: just run the test binary with the default config
  "${BUILD_DIR}/bin/lifecycle_ctrl_test" "${SCRIPT_DIR}/config/accellera_config.ini"
fi
