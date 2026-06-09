#!/usr/bin/env bash
# Build SystemC_Virtual_Platform_Customer_Guide.pdf with the house table style
# (light-blue header, full vertical grid) via the pandoc-grid-tables Lua filter.
#
# Requires: pandoc + a LaTeX engine (tectonic by default).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MD="${SCRIPT_DIR}/SystemC_Virtual_Platform_Customer_Guide.md"
PDF="${SCRIPT_DIR}/SystemC_Virtual_Platform_Customer_Guide.pdf"
FILTER="${SCRIPT_DIR}/pandoc-grid-tables.lua"
ENGINE="${PDF_ENGINE:-tectonic}"

if ! command -v pandoc >/dev/null 2>&1; then
  echo "error: pandoc not installed (try: brew install pandoc)" >&2
  exit 1
fi

pandoc "$MD" \
  -o "$PDF" \
  --pdf-engine="$ENGINE" \
  --lua-filter="$FILTER"

echo "wrote: $PDF"
