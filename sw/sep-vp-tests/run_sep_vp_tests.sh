#!/usr/bin/env bash
# =============================================================================
# sw/sep-vp-tests/run_sep_vp_tests.sh
#
# Host-agnostic runner for the bare-metal RV32 SEP VP firmware tests.
# Detects the RISC-V toolchain, the sep-vp executable, and the SystemC/CCI/Boost
# install locations on macOS / RHEL / Ubuntu, then builds and runs tests.
#
# Usage:
#   cd sw/sep-vp-tests
#   ./run_sep_vp_tests.sh              # build + run all tests
#   ./run_sep_vp_tests.sh sep-gpio-test # build + run a single test
#   ./run_sep_vp_tests.sh --build-vp   # (re)build sep-vp first, then run all
#   ./run_sep_vp_tests.sh --help
#
# Override auto-detection with environment variables:
#   RISCV_PREFIX        - e.g. riscv64-elf- (defaults to a detected prefix)
#   RISCV_TOOLCHAIN_PATH - base directory containing bin/<prefix>gcc
#   SYSTEMC_HOME        - SystemC 3.0.2 install prefix (C++20 build)
#   CCI_HOME            - CCI 1.0 install prefix
#   WHISPER_HOME        - Whisper build directory
#   BOOST_DIR           - Boost root (macOS: /opt/homebrew/opt/boost)
#   VP                  - explicit sep-vp executable path
#   VP_BUILD_DIR        - build directory used for sep-vp (default: vp/build_sep)
#   SIM_TIMEOUT         - hard cap in seconds for one test (default 300)
#   SIM_IDLE_TIMEOUT    - stop a test after this many seconds without new output
#                         (default 30)
#   TEST_PASS_REGEX     - extended regex marking a passing run (case-insensitive)
#   TEST_FAIL_REGEX     - extended regex marking a failing run (case-insensitive)
#   LOG_DIR             - where per-test logs are written (default ./logs)
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# -----------------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------------
log_info()  { echo "[run_sep_vp_tests] $*"; }
log_warn()  { echo "[run_sep_vp_tests] WARN: $*" >&2; }
log_error() { echo "[run_sep_vp_tests] ERROR: $*" >&2; }

# Detect host OS family.
detect_os() {
    case "$(uname -s)" in
        Darwin*)  echo "macos" ;;
        Linux*)
            if [ -f /etc/redhat-release ] || [ -f /etc/os-release ] && grep -qiE 'rhel|red hat|centos|rocky|almalinux' /etc/os-release; then
                echo "rhel"
            elif [ -f /etc/os-release ] && grep -qiE 'ubuntu|debian' /etc/os-release; then
                echo "ubuntu"
            else
                echo "linux"
            fi
            ;;
        *)        echo "unknown" ;;
    esac
}

OS="$(detect_os)"
log_info "detected host: ${OS}"

# -----------------------------------------------------------------------------
# RISC-V toolchain detection
# -----------------------------------------------------------------------------
PREFIX_CANDIDATES=(
    "riscv64-unknown-elf-"
    "riscv64-elf-"
    "riscv-none-elf-"
    "riscv64-linux-gnu-"
    "riscv-none-embed-"
)

# Append any user-supplied prefix first if it looks complete.
if [ -n "${RISCV_PREFIX:-}" ]; then
    PREFIX_CANDIDATES=("${RISCV_PREFIX}" "${PREFIX_CANDIDATES[@]}")
fi

# Extra directories to search beyond PATH.
TOOLCHAIN_SEARCH_DIRS=()
if [ -n "${RISCV_TOOLCHAIN_PATH:-}" ]; then
    TOOLCHAIN_SEARCH_DIRS+=("${RISCV_TOOLCHAIN_PATH}/bin")
fi
# macOS (Homebrew) generic prefix
TOOLCHAIN_SEARCH_DIRS+=(
    "/opt/homebrew/bin"
    "/usr/local/bin"
)
# Site install trees: export RISCV_TOOLCHAIN_PATH (or put the toolchain on PATH).
TOOLCHAIN_SEARCH_DIRS+=(
    "/opt/riscv/bin"
    "/usr/local/riscv/bin"
    "/usr/bin"
)

