#!/usr/bin/env bash
# Regenerate CPU Control PDF documents from Markdown sources.
set -euo pipefail
DOC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CSS="${DOC_DIR}/print.css"
CHROME="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"

if ! command -v pandoc &>/dev/null; then
    echo "ERROR: pandoc not found." >&2; exit 1
fi

for entry in \
  "01_CPU_CTRL_Specification|SMC CPU Control — Functional Specification" \
  "02_CPU_CTRL_LowLevel_Design|SMC CPU Control — Low-Level Design" \
  "03_CPU_CTRL_Test_Plan|SMC CPU Control — Test Plan"
do
  base="${entry%%|*}"
  title="${entry##*|}"
  md="${DOC_DIR}/${base}.md"
  pdf="${DOC_DIR}/${base}.pdf"
  tmp="/tmp/${base}.html"

  echo ">> Building ${base}.pdf …"
  pandoc "${md}" -t html5 -s --embed-resources --standalone \
    --css "${CSS}" --metadata title="${title}" -o "${tmp}"

  if [[ -x "${CHROME}" ]]; then
    "${CHROME}" --headless=new --disable-gpu --no-sandbox \
      --print-to-pdf="${pdf}" --print-to-pdf-no-header \
      --run-all-compositor-stages-before-draw --no-margins "${tmp}"
  else
    echo "  (Chrome not found — HTML at ${tmp})"
  fi
done
