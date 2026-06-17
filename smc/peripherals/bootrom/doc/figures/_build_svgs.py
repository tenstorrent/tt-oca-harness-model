#!/usr/bin/env python3
"""Generate the Boot ROM function-call-flow SVG.

Run from any cwd; writes <here>/call_flow.svg next to this script.
Using a Python writer (rather than direct file editing) guarantees clean
UTF-8 byte output and keeps the figure reproducible / version-controlled
even though the generated *.svg is a git-ignored build artifact.
"""
from __future__ import annotations
import pathlib

HERE = pathlib.Path(__file__).resolve().parent

CALL_FLOW = r"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 920 600" font-family="Helvetica, Arial, sans-serif">
  <defs>
    <marker id="a" markerWidth="10" markerHeight="10" refX="8" refY="3" orient="auto">
      <path d="M0,0 L9,3 L0,6 Z" fill="#555"/>
    </marker>
  </defs>
  <style>
    .trig rect { fill:#fce8cd; stroke:#c47f1a; }
    .fn   rect { fill:#d8e6ff; stroke:#2b6cb0; }
    .hlp  rect { fill:#eceff1; stroke:#607d8b; }
    .cont rect { fill:none; stroke:#b6c0cc; stroke-dasharray:6,4; stroke-width:1.4; }
    rect { rx:7; stroke-width:1.5; }
    .t   { font-size:12.5px; fill:#10243e; text-anchor:middle; }
    .code{ font-family:"DejaVu Sans Mono",monospace; font-weight:bold; }
    .sub { font-size:10px; fill:#3a4a5a; text-anchor:middle; }
    .ctl { font-size:11px; fill:#5b6b7b; font-weight:bold; }
    .lbl { font-size:10px; fill:#6b4a12; text-anchor:middle; font-style:italic;
           paint-order:stroke; stroke:#ffffff; stroke-width:5px; stroke-linejoin:round; }
    .edge{ fill:none; stroke:#555; stroke-width:1.5; marker-end:url(#a); }
  </style>

  <text x="460" y="30" font-size="18" font-weight="bold" fill="#10243e" text-anchor="middle">Boot ROM &#8212; SystemC function-call flow</text>

  <!-- Containers -->
  <g class="cont">
    <rect x="20" y="66" width="680" height="150"/>
    <rect x="20" y="240" width="680" height="290"/>
  </g>
  <text class="ctl" x="32" y="84">Elaboration (constructor)</text>
  <text class="ctl" x="32" y="258">Run time</text>

  <!-- Shared ROM image store -->
  <g class="hlp"><rect x="720" y="88" width="178" height="442"/></g>
  <text class="t code" x="809" y="120" font-size="14">data_</text>
  <text class="sub" x="809" y="140">ROM image</text>
  <text class="sub" x="809" y="155">vector&lt;uint8_t&gt;</text>
  <text class="sub" x="809" y="175">(immutable after</text>
  <text class="sub" x="809" y="189">construction)</text>

  <!-- Elaboration nodes -->
  <g class="trig"><rect x="40" y="98" width="150" height="46"/></g>
  <text class="t code" x="115" y="118">bootrom()</text>
  <text class="sub" x="115" y="134">constructor</text>
  <g class="fn">
    <rect x="240" y="98" width="160" height="46"/>
    <rect x="455" y="88" width="200" height="32"/>
    <rect x="455" y="128" width="200" height="32"/>
  </g>
  <text class="t code" x="320" y="118">load_preload()</text>
  <text class="sub" x="320" y="134">(if init_file set)</text>
  <text class="t code" x="555" y="108" font-size="11.5">resolve_format()</text>
  <text class="t code" x="555" y="148" font-size="11.5">parse_hex_line()</text>
  <text class="sub" x="115" y="170" font-style="italic">+ register b_transport / transport_dbg,</text>
  <text class="sub" x="115" y="184" font-style="italic">SC_METHOD(reset_proc)</text>

  <!-- Elaboration edges -->
  <path class="edge" d="M190,121 L240,121"/>
  <text class="lbl" x="215" y="113">preload</text>
  <path class="edge" d="M400,114 L455,106"/>
  <path class="edge" d="M400,130 L455,140"/>
  <path class="edge" d="M320,144 L320,196 L720,196"/>
  <text class="lbl" x="520" y="189">fills data_</text>

  <!-- Run-time triggers -->
  <g class="trig">
    <rect x="40"  y="276" width="250" height="48"/>
    <rect x="320" y="276" width="140" height="48"/>
    <rect x="490" y="276" width="200" height="48"/>
  </g>
  <text class="t code" x="165" y="296">TLM initiator</text>
  <text class="sub code" x="165" y="313">b_transport()/transport_dbg()</text>
  <text class="t" x="390" y="297">input pin</text>
  <text class="sub code" x="390" y="314">rst_n_i</text>
  <text class="t" x="590" y="296">debug API</text>
  <text class="sub code" x="590" y="313">dbg_read32/64, dbg_load_bytes</text>

  <!-- Run-time handlers -->
  <g class="fn">
    <rect x="40"  y="372" width="250" height="60"/>
    <rect x="320" y="372" width="140" height="60"/>
  </g>
  <text class="t code" x="165" y="394">b_transport()</text>
  <text class="sub" x="165" y="411">validate width/align,</text>
  <text class="sub" x="165" y="425">read-only, window</text>
  <text class="t code" x="390" y="396">reset_proc()</text>
  <text class="sub" x="390" y="413">empty body</text>
  <text class="sub" x="390" y="426">(ROM immutable)</text>

  <!-- Run-time edges -->
  <path class="edge" d="M165,324 L165,372"/>
  <path class="edge" d="M390,324 L390,372"/>
  <path class="edge" d="M165,432 L165,478 L720,478"/>
  <text class="lbl" x="430" y="471">reads data_  (writes silently discarded)</text>
  <path class="edge" d="M690,300 L720,300"/>
  <text class="lbl" x="700" y="333">read / load</text>

  <!-- Legend -->
  <g font-size="10.5" fill="#10243e">
    <rect x="20" y="548" width="16" height="12" rx="3" fill="#fce8cd" stroke="#c47f1a"/>
    <text x="42" y="558">trigger / event source</text>
    <rect x="220" y="548" width="16" height="12" rx="3" fill="#d8e6ff" stroke="#2b6cb0"/>
    <text x="242" y="558">method / function</text>
    <rect x="400" y="548" width="16" height="12" rx="3" fill="#eceff1" stroke="#607d8b"/>
    <text x="422" y="558">backing store</text>
  </g>
  <text x="20" y="582" font-size="10.5" fill="#3a4a5a">The Boot ROM is read-only: b_transport / transport_dbg serve reads from the preloaded data_ image; writes return TLM_OK but are discarded.</text>
  <text x="20" y="596" font-size="10.5" fill="#3a4a5a">reset_proc() has an empty body (binding hook / extension point). b_transport annotates access_delay_; transport_dbg does not.</text>
</svg>
"""


def main() -> None:
    text = CALL_FLOW.encode("ascii").decode("unicode_escape")
    path = HERE / "call_flow.svg"
    path.write_text(text, encoding="utf-8")
    print(f"wrote {path}  ({path.stat().st_size:,} bytes)")


if __name__ == "__main__":
    main()
