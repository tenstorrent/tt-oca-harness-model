#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# Regenerate SMC CPU Cluster PDF documents from Markdown sources.
#
# Usage:  ./doc/build_docs.sh
#
# Requirements: pandoc, Chrome/Chromium (headless PDF print)

set -euo pipefail
DOC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CSS="${DOC_DIR}/print.css"

find_chrome() {
    local candidate
    for candidate in \
        "${CHROME:-}" \
        "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" \
        google-chrome-stable google-chrome chromium chromium-browser
    do
        [[ -n "${candidate}" ]] || continue
        if command -v "${candidate}" &>/dev/null; then
            command -v "${candidate}"
            return 0
        fi
        if [[ -x "${candidate}" ]]; then
            echo "${candidate}"
            return 0
        fi
    done
    return 1
}

if ! command -v pandoc &>/dev/null; then
    echo "ERROR: pandoc not found. Install pandoc (dnf install pandoc / brew install pandoc)." >&2
    exit 1
fi

CHROME="$(find_chrome)" || {
    echo "ERROR: Chrome/Chromium not found. Set CHROME=/path/to/chrome." >&2
    exit 1
}

echo ">> Using ${CHROME} for PDF generation"

for entry in \
  "01_CPU_Cluster_Specification|SMC CPU Cluster — Specification" \
  "02_CPU_Cluster_LowLevel_Design|SMC CPU Cluster — Low-Level Design" \
  "03_CPU_Cluster_Test_Plan|SMC CPU Cluster — Test Plan" \
  "04_CCI_Integration_Guide|SMC CPU Cluster — CCI Integration Guide"
do
  base="${entry%%|*}"
  title="${entry##*|}"
  md="${DOC_DIR}/${base}.md"
  pdf="${DOC_DIR}/${base}.pdf"
  tmp="$(mktemp "/tmp/${base}.XXXXXX.html")"

  echo ">> Building ${base}.pdf …"
  pandoc "${md}" \
    -t html5 -s --embed-resources --standalone \
    --css "${CSS}" \
    --metadata title="${title}" \
    --toc --toc-depth=3 \
    -o "${tmp}"

  if ! "${CHROME}" \
    --headless=new --disable-gpu --no-sandbox \
    --print-to-pdf="${pdf}" \
    --print-to-pdf-no-header \
    --run-all-compositor-stages-before-draw \
    --no-margins \
    "file://${tmp}"; then
    echo "ERROR: Chrome/Chromium failed to generate ${pdf}" >&2
    rm -f "${tmp}" "${pdf}"
    exit 1
  fi

  if [[ ! -s "${pdf}" ]]; then
    echo "ERROR: failed to generate ${pdf} (Chrome/Chromium print-to-pdf failed)." >&2
    exit 1
  fi

  rm -f "${tmp}"
  if [[ ! -s "${pdf}" ]]; then
    echo "ERROR: ${pdf} is missing or empty after Chrome print." >&2
    exit 1
  fi
  echo "   => ${pdf}"
done

echo ""
echo "All PDFs built successfully."
