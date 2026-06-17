#!/usr/bin/env bash
# Regenerate SMC Fabric PDF documents from their Markdown sources.
#
# Usage:  ./doc/build_docs.sh
#
# Requirements:
#   - pandoc   (e.g. dnf install pandoc / brew install pandoc)
#   - Google Chrome or Chromium (headless PDF print)
#
# The stylesheet print.css in this directory controls fonts, table borders,
# code-block styling, and page margins.

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
    # Optional user-local install (see README — doc PDF build).
    for candidate in "${DOC_DIR}"/../.tools/pandoc-*/bin/pandoc; do
        if [[ -x "${candidate}" ]]; then
            export PATH="$(dirname "${candidate}"):${PATH}"
            break
        fi
    done
fi

if ! command -v pandoc &>/dev/null; then
    echo "ERROR: pandoc not found. Install pandoc or run:" >&2
    echo "  curl -fsSL -o /tmp/pandoc.tar.gz \\" >&2
    echo "    https://github.com/jgm/pandoc/releases/download/3.6.4/pandoc-3.6.4-linux-amd64.tar.gz" >&2
    echo "  tar -xzf /tmp/pandoc.tar.gz -C smc_fabric/.tools" >&2
    exit 1
fi

CHROME="$(find_chrome)" || {
    echo "ERROR: Chrome/Chromium not found. Set CHROME=/path/to/chrome or install google-chrome/chromium." >&2
    exit 1
}

echo ">> Using ${CHROME} for PDF generation"

for entry in \
  "01_overview_and_architecture|SMC Fabric — Overview and Architecture" \
  "02_tlm_interface|SMC Fabric — TLM-2.0 Interface Specification" \
  "03_internal_architecture|SMC Fabric — Internal Architecture" \
  "04_register_interface|SMC Fabric — Register Interface" \
  "05_systemc_implementation|SMC Fabric — SystemC Implementation Guide"
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

  "${CHROME}" \
    --headless=new --disable-gpu --no-sandbox \
    --print-to-pdf="${pdf}" \
    --print-to-pdf-no-header \
    --run-all-compositor-stages-before-draw \
    --no-margins \
    "file://${tmp}" 2>&1 | grep -v "^$" | tail -1 || true

  rm -f "${tmp}"
  echo "   => ${pdf}"
done

echo ""
echo "All PDFs built successfully."
