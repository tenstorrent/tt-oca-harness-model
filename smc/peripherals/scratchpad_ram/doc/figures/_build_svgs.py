#!/usr/bin/env python3
"""Generate the Scratchpad RAM function-call-flow SVG.

Run from any cwd; writes <here>/call_flow.svg next to this script.
Using a Python writer (rather than direct file editing) guarantees clean
UTF-8 byte output and keeps the figure reproducible / version-controlled
even though the generated *.svg is a git-ignored build artifact.
"""
from __future__ import annotations
import pathlib

HERE = pathlib.Path(__file__).resolve().parent

CALL_FLOW = r"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 960 600" font-family="Helvetica, Arial, sans-serif">
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

  <text x="470" y="30" font-size="18" font-weight="bold" fill="#10243e" text-anchor="middle">Scratchpad RAM &#8212; SystemC function-call flow</text>

  <!-- Containers -->
  <g class="cont">
    <rect x="20" y="64" width="720" height="122"/>
    <rect x="20" y="210" width="720" height="320"/>
  </g>
  <text class="ctl" x="32" y="82">Elaboration (constructor)</text>
  <text class="ctl" x="32" y="228">Run time</text>

  <!-- Shared SRAM store -->
  <g class="hlp"><rect x="770" y="88" width="170" height="442"/></g>
  <text class="t code" x="855" y="120" font-size="14">data_</text>
  <text class="sub" x="855" y="140">SRAM array</text>
  <text class="sub" x="855" y="155">vector&lt;uint8_t&gt;</text>
  <text class="sub" x="855" y="175">(retained on reset)</text>

  <!-- Elaboration nodes -->
  <g class="trig"><rect x="40" y="94" width="170" height="46"/></g>
  <text class="t code" x="125" y="113" font-size="11.5">scratchpad_ram()</text>
  <text class="sub" x="125" y="130">constructor</text>
  <g class="fn"><rect x="250" y="94" width="160" height="46"/></g>
  <text class="t code" x="330" y="114">load_preload()</text>
  <text class="sub" x="330" y="130">(if init_file set)</text>
  <text class="sub" x="125" y="160" font-style="italic">+ register b_transport / transport_dbg,</text>
  <text class="sub" x="125" y="174" font-style="italic">SC_METHOD(reset_proc)</text>

  <!-- Elaboration edges -->
  <path class="edge" d="M210,117 L250,117"/>
  <text class="lbl" x="230" y="109">preload</text>
  <path class="edge" d="M330,140 L330,165 L770,165"/>
  <text class="lbl" x="540" y="158">fills data_</text>

  <!-- Run-time triggers -->
  <g class="trig">
    <rect x="40"  y="236" width="250" height="48"/>
    <rect x="320" y="236" width="130" height="48"/>
    <rect x="480" y="236" width="240" height="48"/>
  </g>
  <text class="t code" x="165" y="256">TLM initiator</text>
  <text class="sub code" x="165" y="273">b_transport()/transport_dbg()</text>
  <text class="t" x="385" y="257">input pin</text>
  <text class="sub code" x="385" y="274">rst_n_i</text>
  <text class="t" x="600" y="256">debug API</text>
  <text class="sub code" x="600" y="273">dbg_inject_ecc_error / dbg_clear</text>

  <!-- Run-time handlers -->
  <g class="fn">
    <rect x="40"  y="330" width="250" height="80"/>
    <rect x="320" y="330" width="130" height="80"/>
  </g>
  <text class="t code" x="165" y="350">b_transport()</text>
  <text class="sub" x="165" y="367">validate, byte-enables</text>
  <text class="sub" x="165" y="382">read: ecc_check(scrub=true)</text>
  <text class="sub" x="165" y="397">write: commit + ecc_clear_range()</text>
  <text class="t code" x="385" y="358">reset_proc()</text>
  <text class="sub" x="385" y="375">empty body</text>
  <text class="sub" x="385" y="389">(SRAM retained)</text>

  <!-- ECC fault-map store -->
  <g class="hlp"><rect x="505" y="438" width="215" height="64"/></g>
  <text class="t code" x="612" y="462" font-size="11.5">ecc_errors_</text>
  <text class="sub" x="612" y="480">injected-fault map</text>
  <text class="sub" x="612" y="494">(word &#8594; uncorrectable?)</text>

  <!-- Run-time edges -->
  <path class="edge" d="M165,284 L165,330"/>
  <path class="edge" d="M385,284 L385,330"/>
  <path class="edge" d="M290,408 L290,512 L770,512"/>
  <text class="lbl" x="470" y="505">read / write data_</text>
  <path class="edge" d="M290,392 L290,420 L470,420 L470,452 L505,452"/>
  <text class="lbl" x="392" y="413">ecc_check / ecc_clear_range</text>
  <path class="edge" d="M600,284 L600,438"/>
  <text class="lbl" x="600" y="360">inject / clear</text>

  <!-- Legend -->
  <g font-size="10.5" fill="#10243e">
    <rect x="20" y="548" width="16" height="12" rx="3" fill="#fce8cd" stroke="#c47f1a"/>
    <text x="42" y="558">trigger / event source</text>
    <rect x="220" y="548" width="16" height="12" rx="3" fill="#d8e6ff" stroke="#2b6cb0"/>
    <text x="242" y="558">method / function</text>
    <rect x="400" y="548" width="16" height="12" rx="3" fill="#eceff1" stroke="#607d8b"/>
    <text x="422" y="558">backing store / state</text>
  </g>
  <text x="20" y="582" font-size="10.5" fill="#3a4a5a">True RAM: b_transport commits writes to data_ and re-encodes ECC (ecc_clear_range); reads scrub correctable errors and fail on uncorrectable.</text>
  <text x="20" y="596" font-size="10.5" fill="#3a4a5a">SRAM survives reset (reset_proc empty). transport_dbg is a back-door read/write (no ECC side effects, no delay); b_transport adds access_delay_.</text>
</svg>
"""


def main() -> None:
    text = CALL_FLOW.encode("ascii").decode("unicode_escape")
    path = HERE / "call_flow.svg"
    path.write_text(text, encoding="utf-8")
    print(f"wrote {path}  ({path.stat().st_size:,} bytes)")


if __name__ == "__main__":
    main()
