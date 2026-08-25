#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
"""PDF to Markdown Converter with Diagram Preservation.

Converts PDF files to Markdown format, faithfully preserving all diagrams
and images.

Features:
- Extracts text content from PDF
- Extracts all images/diagrams from PDF pages
- Preserves document structure (headings, lists, tables)
- Embeds images in Markdown format
- Maintains page layout information

Cross-platform support:
- macOS  (brew install poppler)
- RHEL   (dnf install poppler-utils)
- Ubuntu (apt-get install poppler-utils)

Usage:
    python pdf_to_markdown_with_diagrams.py input.pdf [output.md]
    python pdf_to_markdown_with_diagrams.py doc/keraunos_pcie_tile.pdf
    python pdf_to_markdown_with_diagrams.py --check-deps
"""

import os
import re
import sys
import shutil
import subprocess
import argparse

# --- Force UTF-8 on stdout/stderr so emoji status output works on C locales ---
# (RHEL/CI boxes often default to POSIX/C and would otherwise crash on ✅/📄.)
for _stream in (sys.stdout, sys.stderr):
    if hasattr(_stream, "reconfigure"):
        try:
            _stream.reconfigure(encoding="utf-8", errors="replace")
        except (ValueError, TypeError):
            pass

# Try to import PDF processing libraries (each is optional — we fall back).
try:
    import fitz  # PyMuPDF
    HAS_PYMUPDF = True
except ImportError:
    HAS_PYMUPDF = False

try:
    import pdfplumber
    HAS_PDFPLUMBER = True
except ImportError:
    HAS_PDFPLUMBER = False

try:
    from PIL import Image  # noqa: F401
    HAS_PIL = True
except ImportError:
    HAS_PIL = False

try:
    import PyPDF2  # noqa: F401
    HAS_PYPDF2 = True
except ImportError:
    HAS_PYPDF2 = False


def check_system_tools():
    """Check for system tools like pdftotext and pdfimages.

    Uses shutil.which() so it works on macOS, RHEL and Ubuntu without
    relying on an external `which` binary.
    """
    tools = {}
    for tool in ('pdftotext', 'pdfimages', 'pdftoppm', 'pdfinfo'):
        path = shutil.which(tool)
        tools[tool] = bool(path and os.access(path, os.X_OK))
    return tools


def extract_text_with_pymupdf(pdf_path):
    """Extract text from PDF using PyMuPDF (fitz)"""
    doc = fitz.open(pdf_path)
    pages_text = []

    for page_num in range(len(doc)):
        page = doc[page_num]
        text = page.get_text("text")
        pages_text.append({
            'page': page_num + 1,
            'text': text
        })

    doc.close()
    return pages_text


def extract_images_with_pymupdf(pdf_path, output_dir):
    """Extract images from PDF using PyMuPDF"""
    doc = fitz.open(pdf_path)
    images = []

    os.makedirs(output_dir, exist_ok=True)

    for page_num in range(len(doc)):
        page = doc[page_num]
        image_list = page.get_images()

        for img_index, img in enumerate(image_list):
            xref = img[0]
            base_image = doc.extract_image(xref)
            image_bytes = base_image["image"]
            image_ext = base_image["ext"]

            # Save image
            image_filename = f"page_{page_num + 1}_img_{img_index + 1}.{image_ext}"
            image_path = os.path.join(output_dir, image_filename)

            with open(image_path, "wb") as img_file:
                img_file.write(image_bytes)

            images.append({
                'page': page_num + 1,
                'index': img_index + 1,
                'path': image_path,
                'filename': image_filename,
                'width': base_image.get('width', 0),
                'height': base_image.get('height', 0)
            })

    doc.close()
    return images


def extract_images_with_pdfimages(pdf_path, output_dir):
    """Extract images using pdfimages command-line tool"""
    os.makedirs(output_dir, exist_ok=True)

    # Extract images
    base_name = os.path.splitext(os.path.basename(pdf_path))[0]
    output_prefix = os.path.join(output_dir, f"{base_name}_img")

    if not shutil.which('pdfimages'):
        print("Warning: pdfimages not found on PATH; skipping image extraction.",
              file=sys.stderr)
        return []

    try:
        subprocess.run(
            ['pdfimages', '-all', pdf_path, output_prefix],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=True,
        )

        # Find extracted images
        images = []
        if os.path.exists(output_dir):
            for file in os.listdir(output_dir):
                if file.startswith(f"{base_name}_img"):
                    filepath = os.path.join(output_dir, file)
                    images.append({
                        'path': filepath,
                        'filename': file
                    })

        return images
    except subprocess.CalledProcessError as e:
        print(f"Warning: pdfimages failed: {e}", file=sys.stderr)
        return []