detect_toolchain() {
    local prefix
    for prefix in "${PREFIX_CANDIDATES[@]}"; do
        # First check PATH.
        if command -v "${prefix}gcc" >/dev/null 2>&1; then
            echo "${prefix}"
            return 0
        fi
        # Then known install directories.
        for dir in "${TOOLCHAIN_SEARCH_DIRS[@]}"; do
            if [ -x "${dir}/${prefix}gcc" ]; then
                echo "${prefix}"
                return 0
            fi
        done
    done
    return 1
}

RISCV_PREFIX="$(detect_toolchain)"
if [ -z "${RISCV_PREFIX}" ]; then
    log_error "could not find a RISC-V toolchain in PATH or common install dirs."
    log_error "Candidates: ${PREFIX_CANDIDATES[*]}"
    log_error "Install one of them, or set RISCV_PREFIX / RISCV_TOOLCHAIN_PATH."
    log_error "  macOS:   brew install riscv64-elf-gcc"
    log_error "  Ubuntu:  sudo apt install gcc-riscv64-unknown-elf"
    log_error "  RHEL/TT: module load riscv-gnu-toolchain/2025.01.20-rhel-8.10"
    exit 1
fi
log_info "RISC-V toolchain prefix: ${RISCV_PREFIX}"

# -----------------------------------------------------------------------------
# sep-vp binary detection / build
# -----------------------------------------------------------------------------
VP_BUILD_DIR="${VP_BUILD_DIR:-${REPO_ROOT}/vp/build_sep}"

find_vp_binary() {
    local candidates=()
    [ -n "${VP:-}" ] && candidates+=("${VP}")
    candidates+=(
        "${REPO_ROOT}/vp/build/bin/sep-vp"
        "${REPO_ROOT}/vp/build_sep/bin/sep-vp"
        "${VP_BUILD_DIR}/bin/sep-vp"
    )
    for c in "${candidates[@]}"; do
        if [ -x "${c}" ]; then
            echo "${c}"
            return 0
        fi
    done
    return 1
}

