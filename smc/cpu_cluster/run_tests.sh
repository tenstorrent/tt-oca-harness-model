#!/usr/bin/env bash
# Build (if needed) and run the SMC CPU Cluster SystemC test suite.
#
# Usage:
#   ./run_tests.sh                # incremental build + run cluster_tb (Release)
#   ./run_tests.sh --clean        # wipe build/ first, then configure/build/run
#   ./run_tests.sh --ctest        # run via ctest instead of executing the binary
#   ./run_tests.sh --asan         # build with AddressSanitizer; run and report errors
#                                 #   Linux: also enables LeakSanitizer (detect_leaks=1)
#   ./run_tests.sh --coverage     # build with -DENABLE_COVERAGE=ON; run ctest;
#                                 #   then build the `coverage` target (gcovr HTML + summary)
#
# The --asan and --coverage modes use isolated build directories
# (build_asan/ and cov_build/) so they never clobber a plain Release build.
#
# Environment (or put exports in deps.env — see deps.env.example):
#   SYSTEMC_HOME  Path to a SystemC install (must contain include/systemc.h).
#                 If unset the script probes common Linux (/usr/local, /usr)
#                 and macOS (Homebrew) locations.  REQUIRED by CMakeLists.txt.
#   WHISPER_HOME  Path to a Whisper source tree (GNUmakefile).  If unset the
#                 script probes common locations; if build-<uname>/librvcore.a
#                 is missing it runs Whisper's make automatically.
#   WHISPER_SKIP_BUILD  Set to 1 to skip the automatic Whisper build step.
#   BOOST_DIR     Path to a Boost install containing include/boost/version.hpp.
#                 Used for cpu_cluster and passed as BOOST_ROOT when building
#                 Whisper.  REQUIRED by CMakeLists.txt.
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
            sed -n '2,27p' "$0" | sed 's/^# \{0,1\}//'
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
    CMAKE_EXTRA=(-DENABLE_ASAN=ON)
elif (( USE_COVERAGE )); then
    BUILD_DIR="${SCRIPT_DIR}/cov_build"
    BUILD_TYPE="Debug"
    CMAKE_EXTRA=(-DENABLE_COVERAGE=ON)
else
    BUILD_DIR="${SCRIPT_DIR}/build"
    CMAKE_EXTRA=()
fi

# ---------------------------------------------------------------------------
# Optional local overrides (not checked in).  Copy paths from a teammate or
# set exports in your shell profile.
# ---------------------------------------------------------------------------
if [[ -f "${SCRIPT_DIR}/deps.env" ]]; then
    # shellcheck disable=SC1091
    source "${SCRIPT_DIR}/deps.env"
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
        if [[ -f "${_candidate}/include/systemc.h" ]]; then
            export SYSTEMC_HOME="${_candidate}"
            break
        fi
    done
fi

if [[ -z "${SYSTEMC_HOME:-}" ]]; then
    echo "ERROR: SYSTEMC_HOME not set and SystemC headers not found." >&2
    echo "  Install Accellera SystemC (C++20) and set:" >&2
    echo "    export SYSTEMC_HOME=/path/to/systemc" >&2
    echo "  Or create ${SCRIPT_DIR}/deps.env with that export (see deps.env.example)." >&2
    exit 1
fi
echo ">> Using SYSTEMC_HOME=${SYSTEMC_HOME}"

# ---------------------------------------------------------------------------
# Locate Boost (must contain include/boost/version.hpp).  Whisper's GNUmakefile
# reads BOOST_ROOT at compile time, so resolve Boost before Whisper.
# ---------------------------------------------------------------------------
if [[ -z "${BOOST_DIR:-}" ]]; then
    for _candidate in \
        /opt/homebrew/opt/boost \
        /usr/local \
        /usr
    do
        if [[ -f "${_candidate}/include/boost/version.hpp" ]]; then
            export BOOST_DIR="${_candidate}"
            break
        fi
    done
fi

if [[ -z "${BOOST_DIR:-}" ]]; then
    echo "ERROR: BOOST_DIR not set and Boost headers not found." >&2
    echo "  Install Boost >= 1.74 and set:" >&2
    echo "    export BOOST_DIR=/path/to/boost" >&2
    echo "  Or create ${SCRIPT_DIR}/deps.env with that export (see deps.env.example)." >&2
    exit 1
fi
echo ">> Using BOOST_DIR=${BOOST_DIR}"

# ---------------------------------------------------------------------------
# Locate Whisper and build static archives when missing.
# Whisper's GNUmakefile names its build directory after `uname -s`, so we
# look for build-Linux/ on Linux and build-Darwin/ on macOS.  An explicit
# WHISPER_BUILD_DIR env var (or the same on the cmake line) overrides for
# unusual layouts.
# ---------------------------------------------------------------------------
: "${WHISPER_BUILD_DIR:=build-$(uname -s)}"
export WHISPER_BUILD_DIR

REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

_whisper_is_source_tree() {
    [[ -f "$1/GNUmakefile" ]]
}

_whisper_artifacts_ready() {
    local _root="$1"
    [[ -f "${_root}/${WHISPER_BUILD_DIR}/librvcore.a" ]] &&
    [[ -f "${_root}/virtual_memory/libvirtual_memory.a" ]] &&
    [[ -f "${_root}/pci/libpci.a" ]] &&
    [[ -f "${_root}/third_party/softfloat/build/RISCV-GCC/softfloat.a" ]]
}

