#!/usr/bin/env bash
# Export component-developer-guide.md to PDF with a clickable table of contents.
# Uses pandoc + typst (internal PDF links preserved in TOC).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MD="${SCRIPT_DIR}/component-developer-guide.md"
PDF="${SCRIPT_DIR}/component-developer-guide.pdf"

if ! command -v pandoc >/dev/null 2>&1; then
  echo "error: pandoc not installed (try: brew install pandoc)" >&2
  exit 1
fi

pandoc "$MD" \
  -o "$PDF" \
  --pdf-engine=typst \
  -V mainfont="Helvetica Neue" \
  -V fontsize=11pt \
  -V margin-top=1in \
  -V margin-bottom=1in \
  -V margin-left=1in \
  -V margin-right=1in \
  --toc \
  --toc-depth=3

echo "wrote: $PDF"
