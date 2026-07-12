#!/usr/bin/env bash
# Convert one or more Markdown files to PDF at any location in the repo.
#
# Pipeline: pandoc (Markdown -> standalone HTML5 with embedded CSS)
#           Google Chrome headless (HTML5 -> PDF)
#
# Usage:
#   scripts/build_docs.sh <md-file> [<md-file>...] [options]
#   scripts/build_docs.sh doc/foo.md                  # -> doc/foo.pdf
#   scripts/build_docs.sh doc/foo.md --out build/foo.pdf
#   scripts/build_docs.sh doc/a.md doc/b.md           # batch
#   scripts/build_docs.sh doc/*.md                    # glob (shell-expanded)
#   scripts/build_docs.sh doc/foo.md --no-toc
#   scripts/build_docs.sh doc/foo.md --css my.css --title "My Doc"
#   scripts/build_docs.sh doc/foo.md --engine typst   # pandoc PDF engine bypass
#
# Options:
#   --out <path>     Output PDF path (only valid with a single input file;
#                    for multiple inputs each PDF is written next to its .md).
#   --css <path>     Stylesheet to embed. Default: print.css next to the input
#                    file, else <repo>/smc/cpu_cluster/doc/print.css, else none.
#   --title <text>   Document title (metadata). Default: derived from filename.
#   --toc-depth <n>  Table-of-contents depth (default 3). Implies --toc.
#   --no-toc         Disable the table of contents.
#   --engine <name>  Use a pandoc PDF engine (typst|tectonic|pdflatex|...) and
#                    skip the HTML/Chrome step. `--css` is ignored in this mode.
#   --keep-html      Keep the intermediate HTML next to the PDF.
#   --chrome <path>  Override the Chrome/Chromium executable path or name.
#   -h, --help       Show this help.
#
# Requirements (HTML/Chrome path, the default):
#   - pandoc
#       macOS:  brew install pandoc
#       RHEL:   sudo dnf install pandoc
#       Ubuntu: sudo apt install pandoc
#   - Google Chrome or Chromium (auto-detected; override with --chrome)
#       macOS:  brew install --cask google-chrome
#       RHEL:   sudo dnf install google-chrome-stable
#               (or: sudo dnf install chromium)
#       Ubuntu: sudo apt install google-chrome-stable
#               (or: sudo apt install chromium-browser)
#   - optional: typst or tectonic if you pass --engine
#       macOS:  brew install typst / brew install tectonic
#       RHEL:   sudo dnf install typst / sudo dnf install tectonic
#       Ubuntu: sudo apt install typst / sudo apt install tectonic
#
# The script is portable across macOS, RHEL/Fedora, and Ubuntu, and
# location-agnostic: it resolves every path to an absolute path before
# invoking pandoc/Chrome, so it works from any cwd and for any folder
# hierarchy in the repo.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
DEFAULT_CSS_CANDIDATES=(
  "${REPO_ROOT}/smc/cpu_cluster/doc/print.css"
)

# Shared temp directory for intermediate HTML (cleaned up on EXIT).
TMP_DIR=""
cleanup() {
  if [[ -n "${TMP_DIR}" && -d "${TMP_DIR}" ]]; then
    rm -rf "${TMP_DIR}"
  fi
}
trap cleanup EXIT

# ---- defaults ----------------------------------------------------------------
out=""
css=""
title=""
toc_depth=3
no_toc=0
engine=""
keep_html=0
chrome=""   # auto-detected on first use; override with --chrome

# ---- arg parsing -------------------------------------------------------------
md_files=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      sed -n '3,40p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
      exit 0
      ;;
    --out)        out="$2"; shift 2 ;;
    --css)        css="$2"; shift 2 ;;
    --title)      title="$2"; shift 2 ;;
    --toc-depth)  toc_depth="$2"; shift 2 ;;
    --no-toc)     no_toc=1; shift ;;
    --engine)     engine="$2"; shift 2 ;;
    --keep-html)  keep_html=1; shift ;;
    --chrome)     chrome="$2"; shift 2 ;;
    --)
      shift; while [[ $# -gt 0 ]]; do md_files+=("$1"); shift; done ;;
    -*)
      echo "ERROR: unknown option: $1" >&2; exit 2 ;;
    *)
      md_files+=("$1"); shift ;;
  esac
done