# Detect / sanity-check SystemC, CCI, Boost, and Whisper.
ensure_vp_build_env() {
    if [ -z "${SYSTEMC_HOME:-}" ]; then
        # Common local install paths used in this repo
        for d in \
            "${HOME}/local/systemc-3.0.2-cxx20" \
            "${HOME}/local/systemc-3.0.2" \
            /usr/local/systemc-3.0.2 \
            /opt/systemc-3.0.2
        do
            if [ -e "${d}/include/systemc" ] || [ -f "${d}/lib/libsystemc.dylib" ] || [ -f "${d}/lib/libsystemc.so" ] || [ -f "${d}/lib-linux64/libsystemc.so" ]; then
                SYSTEMC_HOME="${d}"
                break
            fi
        done
    fi
    if [ -z "${SYSTEMC_HOME:-}" ]; then
        log_error "SYSTEMC_HOME is not set and a C++20 SystemC 3.0.2 install could not be found."
        log_error "Set SYSTEMC_HOME to the install prefix (e.g. /Users/\$USER/local/systemc-3.0.2-cxx20)."
        exit 1
    fi
    log_info "SYSTEMC_HOME: ${SYSTEMC_HOME}"

    if [ -z "${CCI_HOME:-}" ]; then
        for d in \
            "${HOME}/local/cci-cxx20" \
            "${HOME}/local/cci-1.0.1" \
            /usr/local/cci-1.0.1 \
            /usr/local/cci
        do
            if [ -e "${d}/include/cci_configuration" ] || [ -f "${d}/lib/libcci.so" ] || [ -f "${d}/lib/libcci.dylib" ]; then
                CCI_HOME="${d}"
                break
            fi
        done
    fi
    if [ -z "${CCI_HOME:-}" ]; then
        log_error "CCI_HOME is not set and a CCI 1.0 install could not be found."
        log_error "Set CCI_HOME to the install prefix (e.g. /Users/\$USER/local/cci-cxx20)."
        exit 1
    fi
    log_info "CCI_HOME: ${CCI_HOME}"

    if [ -z "${WHISPER_HOME:-}" ]; then
        for d in \
            "${REPO_ROOT}/../whisper" \
            "${HOME}/tt_whisper/whisper" \
            "${HOME}/whisper" \
            /opt/whisper
        do
            if [ -d "${d}/build" ] || [ -d "${d}/build-Darwin" ] || [ -f "${d}/CMakeLists.txt" ]; then
                WHISPER_HOME="${d}"
                break
            fi
        done
    fi
    if [ -z "${WHISPER_HOME:-}" ]; then
        log_error "WHISPER_HOME is not set and the Whisper ISS tree could not be found."
        log_error "Set WHISPER_HOME to the Whisper repo root (e.g. /Users/\$USER/tt_whisper/whisper)."
        exit 1
    fi
    log_info "WHISPER_HOME: ${WHISPER_HOME}"

    if [ -z "${BOOST_DIR:-}" ]; then
        case "${OS}" in
            macos)
                if [ -d /opt/homebrew/opt/boost ]; then
                    BOOST_DIR=/opt/homebrew/opt/boost
                fi
                ;;
            ubuntu|rhel|linux)
                for d in /usr /usr/local /opt/boost; do
                    if [ -f "${d}/lib/libboost_iostreams.so" ] || [ -f "${d}/lib/x86_64-linux-gnu/libboost_iostreams.so" ]; then
                        BOOST_DIR="${d}"
                        break
                    fi
                done
                ;;
        esac
    fi
    if [ -z "${BOOST_DIR:-}" ]; then
        log_warn "BOOST_DIR not auto-detected; cmake will try to find Boost."
    else
        log_info "BOOST_DIR: ${BOOST_DIR}"
    fi
}

build_vp() {
    ensure_vp_build_env
    log_info "configuring sep-vp in ${VP_BUILD_DIR} ..."

    # Export the discovered paths so FindSystemC / FindCCI / smc_cpu_cluster can
    # read them as environment variables, matching the configure_vp.sh convention.
    export SYSTEMC_HOME CCI_HOME WHISPER_HOME
    export CMAKE_BUILD_TYPE=Release
    export CMAKE_CXX_STANDARD=20
    [ -n "${BOOST_DIR:-}" ] && export BOOST_DIR="${BOOST_DIR}"

    local cmake_args=(
        -S "${REPO_ROOT}/vp"
        -B "${VP_BUILD_DIR}"
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_CXX_STANDARD=20
    )
    [ -n "${BOOST_DIR:-}" ] && cmake_args+=(-DBOOST_DIR="${BOOST_DIR}")

    cmake "${cmake_args[@]}"

    log_info "building sep-vp ..."
    cmake --build "${VP_BUILD_DIR}" --target sep-vp -j
}

get_vp() {
    local vp
    if ! vp="$(find_vp_binary)"; then
        log_warn "sep-vp binary not found; building it now."
        build_vp
        vp="$(find_vp_binary)" || {
            log_error "sep-vp still not found after build; check ${VP_BUILD_DIR}/bin/"
            exit 1
        }
    fi
    echo "${vp}"
}

