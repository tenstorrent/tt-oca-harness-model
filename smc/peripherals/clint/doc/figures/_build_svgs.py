#!/usr/bin/env python3
"""Generate the three professional CLINT block-diagram SVGs.

Run from any cwd; writes <here>/0{1,2,3}_*.svg next to this script.
Using a Python writer (rather than direct file editing) guarantees
clean UTF-8 byte output for every glyph (arrows, set-theory symbols, etc.).

The strings below are r-strings to keep the raw \\u escape readable in
source.  We then run them through encode("ascii").decode("unicode_escape")
so the output file contains the *real* UTF-8 bytes for every Unicode
escape (\\u2192 → →, \\u2265 → ≥, etc.).
"""
from __future__ import annotations
import pathlib

HERE = pathlib.Path(__file__).resolve().parent

# ----------------------------------------------------------------------
# 1. Top-level CLINT block diagram
# ----------------------------------------------------------------------
BLOCK_DIAGRAM = r"""<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1000 620" font-family="Helvetica, Arial, sans-serif">
  <defs>
    <marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
      <path d="M0,0 L10,5 L0,10 z" fill="#37474F"/>
    </marker>
    <marker id="arrowSmall" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
      <path d="M0,0 L10,5 L0,10 z" fill="#546E7A"/>
    </marker>
    <filter id="boxshadow" x="-20%" y="-20%" width="140%" height="140%">
      <feDropShadow dx="0" dy="2" stdDeviation="2" flood-color="#000" flood-opacity="0.12"/>
    </filter>
    <linearGradient id="hwGrad" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="#E3F2FD"/>
      <stop offset="100%" stop-color="#BBDEFB"/>
    </linearGradient>
    <linearGradient id="regGrad" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="#F3E5F5"/>
      <stop offset="100%" stop-color="#E1BEE7"/>
    </linearGradient>
    <linearGradient id="busGrad" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="#FFF8E1"/>
      <stop offset="100%" stop-color="#FFECB3"/>
    </linearGradient>
    <linearGradient id="tickGrad" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="#E8F5E9"/>
      <stop offset="100%" stop-color="#C8E6C9"/>
    </linearGradient>
  </defs>

  <text x="500" y="32" font-size="18" font-weight="700" fill="#0D47A1" text-anchor="middle">SMC CLINT \u2014 Top-Level Block Diagram</text>
  <text x="500" y="52" font-size="11" fill="#546E7A" text-anchor="middle">Per-hart MSIP / MTIMECMP, global MTIME, level outputs to mip.MSIP / mip.MTIP</text>

  <!-- Tick generator -->
  <g filter="url(#boxshadow)">
    <rect x="40" y="100" width="170" height="80" rx="8" ry="8"
          fill="url(#tickGrad)" stroke="#2E7D32" stroke-width="2"/>
  </g>
  <text x="125" y="128" font-size="13" font-weight="700" fill="#1B5E20" text-anchor="middle">Tick generator</text>
  <text x="125" y="146" font-size="10.5" fill="#1B5E20" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">tick_event_</text>
  <text x="125" y="162" font-size="10" fill="#37474F" text-anchor="middle">period = tick_period_ns</text>
  <text x="125" y="174" font-size="9.5" fill="#546E7A" text-anchor="middle" font-style="italic">(0 disables auto-tick)</text>

  <!-- Tick -> MTIME -->
  <line x1="210" y1="140" x2="265" y2="140" stroke="#37474F" stroke-width="1.6" marker-end="url(#arrow)"/>
  <text x="237" y="132" font-size="9.5" fill="#546E7A" text-anchor="middle">+1</text>

  <!-- MTIME register -->
  <g filter="url(#boxshadow)">
    <rect x="265" y="100" width="200" height="80" rx="8" ry="8"
          fill="url(#regGrad)" stroke="#7B1FA2" stroke-width="2"/>
  </g>
  <text x="365" y="128" font-size="14" font-weight="700" fill="#4A148C" text-anchor="middle">MTIME</text>
  <text x="365" y="148" font-size="11" fill="#4A148C" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">uint64_t  @0xBFF8</text>
  <text x="365" y="170" font-size="10" fill="#6A1B9A" text-anchor="middle">global free-running counter</text>

  <!-- MTIME -> compare lanes -->
  <line x1="465" y1="140" x2="530" y2="140" stroke="#37474F" stroke-width="1.6" marker-end="url(#arrow)"/>

  <!-- Per-hart compare bus (vertical bus from MTIME to all comparators) -->
  <line x1="530" y1="140" x2="530" y2="500" stroke="#37474F" stroke-width="1.6"/>
  <text x="540" y="156" font-size="10" fill="#546E7A">MTIME bus (read-only)</text>

  <!-- MTIMECMP[h] register column -->
  <g filter="url(#boxshadow)">
    <rect x="265" y="220" width="200" height="270" rx="8" ry="8"
          fill="url(#regGrad)" stroke="#7B1FA2" stroke-width="2"/>
  </g>
  <text x="365" y="246" font-size="14" font-weight="700" fill="#4A148C" text-anchor="middle">MTIMECMP[h]</text>
  <text x="365" y="264" font-size="10.5" fill="#4A148C" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">uint64_t  @0x4000+8h</text>
  <line x1="285" y1="278" x2="445" y2="278" stroke="#CE93D8"/>
  <text x="365" y="298" font-size="11" fill="#37474F" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">MTIMECMP[0]</text>
  <text x="365" y="318" font-size="11" fill="#37474F" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">MTIMECMP[1]</text>
  <text x="365" y="338" font-size="11" fill="#37474F" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">MTIMECMP[2]</text>
  <text x="365" y="358" font-size="11" fill="#37474F" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">MTIMECMP[3]</text>
  <text x="365" y="378" font-size="13" fill="#90A4AE" text-anchor="middle" font-style="italic">\u22EE</text>
  <text x="365" y="402" font-size="11" fill="#37474F" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">MTIMECMP[N\u22121]</text>
  <text x="365" y="438" font-size="10" fill="#6A1B9A" text-anchor="middle">per-hart 64-bit comparator</text>
  <text x="365" y="468" font-size="9.5" fill="#7B1FA2" text-anchor="middle" font-style="italic">reset value: 0xFFFF\u2026F</text>

  <!-- MTIMECMP -> compare logic -->
  <line x1="465" y1="355" x2="530" y2="355" stroke="#37474F" stroke-width="1.4"/>

  <!-- Compare logic block -->
  <g filter="url(#boxshadow)">
    <rect x="540" y="220" width="180" height="270" rx="8" ry="8"
          fill="url(#hwGrad)" stroke="#1565C0" stroke-width="2"/>
  </g>
  <text x="630" y="246" font-size="14" font-weight="700" fill="#0D47A1" text-anchor="middle">Compare logic</text>
  <text x="630" y="262" font-size="10.5" fill="#1565C0" text-anchor="middle" font-style="italic">(per hart)</text>
  <line x1="555" y1="275" x2="705" y2="275" stroke="#90CAF9"/>
  <text x="630" y="300" font-size="11" fill="#1A237E" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">mtip_o[h] =</text>
  <text x="630" y="320" font-size="11" fill="#1A237E" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">(MTIME \u2265 MTIMECMP[h])</text>
  <text x="630" y="350" font-size="10.5" fill="#37474F" text-anchor="middle">level, unsigned</text>
  <text x="630" y="368" font-size="10.5" fill="#37474F" text-anchor="middle">recomputed on any</text>
  <text x="630" y="382" font-size="10.5" fill="#37474F" text-anchor="middle">MTIME / MTIMECMP write</text>
  <line x1="555" y1="408" x2="705" y2="408" stroke="#90CAF9"/>
  <text x="630" y="432" font-size="10.5" fill="#37474F" text-anchor="middle">single-driver:</text>
  <text x="630" y="448" font-size="10.5" fill="#37474F" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">output_method</text>
  <text x="630" y="466" font-size="10" fill="#546E7A" text-anchor="middle" font-style="italic">posts on recompute_event_</text>

  <!-- Compare -> outputs (4 hart MTIP lines) -->
  <line x1="720" y1="245" x2="820" y2="245" stroke="#37474F" stroke-width="1.4" marker-end="url(#arrow)"/>
  <text x="830" y="249" font-family="ui-monospace, SFMono-Regular, Menlo, monospace" font-size="11">mtip_o[0]</text>
  <text x="922" y="249" font-size="10" fill="#546E7A">\u2192 mip.MTIP[0]</text>

  <line x1="720" y1="275" x2="820" y2="275" stroke="#37474F" stroke-width="1.4" marker-end="url(#arrow)"/>
  <text x="830" y="279" font-family="ui-monospace, SFMono-Regular, Menlo, monospace" font-size="11">mtip_o[1]</text>
  <text x="922" y="279" font-size="10" fill="#546E7A">\u2192 mip.MTIP[1]</text>

  <line x1="720" y1="305" x2="820" y2="305" stroke="#37474F" stroke-width="1.4" marker-end="url(#arrow)"/>
  <text x="830" y="309" font-family="ui-monospace, SFMono-Regular, Menlo, monospace" font-size="11">mtip_o[2]</text>
  <text x="922" y="309" font-size="10" fill="#546E7A">\u2192 mip.MTIP[2]</text>

  <line x1="720" y1="335" x2="820" y2="335" stroke="#37474F" stroke-width="1.4" marker-end="url(#arrow)"/>
  <text x="830" y="339" font-family="ui-monospace, SFMono-Regular, Menlo, monospace" font-size="11">mtip_o[3]</text>
  <text x="922" y="339" font-size="10" fill="#546E7A">\u2192 mip.MTIP[3]</text>

  <line x1="720" y1="395" x2="820" y2="395" stroke="#37474F" stroke-width="1.4" marker-end="url(#arrow)"/>
  <text x="830" y="399" font-family="ui-monospace, SFMono-Regular, Menlo, monospace" font-size="11">msip_o[0]</text>
  <text x="922" y="399" font-size="10" fill="#546E7A">\u2192 mip.MSIP[0]</text>

  <line x1="720" y1="425" x2="820" y2="425" stroke="#37474F" stroke-width="1.4" marker-end="url(#arrow)"/>
  <text x="830" y="429" font-family="ui-monospace, SFMono-Regular, Menlo, monospace" font-size="11">msip_o[1]</text>
  <text x="922" y="429" font-size="10" fill="#546E7A">\u2192 mip.MSIP[1]</text>

  <line x1="720" y1="455" x2="820" y2="455" stroke="#37474F" stroke-width="1.4" marker-end="url(#arrow)"/>
  <text x="830" y="459" font-family="ui-monospace, SFMono-Regular, Menlo, monospace" font-size="11">msip_o[2]</text>
  <text x="922" y="459" font-size="10" fill="#546E7A">\u2192 mip.MSIP[2]</text>

  <line x1="720" y1="485" x2="820" y2="485" stroke="#37474F" stroke-width="1.4" marker-end="url(#arrow)"/>
  <text x="830" y="489" font-family="ui-monospace, SFMono-Regular, Menlo, monospace" font-size="11">msip_o[3]</text>
  <text x="922" y="489" font-size="10" fill="#546E7A">\u2192 mip.MSIP[3]</text>

  <!-- MSIP register block -->
  <g filter="url(#boxshadow)">
    <rect x="40" y="380" width="200" height="110" rx="8" ry="8"
          fill="url(#regGrad)" stroke="#7B1FA2" stroke-width="2"/>
  </g>
  <text x="140" y="406" font-size="14" font-weight="700" fill="#4A148C" text-anchor="middle">MSIP[h]</text>
  <text x="140" y="424" font-size="10.5" fill="#4A148C" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">uint8_t  @0x0+4h</text>
  <line x1="60" y1="436" x2="220" y2="436" stroke="#CE93D8"/>
  <text x="140" y="456" font-size="10.5" fill="#37474F" text-anchor="middle">bit[0] = IPI flag</text>
  <text x="140" y="472" font-size="10.5" fill="#37474F" text-anchor="middle" font-style="italic">bits[31:1] RAZ/WI</text>

  <!-- MSIP[h] feeds compare/output method directly -->
  <path d="M 240,435 C 380,435 460,440 540,440" stroke="#7B1FA2" stroke-width="1.4" stroke-dasharray="4 3" fill="none" marker-end="url(#arrowSmall)"/>
  <text x="310" y="427" font-size="9.5" fill="#7B1FA2">MSIP[h] \u2192 msip_o[h]</text>

  <!-- TLM socket -->
  <g filter="url(#boxshadow)">
    <rect x="265" y="520" width="455" height="80" rx="8" ry="8"
          fill="url(#busGrad)" stroke="#EF6C00" stroke-width="2"/>
  </g>
  <text x="492" y="548" font-size="14" font-weight="700" fill="#E65100" text-anchor="middle">reg_socket  (TLM-2.0 target)</text>
  <text x="492" y="568" font-size="11" fill="#BF360C" text-anchor="middle">AXI4-Lite, 32-bit and 64-bit accesses, naturally aligned   \u00B7   64 KiB window</text>
  <text x="492" y="586" font-size="10" fill="#E65100" text-anchor="middle">b_transport / transport_dbg   (DMI never granted)</text>

  <!-- Bus connections to MTIME, MTIMECMP, MSIP register banks -->
  <line x1="365" y1="180" x2="365" y2="218" stroke="#EF6C00" stroke-width="1.4" stroke-dasharray="3 2"/>
  <line x1="365" y1="490" x2="365" y2="520" stroke="#EF6C00" stroke-width="1.4" stroke-dasharray="3 2"/>
  <line x1="140" y1="490" x2="140" y2="540" stroke="#EF6C00" stroke-width="1.4" stroke-dasharray="3 2"/>
  <line x1="140" y1="540" x2="265" y2="540" stroke="#EF6C00" stroke-width="1.4" stroke-dasharray="3 2"/>

  <!-- Reset -->
  <line x1="845" y1="585" x2="965" y2="585" stroke="#37474F" stroke-width="1.4" marker-end="url(#arrow)"/>
  <text x="845" y="577" font-family="ui-monospace, SFMono-Regular, Menlo, monospace" font-size="11" fill="#37474F">rst_n_i</text>
  <text x="845" y="603" font-size="9.5" fill="#546E7A">(active-low, sync)</text>

  <!-- Legend -->
  <g transform="translate(40,545)" font-size="10" fill="#37474F">
    <rect x="0"   y="0"  width="14" height="10" fill="url(#tickGrad)" stroke="#2E7D32"/>
    <text x="20"  y="9">tick generator</text>
    <rect x="0"   y="18" width="14" height="10" fill="url(#hwGrad)"   stroke="#1565C0"/>
    <text x="20"  y="27">compare / output</text>
    <rect x="0"   y="36" width="14" height="10" fill="url(#regGrad)"  stroke="#7B1FA2"/>
    <text x="20"  y="45">register file (state)</text>
    <rect x="125"   y="0" width="14" height="10" fill="url(#busGrad)" stroke="#EF6C00"/>
    <text x="145"  y="9">bus interface</text>
    <line x1="125" y1="22" x2="139" y2="22" stroke="#37474F" stroke-width="1.4" marker-end="url(#arrow)"/>
    <text x="145" y="27">data flow</text>
    <line x1="125" y1="40" x2="139" y2="40" stroke="#7B1FA2" stroke-width="1.4" stroke-dasharray="4 3"/>
    <text x="145" y="45">state read</text>
  </g>
</svg>
"""

