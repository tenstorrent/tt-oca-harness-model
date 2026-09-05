#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# =============================================================================
# Fail if an ASan / LSan / UBSan report is present, even when the test binary
# returned 0 (halt_on_error=0 keeps the run going to surface more errors).
#
# Usage:
#   enforce_asan_clean.sh <build_dir> [extra_log ...]
#
# Scans <build_dir>/asan.log* (and any extra log files) for sanitizer
# violations.  Missing logs mean a clean run — ASan only creates log_path.PID
# when it has something to report.
# =============================================================================

set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "usage: enforce_asan_clean.sh <build_dir> [extra_log ...]" >&2
    exit 2
fi

BUILD_DIR="$1"
shift

LOGS=()
# ASan log_path=foo writes foo.<pid>; also accept a bare asan.log.
while IFS= read -r _f; do
    [[ -n "${_f}" && -f "${_f}" ]] && LOGS+=("${_f}")
done <<EOF
$(find "${BUILD_DIR}" -maxdepth 2 \( -name 'asan.log' -o -name 'asan.log.*' \) 2>/dev/null | sort)
EOF

for _extra in "$@"; do
    [[ -n "${_extra}" && -f "${_extra}" ]] && LOGS+=("${_extra}")
done

if [[ ${#LOGS[@]} -eq 0 ]]; then
    echo ">> ASan gate: no sanitizer log (clean)"
    exit 0
fi

# Concatenate first: grep -c on several files prints one count per file.
# Match ASan, LSan, MSan, and UBSan — a leak-only report says
# "ERROR: LeakSanitizer", not "ERROR: AddressSanitizer".
COUNT=$(cat "${LOGS[@]}" 2>/dev/null |
        grep -cE 'ERROR: (Address|Leak|Memory)Sanitizer|ERROR: UndefinedBehaviorSanitizer|runtime error:' || true)

if [[ "${COUNT}" -eq 0 ]]; then
    echo ">> ASan gate PASS (no sanitizer errors in ${#LOGS[@]} log file(s))"
    exit 0
fi

echo "ERROR: ASan gate FAIL — ${COUNT} sanitizer error(s) in:" >&2
printf '  %s\n' "${LOGS[@]}" >&2
exit 1