if [[ ${#md_files[@]} -eq 0 ]]; then
  echo "ERROR: no input .md file(s) given. See --help." >&2
  exit 2
fi

if [[ -n "${out}" && ${#md_files[@]} -gt 1 ]]; then
  echo "ERROR: --out may only be used with a single input file." >&2
  exit 2
fi

# ---- tool checks -------------------------------------------------------------
if ! command -v pandoc &>/dev/null; then
  cat >&2 <<'EOF'
ERROR: pandoc not found. Install:
  macOS:  brew install pandoc
  RHEL:   sudo dnf install pandoc
  Ubuntu: sudo apt install pandoc
EOF
  exit 1
fi

# ---- Chrome / Chromium auto-detection (macOS + Linux) ------------------------
# Returns the executable path/name on stdout, or non-zero if none found.
find_chrome() {
  local c
  # 1. Linux binaries in PATH (also works for Homebrew-installed Chrome on macOS
  #    if the user has put the binary on PATH).
  for c in google-chrome google-chrome-stable chromium chromium-browser; do
    if command -v "${c}" &>/dev/null; then
      command -v "${c}"
      return 0
    fi
  done
  # 2. macOS standard install locations.
  for c in \
    "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" \
    "/Applications/Chromium.app/Contents/MacOS/Chromium" \
    "${HOME}/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
  do
    if [[ -x "${c}" ]]; then
      printf '%s' "${c}"
      return 0
    fi
  done
  return 1
}

# True if $1 is runnable: either an executable absolute/relative path,
# or a name resolvable via PATH.
chrome_is_runnable() {
  local c="$1"
  if [[ "${c}" == */* ]]; then
    [[ -x "${c}" ]]
  else
    command -v "${c}" &>/dev/null
  fi
}

if [[ -z "${engine}" ]]; then
  if [[ -z "${chrome}" ]]; then
    if ! chrome="$(find_chrome 2>/dev/null)"; then
      cat >&2 <<'EOF'
ERROR: no Google Chrome / Chromium found. Either:
  - install it (macOS: brew install --cask google-chrome;
                RHEL: sudo dnf install google-chrome-stable or chromium;
                Ubuntu: sudo apt install google-chrome-stable or chromium-browser),
  - pass --chrome <path-or-name>, or
  - use --engine typst|tectonic to bypass Chrome entirely.
EOF
      exit 1
    fi
  elif ! chrome_is_runnable "${chrome}"; then
    echo "ERROR: --chrome not runnable: ${chrome}" >&2
    exit 1
  fi
fi

# ---- helpers -----------------------------------------------------------------
resolve_css() {
  local md_abs="$1"
  local md_dir
  md_dir="$(cd "$(dirname "${md_abs}")" && pwd)"

  # 1. explicit --css
  if [[ -n "${css}" ]]; then
    local c="${css}"
    [[ "${c}" != /* ]] && c="${PWD}/${c}"
    if [[ ! -f "${c}" ]]; then
      echo "ERROR: --css not found: ${c}" >&2
      exit 1
    fi
    printf '%s' "${c}"
    return
  fi
  # 2. print.css next to the markdown
  if [[ -f "${md_dir}/print.css" ]]; then
    printf '%s' "${md_dir}/print.css"
    return
  fi
  # 3. repo default
  for c in "${DEFAULT_CSS_CANDIDATES[@]}"; do
    if [[ -f "${c}" ]]; then printf '%s' "${c}"; return; fi
  done
  # 4. none
  printf ''
}

derive_title() {
  local md_abs="$1"
  local base
  base="$(basename "${md_abs}")"
  base="${base%.*}"
  # Foo_Bar_Baz -> "Foo Bar Baz"
  printf '%s' "${base//_/ }"
}

build_one() {
  local md_abs="$1"
  local md_dir md_base pdf_target tmp_html toc_args css_abs

  md_dir="$(cd "$(dirname "${md_abs}")" && pwd)"
  md_base="$(basename "${md_abs}")"
  md_base="${md_base%.*}"

  if [[ -n "${out}" ]]; then
    pdf_target="${out}"
    [[ "${pdf_target}" != /* ]] && pdf_target="${PWD}/${pdf_target}"
  else
    pdf_target="${md_dir}/${md_base}.pdf"
  fi

  mkdir -p "$(dirname "${pdf_target}")"

  if [[ -n "${title}" ]]; then :; else
    title="$(derive_title "${md_abs}")"
  fi

  toc_args=(--toc --toc-depth="${toc_depth}")
  [[ "${no_toc}" -eq 1 ]] && toc_args=()
  echo ">> ${md_abs}  ->  ${pdf_target}"

  if [[ -n "${engine}" ]]; then
    # Direct pandoc -> PDF via a real PDF engine (typst / tectonic / ...).
    pandoc "${md_abs}" \
      -o "${pdf_target}" \
      --pdf-engine="${engine}" \
      --metadata title="${title}" \
      ${toc_args[@]+"${toc_args[@]}"}
    echo "   => ${pdf_target}"
    return
  fi

  css_abs="$(resolve_css "${md_abs}")"

  if [[ ${keep_html} -eq 1 ]]; then
    tmp_html="${pdf_target%.pdf}.html"
  else
    if [[ -z "${TMP_DIR}" ]]; then
      TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/builddocs.XXXXXX")"
    fi
    tmp_html="${TMP_DIR}/output.html"
  fi

  local pandoc_args=(pandoc "${md_abs}"
    -t html5 -s --embed-resources --standalone
    --metadata title="${title}"
    ${toc_args[@]+"${toc_args[@]}"}
    -o "${tmp_html}")

  if [[ -n "${css_abs}" ]]; then
    pandoc_args+=(--css "${css_abs}")
  fi

  "${pandoc_args[@]}"

  "${chrome}" \
    --headless=new --disable-gpu --no-sandbox \
    --print-to-pdf="${pdf_target}" \
    --print-to-pdf-no-header \
    --run-all-compositor-stages-before-draw \
    --no-margins \
    "${tmp_html}" 2>&1 | grep -v "^$" | tail -1 || true

  if [[ ${keep_html} -eq 0 ]]; then
    rm -f "${tmp_html}"
  else
    echo "   (kept HTML: ${tmp_html})"
  fi

  echo "   => ${pdf_target}"
}

# ---- run ---------------------------------------------------------------------
for f in "${md_files[@]}"; do
  if [[ "${f}" != /* ]]; then f="${PWD}/${f}"; fi
  if [[ ! -f "${f}" ]]; then
    echo "ERROR: input not found: ${f}" >&2
    exit 1
  fi
  build_one "${f}"
done

echo ""
echo "Done: ${#md_files[@]} file(s) converted."
