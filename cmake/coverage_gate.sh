#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# Fail if filtered model line coverage is below COVERAGE_MIN_LINE_PCT (default 95).
#
# Reads LH:/LF: records from an lcov .info file so the gate does not depend on
# `lcov --list` surviving Apple LLVM / lcov 2.x inconsistency errors.
#
# Usage:
#   coverage_gate.sh --lcov-info <file.info>
#   coverage_gate.sh <file.info>

set -euo pipefail

MIN="${COVERAGE_MIN_LINE_PCT:-95}"
INFO=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --lcov-info)
      INFO="${2:-}"
      shift 2
      ;;
    --help|-h)
      echo "usage: coverage_gate.sh --lcov-info <file.info>"
      exit 0
      ;;
    *)
      INFO="$1"
      shift
      ;;
  esac
done

if [[ -z "${INFO}" || ! -f "${INFO}" ]]; then
  echo ">> Coverage gate FAIL (missing lcov info: ${INFO:-<unset>})" >&2
  exit 1
fi

PCT="$(awk -F: '
  /^LF:/ { found += $2 }
  /^LH:/ { hit += $2 }
  END {
    if (found <= 0) exit 1
    printf "%.1f\n", 100.0 * hit / found
  }
' "${INFO}")"

if [[ -z "${PCT}" ]]; then
  echo ">> Coverage gate FAIL (could not parse ${INFO})" >&2
  exit 1
fi

echo ">> Line coverage: ${PCT}%  (gate: ≥ ${MIN}%)"
if awk -v p="${PCT}" -v m="${MIN}" 'BEGIN { exit (p+0 < m+0) }'; then
  echo ">> Coverage gate PASS"
  exit 0
fi

echo "ERROR: line coverage ${PCT}% is below the ${MIN}% gate." >&2
exit 1