# -----------------------------------------------------------------------------
# Test discovery / execution
# -----------------------------------------------------------------------------
list_tests() {
    local tests=()
    for d in "${SCRIPT_DIR}"/*/; do
        [ -d "${d}" ] && [ -f "${d}/Makefile" ] && tests+=("$(basename "${d}")")
    done
    if [ ${#tests[@]} -eq 0 ]; then
        log_error "no test directories found in ${SCRIPT_DIR}"
        exit 1
    fi
    printf '%s\n' "${tests[@]}"
}

# The VP never exits on its own: the firmware finishes, but sc_start() keeps the
# SystemC kernel running, so `make sim` would block forever and the loop below
# would stop at the first test.  Each test therefore runs under a watchdog that
# watches the log, stops the VP as soon as the firmware prints its pass/fail
# banner, and gives up on a test that goes quiet or overruns the hard cap.
PASS_REGEX="${TEST_PASS_REGEX:-all[[:space:]]+([a-z]+[[:space:]]+)?(tests|checks)[[:space:]]+passed|test[[:space:]]+passed}"
FAIL_REGEX="${TEST_FAIL_REGEX:-some[[:space:]]+([a-z]+[[:space:]]+)?(tests|checks)[[:space:]]+failed|test[[:space:]]+failed}"
SIM_TIMEOUT="${SIM_TIMEOUT:-300}"
SIM_IDLE_TIMEOUT="${SIM_IDLE_TIMEOUT:-30}"
LOG_DIR="${LOG_DIR:-${SCRIPT_DIR}/logs}"

# `make sim` sits between us and the VP, so signalling make alone would leave
# sep-vp running.  List the whole tree, parents first.
collect_tree() {
    local pid="$1" child
    echo "${pid}"
    for child in $(pgrep -P "${pid}" 2>/dev/null || true); do
        collect_tree "${child}"
    done
}

# Take the tree down from the top: make prints "*** [sim] Terminated" if it is
# still alive when the VP dies, so kill it before its child.
stop_sim() {
    local pid="$1" p
    for p in $(collect_tree "${pid}"); do
        kill -KILL "${p}" 2>/dev/null || true
    done
    # Mute the shell's own "Terminated" report for the reaped job.
    exec 4>&2 2>/dev/null
    wait "${pid}" 2>/dev/null || true
    exec 2>&4 4>&-
}

# The VP writes its own timestamped log lines into the same stream as the
# firmware's printf, so a banner can come out split ("All tests PASS" ... "ED!").
# Drop the VP lines and join what is left before matching.  Only the tail is
# scanned: banners are printed at the end and these logs reach megabytes.
log_has() {
    local text
    text="$(tail -c 262144 "$2" 2>/dev/null \
            | sed -E 's/\[[0-9]+ +(ps|ns|us|ms|s)\] \[[A-Z]+ [0-9]+\].*$//' \
            | tr -d '\n\r')" || return 1
    grep -Eiq -- "$1" <<<"${text}"
}

# Sets RESULT to PASS / FAIL / TIMEOUT / NO-BANNER.
run_sim() {
    local name="$1" test_dir="$2" vp="$3" log="$4"
    local started="${SECONDS}" last_size=0 idle=0 reason="" size pid

    : > "${log}"
    (
        cd "${test_dir}"
        make sim RISCV_PREFIX="${RISCV_PREFIX}" VP="${vp}" 2>&1 | tee -a "${log}"
    ) &
    pid=$!

    while true; do
        if log_has "${FAIL_REGEX}" "${log}"; then reason="banner"; break; fi
        if log_has "${PASS_REGEX}" "${log}"; then reason="banner"; break; fi
        if ! kill -0 "${pid}" 2>/dev/null; then reason="exited"; break; fi
        if [ $((SECONDS - started)) -ge "${SIM_TIMEOUT}" ]; then reason="timeout"; break; fi
        size="$(wc -c < "${log}" 2>/dev/null || echo 0)"
        if [ "${size}" -eq "${last_size}" ]; then
            idle=$((idle + 1))
            if [ "${idle}" -ge "${SIM_IDLE_TIMEOUT}" ]; then reason="idle"; break; fi
        else
            idle=0
            last_size="${size}"
        fi
        sleep 1
    done

    if kill -0 "${pid}" 2>/dev/null; then
        sleep 1   # let the banner and any trailing lines flush into the log
        stop_sim "${pid}"
    else
        wait "${pid}" 2>/dev/null || true
    fi

    if log_has "${FAIL_REGEX}" "${log}"; then
        RESULT="FAIL"
    elif log_has "${PASS_REGEX}" "${log}"; then
        RESULT="PASS"
    elif [ "${reason}" = "timeout" ]; then
        RESULT="TIMEOUT"
    else
        RESULT="NO-BANNER"
    fi

    case "${RESULT}" in
        PASS)      log_info "${name}: PASS" ;;
        FAIL)      log_error "${name}: FAIL (firmware reported failures)" ;;
        TIMEOUT)   log_error "${name}: TIMEOUT after ${SIM_TIMEOUT}s (no pass/fail banner)" ;;
        NO-BANNER) log_error "${name}: no pass/fail banner (vp ${reason}); see ${log}" ;;
    esac
}

# Sets RESULT for the caller's summary.
run_test() {
    local name="$1" test_dir="${SCRIPT_DIR}/$1"
    RESULT="ERROR"

    if [ ! -d "${test_dir}" ]; then
        log_error "test directory not found: ${test_dir}"
        return 1
    fi
    if [ ! -f "${test_dir}/Makefile" ]; then
        log_error "no Makefile in ${test_dir}"
        return 1
    fi

    local vp
    vp="$(get_vp)"

    mkdir -p "${LOG_DIR}"
    local build_log="${LOG_DIR}/${name}.build.log"
    local sim_log="${LOG_DIR}/${name}.log"

    log_info "============================================================"
    log_info "building test: ${name}"
    log_info "============================================================"
    if ! (
        cd "${test_dir}"
        set -o pipefail
        make clean >/dev/null 2>&1 || true
        make RISCV_PREFIX="${RISCV_PREFIX}" 2>&1 | tee "${build_log}"
    ); then
        RESULT="BUILD-FAIL"
        log_error "build failed for ${name}; see ${build_log}"
        return 1
    fi

    log_info "running test: ${name} (stop on banner, ${SIM_IDLE_TIMEOUT}s idle / ${SIM_TIMEOUT}s cap)"
    run_sim "${name}" "${test_dir}" "${vp}" "${sim_log}"
    [ "${RESULT}" = "PASS" ]
}

# -----------------------------------------------------------------------------
# CLI
# -----------------------------------------------------------------------------
choose_test_interactive() {
    SELECTED_TEST=""
    local tests=()
    while IFS= read -r t; do
        [ -n "${t}" ] && tests+=("${t}")
    done < <(list_tests)

    if [ ${#tests[@]} -eq 0 ]; then
        log_error "no tests available to choose from"
        return 1
    fi

    echo ""
    echo "Available SEP VP tests:"
    local i
    for i in "${!tests[@]}"; do
        printf '  %2d) %s\n' "$((i + 1))" "${tests[$i]}"
    done
    echo ""
    printf 'Enter number (1-%d) or q to quit: ' "${#tests[@]}"
    local choice=""
    # In a real terminal stdin is the TTY; in a pipe, try /dev/tty on a spare
    # fd and fall back to stdin so the menu can also be fed non-interactively.
    if { exec 3</dev/tty; } 2>/dev/null; then
        read -r choice <&3
        exec 3<&-
    else
        read -r choice
    fi
    if [ -z "${choice}" ]; then
        log_error "no selection provided"
        return 1
    fi
    if [ "${choice}" = "q" ] || [ "${choice}" = "Q" ]; then
        echo "cancelled"
        exit 0
    fi
    if ! [[ "${choice}" =~ ^[0-9]+$ ]] || [ "${choice}" -lt 1 ] || [ "${choice}" -gt "${#tests[@]}" ]; then
        log_error "invalid selection: ${choice}"
        return 1
    fi
    SELECTED_TEST="${tests[$((choice - 1))]}"
    return 0
}

usage() {
    cat <<EOF
Usage: ${0##*/} [OPTIONS] [TEST_DIR|TEST_DIR ...]

