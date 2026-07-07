OCH Architecture Documentation
==============================

**Architecture and implementation reference - Version 1.0**

This site documents the OCH/OCAH chiplet management architecture using the
same publishing toolchain as the Keraunos PCIe Tile documentation:
Sphinx, MyST Markdown, Mermaid diagrams, and GitHub Pages.

Overview
--------

OCH is the Open Chiplet Harness architecture for reusable chiplet management
infrastructure. The implementation centers on the System Management Unit (SMU),
which composes debug/test access, system management, secure boot, lifecycle,
firmware, and system-management-network attachment into a reusable chiplet
integration block.

Documentation Structure
-----------------------

.. toctree::
   :maxdepth: 2
   :caption: Architecture

   architecture
   implementation
   integration

.. toctree::
   :maxdepth: 2
   :caption: Project Reference

   verification
   roadmap

Quick Links
-----------

* :ref:`genindex`
* :ref:`search`

Document Information
--------------------

:Version: 1.0
:Date: July 2026
:Status: Initial published architecture reference
:Source basis: Local OCH RTL and AsciiDoc documentation under ``tt-oca-hw-main``
