#!/usr/bin/env bash
# Regenerate all three CLINT PDF documents from their Markdown sources.
#
# Usage:  ./doc/build_docs.sh
#
# Requirements:
#   - pandoc   (brew install pandoc)
#   - Google Chrome (for headless PDF print)
#
# The stylesheet print.css in this directory controls fonts, table borders,
# code-block styling, and page margins. Edit it to adjust the look.

set -euo pipefail
DOC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CSS="${DOC_DIR}/print.css"
CHROME="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"

if ! command -v pandoc &>/dev/null; then
    echo "ERROR: pandoc not found. Install with: brew install pandoc" >&2; exit 1
fi
if [[ ! -x "${CHROME}" ]]; then
    echo "ERROR: Google Chrome not found at ${CHROME}" >&2; exit 1
fi

for entry in \
  "01_CLINT_Specification|SMC CLINT — Functional Specification" \
  "02_CLINT_LowLevel_Design|SMC CLINT — Low-Level Design" \
  "03_CLINT_Test_Plan|SMC CLINT — Test Plan"
do
  base="${entry%%|*}"
  title="${entry##*|}"
  md="${DOC_DIR}/${base}.md"
  pdf="${DOC_DIR}/${base}.pdf"
  tmp="/tmp/${base}.html"

  echo ">> Building ${base}.pdf …"
  pandoc "${md}" \
    -t html5 -s --embed-resources --standalone \
    --css "${CSS}" \
    --metadata title="${title}" \
    -o "${tmp}"

  "${CHROME}" \
    --headless=new --disable-gpu --no-sandbox \
    --print-to-pdf="${pdf}" \
    --print-to-pdf-no-header \
    --run-all-compositor-stages-before-draw \
    --no-margins \
    "${tmp}" 2>&1 | grep -v "^$" | tail -1

  echo "   => ${pdf}"
done

echo ""
echo "All PDFs built successfully."
