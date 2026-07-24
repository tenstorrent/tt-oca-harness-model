# scripts/

Repo-wide helper scripts.

## `build_docs.sh` — Markdown to PDF converter

A generic, location-agnostic wrapper around `pandoc` (+ Google Chrome / Chromium
headless, or a pandoc PDF engine like `typst`/`tectonic`) that converts one or
more `.md` files to PDF anywhere in the repository. Portable across **macOS,
RHEL/Fedora, and Ubuntu**.

### Requirements

- **pandoc**
  - macOS:  `brew install pandoc`
  - RHEL:   `sudo dnf install pandoc`
  - Ubuntu: `sudo apt install pandoc`
- **Google Chrome or Chromium** (default HTML/CSS → PDF path) — auto-detected.
  The script searches, in order:
  1. `google-chrome`, `google-chrome-stable`, `chromium`, `chromium-browser` on `PATH`
  2. macOS install locations (`/Applications/Google Chrome.app/...`,
     `/Applications/Chromium.app/...`, `~/Applications/...`)

  Override with `--chrome <path-or-name>` (e.g. `--chrome chromium`).

  Install:
  - macOS:  `brew install --cask google-chrome`
  - RHEL:   `sudo dnf install google-chrome-stable` (or `sudo dnf install chromium`)
  - Ubuntu: `sudo apt install google-chrome-stable` (or `sudo apt install chromium-browser`)

  > On Linux, `--no-sandbox` is passed to Chrome automatically so the script
  > also works as root (CI, containers).
- *Optional* — a pandoc PDF engine (`typst`, `tectonic`, `pdflatex`, ...) if you
  use `--engine`. This bypasses Chrome entirely, which is handy on headless
  Linux boxes without a Chrome install.
  - macOS:  `brew install typst` / `brew install tectonic`
  - RHEL:   `sudo dnf install typst` / `sudo dnf install tectonic`
  - Ubuntu: `sudo apt install typst` / `sudo apt install tectonic`

### Quick start

```bash
# Default: PDF is written next to the .md (same basename, .pdf extension).
./scripts/build_docs.sh doc/foo.md                  # -> doc/foo.pdf

# Explicit output path (single input only).
./scripts/build_docs.sh doc/foo.md --out build/foo.pdf

# Batch: multiple files, or a shell-expanded glob.
./scripts/build_docs.sh doc/a.md doc/b.md
./scripts/build_docs.sh doc/*.md

# Disable the table of contents.
./scripts/build_docs.sh doc/foo.md --no-toc

# Use a pandoc PDF engine instead of the HTML/Chrome path (--css is ignored).
./scripts/build_docs.sh doc/foo.md --engine typst
```

### Options

| Option             | Description                                                                 |
|--------------------|-----------------------------------------------------------------------------|
| `<md-file>...`     | One or more input Markdown files (positional).                             |
| `--out <path>`     | Output PDF path. Single input only; rejected with multiple inputs.         |
| `--css <path>`     | Stylesheet to embed. Default: `print.css` next to the input, else          |
|                    | `smc/cpu_cluster/doc/print.css`, else none.                                 |
| `--title <text>`   | Document title (pandoc metadata). Default: derived from the filename.      |
| `--toc-depth <n>`  | Table-of-contents depth (default `3`). Implies `--toc`.                    |
| `--no-toc`         | Disable the table of contents.                                              |
| `--engine <name>`  | Use a pandoc PDF engine (`typst`, `tectonic`, `pdflatex`, ...). Skips the  |
|                    | HTML/Chrome step; `--css` is ignored in this mode.                          |
| `--keep-html`      | Keep the intermediate HTML next to the PDF (for debugging the stylesheet). |
| `--chrome <path>`  | Override the Chrome/Chromium executable. Accepts an absolute/relative path |
|                    | **or** a name resolved via `PATH` (e.g. `--chrome chromium`).              |
| `-h`, `--help`     | Show the embedded usage.                                                    |

### How the default path works

1. `pandoc` converts the `.md` to a standalone HTML5 document with the
   stylesheet embedded (`--embed-resources --standalone`), a TOC, and title
   metadata.
2. Google Chrome (`--headless=new --print-to-pdf`) renders the HTML to PDF.

To customize fonts, table borders, code-block styling, or page margins, edit
the `print.css` that the script discovers — do **not** patch the script per
document.

### CSS discovery order

1. `--css <path>` if given.
2. `print.css` in the same directory as the input `.md`.
3. `<repo>/smc/cpu_cluster/doc/print.css` (repo default).
4. None (pandoc's default HTML styling).

### Examples

```bash
# Convert every Markdown file under doc/ in one shot.
./scripts/build_docs.sh doc/*.md

# Use a custom stylesheet and title.
./scripts/build_docs.sh doc/design.md --css doc/dark.css --title "Design Doc"

# Keep the HTML to debug a table-rendering issue.
./scripts/build_docs.sh doc/foo.md --keep-html

# Produce a PDF via typst (no Chrome needed — useful on headless Linux CI).
./scripts/build_docs.sh doc/foo.md --engine typst --no-toc

# Point at a Chromium-based browser elsewhere.
./scripts/build_docs.sh doc/foo.md --chrome /usr/bin/chromium

# Use a Chromium binary that's on PATH (Linux package install).
./scripts/build_docs.sh doc/foo.md --chrome chromium-browser
```

### Platform notes

- **macOS**: Chrome is auto-discovered at
  `/Applications/Google Chrome.app/Contents/MacOS/Google Chrome` (or Chromium,
  or under `~/Applications`). System `bash` 3.2 is supported.
- **RHEL / Fedora / CentOS**: install `google-chrome-stable` (RPM from Google)
  or `chromium` from EPEL / Fedora repos; either will be picked up from `PATH`.
  `--no-sandbox` is passed automatically so the script also runs as root in
  CI/containers.
- **Ubuntu / Debian**: install `google-chrome-stable` (`.deb` from Google) or
  `chromium-browser` from the archive; either will be picked up from `PATH`.
- **Headless / no Chrome**: use `--engine typst` (or `tectonic`) to skip the
  Chrome step entirely.
- The script uses `mktemp -d "${TMPDIR:-/tmp}/builddocs.XXXXXX"` for the
  intermediate HTML, which is portable across BSD and GNU `mktemp`, and removes
  it on exit via a trap.

### Exit codes

| Code | Meaning                                              |
|------|------------------------------------------------------|
| `0`  | All inputs converted successfully.                   |
| `1`  | Missing tool (pandoc / Chrome), or input not found.  |
| `2`  | Invalid CLI usage (unknown option, `--out` with      |
|      | multiple inputs, no input files given).              |

### See also

- `.cursor/rules/md-to-pdf.mdc` — repo rule mandating this script for any
  Markdown-to-PDF conversion.
- `smc/cpu_cluster/doc/print.css` — repo-default stylesheet.