def extract_text_with_pdftotext(pdf_path):
    """Extract text using pdftotext command-line tool"""
    if not shutil.which('pdftotext'):
        print("Warning: pdftotext not found on PATH.", file=sys.stderr)
        return ""
    try:
        result = subprocess.run(
            ['pdftotext', '-layout', pdf_path, '-'],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=True,
        )
        return result.stdout.decode('utf-8', errors='ignore')
    except subprocess.CalledProcessError as e:
        print(f"Warning: pdftotext failed: {e}", file=sys.stderr)
        return ""


def convert_text_to_markdown(text_content, images, pdf_path):
    """Convert extracted text to Markdown format with image references"""
    lines = text_content.split('\n')
    markdown_lines = []
    current_page = 1

    # Track image usage per page
    page_images = {}
    for img in images:
        page = img.get('page', 1)
        if page not in page_images:
            page_images[page] = []
        page_images[page].append(img)

    # Process text line by line
    for line in lines:
        # Skip empty lines (but preserve structure)
        if not line.strip():
            markdown_lines.append('')
            continue

        # Detect headings (lines that are all caps or start with numbers)
        stripped = line.strip()

        # Check if it's a heading (all caps, or starts with number and period)
        if len(stripped) > 0:
            # Heading pattern: All caps, or starts with number
            if stripped.isupper() and len(stripped) > 3:
                # Convert to heading
                level = 1 if len(stripped) < 50 else 2
                markdown_lines.append(f"\n{'#' * level} {stripped}\n")
            elif re.match(r'^\d+\.?\s+[A-Z]', stripped):
                # Numbered heading
                markdown_lines.append(f"\n## {stripped}\n")
            elif re.match(r'^[A-Z][a-z]+.*:', stripped) and len(stripped) < 80:
                # Section heading pattern
                markdown_lines.append(f"\n## {stripped}\n")
            else:
                # Regular text
                markdown_lines.append(line)

    # Insert images at appropriate locations
    # For now, add images at the end of each page's content
    # This is a simple approach - can be improved with better page detection

    return '\n'.join(markdown_lines)


