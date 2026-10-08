#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# =============================================================================
# sw/smc-vp-tests/run_smc_vp_tests.sh
#
# Host-agnostic runner for the bare-metal RV64 SMC VP firmware tests.
# Detects the RISC-V toolchain, the smc-vp executable, and the SystemC/CCI/Boost
# install locations on macOS / RHEL / Ubuntu, then builds and runs tests.
#
# Usage:
#   cd sw/smc-vp-tests
#   ./run_smc_vp_tests.sh              # build + run all smc-* tests
#   ./run_smc_vp_tests.sh smc-dma-test  # build + run a single test
#   ./run_smc_vp_tests.sh --build-vp   # (re)build smc-vp first, then run all
#   ./run_smc_vp_tests.sh --help
#
# Override auto-detection with environment variables:
#   RISCV_PREFIX        - e.g. riscv64-elf- (defaults to a detected prefix)
#   RISCV_TOOLCHAIN_PATH - base directory containing bin/<prefix>gcc
#   SYSTEMC_HOME        - SystemC 3.0.2 install prefix (C++20 build)
#   CCI_HOME            - CCI 1.0 install prefix
#   WHISPER_HOME        - Whisper build directory
#   BOOST_DIR           - Boost root (macOS: /opt/homebrew/opt/boost)
#   VP                  - explicit smc-vp executable path
#   VP_BUILD_DIR        - build directory used for smc-vp (default: vp/build_smc)
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# -----------------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------------
# All three log to stderr on purpose: several helpers below "return" a value by
# echoing it to stdout and are read back with `$(...)`, so anything progress-
# related must stay off stdout or it ends up inside the captured value.
log_info()  { echo "[run_smc_vp_tests] $*" >&2; }
log_warn()  { echo "[run_smc_vp_tests] WARN: $*" >&2; }
log_error() { echo "[run_smc_vp_tests] ERROR: $*" >&2; }

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
# Site install trees: export RISCV_TOOLCHAIN_PATH (or put the toolchain on
# PATH). Prefer a recent toolchain — older ones may be missing ISA-string
# support this repo needs (e.g. an assembler that rejects the explicit
# `_zicsr_zifencei` suffix `Makefile.common` passes via -march).
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
    log_error "could not find a RISC-V RV64 toolchain in PATH or common install dirs."
    log_error "Candidates: ${PREFIX_CANDIDATES[*]}"
    log_error "Install one of them, or set RISCV_PREFIX / RISCV_TOOLCHAIN_PATH."
    log_error "  macOS:   brew install riscv64-elf-gcc"
    log_error "  Ubuntu:  sudo apt install gcc-riscv64-unknown-elf"
    log_error "  RHEL/TT: module load riscv-gnu-toolchain/2025.01.20-rhel-8.10"
    exit 1
fi
log_info "RISC-V toolchain prefix: ${RISCV_PREFIX}"

# detect_toolchain() only reports WHICH prefix has a working compiler; if it
# found that compiler through TOOLCHAIN_SEARCH_DIRS rather than the existing
# PATH, that directory must be exported here -- otherwise `make` (a child
# process spawned later by run_test()) still can't find "${RISCV_PREFIX}gcc",
# even though this script's own `command -v` check "found" it in this shell.
if ! command -v "${RISCV_PREFIX}gcc" >/dev/null 2>&1; then
    for dir in "${TOOLCHAIN_SEARCH_DIRS[@]}"; do
        if [ -x "${dir}/${RISCV_PREFIX}gcc" ]; then
            log_info "adding toolchain directory to PATH: ${dir}"
            export PATH="${dir}:${PATH}"
            break
        fi
    done
fi

# -----------------------------------------------------------------------------
# smc-vp binary detection / build
# -----------------------------------------------------------------------------
VP_BUILD_DIR="${VP_BUILD_DIR:-${REPO_ROOT}/vp/build_smc}"

find_vp_binary() {
    local candidates=()
    [ -n "${VP:-}" ] && candidates+=("${VP}")
    # VP_BUILD_DIR first: it is where build_vp() writes, so a stale binary in
    # another build tree must not shadow the one --build-vp just produced.
    candidates+=(
        "${VP_BUILD_DIR}/bin/smc-vp"
        "${REPO_ROOT}/vp/build_smc/bin/smc-vp"
        "${REPO_ROOT}/vp/build/bin/smc-vp"
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
    log_info "configuring smc-vp in ${VP_BUILD_DIR} ..."

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
        -DSMC_CXX_STANDARD=20
    )
    [ -n "${BOOST_DIR:-}" ] && cmake_args+=(-DBOOST_DIR="${BOOST_DIR}")

    cmake "${cmake_args[@]}"

    log_info "building smc-vp ..."
    cmake --build "${VP_BUILD_DIR}" --target smc-vp -j
}

