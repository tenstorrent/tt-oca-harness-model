#!/usr/bin/env bash
# md-to-pdf.sh
# Convert one or more Markdown files into well-formatted PDFs.
#
# Pipeline: Markdown -> (pandoc) -> styled HTML -> (Chrome headless) -> PDF
# This avoids needing a LaTeX install and produces clean GitHub-flavoured output.
#
# Usage:
#   scripts/md-to-pdf.sh [-o OUT_DIR] FILE.md [FILE2.md ...]
#   scripts/md-to-pdf.sh [-o OUT_DIR] DIR        # convert all *.md under DIR
#
# Requirements (macOS):
#   - pandoc            (brew install pandoc)
#   - Google Chrome.app (default at /Applications/Google Chrome.app)

set -euo pipefail

OUT_DIR=""
INPUTS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    -o|--out)
      OUT_DIR="$2"; shift 2 ;;
    -h|--help)
      sed -n '1,16p' "$0"; exit 0 ;;
    *)
      INPUTS+=("$1"); shift ;;
  esac
done

if [[ ${#INPUTS[@]} -eq 0 ]]; then
  echo "error: no input files or directories provided" >&2
  exit 2
fi

# Resolve Chrome binary.
CHROME="${CHROME_BIN:-/Applications/Google Chrome.app/Contents/MacOS/Google Chrome}"
if [[ ! -x "$CHROME" ]]; then
  echo "error: Google Chrome not found at: $CHROME" >&2
  echo "       set CHROME_BIN=/path/to/chrome" >&2
  exit 3
fi
if ! command -v pandoc >/dev/null 2>&1; then
  echo "error: pandoc not installed (try: brew install pandoc)" >&2
  exit 3
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CSS_FILE="${SCRIPT_DIR}/md-pdf.css"
if [[ ! -f "$CSS_FILE" ]]; then
  echo "error: missing stylesheet: $CSS_FILE" >&2
  exit 3
fi

# Expand directories into .md files.
EXPANDED=()
for IN in "${INPUTS[@]}"; do
  if [[ -d "$IN" ]]; then
    while IFS= read -r -d '' f; do EXPANDED+=("$f"); done \
      < <(find "$IN" -type f -name '*.md' -print0 | sort -z)
  elif [[ -f "$IN" ]]; then
    EXPANDED+=("$IN")
  else
    echo "warn: skipping (not found): $IN" >&2
  fi
done

if [[ ${#EXPANDED[@]} -eq 0 ]]; then
  echo "error: no markdown files matched" >&2
  exit 2
fi

convert_one() {
  local md="$1"
  local base out_pdf html
  base="$(basename "${md%.md}")"
  local dir
  if [[ -n "$OUT_DIR" ]]; then
    mkdir -p "$OUT_DIR"
    dir="$OUT_DIR"
  else
    dir="$(dirname "$md")"
  fi
  out_pdf="${dir}/${base}.pdf"
  html="$(mktemp -t mdpdf-XXXXXX).html"

  # Use pandoc to render Markdown -> standalone HTML with embedded CSS,
  # GFM extensions, syntax highlighting, and a numbered table of contents.
  # Resolve the markdown's directory so relative resources (figures,
  # diagrams, screenshots) embed correctly regardless of cwd.
  local md_dir
  md_dir="$(cd "$(dirname "$md")" && pwd)"

  pandoc \
    --from=gfm+yaml_metadata_block+tex_math_dollars \
    --to=html5 \
    --standalone \
    --embed-resources \
    --resource-path="${md_dir}:." \
    --syntax-highlighting=tango \
    --toc --toc-depth=3 \
    --metadata title="$(head -n1 "$md" | sed -E 's/^#+\s*//')" \
    --css="$CSS_FILE" \
    -o "$html" \
    "$md"

  # Render HTML -> PDF using Chrome headless. file:// URI required.
  local file_uri
  file_uri="file://$(python3 -c 'import urllib.parse,sys; print(urllib.parse.quote(sys.argv[1]))' "$html")"

  "$CHROME" \
    --headless=new \
    --disable-gpu \
    --no-pdf-header-footer \
    --no-sandbox \
    --hide-scrollbars \
    --virtual-time-budget=10000 \
    --print-to-pdf-no-header \
    --print-to-pdf="$out_pdf" \
    "$file_uri" 2>/dev/null

  rm -f "$html"
  echo "wrote: $out_pdf"
}

for md in "${EXPANDED[@]}"; do
  convert_one "$md"
done
