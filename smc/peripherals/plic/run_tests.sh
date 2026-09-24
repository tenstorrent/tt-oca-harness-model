#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# Build (if needed) and run the SMC PLIC SystemC test bench.
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
#   SYSTEMC_HOME  Path to an Accellera SystemC install.  If unset the script
#                 probes common macOS (Homebrew) and Linux (/usr/local, /usr)
#                 locations.  Set this if SystemC is in a non-standard prefix.
#   BUILD_TYPE    CMake build type (default: Release; Debug for --coverage).
#   JOBS          Parallel build jobs (default: all available cores).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null \
             || sysctl -n hw.ncpu   2>/dev/null \
             || echo 4)}"
OS="$(uname -s)"   # Linux | Darwin

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

# Choose an isolated build directory for each instrumented mode so different
# compiler flags never invalidate each other's CMakeCache.
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
# Locate SystemC
# Probe well-known prefix directories so the user doesn't have to set
# SYSTEMC_HOME on a freshly-cloned machine.  Covers:
#   macOS Homebrew arm64 : /opt/homebrew/opt/systemc
#   macOS Homebrew x86   : /usr/local/opt/systemc
#   Linux manual install : /usr/local  (./configure --prefix=/usr/local)
#   Linux system package : /usr
# ---------------------------------------------------------------------------
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

# ---------------------------------------------------------------------------
# Locate CCI (SystemC Configuration, Control and Inspection)
# Probes well-known locations; honours CCI_HOME environment variable.
# ---------------------------------------------------------------------------
if [[ -z "${CCI_HOME:-}" ]]; then
    for _candidate in \
        "${HOME}/cci" \
        /usr/local/cci \
        /opt/homebrew/opt/systemc-cci
    do
        if [[ -f "${_candidate}/include/cci_configuration" ]]; then
            export CCI_HOME="${_candidate}"
            break
        fi
    done
fi

if [[ -n "${CCI_HOME:-}" ]]; then
    echo ">> Using CCI_HOME=${CCI_HOME}"
    # Make dynamic CCI libraries visible to the loader at run time.
    if [[ "${OS}" == "Darwin" ]]; then
        export DYLD_LIBRARY_PATH="${CCI_HOME}/lib${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}"
    else
        export LD_LIBRARY_PATH="${CCI_HOME}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
    fi
else
    echo ">> CCI_HOME not set; CCI libs must be in the system library path"
fi

# ---------------------------------------------------------------------------
# (Re)configure
# ---------------------------------------------------------------------------
if (( CLEAN )) && [[ -d "${BUILD_DIR}" ]]; then
    echo ">> Removing ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
fi

if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    echo ">> Configuring (${BUILD_TYPE}) in ${BUILD_DIR}"
    # shellcheck disable=SC2086
    cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
          -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
          ${CMAKE_EXTRA} \
          ${CCI_HOME:+-DCCI_HOME="${CCI_HOME}"}
fi

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
echo ">> Building with -j${JOBS}"
cmake --build "${BUILD_DIR}" -j "${JOBS}"

# ---------------------------------------------------------------------------
# Refresh compile_commands.json symlink at the IP root for IDE navigation
# (clangd / Microsoft C/C++ extension auto-detects it there).
# ---------------------------------------------------------------------------
if [[ -f "${BUILD_DIR}/compile_commands.json" ]]; then
    ln -sf "$(basename "${BUILD_DIR}")/compile_commands.json" \
           "${SCRIPT_DIR}/compile_commands.json"
fi

# ---------------------------------------------------------------------------
# Locate test binary
# ---------------------------------------------------------------------------
TB_BIN="${BUILD_DIR}/test/plic_tb"
WIDE_TB_BIN="${BUILD_DIR}/test/plic_wide_tb"
if [[ ! -x "${TB_BIN}" ]]; then
    echo "ERROR: test binary not found at ${TB_BIN}" >&2
    exit 1
fi
if [[ ! -x "${WIDE_TB_BIN}" ]]; then
    echo "ERROR: test binary not found at ${WIDE_TB_BIN}" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Helper: find a versioned LLVM tool.
# Tries the bare name first, then xcrun (macOS), then llvm-<tool>-N for
# versions 20..13 (common on Debian/Ubuntu: apt install llvm-18).
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

