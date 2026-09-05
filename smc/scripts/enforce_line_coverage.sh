#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# =============================================================================
# Fail if model (src/) line coverage is below the repo gate (default 95%).
#
# Usage:
#   enforce_line_coverage.sh <ip_root> <build_dir>
#
# Environment:
#   COVERAGE_MIN_LINE_PCT   Minimum accepted line coverage (default: 95)
# =============================================================================

set -euo pipefail

MIN="${COVERAGE_MIN_LINE_PCT:-95}"
IP_ROOT="${1:?usage: enforce_line_coverage.sh <ip_root> <build_dir>}"
BUILD_DIR="${2:?usage: enforce_line_coverage.sh <ip_root> <build_dir>}"

_find_llvm_tool() {
    local base="$1"
    if command -v "${base}" &>/dev/null; then echo "${base}"; return; fi
    if command -v xcrun &>/dev/null && xcrun "${base}" --version &>/dev/null 2>&1; then
        echo "xcrun ${base}"; return
    fi
    local _v
    for _v in 20 19 18 17 16 15 14 13; do
        if command -v "${base}-${_v}" &>/dev/null; then
            echo "${base}-${_v}"; return
        fi
    done
    echo ""
}

# Print a single TOTAL line-coverage percentage from llvm-cov / gcovr / lcov.
_extract_line_pct() {
    perl -ne '
        if (/^TOTAL\b/) {
            my @p = /([0-9]+\.[0-9]+)%/g;
            if (defined $p[2]) { print "$p[2]\n"; next; }
            if (/([0-9]+(?:\.[0-9]+)?)%/) { print "$1\n"; }
        }
    ' | tail -1
}

COVERAGE_TOOL="$(cat "${BUILD_DIR}/coverage_tool.txt" 2>/dev/null || echo "llvm")"
SRC_DIR="${IP_ROOT}/src"
if [[ ! -d "${SRC_DIR}" ]]; then
    echo "ERROR: no src/ directory at ${SRC_DIR}; cannot enforce coverage." >&2
    exit 1
fi

SRC_FILES=()
while IFS= read -r _f; do
    SRC_FILES+=("${_f}")
done <<EOF
$(find "${SRC_DIR}" -type f \( -name '*.cpp' -o -name '*.cc' -o -name '*.c' \) | sort)
EOF
if [[ ${#SRC_FILES[@]} -eq 0 ]]; then
    echo "ERROR: no source files under ${SRC_DIR}; cannot enforce coverage." >&2
    exit 1
fi

PCT=""
if [[ "${COVERAGE_TOOL}" == "llvm" ]]; then
    PROFDATA="$(find "${BUILD_DIR}" -maxdepth 2 -name '*.profdata' | head -1)"
    TB_BIN="$(find "${BUILD_DIR}" -type f -perm -111 \( -name '*_tb' -o -name '*_test' \) | head -1)"
    if [[ -z "${PROFDATA}" || -z "${TB_BIN}" ]]; then
        echo "ERROR: llvm coverage artifacts not found in ${BUILD_DIR}." >&2
        exit 1
    fi
    COV_CMD="$(_find_llvm_tool llvm-cov)"
    if [[ -z "${COV_CMD}" ]]; then
        echo "ERROR: llvm-cov not found; cannot enforce the coverage gate." >&2
        exit 1
    fi
    # Measure src/ against the primary TB only. Extra binaries (bootrom_neg_tb,
    # clint_tick_tb, …) each contain their own compile of the same .cpp; passing
    # them as additional -object values makes llvm-cov drop mismatched functions
    # (bootrom::b_transport went from 100% to 0% that way) and fails the gate.
    PCT="$(${COV_CMD} report "${TB_BIN}" \
        -instr-profile="${PROFDATA}" \
        "${SRC_FILES[@]}" | _extract_line_pct)"
else
    if command -v gcovr &>/dev/null; then
        PCT="$(gcovr \
            --root "${SRC_DIR}" \
            --object-directory "${BUILD_DIR}" \
            --filter "${SRC_DIR}/" \
            | _extract_line_pct)"
    elif command -v lcov &>/dev/null; then
        INFO="${BUILD_DIR}/coverage.info"
        if [[ ! -f "${INFO}" ]]; then
            echo "ERROR: ${INFO} missing; cannot enforce the coverage gate." >&2
            exit 1
        fi
        PCT="$(lcov --list "${INFO}" | _extract_line_pct)"
    else
        echo "ERROR: neither gcovr nor lcov found; cannot enforce the coverage gate." >&2
        exit 1
    fi
fi

if [[ -z "${PCT}" ]]; then
    echo "ERROR: could not parse line coverage for ${IP_ROOT}." >&2
    exit 1
fi

echo ">> Line coverage (src/): ${PCT}%  (gate: ≥ ${MIN}%)"

if awk -v p="${PCT}" -v m="${MIN}" 'BEGIN { exit (p+0 < m+0) }'; then
    echo ">> Coverage gate PASS"
    exit 0
fi

echo "ERROR: line coverage ${PCT}% is below the ${MIN}% gate." >&2
exit 1
