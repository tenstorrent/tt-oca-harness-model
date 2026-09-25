# EL2 PIC model/test audit

## Scope and evidence

Audited model files: `include/el2_pic.h`, `include/el2_pic_base.h`,
`include/el2_pic_register.h`, `src/el2_pic.cpp`, and
`src/el2_pic_base.cpp`.

Audited tests and harnesses: all files under `test/inc` and `test/src`,
`CMakeLists.txt`, `run_tests.sh`, and `doc/test_plan.adoc`. The production
`el2_pic.cpp` is compiled into the test model against
`test/inc/VeeR-ISSTlm.hpp`; the full ISS wrapper is not linked
(`CMakeLists.txt:144-193`).

No measured coverage percentage was inferred. The harness has a coverage
target and gate (`CMakeLists.txt:229-260`, `run_tests.sh:63-67`), but this
audit did not treat their existence as proof of a particular result.

## Behavior inventory

- Regmodel-backed 32-bit MMIO for MPICCFG, MEIPL[0..255], MEIP[0..7],
  MEIE[0..255], MEIGWCTRL[0..255], and MEIGWCLR[0..255].
- Active-low reset clears register storage, pending latches, claim tracking,
  and the external interrupt.
- Per-source active-high/active-low and level/edge gateway behavior.
- Edge pending is sticky and clearable only when the effective source is
  inactive.
- Live MEIP bitmap generation; software writes to MEIP are ignored.
- Enabled/pending priority arbitration, priority-order inversion, tie behavior
  from ascending source scan, MEIPT/MEICURPL thresholding, MEIHAP claim update,
  and MEICIDPL update.
- Unbound IRQ ports are tied low at elaboration.
- Blocking transport is inherited from `regmodel::Memory`; no model-specific
  nonblocking or DMI implementation exists.

## Existing scenario matrix

| Area | Existing evidence | Class | Rationale |
|---|---|---|---|
| Register reset/masks | FUNC-001/002 | Genuine | Assertions use MMIO reads/writes and compare exact masks/defaults. |
| Level and edge gateways | FUNC-003/004/005/013 | Genuine | IRQ pins are driven and MEIP/EIP/claim effects are checked. |
| Arbitration and enable | FUNC-006/007/008/014/016 | Genuine | Multiple sources, winner changes, inverted priorities, and raw-priority endpoints are checked. |
| Thresholds | Both FUNC-015 blocks | Partial | PIC behavior is checked, but CSR state and notification are driven directly through the mock and `notify_threshold_changed()`. |
| Reset | FUNC-009 | Genuine | Reset is pin-driven and architectural state is read back. |
| Array boundaries | FUNC-010/011 | Genuine | Source 255 and every MEIP word are checked through ports/MMIO. |
| Unbound ports/null hart | FUNC-012/017 | Partial | Useful elaboration/defensive coverage, but it uses a special second DUT and direct port-interface inspection. |
| TLM protocol | Fixed 32-bit read/write helpers only | Partial | Transactions use valid command/address/pointer/length/streaming-width fields, but response and malformed-payload behavior are not asserted. |

## Shortcut and integrity findings

1. **High — transport failures cannot fail the suite.**
   `test/src/el2_pic_test.cpp:16-50` and
   `test/src/testbench.cpp:67-102` issue `b_transport` but never inspect the
   response status. A decode or protocol error can leave the read buffer at
   zero and still satisfy reset/RO expectations.

2. **Medium — ISS integration is replaced by a structurally smaller test
   double.** `CMakeLists.txt:144-193` compiles the real PIC source against
   `test/inc/VeeR-ISSTlm.hpp`, not the production ISS wrapper. This is valid
   unit isolation, but tests cannot detect callback registration, CSR-access,
   mode-value, or interrupt API drift in the real wrapper.

3. **Medium — direct mock mutation bypasses the CSR write path.**
   `test/src/testbench.cpp:741-753`, `1029-1044`, and `1162-1205` write
   `mock_hart.meipt`/`meicurpl_csr` directly and invoke
   `m_dut->notify_threshold_changed()`. The PIC branch is tested, but the
   actual ISS post-CSR callback path is not.

