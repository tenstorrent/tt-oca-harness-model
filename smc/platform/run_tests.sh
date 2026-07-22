#!/usr/bin/env bash
# Build (if needed) and run the SMC platform integration test bench.
#
# Usage:
#   ./run_tests.sh                # incremental build + run  (Release)
#   ./run_tests.sh --clean        # wipe build/ first, then configure/build/run
#   ./run_tests.sh --ctest        # run via ctest instead of executing the binary
#   ./run_tests.sh --asan         # build with AddressSanitizer; run and report errors
#   ./run_tests.sh --coverage     # build with coverage; run and print line report
#
# The --asan and --coverage modes use isolated build directories
# (build_asan/ and build_cov/) so they never clobber a plain Release build.
#
# Cluster (firmware-on-CPU) support auto-enables when WHISPER_HOME + BOOST_DIR
# are set and exist; override with:
#   --with-cluster       force the CPU cluster on (errors if deps missing)
#   --without-cluster    force the CPU cluster off (cluster-free Release/ASan/coverage)
#
# Environment:
#   SYSTEMC_HOME      Path to an Accellera SystemC install.
#   CCI_HOME          Path to an Accellera SystemC CCI install.
#   WHISPER_HOME       Path to a built Whisper tree (enables the CPU cluster).
#   BOOST_DIR          Path to a Boost install (enables the CPU cluster).
#   BUILD_TYPE         CMake build type (default: Release; Debug for --coverage).
#   JOBS               Parallel build jobs (default: all available cores).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ -f "${SCRIPT_DIR}/deps.env" ]]; then
    # shellcheck disable=SC1091
    source "${SCRIPT_DIR}/deps.env"
fi

BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null \
             || sysctl -n hw.ncpu   2>/dev/null \
             || echo 4)}"
OS="$(uname -s)"

USE_CTEST=0
CLEAN=0
USE_ASAN=0
USE_COVERAGE=0
WITH_CLUSTER=""   # "" = auto (enable if Whisper+Boost present), 1 = force on, 0 = force off

for arg in "$@"; do
    case "$arg" in
        --clean)       CLEAN=1 ;;
        --ctest)       USE_CTEST=1 ;;
        --asan)        USE_ASAN=1 ;;
        --coverage)    USE_COVERAGE=1 ;;
        --with-cluster)    WITH_CLUSTER=1 ;;
        --without-cluster) WITH_CLUSTER=0 ;;
        -h|--help)
            sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'
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

# ---------------------------------------------------------------------------
# Locate SystemC (probe common prefixes if SYSTEMC_HOME is unset).
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
[[ -n "${SYSTEMC_HOME:-}" ]] && echo ">> Using SYSTEMC_HOME=${SYSTEMC_HOME}" \
    || echo ">> SYSTEMC_HOME not set; relying on CMake-installed SystemC::systemc"

# ---------------------------------------------------------------------------
# Locate CCI.
# ---------------------------------------------------------------------------
if [[ -z "${CCI_HOME:-}" ]]; then
    for _candidate in \
        /Users/pdroy/cci \
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
    if [[ "${OS}" == "Darwin" ]]; then
        export DYLD_LIBRARY_PATH="${CCI_HOME}/lib${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}"
    else
        export LD_LIBRARY_PATH="${CCI_HOME}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
    fi
else
    echo ">> CCI_HOME not set; CCI libs must be in the system library path"
fi

# Cluster enablement: auto-enable when Whisper + Boost are available (so the
# top-level orchestrator, which sets WHISPER_HOME/BOOST_DIR when the cpu_cluster
# target is ready, picks up the firmware-on-cluster bench automatically).
# --with-cluster forces it on; --without-cluster forces it off.
CLUSTER_FLAG=""
if [[ "${WITH_CLUSTER}" == "1" ]] \
   || { [[ -z "${WITH_CLUSTER}" ]] && [[ -n "${WHISPER_HOME:-}" && -n "${BOOST_DIR:-}" ]] \
        && [[ -d "${WHISPER_HOME}" && -d "${BOOST_DIR}" ]]; }; then
    echo ">> Cluster ENABLED (WHISPER_HOME=${WHISPER_HOME} BOOST_DIR=${BOOST_DIR})"
    CLUSTER_FLAG="-DSMC_PLATFORM_WITH_CLUSTER=ON -DWHISPER_HOME=${WHISPER_HOME} -DBOOST_DIR=${BOOST_DIR}"