# ----------------------------------------------------------------------
# 2. Pipeline view (4-stage interrupt lifecycle)
# ----------------------------------------------------------------------
PIPELINE = r"""<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1000 460" font-family="Helvetica, Arial, sans-serif">
  <defs>
    <marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse">
      <path d="M0,0 L10,5 L0,10 z" fill="#37474F"/>
    </marker>
    <marker id="arrowOrange" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse">
      <path d="M0,0 L10,5 L0,10 z" fill="#E65100"/>
    </marker>
    <filter id="shadow" x="-20%" y="-20%" width="140%" height="140%">
      <feDropShadow dx="0" dy="2" stdDeviation="2" flood-color="#000" flood-opacity="0.12"/>
    </filter>
    <linearGradient id="hw" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="#E3F2FD"/>
      <stop offset="100%" stop-color="#BBDEFB"/>
    </linearGradient>
    <linearGradient id="sw" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="#FFF3E0"/>
      <stop offset="100%" stop-color="#FFCC80"/>
    </linearGradient>
  </defs>

  <text x="500" y="32" font-size="18" font-weight="700" fill="#0D47A1" text-anchor="middle">CLINT Interrupt Lifecycle \u2014 4-Stage Pipeline</text>
  <text x="500" y="52" font-size="11" fill="#546E7A" text-anchor="middle">Stages 1\u20133 are inside the CLINT.   Stage 4 is software (trap handler / ISR).</text>

  <rect x="20"  y="80"  width="780" height="280" rx="6" ry="6" fill="#F5FAFE" stroke="#90CAF9" stroke-dasharray="4 3" stroke-width="1.2"/>
  <text x="40"  y="100" font-size="11" font-weight="700" fill="#1565C0">CLINT (hardware / SystemC model)</text>

  <rect x="820" y="80"  width="160" height="280" rx="6" ry="6" fill="#FFF8F0" stroke="#FFB74D" stroke-dasharray="4 3" stroke-width="1.2"/>
  <text x="840" y="100" font-size="11" font-weight="700" fill="#E65100">Software (CPU)</text>

  <!-- Stage 1: Tick / Write -->
  <g filter="url(#shadow)">
    <rect x="40" y="130" width="220" height="200" rx="8" ry="8" fill="url(#hw)" stroke="#1565C0" stroke-width="2"/>
  </g>
  <circle cx="62" cy="152" r="14" fill="#1565C0"/>
  <text x="62" y="157" font-size="14" font-weight="700" fill="white" text-anchor="middle">1</text>
  <text x="170" y="156" font-size="14" font-weight="700" fill="#0D47A1" text-anchor="middle">Tick / Write</text>
  <line x1="55" y1="172" x2="245" y2="172" stroke="#90CAF9"/>
  <text x="150" y="195" font-size="11" font-weight="600" fill="#1A237E" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">MTIME += 1</text>
  <text x="150" y="212" font-size="10" fill="#546E7A" text-anchor="middle" font-style="italic">every tick_period_ns</text>
  <text x="150" y="240" font-size="11" font-weight="600" fill="#1A237E" text-anchor="middle">\u2014 or \u2014</text>
  <text x="150" y="262" font-size="11" font-weight="600" fill="#1A237E" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">MSIP[h] \u2190 1</text>
  <text x="150" y="278" font-size="11" font-weight="600" fill="#1A237E" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">MTIMECMP[h] \u2190 v</text>
  <text x="150" y="306" font-size="9.5" fill="#546E7A" text-anchor="middle" font-style="italic">(state mutation)</text>

  <line x1="260" y1="230" x2="300" y2="230" stroke="#37474F" stroke-width="1.6" marker-end="url(#arrow)"/>

  <!-- Stage 2: Compare / Latch -->
  <g filter="url(#shadow)">
    <rect x="300" y="130" width="220" height="200" rx="8" ry="8" fill="url(#hw)" stroke="#1565C0" stroke-width="2"/>
  </g>
  <circle cx="322" cy="152" r="14" fill="#1565C0"/>
  <text x="322" y="157" font-size="14" font-weight="700" fill="white" text-anchor="middle">2</text>
  <text x="420" y="156" font-size="14" font-weight="700" fill="#0D47A1" text-anchor="middle">Compare / Latch</text>
  <line x1="315" y1="172" x2="505" y2="172" stroke="#90CAF9"/>
  <text x="410" y="200" font-size="11" font-weight="600" fill="#1A237E" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">recompute_event_</text>
  <text x="410" y="218" font-size="10" fill="#37474F" text-anchor="middle">posted at SC_ZERO_TIME</text>
  <text x="410" y="248" font-size="11" font-weight="600" fill="#1A237E" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">mtip_lvl = MTIME \u2265 MTIMECMP[h]</text>
  <text x="410" y="266" font-size="11" font-weight="600" fill="#1A237E" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">msip_lvl = MSIP[h].bit[0]</text>
  <text x="410" y="294" font-size="9.5" fill="#546E7A" text-anchor="middle" font-style="italic">(per-hart, idempotent)</text>

  <line x1="520" y1="230" x2="560" y2="230" stroke="#37474F" stroke-width="1.6" marker-end="url(#arrow)"/>

  <!-- Stage 3: Drive line -->
  <g filter="url(#shadow)">
    <rect x="560" y="130" width="220" height="200" rx="8" ry="8" fill="url(#hw)" stroke="#1565C0" stroke-width="2"/>
  </g>
  <circle cx="582" cy="152" r="14" fill="#1565C0"/>
  <text x="582" y="157" font-size="14" font-weight="700" fill="white" text-anchor="middle">3</text>
  <text x="680" y="156" font-size="14" font-weight="700" fill="#0D47A1" text-anchor="middle">Drive line</text>
  <line x1="575" y1="172" x2="765" y2="172" stroke="#90CAF9"/>
  <text x="670" y="195" font-size="11" font-weight="600" fill="#1A237E" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">mtip_o[h] \u2190 mtip_lvl</text>
  <text x="670" y="213" font-size="11" font-weight="600" fill="#1A237E" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">msip_o[h] \u2190 msip_lvl</text>
  <text x="670" y="240" font-size="10" fill="#37474F" text-anchor="middle">single-driver:</text>
  <text x="670" y="254" font-size="10" fill="#37474F" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">output_method</text>
  <text x="670" y="278" font-size="10" fill="#37474F" text-anchor="middle">writes only on level change</text>
  <text x="670" y="306" font-size="9.5" fill="#546E7A" text-anchor="middle" font-style="italic">(crosses HW/SW boundary)</text>

  <line x1="780" y1="230" x2="820" y2="230" stroke="#37474F" stroke-width="1.6" marker-end="url(#arrow)"/>

  <!-- Stage 4: Trap & service -->
  <g filter="url(#shadow)">
    <rect x="820" y="130" width="160" height="200" rx="8" ry="8" fill="url(#sw)" stroke="#E65100" stroke-width="2"/>
  </g>
  <circle cx="842" cy="152" r="14" fill="#E65100"/>
  <text x="842" y="157" font-size="14" font-weight="700" fill="white" text-anchor="middle">4</text>
  <text x="900" y="156" font-size="14" font-weight="700" fill="#BF360C" text-anchor="middle">Trap &amp; service</text>
  <line x1="835" y1="172" x2="970" y2="172" stroke="#FFB74D"/>
  <text x="900" y="194" font-size="10" fill="#37474F" text-anchor="middle">mip.MTIP / mip.MSIP</text>
  <text x="900" y="208" font-size="10" fill="#37474F" text-anchor="middle">causes machine trap</text>
  <text x="900" y="232" font-size="10" fill="#37474F" text-anchor="middle">ISR runs:</text>
  <text x="900" y="248" font-size="10" fill="#37474F" text-anchor="middle">  MTIMECMP[h] \u2190 next</text>
  <text x="900" y="262" font-size="10" fill="#37474F" text-anchor="middle">  or  MSIP[h] \u2190 0</text>
  <text x="900" y="290" font-size="10" fill="#37474F" text-anchor="middle">trap returns</text>
  <text x="900" y="312" font-size="9.5" fill="#546E7A" text-anchor="middle" font-style="italic">(level falls automatically)</text>

  <!-- Re-arm feedback path -->
  <path d="M 900,330 C 900,415 360,415 240,330" stroke="#E65100" stroke-width="1.6" stroke-dasharray="5 4" fill="none" marker-end="url(#arrowOrange)"/>
  <text x="570" y="410" font-size="10.5" fill="#BF360C" text-anchor="middle" font-style="italic">software write to MTIMECMP[h] / MSIP[h]  \u2192  level recomputed  (cycle repeats)</text>

  <!-- Legend -->
  <g transform="translate(20,425)" font-size="11" fill="#37474F">
    <rect x="0" y="0" width="16" height="11" fill="url(#hw)" stroke="#1565C0"/>
    <text x="22" y="10">CLINT stage</text>
    <rect x="120" y="0" width="16" height="11" fill="url(#sw)" stroke="#E65100"/>
    <text x="142" y="10">software stage</text>
    <line x1="270" y1="6" x2="295" y2="6" stroke="#37474F" stroke-width="1.6" marker-end="url(#arrow)"/>
    <text x="300" y="10">data flow</text>
    <line x1="370" y1="6" x2="395" y2="6" stroke="#E65100" stroke-width="1.6" stroke-dasharray="5 4" marker-end="url(#arrowOrange)"/>
    <text x="400" y="10">re-arm path</text>
  </g>
</svg>
"""