if [[ -z "${WHISPER_HOME:-}" ]] || ! _whisper_is_source_tree "${WHISPER_HOME}"; then
    for _candidate in \
        "${WHISPER_HOME:-}" \
        /opt/whisper \
        /usr/local/whisper \
        "${REPO_ROOT}/../whisper/whisper" \
        "${REPO_ROOT}/../whisper" \
        "${HOME}/whisper/whisper" \
        "${HOME}/whisper" \
        "${HOME}/src/whisper/whisper" \
        "${HOME}/src/whisper"
    do
        [[ -n "${_candidate}" ]] || continue
        if _whisper_artifacts_ready "${_candidate}" || _whisper_is_source_tree "${_candidate}"; then
            export WHISPER_HOME="${_candidate}"
            break
        fi
    done
fi

if [[ -z "${WHISPER_HOME:-}" ]] || ! _whisper_is_source_tree "${WHISPER_HOME}"; then
    echo "ERROR: WHISPER_HOME not set and no Whisper source tree found." >&2
    echo "  Clone Whisper next to this repo or set WHISPER_HOME:" >&2
    echo "    git clone <whisper-repo-url> ${REPO_ROOT}/../whisper/whisper" >&2
    echo "    export WHISPER_HOME=/path/to/whisper   # directory with GNUmakefile" >&2
    echo "  Or add WHISPER_HOME=... to ${SCRIPT_DIR}/deps.env (see deps.env.example)." >&2
    exit 1
fi
echo ">> Using WHISPER_HOME=${WHISPER_HOME}"

if ! _whisper_artifacts_ready "${WHISPER_HOME}"; then
    if [[ "${WHISPER_SKIP_BUILD:-0}" == "1" ]]; then
        echo "ERROR: Whisper artifacts missing under ${WHISPER_HOME}/${WHISPER_BUILD_DIR}/" >&2
        echo "  (unset WHISPER_SKIP_BUILD or build Whisper manually)" >&2
        exit 1
    fi
    echo ">> Whisper not built yet; building in ${WHISPER_HOME}"
    echo "   (MEM_CALLBACKS=1, C++20, BOOST_ROOT=${BOOST_DIR})"
    (
        cd "${WHISPER_HOME}"
        export BOOST_ROOT="${BOOST_DIR}"
        make -j"${JOBS}" MEM_CALLBACKS=1 CXX_STD=c++20 \
            "${WHISPER_BUILD_DIR}/librvcore.a" \
            "${WHISPER_BUILD_DIR}/whisper"
    )
    if ! _whisper_artifacts_ready "${WHISPER_HOME}"; then
        echo "ERROR: Whisper build finished but required archives are still missing." >&2
        echo "  Expected under ${WHISPER_HOME}/${WHISPER_BUILD_DIR}/ and subdirs." >&2
        exit 1
    fi
    echo ">> Whisper build complete"
fi

# CCI (needed when SMC_BUILD_PLIC_INTEGRATION=ON and for cluster_tb PLIC phases).
if [[ -z "${CCI_HOME:-}" ]]; then
    for _candidate in \
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
        export DYLD_LIBRARY_PATH="${CCI_HOME}/lib64:${CCI_HOME}/lib${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}"
    else
        export LD_LIBRARY_PATH="${CCI_HOME}/lib64:${CCI_HOME}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
    fi
fi

# Make SystemC's shared lib visible to the loader at run time.
if [[ "${OS}" == "Darwin" ]]; then
    export DYLD_LIBRARY_PATH="${SYSTEMC_HOME}/lib64:${SYSTEMC_HOME}/lib${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}"
else
    export LD_LIBRARY_PATH="${SYSTEMC_HOME}/lib64:${SYSTEMC_HOME}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
fi

# Boost program_options may also be a shared lib.
if [[ -d "${BOOST_DIR}/lib" ]]; then
    if [[ "${OS}" == "Darwin" ]]; then
        export DYLD_LIBRARY_PATH="${BOOST_DIR}/lib${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}"
    else
        export LD_LIBRARY_PATH="${BOOST_DIR}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
    fi
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
          "${CMAKE_EXTRA[@]}" \
          ${CCI_HOME:+-DCCI_HOME="${CCI_HOME}"}
fi

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
echo ">> Building with -j${JOBS}"
cmake --build "${BUILD_DIR}" -j "${JOBS}"

# ---------------------------------------------------------------------------
# Locate test binary
# ---------------------------------------------------------------------------
TB_BIN="${BUILD_DIR}/test/cluster_tb"
if [[ ! -x "${TB_BIN}" ]]; then
    echo "ERROR: test binary not found at ${TB_BIN}" >&2
    exit 1
fi

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
        echo ""
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
    echo ""
    echo ">> Running with gcov coverage instrumentation: ${TB_BIN}"
    echo ""
    "${TB_BIN}"
    echo ""

    if ! command -v gcovr &>/dev/null; then
        echo "WARNING: gcovr not found in PATH; cannot render coverage report." >&2
        echo "  Install with: pip install gcovr" >&2
        exit 0
    fi

    echo ">> Building the 'coverage' target (gcovr)"
    cmake --build "${BUILD_DIR}" --target coverage

    HTML_REPORT="${BUILD_DIR}/coverage/index.html"
    if [[ -f "${HTML_REPORT}" ]]; then
        echo ""
        echo ">> HTML coverage report: ${HTML_REPORT}"
    fi

elif (( USE_CTEST )); then
    echo ">> Running via ctest"
    ctest --test-dir "${BUILD_DIR}" --output-on-failure
else
    echo ">> Running ${TB_BIN}"
    "${TB_BIN}"
fi