else
    echo ">> Cluster DISABLED (set WHISPER_HOME + BOOST_DIR to enable, or pass --with-cluster)"
    CLUSTER_FLAG="-DSMC_PLATFORM_WITH_CLUSTER=OFF"
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
          ${CLUSTER_FLAG} \
          ${CCI_HOME:+-DCCI_HOME="${CCI_HOME}"}
fi

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
echo ">> Building with -j${JOBS}"
cmake --build "${BUILD_DIR}" -j "${JOBS}"

if [[ -f "${BUILD_DIR}/compile_commands.json" ]]; then
    ln -sf "$(basename "${BUILD_DIR}")/compile_commands.json" \
           "${SCRIPT_DIR}/compile_commands.json"
fi

# ---------------------------------------------------------------------------
# Locate test binary
# ---------------------------------------------------------------------------
TB_BIN="${BUILD_DIR}/test/platform_tb"
if [[ ! -x "${TB_BIN}" ]]; then
    echo "ERROR: test binary not found at ${TB_BIN}" >&2
    exit 1
fi

# Firmware-on-cluster test binary (only built when --with-cluster is enabled).
FW_TB_BIN="${BUILD_DIR}/test/platform_fw_tb"

# Helper: run a test binary, honoring ASan log redirection when USE_ASAN=1.
_run_tb() {
    local bin="$1"
    if (( USE_ASAN )); then
        local log="${BUILD_DIR}/asan.log.$(basename "${bin}")"
        local _opts
        if [[ "${OS}" == "Linux" ]]; then
            _opts="halt_on_error=0:detect_leaks=1:log_path=${log}"
        else
            _opts="halt_on_error=0:log_path=${log}"
        fi
        ASAN_OPTIONS="${_opts}" "${bin}"
        return $?
    fi
    "${bin}"
    return $?
}

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

# ---------------------------------------------------------------------------
# Run
# ---------------------------------------------------------------------------
if (( USE_ASAN )); then
    echo ""
    echo ">> Running with AddressSanitizer: ${TB_BIN}"
    [[ "${OS}" == "Linux" ]] && echo "   Linux: LeakSanitizer (detect_leaks=1) also active"
    echo ""

    ASAN_LOG="${BUILD_DIR}/asan.log"
    _run_tb "${TB_BIN}"; TB_EXIT=$?
    echo ""

    FW_EXIT=0
    if [[ -x "${FW_TB_BIN}" ]]; then
        echo ">> Running with AddressSanitizer: ${FW_TB_BIN}"
        echo ""
        _run_tb "${FW_TB_BIN}"; FW_EXIT=$?
        echo ""
    fi

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
    exit $(( TB_EXIT > FW_EXIT ? TB_EXIT : FW_EXIT ))