Host-agnostic runner for the sep-vp bare-metal firmware tests.

Tests run one after another. The VP does not exit by itself, so each run is
stopped as soon as the firmware prints its pass/fail banner, after
SIM_IDLE_TIMEOUT seconds without output, or at the SIM_TIMEOUT hard cap.
Per-test logs are written to ${LOG_DIR}.

Options:
  --build-vp      (re)configure and build sep-vp before running tests
  --rebuild-vp    alias for --build-vp
  --interactive   -i  show a numbered menu and run the chosen test
  --timeout SECS  hard cap per test (default ${SIM_TIMEOUT})
  --idle SECS     stop a test after this long with no new output (default ${SIM_IDLE_TIMEOUT})
  --stop-on-fail  stop at the first test that does not pass
  --list          list available tests and exit
  --help          show this help and exit

Environment overrides:
  RISCV_PREFIX, RISCV_TOOLCHAIN_PATH, SYSTEMC_HOME, CCI_HOME,
  WHISPER_HOME, BOOST_DIR, VP, VP_BUILD_DIR,
  SIM_TIMEOUT, SIM_IDLE_TIMEOUT, TEST_PASS_REGEX, TEST_FAIL_REGEX, LOG_DIR

Examples:
  ${0##*/}                     # run all tests
  ${0##*/} sep-gpio-test       # run a single test by name
  ${0##*/} -i                  # choose a test interactively
  ${0##*/} --build-vp          # build sep-vp first, then run all tests
  ${0##*/} --timeout 60        # give each test at most a minute
EOF
}

BUILD_VP=0
INTERACTIVE=0
STOP_ON_FAIL=0
TESTS=()

while [ $# -gt 0 ]; do
    case "$1" in
        --build-vp|--rebuild-vp)
            BUILD_VP=1
            ;;
        --interactive|-i)
            INTERACTIVE=1
            ;;
        --timeout)
            SIM_TIMEOUT="${2:?--timeout needs a value in seconds}"
            shift
            ;;
        --idle|--idle-timeout)
            SIM_IDLE_TIMEOUT="${2:?--idle needs a value in seconds}"
            shift
            ;;
        --stop-on-fail)
            STOP_ON_FAIL=1
            ;;
        --list)
            list_tests
            exit 0
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        --*)
            log_error "unknown option: $1"
            usage
            exit 1
            ;;
        *)
            TESTS+=("$1")
            ;;
    esac
    shift
