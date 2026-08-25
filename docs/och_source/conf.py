# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
# Configuration file for the Sphinx documentation builder.

project = "OCH Architecture"
copyright = "2026, Tenstorrent"
author = "OCH Architecture Team"
release = "1.0"
version = "1.0"

extensions = [
    "myst_parser",
    "sphinxcontrib.mermaid",
]

myst_enable_extensions = [
    "colon_fence",
    "deflist",
    "fieldlist",
    "html_admonition",
    "html_image",
    "replacements",
    "smartquotes",
    "substitution",
    "tasklist",
]

source_suffix = {
    ".rst": "restructuredtext",
    ".md": "markdown",
}

master_doc = "index"
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]

mermaid_version = "10.6.1"
mermaid_output_format = "raw"
mermaid_init_js = """
mermaid.initialize({
    startOnLoad: true,
    theme: 'default',
    securityLevel: 'loose',
    flowchart: {
        useMaxWidth: true,
        htmlLabels: true,
        curve: 'basis',
        nodeSpacing: 70,
        rankSpacing: 70,
        padding: 20
    },
    fontSize: 16,
    themeVariables: {
        fontSize: '16px',
        primaryColor: '#e1f5ff',
        primaryTextColor: '#000',
        primaryBorderColor: '#3498db',
        lineColor: '#2c3e50',
        secondaryColor: '#fff4e1',
        tertiaryColor: '#ffe1f5'
    }
});
"""

html_theme = "alabaster"
html_theme_options = {
    "description": "OCH architecture and implementation reference",
    "github_button": False,
    "fixed_sidebar": True,
    "sidebar_width": "260px",
    "page_width": "1250px",
}

html_static_path = ["_static"]
html_css_files = ["custom.css"]
html_title = f"{project} v{release}"
html_short_title = project

html_sidebars = {
    "**": [
        "about.html",
        "navigation.html",
        "relations.html",
        "searchbox.html",
    ]
}

latex_documents = [
    (master_doc, "OCHArchitecture.tex", "OCH Architecture Documentation", author, "manual"),
]