elif (( USE_COVERAGE )); then
    COVERAGE_TOOL="$(cat "${BUILD_DIR}/coverage_tool.txt" 2>/dev/null || echo "llvm")"

    echo ""
    echo ">> Running with ${COVERAGE_TOOL} coverage instrumentation: ${TB_BIN}"
    echo ""

    HTML_DIR="${BUILD_DIR}/coverage-report"
    SOURCES=(
        "${SCRIPT_DIR}/src/smc_platform.cpp"
        "${SCRIPT_DIR}/test/platform_tb.cpp"
        # Header-only helpers (touched files): their coverage is attributed
        # to the TUs that include them (smc_platform.cpp + the testbenches).
        "${SCRIPT_DIR}/include/addr_router.h"
        "${SCRIPT_DIR}/include/stub_target.h"
        "${SCRIPT_DIR}/include/multi_stub_target.h"
        "${SCRIPT_DIR}/include/width_adapter.h"
        "${SCRIPT_DIR}/include/interrupt_aggregator.h"
    )
    # When the cluster is enabled, the firmware bench covers the cluster-wired
    # branches of smc_platform.cpp (conditional bindings, presets, etc.).
    if [[ -x "${FW_TB_BIN}" ]]; then
        SOURCES+=("${SCRIPT_DIR}/test/platform_fw_tb.cpp")
    fi

    if [[ "${COVERAGE_TOOL}" == "llvm" ]]; then
        PROFRAW_T="${BUILD_DIR}/platform_tb.profraw"
        PROFDATA="${BUILD_DIR}/platform.profdata"

        LLVM_PROFILE_FILE="${PROFRAW_T}" "${TB_BIN}"
        echo ""

        PROFRAW_ARGS=("${PROFRAW_T}")
        if [[ -x "${FW_TB_BIN}" ]]; then
            PROFRAW_F="${BUILD_DIR}/platform_fw_tb.profraw"
            echo ">> Running with ${COVERAGE_TOOL} coverage instrumentation: ${FW_TB_BIN}"
            echo ""
            LLVM_PROFILE_FILE="${PROFRAW_F}" "${FW_TB_BIN}"
            echo ""
            PROFRAW_ARGS+=("${PROFRAW_F}")
        fi

        PROFDATA_CMD="$(_find_llvm_tool llvm-profdata)"
        COV_CMD="$(_find_llvm_tool llvm-cov)"
        if [[ -z "${PROFDATA_CMD}" || -z "${COV_CMD}" ]]; then
            echo "ERROR: llvm-profdata / llvm-cov not found." >&2
            exit 1
        fi

        echo ">> Merging profile data (${PROFDATA_CMD}) ..."
        ${PROFDATA_CMD} merge -sparse "${PROFRAW_ARGS[@]}" -o "${PROFDATA}"

        echo ""
        echo "===== Line coverage summary ====="
        ${COV_CMD} report "${TB_BIN}" \
            -instr-profile="${PROFDATA}" \
            "${SOURCES[@]}"

        echo ""
        echo "===== Uncovered lines in smc_platform.cpp ====="
        ${COV_CMD} show "${TB_BIN}" \
            -instr-profile="${PROFDATA}" \
            -sources "${SCRIPT_DIR}/src/smc_platform.cpp" \
            -format=text \
            -show-line-counts-or-regions \
            | grep -E "^[[:space:]]+[0-9]+\|[[:space:]]+0\|" \
            || echo "(none -- full coverage)"

        ${COV_CMD} show "${TB_BIN}" \
            -instr-profile="${PROFDATA}" \
            "${SOURCES[@]}" \
            -format=html \
            -output-dir="${HTML_DIR}" \
            -show-line-counts-or-regions 2>/dev/null || true
    else
        "${TB_BIN}"
        if [[ -x "${FW_TB_BIN}" ]]; then
            echo ""
            echo ">> Running ${FW_TB_BIN}"
            "${FW_TB_BIN}"
        fi
        echo ""
        if command -v gcovr &>/dev/null; then
            echo "===== Line coverage summary (gcovr) ====="
            gcovr \
                --root "${SCRIPT_DIR}/src" \
                --object-directory "${BUILD_DIR}" \
                --filter "${SCRIPT_DIR}/src/" \
                --filter "${SCRIPT_DIR}/test/"
            mkdir -p "${HTML_DIR}"
            gcovr \
                --root "${SCRIPT_DIR}/src" \
                --object-directory "${BUILD_DIR}" \
                --filter "${SCRIPT_DIR}/src/" \
                --filter "${SCRIPT_DIR}/test/" \
                --html --html-details \
                -o "${HTML_DIR}/index.html" 2>/dev/null || true
        else
            echo "WARNING: gcovr not found; install for a coverage report." >&2
        fi
    fi

    if [[ -f "${HTML_DIR}/index.html" ]]; then
        echo ""
        echo ">> HTML coverage report: ${HTML_DIR}/index.html"
    fi

elif (( USE_CTEST )); then
    echo ">> Running via ctest"
    ctest --test-dir "${BUILD_DIR}" --output-on-failure
else
    echo ">> Running ${TB_BIN}"
    "${TB_BIN}"
    RC=$?
    if [[ -x "${FW_TB_BIN}" ]]; then
        echo ""
        echo ">> Running ${FW_TB_BIN}"
        "${FW_TB_BIN}" || RC=$?
    fi
    exit "${RC}"
fi
