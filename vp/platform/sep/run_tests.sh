#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# =============================================================================
# vp/platform/sep/run_tests.sh
#
# Build (if needed) and run the SEP platform unit tests
# (inc/xbar_policy.h — the local-crossbar connectivity policy; test/CMakeLists.txt
# is standalone and needs only a C++20 compiler — no SystemC/CCI/VeeR).
#
# Usage:
#   ./run_tests.sh                # incremental build + run  (Release)
#   ./run_tests.sh --clean        # wipe the build dir first, then configure/build/run
#   ./run_tests.sh --ctest        # run via ctest instead of executing the binary
#   ./run_tests.sh --asan         # build with AddressSanitizer+UBSan; run and gate
#                                 #   Linux: also enables LeakSanitizer (detect_leaks=1)
#   ./run_tests.sh --coverage     # build with coverage; run, print report, gate ≥ 95%
#                                 #   Clang/AppleClang: LLVM instrumented coverage
#                                 #   GCC: gcov  (requires gcovr or lcov+genhtml)
#
# The --asan and --coverage modes use isolated build directories
# (build_asan/ and build_cov/) so they never clobber a plain Release build.
#
# Environment:
#   BUILD_TYPE              CMake build type (default: Release; Debug for --coverage).
#   JOBS                    Parallel build jobs (default: all available cores).
#   COVERAGE_MIN_LINE_PCT   Coverage gate on the header under test (default: 95).
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/test"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null \
             || sysctl -n hw.ncpu   2>/dev/null \
             || echo 4)}"
OS="$(uname -s)"   # Linux | Darwin
MIN_PCT="${COVERAGE_MIN_LINE_PCT:-95}"

USE_CTEST=0
CLEAN=0
USE_ASAN=0
USE_COVERAGE=0

for arg in "$@"; do
    case "$arg" in
        --clean)    CLEAN=1 ;;
        --ctest)    USE_CTEST=1 ;;
        --asan)     USE_ASAN=1 ;;
        --coverage) USE_COVERAGE=1 ;;
        -h|--help)
            sed -n '2,28p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            echo "Unknown argument: $arg" >&2
            exit 2
            ;;
    esac
done

if (( USE_ASAN && USE_COVERAGE )); then
    echo "ERROR: --asan and --coverage are mutually exclusive." >&2
    exit 2
fi

# Isolated build directory per instrumented mode (never share CMake caches).
if (( USE_ASAN )); then
    BUILD_DIR="${SCRIPT_DIR}/build_asan"
    CMAKE_EXTRA="-DENABLE_ASAN=ON"
elif (( USE_COVERAGE )); then
    BUILD_DIR="${SCRIPT_DIR}/build_cov"
    BUILD_TYPE="Debug"
    CMAKE_EXTRA="-DENABLE_COVERAGE=ON"
else
    BUILD_DIR="${SCRIPT_DIR}/build"
    CMAKE_EXTRA=""
fi

# ---------------------------------------------------------------------------
# (Re)configure + build
# ---------------------------------------------------------------------------
if (( CLEAN )) && [[ -d "${BUILD_DIR}" ]]; then
    echo ">> Removing ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
fi

if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    echo ">> Configuring (${BUILD_TYPE}) in ${BUILD_DIR}"
    # shellcheck disable=SC2086
    cmake -S "${SRC_DIR}" -B "${BUILD_DIR}" \
          -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
          ${CMAKE_EXTRA}
fi

echo ">> Building with -j${JOBS}"
cmake --build "${BUILD_DIR}" -j "${JOBS}"

TB_BIN="${BUILD_DIR}/xbar_policy_tb"
if [[ ! -x "${TB_BIN}" ]]; then
    echo "ERROR: test binary not found at ${TB_BIN}" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Helper: find a versioned LLVM tool.
# ---------------------------------------------------------------------------
_find_llvm_tool() {
    local base="$1"
    if command -v "${base}" &>/dev/null; then echo "${base}"; return; fi
    if command -v xcrun &>/dev/null && xcrun "${base}" --version &>/dev/null 2>&1; then
        echo "xcrun ${base}"; return
    fi
    for _v in 20 19 18 17 16 15 14 13; do
        if command -v "${base}-${_v}" &>/dev/null; then
            echo "${base}-${_v}"; return
        fi
    done
    echo ""
}

