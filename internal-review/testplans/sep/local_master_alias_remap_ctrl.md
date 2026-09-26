# local_master_alias_remap_ctrl Test Audit and Plan

## Audited files

- Model: `include/local_alias_remap.h`, `include/local_alias_remap_base.h`, `include/local_alias_remap_register.h`, `src/local_alias_remap.cpp`, `src/local_alias_remap_base.cpp`.
- Tests: `test/inc/local_alias_remap_basetest.h`, `test/inc/local_alias_remap_test.h`, `test/src/local_alias_remap_basetest.cpp`, `test/src/local_alias_remap_test.cpp`, `test/src/local_master_alias_remap_ctrl_testbench.cpp`.
- Build/coverage: `CMakeLists.txt`, `run_tests.sh`.
- Existing design/test documentation was considered for naming, but this audit classifies executable checks, not documented intent.

## Behavior inventory

- CSR target socket exposes 16 entries at a 0x20-byte stride: `REGION_START`, `REGION_END`, and `REGION_ATTRS`; address/offset fields are 44 bits at `[55:12]`, with cacheable and valid at bits 62/63.
- `reset_all_registers()` clears all 16 entries. Active-low `rst_ni` invokes it on an asserted edge.
- Data `b_transport` and `transport_dbg` scan entries in ascending order. A hit requires valid and `start <= address < end`; first match wins.
- Translation adds the entry offset only in address bits `[55:12]`, truncates overflow, and preserves bits `[11:0]`.
- On a matching transaction carrying `sep_axi_extension`, the outgoing cacheable bit is replaced with the entry value. Address and cacheability are restored after downstream return.
- A miss forwards unchanged. A payload without an extension is still forwarded and has no cache attribute to override.
- `get_region()` is a public test backdoor and returns a zero object for an invalid index.
- The underlying `regmodel::Memory` owns CSR TLM decode and protocol behavior; the IP adds no CSR-level transport validation of its own.

## Existing tests matrix

### Genuine

- T2-T7 program CSRs through `target_socket`, exercise read/write data traffic through `data_socket`, and check fixed expected translated addresses, overlap priority, lower-bit preservation, passthrough, and write forwarding.
- T8 checks restoration of the initiator-owned address after downstream return.
- T9, T16, and T17 exercise actual `transport_dbg` hit/miss forwarding.
- T11 drives a real reset edge after nonzero programming and checks every entry is cleared.
- T12-T15 drive real `sep_axi_extension` objects and independently check cacheable set, clear, miss passthrough, and post-return restoration.
- The Release test target explicitly uses `-UNDEBUG`, so assertions in transaction helpers remain active (`CMakeLists.txt:166-171`).

### Partial

- T1 and most CSR readback checks use `dut.get_region()` rather than socket reads (`test/src/local_master_alias_remap_ctrl_testbench.cpp:263-272,287-292`). They validate decoded state but can pass if CSR read transport is broken.
- T10 calls itself “field mask enforcement” but checks only valid programmed fields through the backdoor; it never writes reserved bits and proves they are discarded (`test/src/local_master_alias_remap_ctrl_testbench.cpp:403-420`).
- T18 covers the `get_region()` guard, not a software-visible or data-path boundary (`test/src/local_master_alias_remap_ctrl_testbench.cpp:534-543`).
- Data helpers assert only final OK response and observed address. They do not verify command/data pointer/length/byte enables/streaming width/delay received downstream.
- Reset is checked through the backdoor, not by CSR socket readback or post-reset data-path behavior.

### Coverage-only

- No dedicated coverage-only source exists.
- There is no private/public macro or direct private callback invocation.
- The public `get_region()` backdoor is nevertheless used to cover/verify internal decoding and should not count as end-to-end CSR coverage.

## Shortcut findings

