# sep_filter_ctrl Test Audit and Plan

## Audited files

- Model: `include/sep_filter_ctrl.h`, `sep_filter_ctrl_base.h`, `sep_filter_ctrl_register.h`, `src/sep_filter_ctrl.cpp`, `src/sep_filter_ctrl_base.cpp`.
- Tests: `test/inc/sep_filter_ctrl_basetest.h`, `sep_filter_ctrl_test.h`, `test/src/sep_filter_ctrl_basetest.cpp`, `sep_filter_ctrl_test.cpp`, `sep_filter_ctrl_testbench.cpp`.
- Build/coverage: `CMakeLists.txt`, `run_tests.sh`.

## Behavior inventory

- Outbound instance has 32 entries; inbound has 16; each entry has FILTER_CONFIG, START_ADDR, and END_ADDR at 0x20 stride.
- FILTER_CONFIG controls read/write permission, enable, exact secure/nonsecure match, source-ID wildcard/exact match, burst permission, group field, hardwired data bus width 3, and sticky lock.
- START/END are 56-bit and freeze after lock. RV32 paired 32-bit stores are merged with byte enables.
- Auto-correction snaps same-page ranges to a 4 KiB page when bursts are allowed and same-word ranges to an 8-byte word otherwise.
- Data path uses first matching enabled entry. Match includes granular range, source ID, secure attribute, and burst length; direction permission is applied at the winning entry. No match denies by default.
- Missing extension uses default trusted SEP attributes.
- `filter_skip_i` bypasses matching and permissions on functional and debug paths; an unbound port is tied low.
- `b_transport` denies with ADDRESS_ERROR; `transport_dbg` denies with zero byte count.
- Active-low reset clears only configured entry count.
- Public decoded-entry and callback methods expose out-of-range guards.

## Existing tests matrix

### Genuine

- CSR traffic goes through the real target socket; RV32 paired stores exercise byte-enable callback merging.
- Data traffic uses the real data and filtered sockets and checks allowed/denied forwarding.
- Block-by-default, range miss, read-only/write-only permission, outbound/inbound independence, bypass, debug parity, and reset are tested.
- Lock tests verify sticky lock and START/END freeze.
- Release uses `-UNDEBUG`, preserving assertions (`CMakeLists.txt:163-167`).

### Partial

- Reset checks only the first four entries for outbound/inbound (`test/src/sep_filter_ctrl_test.cpp:148-166`).
- “Comprehensive filter scenarios” only programs three CSRs and contains no data-path assertions (`test/src/sep_filter_ctrl_test.cpp:257-281`).
- Stub records little protocol metadata and forces delay to zero.
- CSR helpers usually ignore response status and convert failed reads to zero, which can mask failures (`test/src/sep_filter_ctrl_test.cpp:18-116`).
- Debug tests check count/forwarding but not command, extension, address, or response propagation.

### Coverage-only

- A13 directly invokes public callback methods and `get_filter_entry()` rather than using sockets (`test/src/sep_filter_ctrl_testbench.cpp:334-346`).
- A13 temporarily changes global `SC_ERROR` actions to prevent the expected errors from affecting execution (`test/src/sep_filter_ctrl_testbench.cpp:336-345`). It restores the setting, but this is direct guard coverage, not interface behavior.
- No private-public macro or private state poke was found.

## Shortcut findings

- **High — major match dimensions untested:** no data-path case attaches `sep_axi_extension`; source-ID and secure/nonsecure filtering are therefore not verified.
- **High — burst policy untested:** no payload length above 8 exercises `allow_burst`.
- **High — priority behavior untested:** no overlapping entries prove first-hit permission denial versus later allow.
- **High — auto-correction untested:** same-page/same-word START/END storage snapping is a central callback behavior with no assertion.
- **Medium — direct callback/report suppression coverage:** A13 bypasses sockets and changes error actions.
- **Medium — helper masks response failures:** failed reads become zero and writes return no status.
- **Medium — TLM command hole:** IGNORE command can match and be forwarded because permission checks handle only READ and WRITE (`src/sep_filter_ctrl.cpp:248-260`); no negative command test exists.
- **Low — coverage excludes hand-written register header as “generated”:** `CMakeLists.txt:191-201`.
- No expected-value clone or offset-only coverage loop was found.

## Missing scenarios

- Source ID: wildcard zero; every valid ID; mismatch fallthrough to later entry; missing extension default.
- Security: secure and nonsecure exact match; mismatch fallthrough; bypass.
- Burst: lengths 1/8/9/multi-beat with allow_burst off/on; streaming width and byte enables.
- Priority: overlapping allow/deny entries, disabled earlier entry, earlier attr mismatch, winning deny must not defer.
- Range: exact granular start/end, byte addresses within widened page/word, adjacent range, reversed range, high 56-bit address, reserved upper bits.
- Auto-correction after START write, END write, and allow_burst toggle; locked behavior after correction.
- CSR all 32/16 entries, last entry, inactive upper entries in inbound memory, stride hole at `+0x18`, invalid offset/alignment, partial/byte-enable reads/writes.
- TLM2 bad command, null pointer, lengths, debug/DMI, downstream errors, nonzero delay, extension preservation.
- Reset with locked entries, bypass high, transaction around reset, reset already asserted at startup.
- No interrupts or DMA engine exist in this IP.

## Proposed cases

1. **Extension attribute matrix:** attach explicit extensions and use literal entry configs to test source wildcard/exact, mismatch, secure/nonsecure, missing-extension default, and preservation of unrelated extension fields.
2. **Burst matrix:** lengths 8 and 9 with allow_burst clear/set; assert exact forwarding/ADDRESS_ERROR and downstream receipt. Add nontrivial streaming widths and BEs to ensure policy uses intended burst criterion.
3. **Priority oracle:** two overlapping entries where entry 0 denies direction and entry 1 allows; assert denial at entry 0. Then disable/mismatch entry 0 and assert entry 1 forwards.
4. **Granularity/auto-correction:** program same 8-byte word and same 4 KiB page, read stored START/END exact snapped constants, toggle allow_burst, and repeat across boundary/non-same-page cases.
5. **Lock/RV32 sequence:** set low/high halves, set lock via high-half store, attempt every CSR write afterward, and assert exact frozen values.
6. **CSR protocol matrix:** raw helper asserts status for legal, hole, out-of-range, unaligned, malformed length/pointer/BE/streaming, IGNORE, debug, and DMI cases.
7. **Downstream transparency:** stub captures full payload and adds 13 ns/returns programmable errors; assert all metadata/extensions are preserved and delay/response propagate.
8. **Debug parity matrix:** repeat permission, attrs, burst, overlap, and bypass scenarios through `transport_dbg`, asserting exact return count and no downstream call on denial.
9. **Entry-count boundaries:** exercise outbound entry 31 and inbound entry 15 through CSR/data sockets; accesses to inbound entries 16-31 must return the specified decode error and never affect state.
10. **Reset lifecycle:** program and lock first/last entries, assert reset, read every field through CSR, and verify former hits deny; repeat while bypass high to distinguish reset from bypass behavior.
11. **Remove direct guard coverage:** test invalid CSR offsets through target socket. Keep public API bounds tests separate from model coverage or make a dedicated API contract test without suppressing unrelated errors.

## Verdict

Basic allow/deny, lock, reset, debug, bypass, and instance separation are genuinely tested. Confidence is nevertheless **low-to-medium** because the defining source/security/burst/priority and auto-correction semantics are absent. Current coverage can be high while the central filter policy is wrong.
