#!/usr/bin/env python3
"""Generate the three professional PLIC block-diagram SVGs.

Run from any cwd; writes <here>/0{1,2,3}_*.svg next to this script.
Using a Python writer (rather than direct file editing) guarantees
clean UTF-8 byte output for every glyph (arrows, set-theory symbols, etc.).
"""
from __future__ import annotations
import pathlib

HERE = pathlib.Path(__file__).resolve().parent

# ----------------------------------------------------------------------
# 1. Top-level PLIC block diagram
# ----------------------------------------------------------------------
BLOCK_DIAGRAM = r"""<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 980 600" font-family="Helvetica, Arial, sans-serif">
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
  </defs>

  <text x="490" y="30" font-size="18" font-weight="700" fill="#0D47A1" text-anchor="middle">SMC PLIC \u2014 Top-Level Block Diagram</text>

  <!-- Source labels -->
  <text x="80" y="80"  font-size="11" font-weight="600" fill="#37474F" text-anchor="middle">Interrupt sources</text>
  <text x="80" y="95"  font-size="9.5" fill="#546E7A" text-anchor="middle">(level, active-high)</text>

  <text x="20"  y="135" font-family="ui-monospace, SFMono-Regular, Menlo, monospace" font-size="11">src_in[0]</text>
  <line x1="100" y1="131" x2="200" y2="131" stroke="#37474F" stroke-width="1.4" marker-end="url(#arrow)"/>

  <text x="20"  y="165" font-family="ui-monospace, SFMono-Regular, Menlo, monospace" font-size="11">src_in[1]</text>
  <line x1="100" y1="161" x2="200" y2="161" stroke="#37474F" stroke-width="1.4" marker-end="url(#arrow)"/>

  <text x="20"  y="195" font-family="ui-monospace, SFMono-Regular, Menlo, monospace" font-size="11">src_in[2]</text>
  <line x1="100" y1="191" x2="200" y2="191" stroke="#37474F" stroke-width="1.4" marker-end="url(#arrow)"/>

  <text x="55"  y="225" font-style="italic" font-size="13" fill="#90A4AE">\u22EE</text>

  <text x="20"  y="255" font-family="ui-monospace, SFMono-Regular, Menlo, monospace" font-size="11">src_in[N-1]</text>
  <line x1="100" y1="251" x2="200" y2="251" stroke="#37474F" stroke-width="1.4" marker-end="url(#arrow)"/>

  <!-- Gateway -->
  <g filter="url(#boxshadow)">
    <rect x="200" y="110" width="170" height="170" rx="8" ry="8"
          fill="url(#hwGrad)" stroke="#1565C0" stroke-width="2"/>
  </g>
  <text x="285" y="140" font-size="14" font-weight="700" fill="#0D47A1" text-anchor="middle">Gateway</text>
  <text x="285" y="160" font-size="11" fill="#1565C0" text-anchor="middle">+ Edge Latch</text>
  <line x1="225" y1="175" x2="345" y2="175" stroke="#90CAF9" stroke-width="1"/>
  <text x="285" y="198" font-size="10.5" fill="#1A237E" text-anchor="middle">rising edge sets</text>
  <text x="285" y="214" font-size="10.5" fill="#1A237E" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">pending[s]</text>
  <text x="285" y="240" font-size="10" fill="#37474F" text-anchor="middle">in-flight latch</text>
  <text x="285" y="256" font-size="10" fill="#37474F" text-anchor="middle">suppresses re-pend</text>

  <!-- gateway -> arbiter -->
  <line x1="370" y1="195" x2="430" y2="195" stroke="#37474F" stroke-width="1.6" marker-end="url(#arrow)"/>
  <text x="400" y="186" font-size="9.5" fill="#37474F" text-anchor="middle">pending[]</text>

  <!-- Arbiter -->
  <g filter="url(#boxshadow)">
    <rect x="430" y="110" width="200" height="170" rx="8" ry="8"
          fill="url(#hwGrad)" stroke="#1565C0" stroke-width="2"/>
  </g>
  <text x="530" y="140" font-size="14" font-weight="700" fill="#0D47A1" text-anchor="middle">Per-context Arbiter</text>
  <text x="530" y="158" font-size="10.5" fill="#1565C0" text-anchor="middle">(one path per context)</text>
  <line x1="450" y1="170" x2="610" y2="170" stroke="#90CAF9" stroke-width="1"/>
  <text x="530" y="190" font-size="10.5" fill="#1A237E" text-anchor="middle">filter:  enable[c] \u2227 pending</text>
  <text x="530" y="208" font-size="10.5" fill="#1A237E" text-anchor="middle">priority &gt; threshold[c]</text>
  <text x="530" y="232" font-size="10.5" fill="#1A237E" text-anchor="middle">argmax (priority, \u2212id)</text>
  <text x="530" y="250" font-size="10.5" fill="#37474F" text-anchor="middle">tie-break: lowest source ID</text>

  <!-- Outputs -->
  <text x="900" y="80"  font-size="11" font-weight="600" fill="#37474F" text-anchor="middle">Context outputs</text>
  <text x="900" y="95"  font-size="9.5" fill="#546E7A" text-anchor="middle">(active-high)</text>
"""

