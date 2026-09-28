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
#   COVERAGE_PER_FILE       1 => additionally require EVERY model source (and
#                           every instrumented header under include/) to reach
#                           the gate on its own.  Default 0.
#
# Why per-file matters: an aggregate src/ percentage lets a weakly covered file
# hide behind heavily executed neighbours.  Large declarative register tables
# execute in full during construction, so a model can report a high aggregate
# while a routing or decode source sits well below the gate.  IPs opt in as
# they are remediated; once every IP sets it, the default should flip to 1.
# =============================================================================

set -euo pipefail

MIN="${COVERAGE_MIN_LINE_PCT:-95}"
PER_FILE="${COVERAGE_PER_FILE:-0}"
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

# Print "<file> <line_pct>" for every per-file data row of a coverage report.
# $1 selects which percentage on the row carries LINE coverage:
#   llvm-cov report -> region, function, LINE, branch  (index 2)
#   gcovr / lcov    -> LINE first                      (index 0)
_extract_per_file() {
    LINE_PCT_INDEX="$1" perl -ne '
        my $i = $ENV{LINE_PCT_INDEX};
        next if /^\s*$/ || /^-+$/ || /^=+$/;
        next if /^(Filename|File|TOTAL|Total|Directory|Message|\s+GCC)\b/;
        my ($f) = /^(\S+)/;
        next unless defined $f && $f =~ /\.(c|cc|cpp|h|hh|hpp)$/;
        my @p = /([0-9]+(?:\.[0-9]+)?)%/g;
        next unless defined $p[$i];
        print "$f $p[$i]\n";
    '
}