# ----------------------------------------------------------------------
# 3. State machine — per-hart CLINT line (MSIP or MTIP)
# ----------------------------------------------------------------------
STATE_MACHINE = r"""<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 760 540" font-family="Helvetica, Arial, sans-serif">
  <defs>
    <marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse">
      <path d="M0,0 L10,5 L0,10 z" fill="#37474F"/>
    </marker>
    <marker id="arrowGreen" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse">
      <path d="M0,0 L10,5 L0,10 z" fill="#2E7D32"/>
    </marker>
    <marker id="arrowOrange" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse">
      <path d="M0,0 L10,5 L0,10 z" fill="#E65100"/>
    </marker>
    <filter id="shadow" x="-20%" y="-20%" width="140%" height="140%">
      <feDropShadow dx="0" dy="2" stdDeviation="2" flood-color="#000" flood-opacity="0.15"/>
    </filter>
    <linearGradient id="idle" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="#E8F5E9"/><stop offset="100%" stop-color="#C8E6C9"/>
    </linearGradient>
    <linearGradient id="asserted" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="#FFEBEE"/><stop offset="100%" stop-color="#FFCDD2"/>
    </linearGradient>
  </defs>

  <text x="380" y="32" font-size="18" font-weight="700" fill="#0D47A1" text-anchor="middle">Per-Hart CLINT Line State Machine</text>
  <text x="380" y="52" font-size="11" fill="#546E7A" text-anchor="middle">Same state machine for both MSIP[h] and MTIP[h] outputs (level-driven, no in-flight latch)</text>

  <circle cx="380" cy="80" r="6" fill="#37474F"/>
  <line x1="380" y1="86" x2="380" y2="115" stroke="#37474F" stroke-width="1.6" marker-end="url(#arrow)"/>
  <text x="395" y="105" font-size="10.5" fill="#546E7A" font-style="italic">on reset</text>

  <!-- IDLE -->
  <g filter="url(#shadow)">
    <rect x="240" y="120" width="280" height="100" rx="50" ry="50" fill="url(#idle)" stroke="#2E7D32" stroke-width="2.5"/>
  </g>
  <text x="380" y="155" font-size="18" font-weight="700" fill="#1B5E20" text-anchor="middle">IDLE</text>
  <text x="380" y="180" font-size="11" fill="#37474F" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">msip_o[h] = 0  /  mtip_o[h] = 0</text>
  <text x="380" y="200" font-size="10.5" fill="#546E7A" text-anchor="middle" font-style="italic">cause cleared; line low; CPU not interrupted</text>

  <!-- IDLE -> ASSERTED -->
  <line x1="380" y1="220" x2="380" y2="305" stroke="#37474F" stroke-width="1.8" marker-end="url(#arrow)"/>
  <rect x="395" y="232" width="320" height="60" rx="3" ry="3" fill="white" stroke="#37474F" stroke-width="0.8"/>
  <text x="402" y="248" font-size="10.5" fill="#37474F" font-weight="700">MSIP path:</text>
  <text x="402" y="263" font-size="10.5" fill="#37474F" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">  write MSIP[h] = 1   (incl. cross-hart IPI)</text>
  <text x="402" y="278" font-size="10.5" fill="#37474F" font-weight="700">MTIP path:</text>
  <text x="402" y="293" font-size="10.5" fill="#37474F" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">  MTIME tick crosses MTIMECMP[h]</text>

  <!-- ASSERTED -->
  <g filter="url(#shadow)">
    <rect x="240" y="310" width="280" height="100" rx="50" ry="50" fill="url(#asserted)" stroke="#C62828" stroke-width="2.5"/>
  </g>
  <text x="380" y="345" font-size="18" font-weight="700" fill="#B71C1C" text-anchor="middle">ASSERTED</text>
  <text x="380" y="370" font-size="11" fill="#37474F" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">msip_o[h] = 1  /  mtip_o[h] = 1</text>
  <text x="380" y="390" font-size="10.5" fill="#546E7A" text-anchor="middle" font-style="italic">CPU traps; ISR begins (level stays high until cleared)</text>

  <!-- ASSERTED -> IDLE -->
  <path d="M 240,360 C 60,360 60,170 240,170" stroke="#E65100" stroke-width="1.8" fill="none" marker-end="url(#arrowOrange)"/>
  <rect x="60" y="240" width="180" height="60" rx="3" ry="3" fill="white" stroke="#E65100" stroke-width="0.8"/>
  <text x="68" y="256" font-size="10.5" fill="#BF360C" font-weight="700">MSIP path:</text>
  <text x="68" y="271" font-size="10.5" fill="#37474F" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">  write MSIP[h] = 0</text>
  <text x="68" y="285" font-size="10.5" fill="#BF360C" font-weight="700">MTIP path:</text>
  <text x="68" y="299" font-size="10.5" fill="#37474F" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">  MTIMECMP[h] &gt; MTIME</text>

  <!-- Self-loop on ASSERTED: tick/write that does not change the level -->
  <path d="M 520,360 C 700,360 700,360 522,360" stroke="#37474F" stroke-width="1.4" stroke-dasharray="4 3" fill="none"/>
  <text x="610" y="345" font-size="10" fill="#546E7A" text-anchor="middle" font-style="italic">tick or write that</text>
  <text x="610" y="358" font-size="10" fill="#546E7A" text-anchor="middle" font-style="italic">leaves level unchanged</text>
  <text x="610" y="371" font-size="10" fill="#546E7A" text-anchor="middle" font-style="italic">(idempotent; cache short-circuits)</text>

  <!-- Notes -->
  <g transform="translate(60,440)" font-size="10.5" fill="#37474F">
    <text x="0" y="0" font-weight="700" fill="#0D47A1">Properties</text>
    <text x="0" y="18">\u2022  Pure level: no edge latch, no in-flight state. Two consecutive timer interrupts</text>
    <text x="0" y="32">    cannot be queued \u2014 firmware must reprogram MTIMECMP each ISR.</text>
    <text x="0" y="50">\u2022  Per-hart isolation: MSIP, MTIMECMP, MTIME-vs-MTIMECMP all decoupled across harts.</text>
    <text x="0" y="68">\u2022  Idempotence: writes / ticks that don't flip the level cost only a delta-cycle</text>
    <text x="0" y="82">    recompute; the cache vectors prevent redundant sc_signal updates.</text>
  </g>

  <!-- Legend -->
  <g transform="translate(560,80)" font-size="10.5" fill="#37474F">
    <text x="0"  y="0"  font-weight="700" fill="#0D47A1">Legend</text>
    <rect x="0"  y="14" width="14" height="10" rx="5" ry="5" fill="url(#idle)"     stroke="#2E7D32"/>
    <text x="22" y="23">stable / line low</text>
    <rect x="0"  y="32" width="14" height="10" rx="5" ry="5" fill="url(#asserted)" stroke="#C62828"/>
    <text x="22" y="41">line high; CPU traps</text>
    <line x1="0" y1="58" x2="14" y2="58" stroke="#37474F" stroke-width="1.6" marker-end="url(#arrow)"/>
    <text x="22" y="62">assertion edge</text>
    <line x1="0" y1="76" x2="14" y2="76" stroke="#E65100" stroke-width="1.6" marker-end="url(#arrowOrange)"/>
    <text x="22" y="80">software clears cause</text>
  </g>
</svg>
"""


def main() -> None:
    files = {
        "01_block_diagram.svg": BLOCK_DIAGRAM,
        "02_pipeline.svg":      PIPELINE,
        "03_state_machine.svg": STATE_MACHINE,
    }
    for name, content in files.items():
        # The strings above use Python escape sequences (\u2192 etc.) so we
        # need to interpret them — they are r-strings to keep the raw \u
        # escape readable in source, then encoded once here.
        text = content.encode("ascii").decode("unicode_escape")
        path = HERE / name
        path.write_text(text, encoding="utf-8")
        print(f"wrote {path}  ({path.stat().st_size:,} bytes)")


if __name__ == "__main__":
    main()
