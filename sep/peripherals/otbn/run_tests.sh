#!/usr/bin/env bash
# Build and test the otbn model.
#
# Usage:
#   ./run_tests.sh                             # Release build + run tests
#   ./run_tests.sh --debug                     # Debug build + run tests
#   ./run_tests.sh --asan                      # AddressSanitizer build + run tests
#   ./run_tests.sh --coverage                  # Coverage build + multi-algorithm lcov HTML report
#   ./run_tests.sh --coverage --algos "rsa_2048 smoke"  # run a subset of algorithms
#   ./run_tests.sh --ctest                     # Run via CTest with verbose output
#   ./run_tests.sh --docs                      # Build Doxygen documentation
#   ./run_tests.sh --cppcheck                  # Run cppcheck static analysis
#   ./run_tests.sh --clean                     # Remove build directory before building
#   ./run_tests.sh --no-build                  # Skip cmake/make, re-run tests only
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

# All algorithms recognised by select_algorithm() in otbn.cpp.
# callback_cov exercises CSR/WDR algorithm callbacks (coverage only).
# unknown_algo exercises the default fallback branch.
ALL_ALGOS=(
    rsa_2048
    rsa_2048_key_enabled
    rsa_3072
    p256_ecdsa
    summation
    otbn_loop
    rnd_test
    smoke
    callback_cov
    unknown_algo
)
ALGOS=("${ALL_ALGOS[@]}")

# ---------------------------------------------------------------------------
# Argument parsing  (while/shift so --algos and --jobs can take a value)
# ---------------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
  case "$1" in
    --debug)      BUILD_TYPE="Debug";  shift ;;
    --asan)       BUILD_TYPE="ASAN";   shift ;;
    --coverage)   BUILD_TYPE="Coverage"; shift ;;
    --ctest)      RUN_CTEST=true;      shift ;;
    --docs)       RUN_DOCS=true;       shift ;;
    --cppcheck)   RUN_CPPCHECK=true;   shift ;;
    --clean)      CLEAN=true;          shift ;;
    --no-build)   NO_BUILD=true;       shift ;;
    --algos)      IFS=' ' read -r -a ALGOS <<< "$2"; shift 2 ;;
    --jobs)       MAX_JOBS="$2";       shift 2 ;;
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
  if [ -z "${MAX_JOBS}" ]; then
    MAX_JOBS="$(peripheral_parallel_jobs)"
  fi
  cmake --build "${BUILD_DIR}" --parallel "${MAX_JOBS}"
fi

# ---------------------------------------------------------------------------
# Run
# ---------------------------------------------------------------------------
echo ""
if ${RUN_DOCS}; then
  cmake --build "${BUILD_DIR}" --target otbn_docs

elif ${RUN_CPPCHECK}; then
  cmake --build "${BUILD_DIR}" --target otbn_cppcheck

elif ${RUN_CTEST}; then
  ctest --test-dir "${BUILD_DIR}" --output-on-failure -V

