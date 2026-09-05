#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# Build (if needed) and run the SMC Fabric SystemC test bench.
#
# Usage:
#   ./run_tests.sh                # incremental build + run  (Release)
#   ./run_tests.sh --clean        # wipe build/ first, then configure/build/run
#   ./run_tests.sh --ctest        # run via ctest instead of executing the binary
#   ./run_tests.sh --asan         # build with AddressSanitizer; run and report errors
#                                 #   Linux: also enables LeakSanitizer (detect_leaks=1)
#   ./run_tests.sh --coverage     # build with coverage; run and print line report
#                                 #   Clang/AppleClang: LLVM instrumented coverage
#                                 #   GCC: gcov  (requires gcovr or lcov+genhtml)
#
# The --asan and --coverage modes use isolated build directories
# (build_asan/ and build_cov/) so they never clobber a plain Release build.
#
# Environment:
#   SYSTEMC_HOME  Path to an Accellera SystemC install (C++20 build).  If unset
#                 the script probes common macOS (Homebrew) and Linux locations.
#   BUILD_TYPE    CMake build type (default: Release; Debug for --coverage).
#   JOBS          Parallel build jobs (default: all available cores).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null \
             || sysctl -n hw.ncpu   2>/dev/null \
             || echo 4)}"
OS="$(uname -s)"

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
            sed -n '2,19p' "$0" | sed 's/^# \{0,1\}//'
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

if [[ -z "${SYSTEMC_HOME:-}" ]]; then
    for _candidate in \
        /opt/homebrew/opt/systemc \
        /usr/local/opt/systemc \
        /usr/local \
        /usr
    do
        if [[ -f "${_candidate}/include/systemc.h" ]] || \
           [[ -f "${_candidate}/lib/libsystemc.a" ]] || \
           [[ -f "${_candidate}/lib64/libsystemc.a" ]]; then
            export SYSTEMC_HOME="${_candidate}"
            break
        fi
    done
fi

if [[ -n "${SYSTEMC_HOME:-}" ]]; then
    echo ">> Using SYSTEMC_HOME=${SYSTEMC_HOME}"
else
    echo ">> SYSTEMC_HOME not set; relying on CMake-installed SystemC::systemc"
fi

if (( CLEAN )) && [[ -d "${BUILD_DIR}" ]]; then
    echo ">> Removing ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
fi

if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    echo ">> Configuring (${BUILD_TYPE}) in ${BUILD_DIR}"
    # shellcheck disable=SC2086
    # No -DSMC_CXX_STANDARD override here: CMakeLists.txt calls
    # smc_detect_systemc_cxx_std() to match whatever standard the linked
    # SystemC was built with (e.g. C++17 on macOS/Homebrew vs C++20 on the
    # reference Linux toolchain). Hard-coding 20 here breaks any C++17
    # SystemC install with an undefined sc_api_version_*_cxx202002L symbol.
    cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
          -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
          ${CMAKE_EXTRA}
fi

echo ">> Building with -j${JOBS}"
cmake --build "${BUILD_DIR}" -j "${JOBS}"

if [[ -f "${BUILD_DIR}/compile_commands.json" ]]; then
    ln -sf "$(basename "${BUILD_DIR}")/compile_commands.json" \
           "${SCRIPT_DIR}/compile_commands.json"
fi

TB_BIN="${BUILD_DIR}/test/smc_fabric_tb"
if [[ ! -x "${TB_BIN}" ]]; then
    echo "ERROR: test binary not found at ${TB_BIN}" >&2
    exit 1
fi

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

if (( USE_ASAN )); then
    echo ""
    echo ">> Running with AddressSanitizer: ${TB_BIN}"
    if [[ "${OS}" == "Linux" ]]; then
        echo "   Linux: LeakSanitizer (detect_leaks=1) also active"
    fi
    echo ""

    ASAN_LOG="${BUILD_DIR}/asan.log"
    if [[ "${OS}" == "Linux" ]]; then
        _ASAN_OPTS="halt_on_error=0:detect_leaks=1:log_path=${ASAN_LOG}"
    else
        _ASAN_OPTS="halt_on_error=0:log_path=${ASAN_LOG}"
    fi
    ASAN_OPTIONS="${_ASAN_OPTS}" "${TB_BIN}"; TB_EXIT=$?
    echo ""

    if compgen -G "${ASAN_LOG}.*" > /dev/null 2>&1; then
        echo "===== AddressSanitizer report ====="
        cat "${ASAN_LOG}".*
        echo "==================================="
        LEAK_COUNT=$(grep -c "ERROR: AddressSanitizer" "${ASAN_LOG}".* 2>/dev/null || true)
        if [[ "${LEAK_COUNT}" -eq 0 ]]; then
            echo ">> ASan: NO memory errors detected."
        else
            echo ">> ASan: ${LEAK_COUNT} error(s) detected (see report above)."
        fi
    else
        echo ">> ASan: NO memory errors detected."
    fi
    exit "${TB_EXIT}"

