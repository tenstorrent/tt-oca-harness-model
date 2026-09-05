#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# Build and test every SMC IP — peripherals plus smc_fabric — across four
# quality gates (Release, ASAN, Coverage ≥ 95% line, CTest) and produce a structured
# summary table that mirrors sep/peripherals/run_all_peripherals.sh.
#
# Usage:
#   ./run_all_smc_tests.sh                          # all IPs, incremental
#   ./run_all_smc_tests.sh --clean                  # wipe build dirs first
#   ./run_all_smc_tests.sh plic clint smc_fabric    # run only listed IPs
#   ./run_all_smc_tests.sh --clean plic             # clean build for one IP
#
# Output:
#   logs/<ip>/release_run.log    ← Release build + test output
#   logs/<ip>/asan_run.log       ← ASAN build + test output
#   logs/<ip>/coverage_run.log   ← Coverage build + report (contains line %)
#   logs/<ip>/ctest.log          ← CTest verbose output
#   logs/<ip>/Full_result.log    ← per-IP summary
#   logs/Full_result.log         ← top-level summary across all IPs
#
# Environment:
#   SYSTEMC_HOME   Path to an Accellera SystemC install (C++20 build).
#                  If unset the script probes common macOS/Linux locations.
#   CCI_HOME       Path to an Accellera CCI install.  Required for the
#                  peripherals under smc/peripherals/; not needed for
#                  smc_fabric (which only links against SystemC).
#   WHISPER_HOME   Path to a built Tenstorrent Whisper RISC-V ISS tree
#                  (must contain build-<OS>/librvcore.a).  When set and the
#                  archive exists, cpu_cluster is automatically included in
#                  the test run.  When unset or the archive is missing,
#                  cpu_cluster is silently skipped — CI is not affected.
#   BOOST_DIR      Path to a Boost install (include/boost/version.hpp).
#                  Used by cpu_cluster; also accepted as BOOST_ROOT for
#                  compatibility with the CI/VP configure scripts.
#   JOBS           Parallel build jobs (default: all available cores).
#   COVERAGE_MIN_LINE_PCT
#                  Minimum accepted line coverage for the Coverage stage
#                  (default: 95).  A lower figure fails the stage.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_ROOT="${SCRIPT_DIR}/logs"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null \
             || sysctl -n hw.ncpu   2>/dev/null \
             || echo 4)}"
export JOBS

CLEAN=false

# ── Parse --clean flag (must precede IP names) ────────────────────────────────
ARGS=()
for arg in "$@"; do
    case "$arg" in
        --clean) CLEAN=true ;;
        -h|--help)
            sed -n '2,26p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *) ARGS+=("$arg") ;;
    esac
done
set -- "${ARGS[@]+"${ARGS[@]}"}"