# Resolve the smc-vp executable into the global VP_BIN.
#
# This deliberately does NOT return the path on stdout.  build_vp() streams
# cmake's configure/build output to stdout, so capturing it (`vp="$(get_vp)"`)
# folded that output into the path and handed `make sim` a VP= value such as
#   VP="[run_smc_vp_tests] SYSTEMC_HOME: /... <newline> /path/to/smc-vp"
# which sh then tried to execute ("/bin/sh: 1: [run_smc_vp_tests]: not found").
# find_vp_binary() is the only stdout-returning helper here, and it echoes
# nothing but the path.
#
# Resolving once (from the main flow, not per test) also means a broken VP
# build reports one clear error instead of re-running the whole cmake configure
# for every test in the list.
resolve_vp() {
    if VP_BIN="$(find_vp_binary)"; then
        return 0
    fi
    log_warn "smc-vp binary not found; building it now."
    build_vp
    VP_BIN="$(find_vp_binary)" || {
        log_error "smc-vp still not found after build; check ${VP_BUILD_DIR}/bin/"
        return 1
    }
}

# -----------------------------------------------------------------------------
# Test discovery / execution
# -----------------------------------------------------------------------------
# A passing firmware test must print one of these.  Two conventions are in use:
# the smc-* tests print "PASS: <what worked>", while smc-aou-test prints the
# repo-wide unit-test string mandated by aou/doc/03_AOU_Test_Plan.md.
FW_PASS_RE='(^|[[:space:]])(PASS:|ALL TESTS PASSED)'

list_tests() {
    local tests=()
    for d in "${SCRIPT_DIR}"/smc-*/; do
        [ -d "${d}" ] || continue
        if [ ! -f "${d}/Makefile" ]; then
            # Warn rather than skip in silence: smc-dma-test sat here with a
            # main.c and no Makefile while both CI workflows' comments claimed
            # the suite covered it.
            if compgen -G "${d}*.c" >/dev/null || compgen -G "${d}*.S" >/dev/null; then
                log_warn "skipping $(basename "${d}"): has sources but no Makefile"
            fi
            continue
        fi
        tests+=("$(basename "${d}")")
    done
    if [ ${#tests[@]} -eq 0 ]; then
        log_error "no smc-* test directories found in ${SCRIPT_DIR}"
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

    log_info "============================================================"
    log_info "building test: $1"
    log_info "============================================================"
    (
        cd "${test_dir}"
        make clean RISCV_PREFIX="${RISCV_PREFIX}" || true
        make RISCV_PREFIX="${RISCV_PREFIX}" || {
            log_error "build failed for $1"
            return 1
        }
        log_info "running test: $1"
        local sim_log="run_smc_vp_tests_sim.log"
        if ! make sim RISCV_PREFIX="${RISCV_PREFIX}" VP="${VP_BIN}" >"${sim_log}" 2>&1; then
            cat "${sim_log}"
            log_error "simulation exited non-zero for $1"
            rm -f "${sim_log}"
            return 1
        fi
        cat "${sim_log}"
        if grep -q "FAIL:" "${sim_log}"; then
            rm -f "${sim_log}"
            log_error "firmware reported failure for $1"
            return 1
        fi
        # Require an explicit pass line.  Treating "no FAIL:" as success reports
        # a hang as a pass: the trap handler in common/start.S spins forever, so
        # an access fault (or any early trap) ends the run at the sim time limit
        # with exit 0 and no FAIL: line -- indistinguishable from a clean run.
        if ! grep -Eq "${FW_PASS_RE}" "${sim_log}"; then
            rm -f "${sim_log}"
            log_error "no pass line from $1 (expected 'PASS:' or 'ALL TESTS PASSED')"
            log_error "a hang, early trap, or truncated run looks exactly like this"
            return 1
        fi
        rm -f "${sim_log}"
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
    echo "Available SMC VP tests:"
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

Host-agnostic runner for the smc-vp bare-metal firmware tests.

Options:
  --build-vp      (re)configure and build smc-vp before running tests
  --rebuild-vp    alias for --build-vp
  --interactive   -i  show a numbered menu and run the chosen test
  --list          list available smc-* tests and exit
  --help          show this help and exit

Environment overrides:
  RISCV_PREFIX, RISCV_TOOLCHAIN_PATH, SYSTEMC_HOME, CCI_HOME,
  WHISPER_HOME, BOOST_DIR, VP, VP_BUILD_DIR

Examples:
  ${0##*/}                     # run all smc-* tests
  ${0##*/} smc-dma-test        # run a single test by name
  ${0##*/} -i                  # choose a test interactively
  ${0##*/} --build-vp          # build smc-vp first, then run all tests
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

# Resolve (building if needed) before the loop: every test runs the same VP, and
# without it there is nothing to run, so fail here rather than reporting a
# confusing per-test simulation failure.
if ! resolve_vp; then
    exit 1
fi
log_info "smc-vp: ${VP_BIN}"

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
