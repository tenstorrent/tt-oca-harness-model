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
# TT / RHEL common install tree
for d in /tools_soc/opensrc/riscv-gnu-toolchain/*; do
    [ -d "${d}/bin" ] && TOOLCHAIN_SEARCH_DIRS+=("${d}/bin")
done
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

run_test() {
    local test_dir="${SCRIPT_DIR}/$1"
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

    log_info "============================================================"
    log_info "building test: $1"
    log_info "============================================================"
    (
        cd "${test_dir}"
        make clean || true
        make RISCV_PREFIX="${RISCV_PREFIX}" || {
            log_error "build failed for $1"
            return 1
        }
        log_info "running test: $1"
        make sim RISCV_PREFIX="${RISCV_PREFIX}" VP="${vp}"
    )
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

Options:
  --build-vp      (re)configure and build sep-vp before running tests
  --rebuild-vp    alias for --build-vp
  --interactive   -i  show a numbered menu and run the chosen test
  --list          list available tests and exit
  --help          show this help and exit

Environment overrides:
  RISCV_PREFIX, RISCV_TOOLCHAIN_PATH, SYSTEMC_HOME, CCI_HOME,
  WHISPER_HOME, BOOST_DIR, VP, VP_BUILD_DIR

Examples:
  ${0##*/}                     # run all tests
  ${0##*/} sep-gpio-test       # run a single test by name
  ${0##*/} -i                  # choose a test interactively
  ${0##*/} --build-vp          # build sep-vp first, then run all tests
EOF
}

BUILD_VP=0
INTERACTIVE=0
TESTS=()

while [ $# -gt 0 ]; do
    case "$1" in
        --build-vp|--rebuild-vp)
            BUILD_VP=1
            ;;
        --interactive|-i)
            INTERACTIVE=1
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
FAILED=0
for t in "${TESTS[@]}"; do
    if ! run_test "${t}"; then
        log_error "test failed: ${t}"
        FAILED=1
    fi
done

if [ ${FAILED} -ne 0 ]; then
    log_error "one or more tests failed"
    exit 1
fi

log_info "all tests passed"
exit 0