elif (( USE_COVERAGE )); then
    COVERAGE_TOOL="$(cat "${BUILD_DIR}/coverage_tool.txt" 2>/dev/null || echo "llvm")"
    HTML_DIR="${BUILD_DIR}/coverage-report"
    SOURCES=(
        "${SCRIPT_DIR}/src/smc_fabric.cpp"
        "${SCRIPT_DIR}/test/smc_fabric_tb.cpp"
    )

    echo ""
    echo ">> Running with ${COVERAGE_TOOL} coverage instrumentation: ${TB_BIN}"
    echo ""

    if [[ "${COVERAGE_TOOL}" == "llvm" ]]; then
        PROFRAW="${BUILD_DIR}/smc_fabric_tb.profraw"
        PROFDATA="${BUILD_DIR}/smc_fabric_tb.profdata"
        PROFDATA_CMD="$(_find_llvm_tool llvm-profdata)"
        COV_CMD="$(_find_llvm_tool llvm-cov)"
        if [[ -z "${PROFDATA_CMD}" || -z "${COV_CMD}" ]]; then
            echo "ERROR: llvm-profdata / llvm-cov not found." >&2
            exit 1
        fi

        LLVM_PROFILE_FILE="${PROFRAW}" "${TB_BIN}"
        echo ""
        ${PROFDATA_CMD} merge -sparse "${PROFRAW}" -o "${PROFDATA}"
        echo ""
        echo "===== Line coverage summary ====="
        ${COV_CMD} report "${TB_BIN}" \
            -instr-profile="${PROFDATA}" \
            "${SOURCES[@]}"
        ${COV_CMD} show "${TB_BIN}" \
            -instr-profile="${PROFDATA}" \
            "${SOURCES[@]}" \
            -format=html \
            -output-dir="${HTML_DIR}" \
            -show-line-counts-or-regions 2>/dev/null || true
    else
        "${TB_BIN}"
        echo ""
        if command -v gcovr &>/dev/null; then
            echo "===== Line coverage summary (gcovr) ====="
            gcovr \
                --root "${SCRIPT_DIR}/src" \
                --object-directory "${BUILD_DIR}" \
                --filter "${SCRIPT_DIR}/src/" \
                --filter "${SCRIPT_DIR}/test/"
        else
            echo "WARNING: gcovr not found; run gcov manually on ${BUILD_DIR}." >&2
        fi
    fi

    if [[ -f "${HTML_DIR}/index.html" ]]; then
        echo ""
        echo ">> HTML coverage report: ${HTML_DIR}/index.html"
    fi

    _REPO="$(git -C "${SCRIPT_DIR}" rev-parse --show-toplevel)"
    # shellcheck disable=SC1091
    source "${_REPO}/scripts/coverage_gate.sh"
    if [[ "${COVERAGE_TOOL:-}" == "llvm" && -n "${COV_CMD:-}" && -n "${PROFDATA:-}" && -f "${PROFDATA}" ]]; then
        ${COV_CMD} report "${TB_BIN}" \
            ${_OBJECT_ARGS[@]+"${_OBJECT_ARGS[@]}"} \
            -instr-profile="${PROFDATA}" \
            "${SOURCES[@]}" \
            > "${BUILD_DIR}/coverage_summary.txt"
        coverage_gate_from_log "${BUILD_DIR}/coverage_summary.txt" || exit 1
    elif [[ -f "${BUILD_DIR}/coverage.info" ]]; then
        coverage_gate_from_lcov_info "${BUILD_DIR}/coverage.info" || exit 1
    else
        echo ">> Coverage gate FAIL (no coverage summary to parse)" >&2
        exit 1
    fi

elif (( USE_CTEST )); then
    echo ">> Running via ctest"
    ctest --test-dir "${BUILD_DIR}" --output-on-failure
else
    echo ">> Running ${TB_BIN}"
    "${TB_BIN}"
fi