4. **Low — duplicate FUNC identifier obscures result traceability.**
   `test/src/testbench.cpp:242-264` and `1313-1389` both label different
   scenarios FUNC-EL2PIC-015. The checked-in test plan stops at 014
   (`doc/test_plan.adoc:47-61`), so several implemented scenarios are absent
   from the documented matrix.

5. **No `#define private public`, exception suppression, or model-side
   test-only hook was found.** The mock-hart build substitution remains a
   unit/integration boundary, not a private-access macro.

## Missing scenarios

- Equal-priority tie break, simultaneous assertion ordering, and winner
  fallback when the current winner deasserts while another source remains.
- Priority changes while several sources remain pending; enable/disable of the
  current and non-current winners.
- Active-low edge mode, polarity/type changes while an input is asserted, and
  repeated clear writes with values other than one.
- Threshold boundaries for 0, winner-1, winner, winner+1, and 15 in both
  priority-order modes, through the production CSR callback integration.
- Reset asserted while several edge latches are pending and while the winning
  claim is changing in the same delta cycle.
- MEIP write attempts on every pending word and reserved source-0 register
  writes/readback.
- MMIO below/above aperture, holes, unaligned addresses, unsupported command,
  null data pointer, zero/short/oversized data length, byte enables, streaming
  width mismatch, non-zero incoming delay, and response-status checking.
- `transport_dbg`, DMI denial/behavior, and any applicable transaction
  extension handling.

## Proposed testcase matrix

| ID | Setup | Stimulus | Expected | Model path | Protocol feature |
|---|---|---|---|---|---|
| PIC-F-01 | Two enabled level sources, equal nonzero priority | Assert in both source orders | Lower source ID wins consistently | Arbitration scan/tie | IRQ ports + MMIO |
| PIC-F-02 | Low winner active, higher source pending | Deassert high winner | Claim falls back without dropping EIP | Winner retarget/fallback | Temporal delta ordering |
| PIC-F-03 | Active-low edge source | Assert/deassert/clear/reassert | Sticky pending and clear semantics match effective level | Gateway polarity + edge latch | IRQ port |
| PIC-F-04 | Source asserted in level mode | Change polarity and type through MMIO | Pending recomputes immediately with exact MEIP/EIP result | MEIGWCTRL post-write | MMIO side effect |
| PIC-F-05 | Multiple pending sources | Assert reset in same time step as winner change | Registers, claim and EIP all reset deterministically | Reset/event race | Reset + events |
| PIC-I-01 | Production VeeR wrapper test fixture | Write MEIPT/MEICURPL CSRs | PIC reevaluates without direct DUT call | ISS post-CSR hook | Integration callback |
| PIC-T-01 | Raw generic payload helper | READ/WRITE valid and hole addresses | Exact OK/address-error response; data changes only on OK | Regmodel transport | Command/address/response |
| PIC-T-02 | Raw payload matrix | Null pointer; lengths 0/1/2/3/5/8; unaligned | Defined rejection with no state change | Regmodel transport | Pointer/length/alignment |
| PIC-T-03 | Raw payload matrix | Byte enables and streaming widths 0/1/2/4/8 | Supported forms exact; unsupported forms rejected | Regmodel transport | BE/streaming width |
| PIC-T-04 | Valid access with nonzero delay | Call `b_transport` | Delay contract is asserted, not ignored | Regmodel transport | Delay |
| PIC-T-05 | Debug/DMI initiator | `transport_dbg`, `get_direct_mem_ptr` | Debug read is side-effect policy compliant; DMI result explicit | Regmodel transport | Debug/DMI |

## Verdict

**Mostly genuine functional suite, but not transport-complete.** The gateway,
priority, reset, and upper-bound tests are strong. The principal false-green
risk is unchecked TLM response status; the principal integration gap is that
threshold notification is tested only through a mock/direct call rather than
the production ISS CSR callback.