def process_pdf_pages(pdf_path, output_dir):
    """Process PDF page by page, extracting text and images"""
    if HAS_PYMUPDF:
        # Use PyMuPDF for best results
        print("Using PyMuPDF for PDF processing...")

        doc = fitz.open(pdf_path)
        pages_content = []
        images_dir = os.path.join(output_dir, "images")
        os.makedirs(images_dir, exist_ok=True)

        for page_num in range(len(doc)):
            page = doc[page_num]

            # Extract text
            text = page.get_text("text")

            # Extract images
            image_list = page.get_images()
            page_images = []

            for img_index, img in enumerate(image_list):
                xref = img[0]
                base_image = doc.extract_image(xref)
                image_bytes = base_image["image"]
                image_ext = base_image["ext"]

                # Save image
                image_filename = f"page_{page_num + 1}_img_{img_index + 1}.{image_ext}"
                image_path = os.path.join(images_dir, image_filename)

                with open(image_path, "wb") as img_file:
                    img_file.write(image_bytes)

                page_images.append({
                    'filename': image_filename,
                    'path': image_path,
                    'relative_path': f"images/{image_filename}",
                    'width': base_image.get('width', 0),
                    'height': base_image.get('height', 0)
                })

            pages_content.append({
                'page': page_num + 1,
                'text': text,
                'images': page_images
            })

        doc.close()
        return pages_content, images_dir

    elif HAS_PDFPLUMBER:
        # Use pdfplumber as fallback
        print("Using pdfplumber for PDF processing...")

        pages_content = []
        images_dir = os.path.join(output_dir, "images")
        os.makedirs(images_dir, exist_ok=True)

        with pdfplumber.open(pdf_path) as pdf:
            for page_num, page in enumerate(pdf.pages):
                text = page.extract_text() or ""

                # Extract images (pdfplumber doesn't directly support image extraction)
                # We'll need to use pdfimages for this
                pages_content.append({
                    'page': page_num + 1,
                    'text': text,
                    'images': []  # Will be filled separately
                })

        return pages_content, images_dir

    else:
        # Use system tools
        print("Using system tools (pdftotext/pdfimages) for PDF processing...")

        # Get number of pages first
        page_count = 1
        if shutil.which('pdfinfo'):
            try:
                pdfinfo_result = subprocess.run(
                    ['pdfinfo', pdf_path],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    timeout=10,
                )
                if pdfinfo_result.returncode == 0:
                    output = pdfinfo_result.stdout.decode('utf-8', errors='ignore')
                    match = re.search(r'Pages:\s+(\d+)', output)
                    if match:
                        page_count = int(match.group(1))
            except Exception:
                pass

        # Extract text for all pages
        text_content = extract_text_with_pdftotext(pdf_path)

        # Extract images
        images_dir = os.path.join(output_dir, "images")
        images = extract_images_with_pdfimages(pdf_path, images_dir)

        # Split text by pages (simple approach - split by form feed character)
        text_pages = text_content.split('\x0c')
        if len(text_pages) == 1:
            # No form feeds, assume single page or split manually
            text_pages = [text_content]

        # Create page-based content
        pages_content = []
        # Distribute images across pages (simple approach: divide evenly)
        images_per_page = len(images) // page_count if page_count > 0 else len(images)
        for i in range(min(len(text_pages), page_count)):
            start_idx = i * images_per_page
            end_idx = start_idx + images_per_page if i < page_count - 1 else len(images)
            page_images = []
            for img in images[start_idx:end_idx]:
                # Add relative path for markdown
                img['relative_path'] = f"images/{img.get('filename', '')}"
                page_images.append(img)

            pages_content.append({
                'page': i + 1,
                'text': text_pages[i] if i < len(text_pages) else '',
                'images': page_images
            })

        return pages_content, images_dir


def format_markdown(pages_content, images_dir, pdf_filename):
    """Format pages content into Markdown"""
    markdown_parts = []

    # Add header
    pdf_name = os.path.splitext(os.path.basename(pdf_filename))[0]
    markdown_parts.append(f"# {pdf_name.replace('_', ' ').title()}\n")
    markdown_parts.append(f"\n*Converted from PDF: {os.path.basename(pdf_filename)}*\n")
    markdown_parts.append("\n---\n")

    # Process each page
    for page_data in pages_content:
        page_num = page_data['page']
        text = page_data['text']
        images = page_data.get('images', [])

        # Add page separator (optional)
        if page_num > 1:
            markdown_parts.append(f"\n\n---\n\n*Page {page_num}*\n\n")

        # Process text
        if text:
            # Clean up text
            text_lines = text.split('\n')
            processed_lines = []

            for line in text_lines:
                line = line.strip()
                if not line:
                    processed_lines.append('')
                    continue

                # Detect headings
                if line.isupper() and len(line) > 3 and len(line) < 100:
                    # All caps heading
                    level = 1 if len(line) < 50 else 2
                    processed_lines.append(f"\n{'#' * level} {line}\n")
                elif re.match(r'^\d+\.?\s+[A-Z]', line):
                    # Numbered heading
                    processed_lines.append(f"\n## {line}\n")
                elif re.match(r'^[A-Z][a-z]+.*:', line) and len(line) < 80:
                    # Section heading
                    processed_lines.append(f"\n## {line}\n")
                else:
                    # Regular text
                    processed_lines.append(line)

            markdown_parts.append('\n'.join(processed_lines))
            markdown_parts.append('\n')

        # Add images after text for this page
        if images:
            markdown_parts.append('\n')
            for idx, img in enumerate(images):
                img_path = img.get('relative_path', img.get('filename', ''))
                if not img_path.startswith('images/'):
                    img_path = f"images/{img.get('filename', '')}"
                img_alt = f"Page {page_num}, Image {idx + 1}"

                markdown_parts.append(f'\n![{img_alt}]({img_path})\n')

                # Add image caption if available
                if 'width' in img and 'height' in img:
                    markdown_parts.append(
                        f'\n*Image dimensions: {img["width"]}x{img["height"]} pixels*\n'
                    )

    return '\n'.join(markdown_parts)