# Build per-output rows in Python so we don't repeat ourselves
def output_row(y, idx, label):
    # Keep \u2192 literal here so the unicode_escape pass below resolves
    # it together with all the other escapes in the raw template strings.
    return (
        f'  <line x1="630" y1="{y}" x2="800" y2="{y}" stroke="#37474F" stroke-width="1.4" marker-end="url(#arrow)"/>\n'
        f'  <text x="810" y="{y+4}" font-family="ui-monospace, SFMono-Regular, Menlo, monospace" font-size="11">ctx_out[{idx}]</text>\n'
        f'  <text x="900" y="{y+4}" font-size="10" fill="#546E7A">\\u2192 {label}</text>\n'
    )

ROWS = "".join(
    output_row(y, idx, label)
    for y, idx, label in [
        (120, 0, "MEIP[0]"),
        (140, 1, "MEIP[1]"),
        (160, 2, "MEIP[2]"),
        (180, 3, "MEIP[3]"),
        (210, 4, "SEIP[0]"),
        (230, 5, "SEIP[1]"),
        (250, 6, "SEIP[2]"),
        (270, 7, "SEIP[3]"),
    ]
)

BLOCK_DIAGRAM += ROWS + r"""
  <!-- Register file -->
  <g filter="url(#boxshadow)">
    <rect x="200" y="340" width="430" height="100" rx="8" ry="8"
          fill="url(#regGrad)" stroke="#7B1FA2" stroke-width="2"/>
  </g>
  <text x="415" y="368" font-size="14" font-weight="700" fill="#4A148C" text-anchor="middle">Register File</text>
  <line x1="225" y1="380" x2="610" y2="380" stroke="#CE93D8" stroke-width="1"/>
  <text x="270" y="402" font-size="11" fill="#4A148C" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">PRIORITY[s]</text>
  <text x="370" y="402" font-size="11" fill="#4A148C" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">ENABLE[c][s]</text>
  <text x="470" y="402" font-size="11" fill="#4A148C" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">THRESHOLD[c]</text>
  <text x="570" y="402" font-size="11" fill="#4A148C" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">PENDING[s]</text>
  <text x="415" y="425" font-size="10" fill="#6A1B9A" text-anchor="middle">+ CLAIM/COMPLETE per context</text>

  <!-- regfile -> gateway / arbiter (state inputs) -->
  <path d="M 285,340 L 285,290" stroke="#7B1FA2" stroke-width="1.4" stroke-dasharray="4 3" fill="none" marker-end="url(#arrowSmall)"/>
  <path d="M 530,340 L 530,290" stroke="#7B1FA2" stroke-width="1.4" stroke-dasharray="4 3" fill="none" marker-end="url(#arrowSmall)"/>
  <text x="297" y="328" font-size="9" fill="#6A1B9A" text-anchor="start">in-flight</text>
  <text x="540" y="328" font-size="9" fill="#6A1B9A" text-anchor="start">priority/enable/threshold</text>

  <!-- TLM socket -->
  <g filter="url(#boxshadow)">
    <rect x="200" y="490" width="430" height="80" rx="8" ry="8"
          fill="url(#busGrad)" stroke="#EF6C00" stroke-width="2"/>
  </g>
  <text x="415" y="518" font-size="14" font-weight="700" fill="#E65100" text-anchor="middle">reg_socket  (TLM-2.0 target)</text>
  <text x="415" y="538" font-size="11" fill="#BF360C" text-anchor="middle">AXI4-Lite, 32-bit, 4-byte aligned   \u00B7   4 MB window</text>
  <text x="415" y="556" font-size="10" fill="#E65100" text-anchor="middle">b_transport / transport_dbg   (DMI never granted)</text>

  <!-- bus <-> register file (bidirectional) -->
  <line x1="415" y1="490" x2="415" y2="440" stroke="#EF6C00" stroke-width="1.6"/>
  <polygon points="415,440 411,448 419,448" fill="#EF6C00"/>
  <polygon points="415,490 411,482 419,482" fill="#EF6C00"/>

  <!-- Reset -->
  <line x1="700" y1="530" x2="850" y2="530" stroke="#37474F" stroke-width="1.4" marker-end="url(#arrow)"/>
  <text x="700" y="522" font-family="ui-monospace, SFMono-Regular, Menlo, monospace" font-size="11" fill="#37474F">rst_n_i</text>
  <text x="700" y="548" font-size="9.5" fill="#546E7A">(active-low, sync)</text>
  <line x1="775" y1="530" x2="775" y2="445" stroke="#37474F" stroke-width="1.4" stroke-dasharray="3 2"/>

  <!-- Legend -->
  <g transform="translate(20,520)" font-size="10" fill="#37474F">
    <rect x="0"   y="0"  width="14" height="10" fill="url(#hwGrad)"  stroke="#1565C0"/>
    <text x="20"  y="9">datapath / arbiter</text>
    <rect x="0"   y="18" width="14" height="10" fill="url(#regGrad)" stroke="#7B1FA2"/>
    <text x="20"  y="27">register file (state)</text>
    <rect x="0"   y="36" width="14" height="10" fill="url(#busGrad)" stroke="#EF6C00"/>
    <text x="20"  y="45">bus interface</text>
  </g>
</svg>
"""