# Shared gate scripts live under smc/scripts at the repo root.
_repo_root() {
    local _d="${SCRIPT_DIR}"
    while [[ -n "${_d}" && "${_d}" != "/" ]]; do
        if [[ -f "${_d}/smc/scripts/enforce_asan_clean.sh" ]]; then
            echo "${_d}"; return
        fi
        _d="$(dirname "${_d}")"
    done
    echo ""
}

_gate_pct() {
    local pct="$1"
    if [[ -z "${pct}" ]]; then
        echo "ERROR: could not parse line coverage." >&2
        exit 1
    fi
    echo ">> Line coverage (inc/xbar_policy.h): ${pct}%  (gate: ≥ ${MIN_PCT}%)"
    if awk -v p="${pct}" -v m="${MIN_PCT}" 'BEGIN { exit (p+0 < m+0) }'; then
        echo ">> Coverage gate PASS"
    else
        echo "ERROR: line coverage ${pct}% is below the ${MIN_PCT}% gate." >&2
        exit 1
    fi
}

if (( USE_CTEST )); then
    (cd "${BUILD_DIR}" && ctest --output-on-failure)
    exit $?
fi

# ---------------------------------------------------------------------------
# Run
# ---------------------------------------------------------------------------
if (( USE_ASAN )); then
    echo ""
    echo ">> Running with AddressSanitizer: ${TB_BIN}"
    echo ""

    ASAN_LOG="${BUILD_DIR}/asan.log"
    if [[ "${OS}" == "Linux" ]]; then
        _ASAN_OPTS="halt_on_error=0:detect_leaks=1:log_path=${ASAN_LOG}"
    else
        _ASAN_OPTS="halt_on_error=0:log_path=${ASAN_LOG}"
    fi
    TB_EXIT=0
    ASAN_OPTIONS="${_ASAN_OPTS}" UBSAN_OPTIONS="print_stacktrace=1:log_path=${ASAN_LOG}" \
        "${TB_BIN}" || TB_EXIT=$?
    echo ""

    if compgen -G "${ASAN_LOG}.*" > /dev/null 2>&1; then
        echo "===== Sanitizer report ====="
        cat "${ASAN_LOG}".*
        echo "============================"
    fi
    ROOT="$(_repo_root)"
    if [[ -z "${ROOT}" ]]; then
        echo "ERROR: enforce_asan_clean.sh not found" >&2
        exit 1
    fi
    "${ROOT}/smc/scripts/enforce_asan_clean.sh" "${BUILD_DIR}" || exit 1
    exit "${TB_EXIT}"