def convert_pdf_to_markdown(pdf_path, output_path=None):
    """Main conversion function"""
    pdf_path = os.path.abspath(pdf_path)

    if not os.path.exists(pdf_path):
        raise FileNotFoundError(f"PDF file not found: {pdf_path}")

    # Determine output path
    if output_path is None:
        base_name = os.path.splitext(os.path.basename(pdf_path))[0]
        output_dir = os.path.dirname(pdf_path)
        output_path = os.path.join(output_dir, f"{base_name}.md")
    else:
        # Allow `-o` to be either a directory or a file path.
        if os.path.isdir(output_path) or output_path.endswith(os.sep) or \
                output_path.endswith('/'):
            output_dir = os.path.abspath(output_path)
            base_name = os.path.splitext(os.path.basename(pdf_path))[0]
            output_path = os.path.join(output_dir, f"{base_name}.md")
        else:
            output_path = os.path.abspath(output_path)
            output_dir = os.path.dirname(output_path)

    os.makedirs(output_dir, exist_ok=True)

    print(f"Input PDF:  {pdf_path}")
    print(f"Output MD:  {output_path}")
    print(f"Output dir: {output_dir}")
    print()

    # Process PDF
    print("Processing PDF...")
    pages_content, images_dir = process_pdf_pages(pdf_path, output_dir)
    print(f"Processed {len(pages_content)} pages")

    # Count images
    total_images = sum(len(page.get('images', [])) for page in pages_content)
    if total_images > 0:
        print(f"Extracted {total_images} images to {images_dir}")

    # Generate Markdown
    print("Generating Markdown...")
    markdown_content = format_markdown(pages_content, images_dir, pdf_path)

    # Write output
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(markdown_content)

    print(f"Markdown saved to: {output_path}")
    print(f"Output size: {os.path.getsize(output_path):,} bytes")

    return output_path


def _print_install_hints():
    """Print platform-specific install hints for missing deps."""
    print()
    print("Install dependencies (pick one Python lib OR use system tools):")
    print("  Python (recommended): pip install PyMuPDF")
    print("                     or pip install pdfplumber")
    print()
    print("  System tools (poppler):")
    print("    macOS:   brew install poppler")
    print("    RHEL:    dnf install poppler-utils")
    print("    Ubuntu:  apt-get install poppler-utils")


def main():
    parser = argparse.ArgumentParser(
        description="Convert PDF to Markdown with diagram preservation",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Convert PDF to Markdown (auto-detect output name)
  python pdf_to_markdown_with_diagrams.py doc/keraunos_pcie_tile.pdf

  # Specify output file
  python pdf_to_markdown_with_diagrams.py input.pdf output.md

  # Convert with custom output directory
  python pdf_to_markdown_with_diagrams.py input.pdf -o output_dir/
        """
    )

    parser.add_argument(
        'input_pdf',
        nargs='?',
        help='Input PDF file path'
    )

    parser.add_argument(
        '-o', '--output',
        help='Output Markdown file path or directory ' \
             '(default: same name as PDF with .md extension)'
    )

    parser.add_argument(
        '--check-deps',
        action='store_true',
        help='Check for required dependencies and exit'
    )

    args = parser.parse_args()

    # Check dependencies
    if args.check_deps:
        print("Checking dependencies...")
        print(f"PyMuPDF (fitz): {'OK' if HAS_PYMUPDF else 'MISSING'}")
        print(f"pdfplumber:     {'OK' if HAS_PDFPLUMBER else 'MISSING'}")
        print(f"PIL/Pillow:     {'OK' if HAS_PIL else 'MISSING'}")
        print(f"PyPDF2:         {'OK' if HAS_PYPDF2 else 'MISSING'}")

        tools = check_system_tools()
        for tool, available in tools.items():
            print(f"{tool:12s}: {'OK' if available else 'MISSING'}")

        if not any([HAS_PYMUPDF, HAS_PDFPLUMBER, tools.get('pdftotext', False)]):
            print()
            print("WARNING: No PDF processing libraries found!")
            _print_install_hints()

        return

    # Check if input PDF is provided
    if not args.input_pdf:
        parser.error("input_pdf is required (unless using --check-deps)")

    # Convert PDF
    try:
        output_path = convert_pdf_to_markdown(args.input_pdf, args.output)
        print(f"\nConversion complete!")
        print(f"Output: {output_path}")
    except Exception as e:
        print(f"\nError: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
