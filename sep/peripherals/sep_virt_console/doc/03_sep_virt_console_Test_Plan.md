# 03 — sep_virt_console Test Plan

## Strategy

The decode logic is pure C++ (`VirtConsoleDecoder`), so it is verified in isolation by
self-checking tests that feed crafted packed words and assert the emitted lines. Output is
compared on decoded message **content and order** (the message text), independent of any
console line prefix. The SystemC wrapper (`SimVirtConsole`) is exercised when `sep-vp`
runs SEP bootcode (end-to-end).

## Unit tests — `test/src/virt_console_decoder_test.cpp` (CTest)

| Case | Asserts |
|------|---------|
| ASCII `"OK\n"` | line `OK` |
| ASCII `"DECRYPT_OK\n"` over multiple words | reassembled `DECRYPT_OK` |
| duplicate `"AB\n"`, 2nd with toggled bit[0] | two `AB` lines |
| `"PARTIAL"` (no newline), then `flush()` | none before flush; one `PARTIAL` after |
| HEX16 `0xF001` | `f001` |
| `"0x"` + HEX16(0xDEAD) + HEX16(0xBEEF) + `\n` | `0xdeadbeef` |
| DEC24 `123` | `123` |
| `"X="` + hex32 + `\n` | `X=0xdeadbeef` |
| unknown opcode (3, 7) | no line, no crash |
| sub-word write (len<4), null ptr | ignored; full 4-byte write decodes |
| `enable=false` | no output for any input |

Run: `./run_tests.sh` (Release) and `./run_tests.sh --asan`. Pass criterion: CTest green
and the test prints `ALL TESTS PASSED` (enforced by a CTest pass-regex).

## Integration / end-to-end (on `sep-vp`)

| Check | Method |
|-------|--------|
| Text status visible | run a text-emitting bootcode build; `grep '\[SIM_OUT\]'` |
| Numeric values correct | run a hex/decimal-emitting bootcode build; values match firmware |
| Disable | set `och_sep_ss1.sim_out.enable : false`; expect zero `SIM_OUT` lines |
| Log-file capture | with the CSML global log file active, lines appear there too |
| Determinism | run the same image + config twice; the `SIM_OUT` lines are identical |
| Non-DEBUG firmware | a build with the `simput*` helpers compiled out yields zero lines |

## Status

- Unit tests: implemented and passing (built standalone and via CTest; 13 checks green).
- End-to-end: verified on real SEP boot ROM output (text and numeric lines decode
  correctly, disable produces zero lines, two runs are byte-identical). The `--asan`
  variant and a from-scratch non-DEBUG firmware build should be run in the full toolchain
  environment.