# ---------------------------------------------------------------------------
# Run
# ---------------------------------------------------------------------------
if (( USE_ASAN )); then
    echo ""
    echo ">> Running with AddressSanitizer: ${TB_BIN}"
    echo "   Detects: heap/stack buffer overflows, use-after-free, use-after-scope"
    if [[ "${OS}" == "Linux" ]]; then
        echo "   Linux: LeakSanitizer (detect_leaks=1) also active"
    fi
    echo ""

    ASAN_LOG="${BUILD_DIR}/asan.log"
    # detect_leaks=1 is supported on Linux (LeakSanitizer).
    # Apple Clang's ASan aborts on this option, so it is omitted on macOS.
    if [[ "${OS}" == "Linux" ]]; then
        _ASAN_OPTS="halt_on_error=0:detect_leaks=1:log_path=${ASAN_LOG}"
    else
        _ASAN_OPTS="halt_on_error=0:log_path=${ASAN_LOG}"
    fi
    ASAN_OPTIONS="${_ASAN_OPTS}" "${TB_BIN}"; TB_EXIT=$?
    if (( TB_EXIT == 0 )); then
        echo ">> Running with AddressSanitizer: ${WIDE_TB_BIN}"
        ASAN_OPTIONS="${_ASAN_OPTS}" "${WIDE_TB_BIN}"; TB_EXIT=$?
    fi
    echo ""

    if compgen -G "${ASAN_LOG}.*" > /dev/null 2>&1; then
        echo "===== AddressSanitizer report ====="
        cat "${ASAN_LOG}".*
        echo "==================================="
        LEAK_COUNT=$(grep -c "ERROR: AddressSanitizer" "${ASAN_LOG}".* 2>/dev/null || true)
        echo ""
        if [[ "${LEAK_COUNT}" -eq 0 ]]; then
            echo ">> ASan: NO memory errors detected."
        else
            echo ">> ASan: ${LEAK_COUNT} error(s) detected (see report above)."
        fi
    else
        echo ">> ASan: NO memory errors detected."
    fi
    _asan_gate=""
    _d="${SCRIPT_DIR}"
    while [[ -n "${_d}" && "${_d}" != "/" ]]; do
        if [[ -f "${_d}/smc/scripts/enforce_asan_clean.sh" ]]; then
            _asan_gate="${_d}/smc/scripts/enforce_asan_clean.sh"
            break
        fi
        _d="$(dirname "${_d}")"
    done
    if [[ -z "${_asan_gate}" ]]; then
        echo "ERROR: enforce_asan_clean.sh not found" >&2
        exit 1
    fi
    "${_asan_gate}" "${BUILD_DIR}" || exit 1
    exit "${TB_EXIT}"

