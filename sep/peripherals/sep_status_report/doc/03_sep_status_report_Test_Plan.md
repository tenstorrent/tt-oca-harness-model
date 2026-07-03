# 03 — sep_status_report Test Plan

## Strategy

The decode and TSV-parse logic are pure C++ and form the correctness oracle. They are
tested in isolation via a self-checking CTest executable (`sep_status_decoder_test`),
built Release and under AddressSanitizer. End-to-end behavior is validated by running the
boot firmware on `sep-vp` and inspecting `SEP_STATUS` output.

## Unit tests (`test/src/sep_status_decoder_test.cpp`)

Self-checking; prints `ALL TESTS PASSED` / `TESTS FAILED` (matched by CTest pass/fail
regex). Cases assert decoded line *tokens* (stage, severity, value, name), not padding.

| # | Case | Expectation |
|---|------|-------------|
| 1 | `INFO`/`WARN`/`ERROR`/`INFO_EXT` words (no names) | 4 lines, correct stage/severity/value, name `SEP_MSG_UNKNOWN` |
| 2 | unknown type `0x55`, unknown fw-id `5` | `ID5 T0x55 0x1234` |
| 3 | little-endian `on_bytes`; `len<4`; null ptr | short/null ignored; full 4-byte write decodes |
| 4 | decoder disabled | no output |
| 5 | TSV with `/* */`, `//`, blank, malformed, bad-hex rows | only valid rows mapped (3) |
| 6 | empty / whitespace / comment-only TSV | empty map |
| 7 | name resolution (hit / miss) | known → name; unknown value → `SEP_MSG_UNKNOWN` |

Run: `./run_tests.sh` and `./run_tests.sh --asan` (both must pass / be clean).

## End-to-end (on `sep-vp`)

| Check | Steps | Expectation |
|-------|-------|-------------|
| Visible stream | run the boot firmware; `grep SEP_STATUS` | lines in firmware emission order |
| Names resolved | with `names_tsv` set to the canonical TSV | lines show `SEP_MSG_*` names |
| Debug path intact | same run; `grep SIM_OUT` | `SIM_OUT` lines still present (no regression) |
| Determinism | run twice; `diff` the `SEP_STATUS` lines | identical |
| Disable | set `sep_status.enable : false`; re-run | no `SEP_STATUS` lines; boot outcome identical to the enabled run |
| TSV failure | point `names_tsv` at a missing path | one reported problem; lines show `SEP_MSG_UNKNOWN`; run continues |

## Coverage notes

- Decode label maps and the value-formatting path are exercised by cases 1–2.
- The byte-assembly / robustness path (sub-word, null) is exercised by case 3.
- The TSV parser's comment/blank/malformed handling is exercised by cases 5–6.
- The platform write-tap window filter is exercised implicitly by the end-to-end run
  (only `entries[]` writes produce lines; header writes do not).