elif [ "${BUILD_TYPE}" = "Coverage" ]; then
  # -------------------------------------------------------------------------
  # Multi-algorithm parallel coverage
  #
  # Each algorithm is run in a separate sub-process with GCOV_PREFIX pointing
  # to a private work directory, so parallel runs never overwrite each other's
  # .gcda files.  Per-algo .info files are merged at the end.
  # -------------------------------------------------------------------------
  BINARY="${BUILD_DIR}/bin/otbn_test"
  if [ ! -x "${BINARY}" ]; then
    echo "ERROR: binary not found at ${BINARY}"
    exit 1
  fi

  CONFIG_DIR="${BUILD_DIR}/configs"
  INFO_DIR="${BUILD_DIR}/coverage"
  mkdir -p "${CONFIG_DIR}" "${INFO_DIR}"

  find "${BUILD_DIR}" -name '*.gcda' -delete 2>/dev/null || true

  # Detect lcov version: --ignore-errors flags only exist in lcov 2.x+
  LCOV_IGNORE=""
  GENHTML_IGNORE=""
  LCOV_MAJOR=$(lcov --version 2>/dev/null | grep -oE "LCOV version ([0-9]+)" | grep -oE "[0-9]+$" || echo "0")
  if [ "${LCOV_MAJOR}" -ge 2 ] 2>/dev/null; then
    LCOV_IGNORE="--ignore-errors inconsistent,unsupported,format,mismatch"
    GENHTML_IGNORE="--ignore-errors inconsistent,unsupported,format,corrupt,category"
  fi

  lcov ${LCOV_IGNORE} --zerocounters --directory "${BUILD_DIR}" 2>/dev/null || true

  # GCOV_PREFIX_STRIP = number of '/' in BUILD_DIR so stripped relative paths
  # look like  CMakeFiles/otbn_model.dir/src/otbn.cpp.gcda  under WORK_DIR.
  GCOV_STRIP=$(echo "${BUILD_DIR}" | tr -dc '/' | wc -c)

  # Per-algorithm worker — runs in a subshell so it can be backgrounded
  run_one_algo() {
    local ALGO="$1"
    local LOG="${INFO_DIR}/log_${ALGO}.txt"
    {
    echo "[${ALGO}] Starting at $(date +%T)"

    local CFG="${CONFIG_DIR}/config_${ALGO}.ini"
    local WORK_DIR="${INFO_DIR}/work_${ALGO}"
    local RAW_INFO="${INFO_DIR}/raw_${ALGO}.info"
    local FILTERED_INFO="${INFO_DIR}/filtered_${ALGO}.info"

    cat > "${CFG}" <<INIEOF
# Auto-generated config for algorithm: ${ALGO}
[string]
instance_prefix: testbench.otbn_dut
algorithm_type: ${ALGO}

[int]
verbosity: 1
INIEOF

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
      echo "[${ALGO}] WARNING: binary exited ${RUN_EXIT} — coverage still captured"

    lcov ${LCOV_IGNORE} \
      --capture --directory "${WORK_DIR}" \
      --output-file "${RAW_INFO}" 2>&1

    lcov ${LCOV_IGNORE} \
      --extract "${RAW_INFO}" \
        "${SCRIPT_DIR}/src/*" \
        "${SCRIPT_DIR}/include/*" \
        "${SCRIPT_DIR}/algo/*" \
      --output-file "${FILTERED_INFO}" 2>&1

    echo "[${ALGO}] Done at $(date +%T) — ${FILTERED_INFO}"
    } > "${LOG}" 2>&1
  }

  echo "============================================================"
  echo "  Launching ${#ALGOS[@]} algorithm(s) in parallel (max ${MAX_JOBS} jobs)"
  echo "  Binary : ${BINARY}"
  echo "============================================================"

  PIDS=()
  RUNNING=0
  for ALGO in "${ALGOS[@]}"; do
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
    echo "  Launching: ${ALGO}"
    run_one_algo "${ALGO}" &
    PIDS+=($!)
    RUNNING=$((RUNNING + 1))
  done

  echo "  Waiting for all jobs to finish..."
  for PID in "${PIDS[@]+"${PIDS[@]}"}"; do
    wait "${PID}" 2>/dev/null || true
  done

  # Print per-algo summary lines
  echo ""
  echo "============================================================"
  echo "  Per-algorithm results:"
  echo "============================================================"
  for ALGO in "${ALGOS[@]}"; do
    [ -f "${INFO_DIR}/log_${ALGO}.txt" ] && \
      tail -2 "${INFO_DIR}/log_${ALGO}.txt" | sed "s/^/  /"
  done

  # Merge all filtered .info files into one
  echo ""
  echo "============================================================"
  echo "  Merging coverage from ${#ALGOS[@]} algorithm(s)"
  echo "============================================================"
  MERGED_ARGS=()
  for ALGO in "${ALGOS[@]}"; do
    FILTERED="${INFO_DIR}/filtered_${ALGO}.info"
    if [ -f "${FILTERED}" ]; then
      MERGED_ARGS+=(-a "${FILTERED}")
    else
      echo "  WARNING: missing ${FILTERED} — skipping ${ALGO}"
    fi
  done

  MERGED_INFO="${INFO_DIR}/coverage_merged.info"
  lcov ${LCOV_IGNORE} "${MERGED_ARGS[@]}" --output-file "${MERGED_INFO}"

  HTML_DIR="${INFO_DIR}/html"
  genhtml ${GENHTML_IGNORE} \
    "${MERGED_INFO}" \
    --output-directory "${HTML_DIR}" \
    --title "OTBN Multi-Algorithm Coverage (${ALGOS[*]})"

  echo ""
  echo "============================================================"
  echo "  Done."
  echo "  Algorithms : ${ALGOS[*]}"
  echo "  Merged info: ${MERGED_INFO}"
  echo "  HTML report: ${HTML_DIR}/index.html"
  echo "============================================================"

else
  # Default: just run the test binary
  "${BUILD_DIR}/bin/otbn_test"
fi