elif (( USE_COVERAGE )); then
    # Read the coverage tool written by CMake at configure time.
    # Defaults to "llvm" if the file is missing (backwards-compat).
    COVERAGE_TOOL="$(cat "${BUILD_DIR}/coverage_tool.txt" 2>/dev/null || echo "llvm")"

    echo ""
    echo ">> Running with ${COVERAGE_TOOL} coverage instrumentation: ${TB_BIN}"
    echo ""

    HTML_DIR="${BUILD_DIR}/coverage-report"
    SOURCES=(
        "${SCRIPT_DIR}/src/plic.cpp"
        "${SCRIPT_DIR}/test/plic_tb.cpp"
    )

    # ---- LLVM instrumented coverage (Clang / AppleClang) ------------------
    if [[ "${COVERAGE_TOOL}" == "llvm" ]]; then
        PROFRAW="${BUILD_DIR}/plic_tb.profraw"
        PROFDATA="${BUILD_DIR}/plic_tb.profdata"

        LLVM_PROFILE_FILE="${PROFRAW}" "${TB_BIN}"
        LLVM_PROFILE_FILE="${BUILD_DIR}/plic_wide_tb.profraw" "${WIDE_TB_BIN}"
        echo ""

        PROFDATA_CMD="$(_find_llvm_tool llvm-profdata)"
        COV_CMD="$(_find_llvm_tool llvm-cov)"
        if [[ -z "${PROFDATA_CMD}" || -z "${COV_CMD}" ]]; then
            echo "ERROR: llvm-profdata / llvm-cov not found." >&2
            echo "  macOS : installed with Xcode command-line tools" >&2
            echo "  Linux : sudo apt install llvm  (or llvm-18, etc.)" >&2
            exit 1
        fi

        echo ">> Merging profile data (${PROFDATA_CMD}) …"
        ${PROFDATA_CMD} merge -sparse "${PROFRAW}" -o "${PROFDATA}"

        echo ""
        echo "===== Line coverage summary ====="
        ${COV_CMD} report "${TB_BIN}" \
            -instr-profile="${PROFDATA}" \
            "${SOURCES[@]}"

        echo ""
        echo "===== Uncovered lines in plic.cpp ====="
        ${COV_CMD} show "${TB_BIN}" \
            -instr-profile="${PROFDATA}" \
            -sources "${SCRIPT_DIR}/src/plic.cpp" \
            -format=text \
            -show-line-counts-or-regions \
            | grep -E "^\s+[0-9]+\|[[:space:]]+0\|" \
            || echo "(none — full coverage)"

        ${COV_CMD} show "${TB_BIN}" \
            -instr-profile="${PROFDATA}" \
            "${SOURCES[@]}" \
            -format=html \
            -output-dir="${HTML_DIR}" \
            -show-line-counts-or-regions 2>/dev/null || true

    # ---- gcov coverage (GCC) -----------------------------------------------
    else
        "${TB_BIN}"
        "${WIDE_TB_BIN}"
        echo ""

        if command -v gcovr &>/dev/null; then
            echo "===== Line coverage summary (gcovr) ====="
            gcovr \
                --root "${SCRIPT_DIR}/src" \
                --object-directory "${BUILD_DIR}" \
                --filter "${SCRIPT_DIR}/src/" \
                --filter "${SCRIPT_DIR}/test/"

            echo ""
            echo "===== Uncovered lines in plic.cpp ====="
            gcovr \
                --root "${SCRIPT_DIR}/src" \
                --object-directory "${BUILD_DIR}" \
                --filter "${SCRIPT_DIR}/src/plic.cpp" \
                --txt \
                | grep -E "^\s+[0-9]+: +0:" \
                || echo "(none — full coverage)"

            mkdir -p "${HTML_DIR}"
            gcovr \
                --root "${SCRIPT_DIR}/src" \
                --object-directory "${BUILD_DIR}" \
                --filter "${SCRIPT_DIR}/src/" \
                --filter "${SCRIPT_DIR}/test/" \
                --html --html-details \
                -o "${HTML_DIR}/index.html" 2>/dev/null || true

        elif command -v lcov &>/dev/null && command -v genhtml &>/dev/null; then
            INFO="${BUILD_DIR}/coverage.info"
            lcov --capture \
                 --directory "${BUILD_DIR}" \
                 --output-file "${INFO}" \
                 --quiet
            # Strip system / third-party headers.
            lcov --remove "${INFO}" '/usr/*' "${BUILD_DIR}/*" \
                 --output-file "${INFO}" --quiet
            echo "===== Line coverage summary (lcov) ====="
            lcov --list "${INFO}"
            mkdir -p "${HTML_DIR}"
            genhtml "${INFO}" --output-directory "${HTML_DIR}" --quiet
        else
            echo "WARNING: neither gcovr nor lcov/genhtml found." >&2
            echo "  Install one for a coverage report:" >&2
            echo "    pip install gcovr   OR   sudo apt install lcov" >&2
        fi
    fi

    if [[ -f "${HTML_DIR}/index.html" ]]; then
        echo ""
        echo ">> HTML coverage report: ${HTML_DIR}/index.html"
    fi

    _gate=""
    for _cand in \
        "${SCRIPT_DIR}/../../scripts/enforce_line_coverage.sh" \
        "${SCRIPT_DIR}/../scripts/enforce_line_coverage.sh" \
        "${SCRIPT_DIR}/../smc/scripts/enforce_line_coverage.sh"
    do
        if [[ -f "${_cand}" ]]; then _gate="${_cand}"; break; fi
    done
    if [[ -z "${_gate}" ]]; then
        echo "ERROR: enforce_line_coverage.sh not found" >&2
        exit 1
    fi
    "${_gate}" "${SCRIPT_DIR}" "${BUILD_DIR}"

elif (( USE_CTEST )); then
    echo ">> Running via ctest"
    ctest --test-dir "${BUILD_DIR}" --output-on-failure
else
    echo ">> Running ${TB_BIN}"
    "${TB_BIN}"
    echo ">> Running ${WIDE_TB_BIN}"
    "${WIDE_TB_BIN}"
fi