# Fail the build if any row of "<file> <pct>" on stdin is below ${MIN}.
_enforce_per_file() {
    local failed=0 file pct
    echo ">> Per-file line coverage (gate: ≥ ${MIN}% each):"
    while read -r file pct; do
        [[ -z "${file}" ]] && continue
        if awk -v p="${pct}" -v m="${MIN}" 'BEGIN { exit (p+0 < m+0) }'; then
            printf '   PASS  %7s%%  %s\n' "${pct}" "${file}"
        else
            printf '   FAIL  %7s%%  %s\n' "${pct}" "${file}"
            failed=1
        fi
    done
    if [[ "${failed}" -ne 0 ]]; then
        echo "ERROR: at least one file is below the ${MIN}% per-file gate." >&2
        return 1
    fi
    return 0
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

# Headers carrying inline model code count as touched model files under the
# per-file gate; they are left out of the aggregate so its meaning is unchanged.
HDR_FILES=()
INC_DIR="${IP_ROOT}/include"
if [[ "${PER_FILE}" == "1" && -d "${INC_DIR}" ]]; then
    while IFS= read -r _f; do
        if [[ -n "${_f}" ]]; then HDR_FILES+=("${_f}"); fi
    done <<EOF
$(find "${INC_DIR}" -type f \( -name '*.h' -o -name '*.hh' -o -name '*.hpp' \) | sort)
EOF
fi

PCT=""
if [[ "${COVERAGE_TOOL}" == "llvm" ]]; then
    PROFDATA="$(find "${BUILD_DIR}" -maxdepth 2 -name '*.profdata' | head -1)"
    # Prefer the bench named after the IP (the primary one).  An IP with
    # several benches would otherwise be measured against whichever binary
    # `find` happened to return first, which is filesystem-order dependent and
    # made the reported percentage differ between machines.
    _ip_name="$(basename "${IP_ROOT}")"
    TB_BIN="$(find "${BUILD_DIR}" -type f -perm -111 -name "${_ip_name}_tb" | head -1)"
    if [[ -z "${TB_BIN}" ]]; then
        TB_BIN="$(find "${BUILD_DIR}" -type f -perm -111 \( -name '*_tb' -o -name '*_test' \) | sort | head -1)"
    fi
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

    if [[ "${PER_FILE}" == "1" ]]; then
        PER_FILE_ROWS="$(${COV_CMD} report "${TB_BIN}" \
            -instr-profile="${PROFDATA}" \
            "${SRC_FILES[@]}" "${HDR_FILES[@]+"${HDR_FILES[@]}"}" \
            | _extract_per_file 2)"
    fi
else
    if command -v gcovr &>/dev/null; then
        GCOVR_ARGS=(--root "${SRC_DIR}"
                    --object-directory "${BUILD_DIR}"
                    --filter "${SRC_DIR}/")
        if [[ "${PER_FILE}" == "1" && -d "${INC_DIR}" ]]; then
            GCOVR_ARGS+=(--filter "${INC_DIR}/")
        fi
        PCT="$(gcovr "${GCOVR_ARGS[@]}" | _extract_line_pct)"
        if [[ "${PER_FILE}" == "1" ]]; then
            PER_FILE_ROWS="$(gcovr "${GCOVR_ARGS[@]}" | _extract_per_file 0)"
        fi
    elif command -v lcov &>/dev/null; then
        INFO="${BUILD_DIR}/coverage.info"
        if [[ ! -f "${INFO}" ]]; then
            echo "ERROR: ${INFO} missing; cannot enforce the coverage gate." >&2
            exit 1
        fi
        # The raw tracefile also carries SystemC, CCI and libstdc++ headers, so
        # its total says nothing about the model. Narrow it to src/ first, then
        # read the one-line summary --list does not emit in a parseable form.
        SRC_INFO="${BUILD_DIR}/coverage_src.info"
        if ! lcov --extract "${INFO}" "${SRC_DIR}/*" \
                  --output-file "${SRC_INFO}" &>/dev/null; then
            echo "ERROR: lcov could not extract ${SRC_DIR} from ${INFO}." >&2
            exit 1
        fi
        PCT="$(lcov --summary "${SRC_INFO}" 2>&1 |
               sed -n 's/^ *lines\.*: *\([0-9.]*\)%.*/\1/p' | tail -1)"
        if [[ "${PER_FILE}" == "1" ]]; then
            # SRC_INFO was extracted with src/ only, so listing it would never
            # show an include/ row and the header half of the per-file gate
            # would silently do nothing on a GCC+lcov host.  Extract a second
            # trace covering both trees for the per-file listing.
            PER_FILE_INFO="${BUILD_DIR}/coverage_per_file.info"
            PER_FILE_PATTERNS=("${SRC_DIR}/*")
            if [[ -d "${INC_DIR}" ]]; then
                PER_FILE_PATTERNS+=("${INC_DIR}/*")
            fi
            if ! lcov --extract "${INFO}" "${PER_FILE_PATTERNS[@]}" \
                      --output-file "${PER_FILE_INFO}" &>/dev/null; then
                echo "ERROR: lcov could not extract per-file coverage from ${INFO}." >&2
                exit 1
            fi
            # --list prints "path | <line%> <lines> | ..." per file.
            PER_FILE_ROWS="$(lcov --list "${PER_FILE_INFO}" 2>/dev/null |
                             sed 's/|/ /g' | _extract_per_file 0)"
        fi
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

RC=0
if ! awk -v p="${PCT}" -v m="${MIN}" 'BEGIN { exit (p+0 < m+0) }'; then
    echo "ERROR: line coverage ${PCT}% is below the ${MIN}% gate." >&2
    RC=1
fi

if [[ "${PER_FILE}" == "1" ]]; then
    if [[ -z "${PER_FILE_ROWS:-}" ]]; then
        echo "ERROR: per-file coverage requested but no per-file rows parsed." >&2
        RC=1
    elif ! printf '%s\n' "${PER_FILE_ROWS}" | _enforce_per_file; then
        RC=1
    fi
fi

if [[ "${RC}" -eq 0 ]]; then
    echo ">> Coverage gate PASS"
fi
exit "${RC}"