- **Medium — backdoor substitutes for CSR read semantics:** reset and programming checks inspect `get_region()` directly (`test/src/local_master_alias_remap_ctrl_testbench.cpp:263-292,440-448`). A broken read callback, read mask, access width, or response status could escape.
- **Medium — “field mask” claim lacks adversarial data:** T10 never sets reserved bits (`test/src/local_master_alias_remap_ctrl_testbench.cpp:403-420`).
- **Medium — helper swallows CSR read failures:** `register_read_64/8()` converts any response error to zero and returns no status (`test/src/local_alias_remap_test.cpp:23-27,62-66`). Reset-value tests can therefore report the expected zero after a failed transaction.
- **Low — downstream stub overwrites delay:** the stub forces delay to zero (`test/src/local_master_alias_remap_ctrl_testbench.cpp:61-66`), preventing any proof that the remapper preserves downstream timing annotation.
- **Low — coverage reports headers and sources but the coverage gate alone says nothing about semantic quality:** `CMakeLists.txt:184-209`.
- No private-public macro, disabled fatal report, expected-value implementation clone, or offset-touch loop was found.

## Missing scenarios

- CSR: all 16 entries via socket readback; last entry; stride holes; first and last mapped bytes; reserved-bit masking; 8/32-bit partial accesses; invalid offset/alignment; invalid command; null data pointer; zero/short/oversized length; byte enables; streaming width; CSR debug/DMI behavior.
- Range boundaries: exact start, `end-1`, exact end, `start==end`, start greater than end, address bits above bit 55, zero and maximum offset, positive-add overflow truncation.
- Priority: overlap where entry 0 is disabled, invalid, source-unrelated, or range-missing and a later entry must win; several overlapping valid entries.
- Extension: absent extension on hit, all unrelated extension fields preserved, downstream mutation/restoration behavior, cache override when downstream returns an error.
- Transport: `TLM_IGNORE_COMMAND`, downstream error propagation, downstream modified data, nonzero incoming delay plus added downstream delay, byte-enable and streaming metadata transparency, debug return count and response propagation.
- Reset: assertion during/around a transaction, CSR socket readback after reset, post-reset deny/passthrough behavior, repeated reset, startup with reset already low.
- No interrupts or DMA behavior exist in this IP; those categories are not applicable.

## Proposed cases

1. **CSR reset/readback matrix:** after a real reset, read all 48 implemented registers through `target_socket`; expect exact reset/masked values and `TLM_OK_RESPONSE`. This covers base reset loops and software-visible decode without `get_region()`.
2. **CSR mask and partial-write matrix:** write all ones, alternating values, and RV32 low/high halves to each register type. Expect bits `[11:0]` and `[63:56]` to read zero; only attrs bits 62/63 survive. Verify untouched halfwords remain unchanged.
3. **CSR malformed payloads:** issue ignore command, unmapped/unaligned offsets, lengths 0/1/4/8/9, null pointer where the shared memory safely rejects it, byte enables, and streaming width smaller than length. Assert the exact response and that state is unchanged.
4. **Range boundary oracle:** program `[0x10000,0x12000)` with fixed offset and check `0x10000`, `0x11fff`, and `0x12000` against literal expected addresses. Add empty and reversed ranges and assert passthrough.
5. **Overflow and upper-bit policy:** use an address/offset whose 44-bit sum overflows; expect literal modulo-`2^44` upper result and unchanged low 12 bits. Separately verify how bits above 55 are handled and lock that behavior to the specification.
6. **Priority matrix:** create three overlapping entries, disable or invalidate earlier entries one at a time, and assert the first remaining hit's literal translation/cache policy. This covers every scan continuation reason.
7. **Payload transparency:** downstream records command, pointer identity, data length, streaming width, byte-enable pointer/content, DMI flag, extension fields, and incoming delay. Assert only address/cacheable differ during the call and all initiator-owned fields are restored afterward.
8. **Error and timing propagation:** downstream returns each TLM error and adds 17 ns. Assert response and accumulated delay are preserved and address/cacheability are restored on every return path.
9. **Debug parity:** repeat hit, miss, overlap, extension, and denial/error-like downstream return-count scenarios through `transport_dbg`; assert exact byte count and restoration.
10. **Reset lifecycle:** program first/last entries, assert reset while low at startup and by a later edge, then read via CSR and issue former-hit traffic; expect cleared CSRs and unchanged passthrough.

## Verdict

The core remap, priority, cacheability, reset-edge, and debug happy paths have genuine socket-level checks. Confidence remains **medium** because CSR semantics are frequently verified through a public backdoor, malformed TLM behavior is absent, overflow and exact range boundaries are untested, and payload/delay/error transparency is not checked. No blatant coverage-only file exists, but reported coverage should not be treated as full protocol coverage.