# ── Colour helpers (disabled when not a TTY) ──────────────────────────────────
if [ -t 1 ]; then
    GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[0;33m'; NC='\033[0m'
else
    GREEN=''; RED=''; YELLOW=''; NC=''
fi

pass() { printf "${GREEN}PASS${NC}"; }
fail() { printf "${RED}FAIL${NC}"; }

# ── Locate SystemC ────────────────────────────────────────────────────────────
if [[ -z "${SYSTEMC_HOME:-}" ]]; then
    for _c in \
        "${HOME}/local/systemc-3.0.2-cxx20" \
        "${HOME}/local/systemc-3.0.2-cxx17" \
        "${HOME}/local/systemc-3.0.2" \
        "${HOME}/local/systemc" \
        "${HOME}/systemc-3.0.2" \
        "${HOME}/systemc" \
        /usr/local/systemc-3.0.2 \
        /usr/local/systemc \
        /usr/local \
        /usr
    do
        [[ -f "${_c}/include/systemc.h" ]] && { export SYSTEMC_HOME="${_c}"; break; }
    done
fi

# ── Locate CCI ────────────────────────────────────────────────────────────────
if [[ -z "${CCI_HOME:-}" ]]; then
    for _c in \
        "${HOME}/local/cci-cxx20" \
        "${HOME}/local/cci-1.0.1" \
        "${HOME}/local/cci" \
        "${HOME}/cci-1.0.1" \
        "${HOME}/cci" \
        /usr/local/cci-1.0.1 \
        /usr/local/cci \
        /opt/homebrew/opt/systemc-cci \
        /usr/local/opt/systemc-cci
    do
        [[ -d "${_c}/include/cci_configuration" ]] && { export CCI_HOME="${_c}"; break; }
    done
fi

# ── Locate Whisper (optional — enables cpu_cluster) ───────────────────────────
_WHISPER_OS_DIR="build-$(uname -s)"
_WHISPER_OK=false
if [[ -n "${WHISPER_HOME:-}" ]] && \
   [[ -f "${WHISPER_HOME}/${_WHISPER_OS_DIR}/librvcore.a" ]]; then
    _WHISPER_OK=true
fi

# ── Locate Boost (used by cpu_cluster; accept BOOST_ROOT alias from CI/VP) ───
if [[ -z "${BOOST_DIR:-}" ]]; then
    BOOST_DIR="${BOOST_ROOT:-}"
fi
if [[ -z "${BOOST_DIR:-}" ]]; then
    for _c in \
        "${HOME}/local/boost-1.84.0" \
        "${HOME}/local/boost" \
        /opt/homebrew \
        /usr/local \
        /usr
    do
        [[ -f "${_c}/include/boost/version.hpp" ]] && { export BOOST_DIR="${_c}"; break; }
    done
fi

echo ""
if [[ -n "${SYSTEMC_HOME:-}" ]]; then
    echo "  SYSTEMC_HOME = ${SYSTEMC_HOME}"
else
    printf "  ${YELLOW}warning: SYSTEMC_HOME not found — builds may fail${NC}\n" >&2
fi
if [[ -n "${CCI_HOME:-}" ]]; then
    echo "  CCI_HOME     = ${CCI_HOME}"
else
    printf "  ${YELLOW}warning: CCI_HOME not found — peripheral builds may fail${NC}\n" >&2
fi
if $_WHISPER_OK; then
    echo "  WHISPER_HOME = ${WHISPER_HOME}  (cpu_cluster ENABLED)"
    if [[ -n "${BOOST_DIR:-}" ]]; then
        echo "  BOOST_DIR    = ${BOOST_DIR}"
    else
        printf "  ${YELLOW}warning: BOOST_DIR not found — cpu_cluster build may fail${NC}\n" >&2
    fi
else
    printf "  ${YELLOW}cpu_cluster SKIPPED (WHISPER_HOME not set or librvcore.a not found)${NC}\n" >&2
fi
echo ""

# ── Optional: Whisper + Boost for cpu_cluster ───────────────────────────────
WHISPER_BUILD_DIR="${WHISPER_BUILD_DIR:-build-$(uname -s)}"
if [[ -z "${WHISPER_HOME:-}" ]]; then
    REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
    for _w in \
        "${REPO_ROOT}/../whisper/whisper" \
        "${REPO_ROOT}/../whisper" \
        "${HOME}/whisper/whisper" \
        "${HOME}/whisper"
    do
        [[ -f "${_w}/${WHISPER_BUILD_DIR}/librvcore.a" ]] && { export WHISPER_HOME="${_w}"; break; }
    done
fi
_cpu_cluster_ready=false
if [[ -n "${WHISPER_HOME:-}" && -f "${WHISPER_HOME}/${WHISPER_BUILD_DIR}/librvcore.a" ]]; then
    if [[ -n "${BOOST_DIR:-}" && -f "${BOOST_DIR}/include/boost/version.hpp" ]]; then
        _cpu_cluster_ready=true
    elif [[ -z "${BOOST_DIR:-}" ]]; then
        for _b in \
            "${HOME}/local/boost" \
            /usr/local/boost \
            /opt/homebrew/opt/boost
        do
            [[ -f "${_b}/include/boost/version.hpp" ]] && { export BOOST_DIR="${_b}"; _cpu_cluster_ready=true; break; }
        done
    fi
fi
if [[ "${_cpu_cluster_ready}" == true ]]; then
    echo "  WHISPER_HOME = ${WHISPER_HOME}"
    echo "  BOOST_DIR    = ${BOOST_DIR}"
    echo ""
fi

# ── IP registry ───────────────────────────────────────────────────────────────
# Format: "display_name:src_dir"
# smc_fabric is listed last; it does not use CCI and its run_tests.sh will
# simply ignore CCI_HOME even if the variable happens to be set.
ALL_IPS=(
    "avsbus_controller:${SCRIPT_DIR}/peripherals/avsbus_controller"
    "beu:${SCRIPT_DIR}/peripherals/beu"
    "bootrom:${SCRIPT_DIR}/peripherals/bootrom"
    "clint:${SCRIPT_DIR}/peripherals/clint"
    "i2c_controller:${SCRIPT_DIR}/peripherals/i2c_controller"
    "cpu_ctrl:${SCRIPT_DIR}/peripherals/cpu_ctrl"
    "i3c_controller:${SCRIPT_DIR}/peripherals/i3c_controller"
    "dma:${SCRIPT_DIR}/peripherals/dma"
    "memory_zeroer:${SCRIPT_DIR}/peripherals/memory_zeroer"
    "pll_wrapper:${SCRIPT_DIR}/peripherals/pll_wrapper"
    "pvt_wrap:${SCRIPT_DIR}/peripherals/pvt_wrap"
    "plic:${SCRIPT_DIR}/peripherals/plic"
    "reset_unit:${SCRIPT_DIR}/peripherals/reset_unit"
    "scratchpad_ram:${SCRIPT_DIR}/peripherals/scratchpad_ram"
    "telemetry_receiver:${SCRIPT_DIR}/peripherals/telemetry_receiver"
    "uart:${SCRIPT_DIR}/peripherals/uart"
    "wdt:${SCRIPT_DIR}/peripherals/wdt"
    "smc_fabric:${SCRIPT_DIR}/smc_fabric"
    "octs_system_timer:${SCRIPT_DIR}/peripherals/octs_system_timer"
    "aou:${SCRIPT_DIR}/../aou"
)
# cpu_cluster requires Whisper (Tenstorrent internal) and Boost; only include
# when both were found (_cpu_cluster_ready, computed above alongside the
# WHISPER_HOME/BOOST_DIR banner).
if [[ "${_cpu_cluster_ready}" == true ]]; then
    ALL_IPS+=("cpu_cluster:${SCRIPT_DIR}/cpu_cluster")
fi

# ── Filter by command-line IP names ──────────────────────────────────────────
if [[ $# -gt 0 ]]; then
    IPS=()
    for req in "$@"; do
        found=false
        for entry in "${ALL_IPS[@]}"; do
            [[ "${entry%%:*}" == "$req" ]] && { IPS+=("$entry"); found=true; break; }
        done
        $found || printf "${YELLOW}warning: unknown IP '%s' — skipping${NC}\n" "$req" >&2
    done
else
    IPS=("${ALL_IPS[@]}")
fi

# ── Display run plan ──────────────────────────────────────────────────────────
IP_NAMES=()
for entry in "${IPS[@]}"; do IP_NAMES+=("${entry%%:*}"); done

echo "IPs to test (${#IPS[@]}): ${IP_NAMES[*]}"
echo "Build mode       : $($CLEAN && echo 'clean' || echo 'incremental')"
echo "Logs directory   : ${LOG_ROOT}"
echo ""

mkdir -p "${LOG_ROOT}"

# ── Helper: extract line-coverage % from a coverage run log ──────────────────
#
# Handles two formats produced by smc run_tests.sh --coverage:
#
#   llvm-cov report (Clang / AppleClang) — TOTAL line carries four float
#   percentages in order: regions, functions, lines, branches.  Lines% is
#   the third value (index 2).
#
#     TOTAL    150   30   80.00%   45   7   84.44%   450   70   84.44%   ...
#
#   gcovr (GCC on Linux) — TOTAL line carries a single integer percent:
#
#     TOTAL                               450     380    84%
#
extract_coverage() {
    local log="$1"
    local pct

    # llvm-cov: third float percentage on the TOTAL line = line coverage.
    pct=$(perl -ne '
        if (/^TOTAL\b/) {
            my @p = /([0-9]+\.[0-9]+)%/g;
            print "$p[2]\n" if defined $p[2];
        }
    ' "$log" 2>/dev/null | tail -1)

    # gcovr / lcov fallback: any integer % on the TOTAL line.
    if [[ -z "$pct" ]]; then
        pct=$(perl -ne '/^TOTAL\b/ and /([0-9]+)%/ and print "$1\n"' \
              "$log" 2>/dev/null | tail -1)
    fi

    echo "${pct:-n/a}"
}

# True if $1 is a number >= $2 (default 95).  "n/a" / empty fails the gate.
COVERAGE_MIN_LINE_PCT="${COVERAGE_MIN_LINE_PCT:-95}"
coverage_meets_min() {
    local pct="$1" min="${2:-${COVERAGE_MIN_LINE_PCT}}"
    [[ -z "${pct}" || "${pct}" == "n/a" ]] && return 1
    awk -v p="${pct}" -v m="${min}" 'BEGIN { exit (p+0 < m+0) }'
}

# ── Helper: run one test stage and capture output ────────────────────────────
# run_stage  src_dir  log_file  [run_tests_flags...]
# Returns the exit code of run_tests.sh.
run_stage() {
    local src_dir="$1"
    local log_file="$2"
    shift 2
    (
        cd "${src_dir}"
        bash run_tests.sh "$@"
    ) > "${log_file}" 2>&1
    return $?
}

# ── Top-level summary header ──────────────────────────────────────────────────
TOP_SUMMARY="${LOG_ROOT}/Full_result.log"
{
    echo "============================================================"
    echo "  run_all_smc_tests.sh — Full Result Summary"
    echo "  $(date)"
    echo "============================================================"
    printf "%-20s  %-12s  %-12s  %-12s  %-12s  %s\n" \
        "IP" "Release" "ASAN" "Coverage" "CTest" "Coverage%"
    echo "------------------------------------------------------------"
} > "${TOP_SUMMARY}"

# ── Per-IP run ────────────────────────────────────────────────────────────────
OVERALL_PASS=true
CLEAN_FLAG=(); $CLEAN && CLEAN_FLAG=(--clean)

for entry in "${IPS[@]}"; do
    name="${entry%%:*}"
    src_dir="${entry##*:}"
    plog="${LOG_ROOT}/${name}"
    mkdir -p "${plog}"

    echo "──────────────────────────────────────────────"
    echo "  ${name}"
    echo "──────────────────────────────────────────────"

    # 1. Release build + run
    printf "  Release build ... "
    run_stage "${src_dir}" "${plog}/release_run.log" "${CLEAN_FLAG[@]+"${CLEAN_FLAG[@]}"}"
    rc=$?
    if [[ $rc -eq 0 ]]; then pass; echo; rel_label="PASS"
    else                       fail; echo; rel_label="FAIL"; OVERALL_PASS=false; fi

    # 2. ASAN build + run
    if [[ "${SKIP_ASAN:-0}" == "1" ]]; then
      printf "  ASAN build    ... SKIP (SKIP_ASAN=1)\n"
      asan_label="SKIP"
    else
      printf "  ASAN build    ... "
      run_stage "${src_dir}" "${plog}/asan_run.log" --asan "${CLEAN_FLAG[@]+"${CLEAN_FLAG[@]}"}"
      rc=$?
      if [[ $rc -eq 0 ]] && ! "${SCRIPT_DIR}/scripts/enforce_asan_clean.sh" \
            "${src_dir}/build_asan" "${plog}/asan_run.log"; then
        rc=1
      fi
      if [[ $rc -eq 0 ]]; then pass; echo; asan_label="PASS"
      else                      fail; echo; asan_label="FAIL"; OVERALL_PASS=false; fi
    fi

    # 3. Coverage build + report
    printf "  Coverage      ... "
    run_stage "${src_dir}" "${plog}/coverage_run.log" --coverage "${CLEAN_FLAG[@]+"${CLEAN_FLAG[@]}"}"
    rc=$?
    cov_pct=$(extract_coverage "${plog}/coverage_run.log")
    if [[ $rc -ne 0 ]]; then
        fail; echo; cov_label="FAIL"; OVERALL_PASS=false
    elif ! coverage_meets_min "${cov_pct}"; then
        fail; printf " (%s%% < %s%%)\n" "${cov_pct}" "${COVERAGE_MIN_LINE_PCT}"
        cov_label="FAIL"; OVERALL_PASS=false
    else
        pass; printf " (%s%%)\n" "${cov_pct}"; cov_label="PASS"
    fi

    # 4. CTest  (uses the Release build directory created in step 1)
    printf "  CTest         ... "
    run_stage "${src_dir}" "${plog}/ctest.log" --ctest
    rc=$?
    if [[ $rc -eq 0 ]]; then pass; echo; ctest_label="PASS"
    else                      fail; echo; ctest_label="FAIL"; OVERALL_PASS=false; fi

    # Per-IP Full_result.log
    {
        echo "============================================================"
        echo "  ${name} — Full Result"
        echo "  $(date)"
        echo "============================================================"
        printf "  %-16s : %s\n"          "Release build" "${rel_label}"
        printf "  %-16s : %s\n"          "ASAN build"    "${asan_label}"
        printf "  %-16s : %s  (%s%%)\n"  "Coverage"      "${cov_label}" "${cov_pct}"
        printf "  %-16s : %s\n"          "CTest"         "${ctest_label}"
        echo "------------------------------------------------------------"
        echo "  Log files:"
        echo "    release_run.log"
        echo "    asan_run.log"
        echo "    coverage_run.log"
        echo "    ctest.log"
    } > "${plog}/Full_result.log"

    # Append row to top-level summary
    printf "%-20s  %-12s  %-12s  %-12s  %-12s  %s\n" \
        "${name}" "${rel_label}" "${asan_label}" "${cov_label}" "${ctest_label}" "${cov_pct}%" \
        >> "${TOP_SUMMARY}"

    echo ""
done

# ── Top-level footer ──────────────────────────────────────────────────────────
{
    echo "------------------------------------------------------------"
    if $OVERALL_PASS; then
        echo "  Overall: PASS — all IPs passed all stages"
    else
        echo "  Overall: FAIL — one or more IPs/stages failed"
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