# ----------------------------------------------------------------------
# 2. Pipeline view
# ----------------------------------------------------------------------
PIPELINE = r"""<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 980 460" font-family="Helvetica, Arial, sans-serif">
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

  <text x="490" y="32" font-size="18" font-weight="700" fill="#0D47A1" text-anchor="middle">Interrupt Lifecycle \u2014 5-Stage Pipeline</text>
  <text x="490" y="52" font-size="11" fill="#546E7A" text-anchor="middle">Stages 1\u20134 are inside the PLIC.   Stage 5 is software (ISR / driver).</text>

  <rect x="20"  y="80"  width="760" height="280" rx="6" ry="6" fill="#F5FAFE" stroke="#90CAF9" stroke-dasharray="4 3" stroke-width="1.2"/>
  <text x="40"  y="100" font-size="11" font-weight="700" fill="#1565C0">PLIC (hardware / SystemC model)</text>

  <rect x="800" y="80"  width="160" height="280" rx="6" ry="6" fill="#FFF8F0" stroke="#FFB74D" stroke-dasharray="4 3" stroke-width="1.2"/>
  <text x="820" y="100" font-size="11" font-weight="700" fill="#E65100">Software (CPU)</text>

  <!-- Stage 1: Detect -->
  <g filter="url(#shadow)">
    <rect x="40" y="130" width="160" height="170" rx="8" ry="8" fill="url(#hw)" stroke="#1565C0" stroke-width="2"/>
  </g>
  <circle cx="62" cy="152" r="14" fill="#1565C0"/>
  <text x="62" y="157" font-size="14" font-weight="700" fill="white" text-anchor="middle">1</text>
  <text x="120" y="156" font-size="14" font-weight="700" fill="#0D47A1" text-anchor="middle">Detect</text>
  <line x1="55" y1="172" x2="185" y2="172" stroke="#90CAF9"/>
  <text x="120" y="195" font-size="11" font-weight="600" fill="#1A237E" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">src_in[i] : 0 \u2192 1</text>
  <text x="120" y="220" font-size="10" fill="#37474F" text-anchor="middle">rising-edge gateway</text>
  <text x="120" y="244" font-size="10" fill="#37474F" text-anchor="middle">drops the edge if the</text>
  <text x="120" y="258" font-size="10" fill="#37474F" text-anchor="middle">source is in-flight</text>
  <text x="120" y="284" font-size="9" fill="#546E7A" text-anchor="middle" font-style="italic">(level \u2192 message)</text>

  <line x1="200" y1="215" x2="240" y2="215" stroke="#37474F" stroke-width="1.6" marker-end="url(#arrow)"/>

  <!-- Stage 2: Latch -->
  <g filter="url(#shadow)">
    <rect x="240" y="130" width="160" height="170" rx="8" ry="8" fill="url(#hw)" stroke="#1565C0" stroke-width="2"/>
  </g>
  <circle cx="262" cy="152" r="14" fill="#1565C0"/>
  <text x="262" y="157" font-size="14" font-weight="700" fill="white" text-anchor="middle">2</text>
  <text x="320" y="156" font-size="14" font-weight="700" fill="#0D47A1" text-anchor="middle">Latch</text>
  <line x1="255" y1="172" x2="385" y2="172" stroke="#90CAF9"/>
  <text x="320" y="195" font-size="11" font-weight="600" fill="#1A237E" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">pending[s] = 1</text>
  <text x="320" y="220" font-size="10" fill="#37474F" text-anchor="middle">latched until claim;</text>
  <text x="320" y="234" font-size="10" fill="#37474F" text-anchor="middle">survives a falling</text>
  <text x="320" y="248" font-size="10" fill="#37474F" text-anchor="middle">edge on the line</text>
  <text x="320" y="284" font-size="9" fill="#546E7A" text-anchor="middle" font-style="italic">(no host action yet)</text>

  <line x1="400" y1="215" x2="440" y2="215" stroke="#37474F" stroke-width="1.6" marker-end="url(#arrow)"/>

  <!-- Stage 3: Arbitrate -->
  <g filter="url(#shadow)">
    <rect x="440" y="130" width="160" height="170" rx="8" ry="8" fill="url(#hw)" stroke="#1565C0" stroke-width="2"/>
  </g>
  <circle cx="462" cy="152" r="14" fill="#1565C0"/>
  <text x="462" y="157" font-size="14" font-weight="700" fill="white" text-anchor="middle">3</text>
  <text x="520" y="156" font-size="14" font-weight="700" fill="#0D47A1" text-anchor="middle">Arbitrate</text>
  <line x1="455" y1="172" x2="585" y2="172" stroke="#90CAF9"/>
  <text x="520" y="192" font-size="10" fill="#37474F" text-anchor="middle">for each context c :</text>
  <text x="520" y="208" font-size="10" fill="#37474F" text-anchor="middle">eligible(c) =  pending</text>
  <text x="520" y="222" font-size="10" fill="#37474F" text-anchor="middle">  \u2227 enable[c] \u2227 priority &gt; thr[c]</text>
  <text x="520" y="248" font-size="10" fill="#37474F" text-anchor="middle">winner =  argmax</text>
  <text x="520" y="262" font-size="10" fill="#37474F" text-anchor="middle">(priority, lowest id)</text>
  <text x="520" y="285" font-size="9" fill="#546E7A" text-anchor="middle" font-style="italic">(re-runs only on change)</text>

  <line x1="600" y1="215" x2="640" y2="215" stroke="#37474F" stroke-width="1.6" marker-end="url(#arrow)"/>

  <!-- Stage 4: Deliver -->
  <g filter="url(#shadow)">
    <rect x="640" y="130" width="140" height="170" rx="8" ry="8" fill="url(#hw)" stroke="#1565C0" stroke-width="2"/>
  </g>
  <circle cx="662" cy="152" r="14" fill="#1565C0"/>
  <text x="662" y="157" font-size="14" font-weight="700" fill="white" text-anchor="middle">4</text>
  <text x="715" y="156" font-size="14" font-weight="700" fill="#0D47A1" text-anchor="middle">Deliver</text>
  <line x1="655" y1="172" x2="775" y2="172" stroke="#90CAF9"/>
  <text x="710" y="195" font-size="11" font-weight="600" fill="#1A237E" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">ctx_out[c] = 1</text>
  <text x="710" y="220" font-size="10" fill="#37474F" text-anchor="middle">drives MEIP / SEIP</text>
  <text x="710" y="234" font-size="10" fill="#37474F" text-anchor="middle">into target hart's</text>
  <text x="710" y="248" font-size="10" fill="#37474F" text-anchor="middle">trap-handler input</text>
  <text x="710" y="284" font-size="9" fill="#546E7A" text-anchor="middle" font-style="italic">(crosses HW/SW boundary)</text>

  <line x1="780" y1="215" x2="820" y2="215" stroke="#37474F" stroke-width="1.6" marker-end="url(#arrow)"/>

  <!-- Stage 5: Service -->
  <g filter="url(#shadow)">
    <rect x="820" y="130" width="140" height="170" rx="8" ry="8" fill="url(#sw)" stroke="#E65100" stroke-width="2"/>
  </g>
  <circle cx="842" cy="152" r="14" fill="#E65100"/>
  <text x="842" y="157" font-size="14" font-weight="700" fill="white" text-anchor="middle">5</text>
  <text x="893" y="156" font-size="14" font-weight="700" fill="#BF360C" text-anchor="middle">Service</text>
  <line x1="835" y1="172" x2="950" y2="172" stroke="#FFB74D"/>
  <text x="890" y="194" font-size="10" fill="#37474F" text-anchor="middle">CPU enters trap;</text>
  <text x="890" y="208" font-size="10" fill="#37474F" text-anchor="middle">reads CLAIM/COMPLETE</text>
  <text x="890" y="222" font-size="10" fill="#37474F" text-anchor="middle">\u2192 atomic claim</text>
  <text x="890" y="244" font-size="10" fill="#37474F" text-anchor="middle">runs ISR for s,</text>
  <text x="890" y="258" font-size="10" fill="#37474F" text-anchor="middle">writes s back</text>
  <text x="890" y="272" font-size="10" fill="#37474F" text-anchor="middle">\u2192 complete (EOI)</text>

  <!-- Re-arm feedback path -->
  <path d="M 890,300 C 890,400 380,400 320,300" stroke="#E65100" stroke-width="1.6" stroke-dasharray="5 4" fill="none" marker-end="url(#arrowOrange)"/>
  <text x="600" y="395" font-size="10.5" fill="#BF360C" text-anchor="middle" font-style="italic">complete + line still high  \u2192  re-latches pending[s]  (cycle repeats)</text>

  <!-- Legend -->
  <g transform="translate(20,420)" font-size="11" fill="#37474F">
    <rect x="0" y="0" width="16" height="11" fill="url(#hw)" stroke="#1565C0"/>
    <text x="22" y="10">PLIC stage</text>
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
# 3. State machine
# ----------------------------------------------------------------------
STATE_MACHINE = r"""<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 760 600" font-family="Helvetica, Arial, sans-serif">
  <defs>
    <marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse">
      <path d="M0,0 L10,5 L0,10 z" fill="#37474F"/>
    </marker>
    <marker id="arrowGreen" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse">
      <path d="M0,0 L10,5 L0,10 z" fill="#2E7D32"/>
    </marker>
    <marker id="arrowBlue"  viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse">
      <path d="M0,0 L10,5 L0,10 z" fill="#1565C0"/>
    </marker>
    <marker id="arrowRed"   viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
      <path d="M0,0 L10,5 L0,10 z" fill="#C62828"/>
    </marker>
    <filter id="shadow" x="-20%" y="-20%" width="140%" height="140%">
      <feDropShadow dx="0" dy="2" stdDeviation="2" flood-color="#000" flood-opacity="0.15"/>
    </filter>
    <linearGradient id="idle" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="#E8F5E9"/><stop offset="100%" stop-color="#C8E6C9"/>
    </linearGradient>
    <linearGradient id="pending" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="#FFF3E0"/><stop offset="100%" stop-color="#FFCC80"/>
    </linearGradient>
    <linearGradient id="inflight" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="#FFEBEE"/><stop offset="100%" stop-color="#FFCDD2"/>
    </linearGradient>
    <linearGradient id="diamond" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="#F3E5F5"/><stop offset="100%" stop-color="#E1BEE7"/>
    </linearGradient>
  </defs>

  <text x="380" y="32" font-size="18" font-weight="700" fill="#0D47A1" text-anchor="middle">Per-Source State Machine</text>
  <text x="380" y="52" font-size="11" fill="#546E7A" text-anchor="middle">State per source ID s = (pending[s], in_flight[s])</text>

  <circle cx="380" cy="80" r="6" fill="#37474F"/>
  <line x1="380" y1="86" x2="380" y2="115" stroke="#37474F" stroke-width="1.6" marker-end="url(#arrow)"/>
  <text x="395" y="105" font-size="10.5" fill="#546E7A" font-style="italic">on reset</text>

  <!-- IDLE -->
  <g filter="url(#shadow)">
    <rect x="270" y="120" width="220" height="80" rx="40" ry="40" fill="url(#idle)" stroke="#2E7D32" stroke-width="2.5"/>
  </g>
  <text x="380" y="150" font-size="17" font-weight="700" fill="#1B5E20" text-anchor="middle">IDLE</text>
  <text x="380" y="172" font-size="11" fill="#37474F" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">pending = 0,  in_flight = 0</text>
  <text x="380" y="190" font-size="10.5" fill="#546E7A" text-anchor="middle" font-style="italic">no event captured</text>

  <line x1="380" y1="200" x2="380" y2="250" stroke="#37474F" stroke-width="1.8" marker-end="url(#arrow)"/>
  <rect x="395" y="212" width="200" height="34" rx="3" ry="3" fill="white" stroke="#37474F" stroke-width="0.8"/>
  <text x="402" y="225" font-size="10.5" fill="#37474F">src_in[s] :  0 \u2192 1</text>
  <text x="402" y="240" font-size="10" fill="#546E7A" font-style="italic">(gateway latches edge)</text>

  <!-- PENDING -->
  <g filter="url(#shadow)">
    <rect x="270" y="255" width="220" height="80" rx="40" ry="40" fill="url(#pending)" stroke="#E65100" stroke-width="2.5"/>
  </g>
  <text x="380" y="285" font-size="17" font-weight="700" fill="#BF360C" text-anchor="middle">PENDING</text>
  <text x="380" y="307" font-size="11" fill="#37474F" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">pending = 1,  in_flight = 0</text>
  <text x="380" y="325" font-size="10.5" fill="#546E7A" text-anchor="middle" font-style="italic">awaiting claim from any context</text>

  <line x1="380" y1="335" x2="380" y2="385" stroke="#37474F" stroke-width="1.8" marker-end="url(#arrow)"/>
  <rect x="395" y="347" width="220" height="34" rx="3" ry="3" fill="white" stroke="#37474F" stroke-width="0.8"/>
  <text x="402" y="360" font-size="10.5" fill="#37474F">read CLAIM/COMPLETE[c]</text>
  <text x="402" y="374" font-size="10" fill="#546E7A" font-style="italic">\u2192 claim returns s, clears pending</text>

  <!-- IN_FLIGHT -->
  <g filter="url(#shadow)">
    <rect x="270" y="390" width="220" height="80" rx="40" ry="40" fill="url(#inflight)" stroke="#C62828" stroke-width="2.5"/>
  </g>
  <text x="380" y="420" font-size="17" font-weight="700" fill="#B71C1C" text-anchor="middle">IN_FLIGHT</text>
  <text x="380" y="442" font-size="11" fill="#37474F" text-anchor="middle" font-family="ui-monospace, SFMono-Regular, Menlo, monospace">pending = 0,  in_flight = 1</text>
  <text x="380" y="460" font-size="10.5" fill="#546E7A" text-anchor="middle" font-style="italic">ISR running; rising edges suppressed</text>

  <!-- self-loop annotation: rising edges suppressed -->
  <path d="M 270,440 C 200,455 200,420 245,425" stroke="#C62828" stroke-width="1.4" stroke-dasharray="4 3" fill="none" marker-end="url(#arrowRed)"/>
  <text x="100" y="430" font-size="10" fill="#C62828">src_in rising edge</text>
  <text x="100" y="444" font-size="10" fill="#C62828" font-style="italic">\u2192 DROPPED</text>
  <text x="100" y="458" font-size="10" fill="#C62828" font-style="italic">(in-flight protection)</text>

  <line x1="380" y1="470" x2="380" y2="490" stroke="#37474F" stroke-width="1.8"/>
  <text x="395" y="486" font-size="10.5" fill="#37474F">write s to CLAIM/COMPLETE  (complete)</text>

  <!-- DECISION DIAMOND -->
  <g filter="url(#shadow)">
    <polygon points="380,490 540,520 380,550 220,520" fill="url(#diamond)" stroke="#7B1FA2" stroke-width="2"/>
  </g>
  <text x="380" y="517" font-size="13" font-weight="700" fill="#4A148C" text-anchor="middle">src_in[s] still high?</text>
  <text x="380" y="535" font-size="10" fill="#6A1B9A" text-anchor="middle">(level recheck at complete)</text>

  <!-- DIAMOND -> PENDING (yes) -->
  <path d="M 540,520 C 660,520 670,400 540,295" stroke="#2E7D32" stroke-width="1.8" fill="none" marker-end="url(#arrowGreen)"/>
  <rect x="610" y="395" width="80" height="40" rx="3" ry="3" fill="white" stroke="#2E7D32" stroke-width="0.8"/>
  <text x="650" y="412" font-size="11" font-weight="700" fill="#2E7D32" text-anchor="middle">YES</text>
  <text x="650" y="427" font-size="10" fill="#2E7D32" text-anchor="middle">re-latch</text>

  <!-- DIAMOND -> IDLE (no) -->
  <path d="M 220,520 C 100,520 80,180 270,160" stroke="#1565C0" stroke-width="1.8" fill="none" marker-end="url(#arrowBlue)"/>
  <rect x="80" y="240" width="80" height="40" rx="3" ry="3" fill="white" stroke="#1565C0" stroke-width="0.8"/>
  <text x="120" y="257" font-size="11" font-weight="700" fill="#1565C0" text-anchor="middle">NO</text>
  <text x="120" y="272" font-size="10" fill="#1565C0" text-anchor="middle">return to idle</text>

  <!-- Legend -->
  <g transform="translate(540,80)" font-size="10.5" fill="#37474F">
    <text x="0"  y="0"  font-weight="700" fill="#0D47A1">Legend</text>
    <rect x="0"  y="14" width="14" height="10" rx="5" ry="5" fill="url(#idle)"     stroke="#2E7D32"/>
    <text x="22" y="23">stable state</text>
    <rect x="0"  y="32" width="14" height="10" rx="5" ry="5" fill="url(#pending)"  stroke="#E65100"/>
    <text x="22" y="41">visible to software</text>
    <rect x="0"  y="50" width="14" height="10" rx="5" ry="5" fill="url(#inflight)" stroke="#C62828"/>
    <text x="22" y="59">claim \u2192 complete window</text>
    <polygon points="0,72 7,68 14,72 7,76" fill="url(#diamond)" stroke="#7B1FA2"/>
    <text x="22" y="77">decision</text>
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