done

if [ ${INTERACTIVE} -eq 1 ]; then
    if [ ${#TESTS[@]} -gt 0 ]; then
        log_error "cannot combine --interactive with explicit test names"
        exit 1
    fi
    if ! choose_test_interactive; then
        exit 1
    fi
    TESTS=("${SELECTED_TEST}")
elif [ ${#TESTS[@]} -eq 0 ]; then
    TESTS=()
    while IFS= read -r t; do
        [ -n "${t}" ] && TESTS+=("${t}")
    done < <(list_tests)
fi

if [ ${BUILD_VP} -eq 1 ]; then
    build_vp
fi

log_info "tests to run: ${TESTS[*]}"
mkdir -p "${LOG_DIR}"

FAILED=0
NAMES=()
STATUSES=()
DURATIONS=()

for t in "${TESTS[@]}"; do
    start="${SECONDS}"
    RESULT="ERROR"
    run_test "${t}" || true
    NAMES+=("${t}")
    STATUSES+=("${RESULT}")
    DURATIONS+=("$((SECONDS - start))")
    if [ "${RESULT}" != "PASS" ]; then
        FAILED=$((FAILED + 1))
        if [ "${STOP_ON_FAIL}" -eq 1 ]; then
            log_error "stopping after first failure (--stop-on-fail)"
            break
        fi
    fi
done

echo ""
log_info "============================================================"
log_info "summary  (logs in ${LOG_DIR})"
log_info "============================================================"
for i in "${!NAMES[@]}"; do
    printf '  %-28s %-11s %4ss\n' "${NAMES[$i]}" "${STATUSES[$i]}" "${DURATIONS[$i]}"
done
echo ""

if [ "${FAILED}" -ne 0 ]; then
    log_error "${FAILED} of ${#NAMES[@]} test(s) did not pass"
    exit 1
fi

log_info "all ${#NAMES[@]} tests passed"
exit 0