elif (( USE_COVERAGE )); then
    COVERAGE_TOOL="$(cat "${BUILD_DIR}/coverage_tool.txt" 2>/dev/null || echo "llvm")"

    echo ""
    echo ">> Running with ${COVERAGE_TOOL} coverage instrumentation: ${TB_BIN}"
    echo ""

    HTML_DIR="${BUILD_DIR}/coverage-report"
    # Coverage is measured against the model under test (the header).
    SOURCES=("${SCRIPT_DIR}/inc/xbar_policy.h")

    # ---- LLVM instrumented coverage (Clang / AppleClang) ------------------
    if [[ "${COVERAGE_TOOL}" == "llvm" ]]; then
        PROFRAW="${BUILD_DIR}/xbar_policy_tb.profraw"
        PROFDATA="${BUILD_DIR}/xbar_policy_tb.profdata"

        LLVM_PROFILE_FILE="${PROFRAW}" "${TB_BIN}"
        echo ""

        PROFDATA_CMD="$(_find_llvm_tool llvm-profdata)"
        COV_CMD="$(_find_llvm_tool llvm-cov)"
        if [[ -z "${PROFDATA_CMD}" || -z "${COV_CMD}" ]]; then
            echo "ERROR: llvm-profdata / llvm-cov not found." >&2
            exit 1
        fi

        echo ">> Merging profile data (${PROFDATA_CMD}) …"
        ${PROFDATA_CMD} merge -sparse "${PROFRAW}" -o "${PROFDATA}"

        echo ""
        echo "===== Line coverage summary (model under test) ====="
        REPORT="$(${COV_CMD} report "${TB_BIN}" -instr-profile="${PROFDATA}" "${SOURCES[@]}")"
        echo "${REPORT}"

        for src in "${SOURCES[@]}"; do
            echo ""
            echo "===== Uncovered lines in $(basename "${src}") ====="
            ${COV_CMD} show "${TB_BIN}" \
                -instr-profile="${PROFDATA}" \
                -sources "${src}" \
                -format=text \
                -show-line-counts-or-regions \
                | grep -E "^\s+[0-9]+\|[[:space:]]+0\|" \
                || echo "(none — full coverage)"
        done

        ${COV_CMD} show "${TB_BIN}" \
            -instr-profile="${PROFDATA}" \
            "${SOURCES[@]}" \
            -format=html \
            -output-dir="${HTML_DIR}" \
            -show-line-counts-or-regions 2>/dev/null || true

        # llvm-cov TOTAL columns: Regions Missed Cover Functions Missed Executed
        # Lines Missed Cover ... — the third percentage is line coverage.
        PCT="$(echo "${REPORT}" | perl -ne '
            if (/^TOTAL\b/) { my @p = /([0-9]+\.[0-9]+)%/g;
                              print(defined $p[2] ? "$p[2]\n" : "$p[0]\n"); }')"
        _gate_pct "${PCT}"

    # ---- gcov coverage (GCC) -----------------------------------------------
    else
        "${TB_BIN}"
        echo ""

        if command -v gcovr &>/dev/null; then
            echo "===== Line coverage summary (gcovr) ====="
            REPORT="$(gcovr \
                --root "${SCRIPT_DIR}" \
                --object-directory "${BUILD_DIR}" \
                --filter "${SCRIPT_DIR}/inc/")"
            echo "${REPORT}"

            mkdir -p "${HTML_DIR}"
            gcovr \
                --root "${SCRIPT_DIR}" \
                --object-directory "${BUILD_DIR}" \
                --filter "${SCRIPT_DIR}/inc/" \
                --html --html-details \
                -o "${HTML_DIR}/index.html" 2>/dev/null || true

            PCT="$(echo "${REPORT}" | perl -ne '
                if (/^TOTAL\b/ && /([0-9]+(?:\.[0-9]+)?)%/) { print "$1\n"; }' | tail -1)"
            _gate_pct "${PCT}"

        elif command -v lcov &>/dev/null && command -v genhtml &>/dev/null; then
            INFO="${BUILD_DIR}/coverage.info"
            # Honour $GCOV (CI RHEL 8 exports the gcc-toolset gcov path).
            LCOV_GCOV_ARGS=()
            [[ -n "${GCOV:-}" ]] && LCOV_GCOV_ARGS=(--gcov-tool "${GCOV}")
            lcov --capture --directory "${BUILD_DIR}" --output-file "${INFO}" \
                 --quiet ${LCOV_GCOV_ARGS[@]+"${LCOV_GCOV_ARGS[@]}"}
            lcov --extract "${INFO}" "${SCRIPT_DIR}/inc/*" \
                 --output-file "${INFO}" --quiet
            echo "===== Line coverage summary (lcov) ====="
            lcov --list "${INFO}"
            mkdir -p "${HTML_DIR}"
            genhtml "${INFO}" --output-directory "${HTML_DIR}" --quiet
            PCT="$(lcov --summary "${INFO}" 2>&1 |
                   sed -n 's/^ *lines\.*: *\([0-9.]*\)%.*/\1/p' | tail -1)"
            _gate_pct "${PCT}"
        else
            echo "ERROR: neither gcovr nor lcov/genhtml found; cannot gate coverage." >&2
            exit 1
        fi
    fi
    exit 0
fi

# Plain Release run.
"${TB_BIN}"
