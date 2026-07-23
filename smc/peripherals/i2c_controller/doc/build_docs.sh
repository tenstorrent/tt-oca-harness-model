#!/usr/bin/env bash
# Regenerate all three I2C Controller PDF documents from their Markdown sources.
#
# Usage:  ./doc/build_docs.sh
#
# Requirements:
#   - pandoc          (brew install pandoc  |  dnf/apt install pandoc)
#   - Google Chrome / Chromium (for headless PDF print)
#
# The stylesheet print.css in this directory controls fonts, table borders,
# code-block styling, figure sizing, and page margins.  Edit it to adjust the
# look.  Referenced figures (doc/figures/*.svg) are embedded into the PDF via
# pandoc's --embed-resources, so the output is self-contained.

set -euo pipefail
DOC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CSS="${DOC_DIR}/print.css"

if ! command -v pandoc &>/dev/null; then
    echo "ERROR: pandoc not found. Install with: brew install pandoc (macOS) or dnf/apt install pandoc (Linux)" >&2
    exit 1
fi

# Locate a Chrome/Chromium binary across macOS and Linux.
CHROME=""
for cand in \
    "${CHROME:-}" \
    "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" \
    "$(command -v google-chrome 2>/dev/null || true)" \
    "$(command -v google-chrome-stable 2>/dev/null || true)" \
    "$(command -v chromium 2>/dev/null || true)" \
    "$(command -v chromium-browser 2>/dev/null || true)"
do
    if [[ -n "${cand}" && -x "${cand}" ]]; then CHROME="${cand}"; break; fi
done

if [[ -z "${CHROME}" ]]; then
    echo "ERROR: Google Chrome / Chromium not found. Set CHROME=/path/to/chrome and re-run." >&2
    exit 1
fi
echo "Using Chrome: ${CHROME}"

for entry in \
  "01_I2C_CONTROLLER_Specification|OCA I2C Controller — Functional Specification" \
  "02_I2C_CONTROLLER_LowLevel_Design|OCA I2C Controller — Low-Level Design" \
  "03_I2C_CONTROLLER_Test_Plan|OCA I2C Controller — Test Plan"
do
  base="${entry%%|*}"
  title="${entry##*|}"
  md="${DOC_DIR}/${base}.md"
  pdf="${DOC_DIR}/${base}.pdf"
  tmp="$(mktemp -t "${base}.XXXXXX.html")"

  echo ">> Building ${base}.pdf …"
  pandoc "${md}" \
    -t html5 -s --embed-resources --standalone \
    --resource-path="${DOC_DIR}" \
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
    "${tmp}" 2>&1 | grep -v "^$" | tail -1 || true

  rm -f "${tmp}"
  echo "   => ${pdf}"
done

echo ""
echo "All PDFs built successfully."
