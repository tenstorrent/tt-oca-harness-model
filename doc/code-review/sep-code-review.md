# SEP Code Review

Date: 2026-09-21

## Scope

This review covers hand-written production code and testbenches under `sep/`.
It excludes build products, logs, empty placeholder peripherals,
`sep/cpu/VeeR-ISS/softfloat/`, generated artifacts, and all `sw/` content.

Functional comparisons use the RTL/RDL tree at:

`/Users/pdroy/Library/CloudStorage/GoogleDrive-pdroy@tenstorrent.com/My Drive/tt-oca-harness`

## Alignment update — 2026-09-22

The artifact-alignment branch now follows harness `main` at `3753f0ff` for the
platform contracts that sit outside this review's original `sep/` scope:

- POR and watchdog reset are split into cold and warm domains; cold scratch
  clears only on cold reset, matching `sep_system_csr.sv`.
- `SEP_NMI_VEC` is published at its reset value even when POR begins asserted
  without a falling edge.
- PIC sources 39–43 are represented explicitly; absent integrity-source models
  are documented tie-lows instead of omitted map entries.
- The vendored boot vector uses retained cold scratch 7 and current DFX/MBIST
  gating, and mailbox `STATUS.empty` resets to one.

The per-IP timing, byte-enable, isolation sequencing, entropy, and register
behavior findings below remain open unless their individual text says otherwise.

Severity means:

- **High**: materially different externally visible behavior, data corruption,
  or a regression that can silently pass.
- **Medium**: narrower correctness mismatch, incomplete contract, or important
  verification gap.
- **Low**: tooling, maintainability, style, or efficiency problem with limited
  immediate functional impact.

## Executive summary

The review found **13 high**, **10 medium**, and **1 low** issue, for 24 total.
The highest
priority themes are:

1. Timer and reference-counter state changes are incorrectly driven by CSR
   accesses instead of clocks.
2. Several register models have drifted from current RDL, especially
   `entropy_src`, `sep_cpu_ctrl`, and eFuse.
3. Some memory/register paths mishandle full-width writes with partial byte
   enables.
4. Reset sequencing omits RTL isolation/drain behavior.
5. Some tests either enforce wrong behavior or can report failure while still
   returning success to CI.

Passing unit tests do not invalidate these findings: several affected tests
explicitly expect the model behavior that differs from RTL.

## Correctness findings

### SEP-01 — High — CPU wrapper forces `mstatus.MIE` at startup

**Model:** `sep/cpu/VeeR-ISSTlm/model/src/VeeR-ISSTlm.cpp:119-122`

**Reference:** `vendor/chipsalliance/Cores-VeeR-EL2/upstream/design/dec/el2_dec_tlu_ctl.sv:2688-2700`

The wrapper enables global machine interrupts during elaboration. RTL resets
the architectural state to zero and expects firmware to enable interrupts.
The model can therefore take an interrupt before firmware is ready, and its
initial and subsequent reset behavior differ.

Recommended change:

```cpp
// Do not alter architectural interrupt state during elaboration.
// Firmware owns mstatus.MIE.
hart0->enableNmi(enableNmi.get_param_value());
```

Add a test that resets the CPU, injects a pending interrupt, and proves no trap
is taken until firmware writes `mstatus.MIE`.

### SEP-02 — High — SEP memory ignores TLM byte enables

**Model:** `sep/peripherals/sep_memory/model/src/sep_memory.cpp:54-89`

**Reference:** `hw/sys/sep/rtl/sep_sram_interface_shim.sv:53-61`

Every write replaces all payload bytes. RTL maps AXI strobes to a per-byte
write mask, so disabled lanes must preserve old data.

Recommended change:

```cpp
const auto* be = trans.get_byte_enable_ptr();
const unsigned be_len = trans.get_byte_enable_length();
if (be != nullptr && be_len == 0) {
    trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
    return;
}

for (unsigned i = 0; i < len; ++i) {
    if (be == nullptr || be[i % be_len] == tlm::TLM_BYTE_ENABLED)
        m_mem.write(addr + i, ptr[i]);
}
```

The transport should also validate null data pointers, streaming width, bounds,
and unsupported commands before accessing memory.

### SEP-03 — Medium — PIC arbitration is stale after `MPICCFG.priord` changes

**Model:** `sep/peripherals/el2_pic/src/el2_pic.cpp:278-330`

**Reference:** `vendor/chipsalliance/Cores-VeeR-EL2/upstream/design/el2_pic_ctrl.sv:329-331,445-466`

Priority, enable, and gateway writes trigger arbitration, but `MPICCFG` does
not. RTL applies `intpriord` directly to pending priority comparisons. Changing
the order while interrupts are pending should immediately change the winner.

Recommended change:

```cpp
memory.register_post_write_callback(
    [this]() {
        reevaluate_arbitration();
        return true;
    },
    MPICCFG.offset);
```

Extend the test so two sources remain pending while `priord` is changed.

### SEP-04 — High — AON counter reads mutate timer state

**Model:** `sep/peripherals/aon_timer/src/aon_timer.cpp:1254-1313`

**Reference:** `vendor/lowRISC/opentitan/upstream/hw/ip/aon_timer/rtl/aon_timer_core.sv:60-66`

Reading `WKUP_COUNT_HI` arms a flag and the next low-half read increments the
counter. RTL advances only from the clock/prescaler. A read can therefore
synthesize a tick and trigger a threshold early.

Recommended change:

```cpp
bool aon_timer_ip::handle_read_WKUP_COUNT_HI(uint32_t& value, uint32_t) {
    if (m_qk.need_sync()) m_qk.sync();
    value = static_cast<uint32_t>(m_wkup_counter >> 32);
    return true;
}

bool aon_timer_ip::handle_read_WKUP_COUNT_LO(uint32_t& value, uint32_t) {
    if (m_qk.need_sync()) m_qk.sync();
    value = static_cast<uint32_t>(m_wkup_counter);
    return true;
}
```

Non-atomicity should arise from elapsed simulation time, not a read side effect.
Update `aon_timer/test/src/testbench.cpp:4025-4045`, which currently expects
the artificial increment.

### SEP-05 — High — AON events fire on CSR writes instead of AON edges

**Model:** `sep/peripherals/aon_timer/src/aon_timer.cpp:624-942`

**Reference:** `vendor/lowRISC/opentitan/upstream/hw/ip/aon_timer/rtl/aon_timer_core.sv:60-84`

Enable, threshold, and counter writes immediately evaluate wakeup, bark, and
bite. RTL gates these events with `wkup_incr`/`wdog_incr`. This is observably
early, including a zero-threshold bite at enable time.

Recommended structure:

```cpp
// CSR callback: update configuration and wake the timer thread only.
m_ev_wkup_tick.notify(sc_core::SC_ZERO_TIME);

// Timer thread, after the real timed increment:
++m_wkup_counter;
evaluate_wkup_threshold();
```

Apply the same rule to bark and bite. Tests should verify no output before one
complete AON counting period.

### SEP-06 — High — `REFERENCE_COUNTER` advances on reads

**Model:** `sep/peripherals/sep_cpu_ctrl/src/sep_cpu_ctrl.cpp:250-254`

**Reference:** `hw/common/och_prim/rtl/prim_refclk_count_w_cdc.sv:120-125`

`value = ++reference_counter_rc` makes polling frequency define elapsed time.
RTL uses a free-running reference clock.

Recommended design:

```cpp
void sep_cpu_ctrl_ip::reference_counter_thread() {
    while (true) {
        wait(ref_clock_period_p_.get_value(), sc_core::SC_NS);
        ++hwif_in.reference_counter_rc;
    }
}

bool sep_cpu_ctrl_ip::handle_read_REFERENCE_COUNTER(DT& value, DT) {
    value = hwif_in.reference_counter_rc;
    return true;
}
```

Expose the period through CCI and make repeated same-time reads side-effect
free.

### SEP-07 — High — Reset controller skips isolation and drain sequencing

**Model:** `sep/peripherals/sep_reset_ctrl/src/sep_reset_ctrl.cpp:72-84`

**Reference:** `hw/sys/sep/rtl/sep_isolate_rst_seq.sv:31-76`

The model directly maps CSR bits to reset outputs. RTL requests isolation,
waits for affected paths to drain, then asserts a clock-aligned reset. The
current abstraction can hide in-flight transaction loss and ordering defects.

Recommended direction:

```cpp
enum class reset_state { run, drain, reset };

case reset_state::run:
    if (reset_requested) {
        isolate_req_o.write(true);
        state = reset_state::drain;
    }
    break;
case reset_state::drain:
    if (isolated_i.read()) {
        reset_o.write(true);
        state = reset_state::reset;
    }
    break;
```

The interface needs isolation request/acknowledgment signals. Tests must hold
the acknowledgment low and prove reset remains deasserted.

### SEP-08 — Medium — `sep_cpu_ctrl` register contract is stale

**Model:** `sep/peripherals/sep_cpu_ctrl/include/sep_cpu_ctrl_register.h:30-409`

**Reference:** `hw/sys/sep/regs/blocks/sep_cpu_ctrl/sep_cpu_ctrl.rdl:19-118`

The model exposes 11 clock-gate enables plus a six-bit hysteresis field and
eight timeout status/counter groups. Current RDL defines one placeholder
clock-gate bit and one placeholder timeout register. Firmware can appear to
configure fields that do not exist in current silicon.

Recommended change:

```cpp
regmodel::Reg<N>(reg_name, memory, offset, 0x1, 0x1, 0x0); // clock gate
regmodel::Reg<N>(reg_name, memory, offset, 0x1, 0x0, 0x0); // RO status
```

Remove obsolete handlers/tests or explicitly version the model against the
older RDL revision.

### SEP-09 — Medium — Output-remap tests protect wrong byte-enable behavior

**Model:** `common/include/reg_file.h:223-232`

**Test:** `sep/peripherals/sep_output_remap_ctrl/test/src/sep_output_remap_ctrl_testbench.cpp:402-431`

**Reference:** `hw/ip/output_remap/regs/gen/sv/output_remap_reg.sv:284-302`

The shared memory code merges byte enables only when the payload covers less
than one register word. A full-width payload with some lanes disabled bypasses
that merge, and the test explicitly expects all implemented bits to change.
RTL merges every write using decoded write strobes.

Recommended API-level fix:

```cpp
memory.register_write_callback_with_be(
    [this, i](DT value, uint8_t be) {
        DT merged = static_cast<DT>(REGION_ATTRS[i]);
        for (unsigned lane = 0; lane < sizeof(DT); ++lane) {
            if (be & (uint8_t{1} << lane)) {
                const DT mask = DT{0xff} << (lane * 8);
                merged = (merged & ~mask) | (value & mask);
            }
        }
        return REGION_ATTRS[i].handle_write(
            merged, REGION_ATTRS[i].write_bit_mask);
    },
    REGION_ATTRS[i].offset);
```

Fix this in the shared register library, then change T8 to verify disabled
bytes retain nonzero prior values.

### SEP-10 — High — Entropy interrupt map omits four RTL sources

**Model:** `sep/peripherals/entropy_src/include/entropy_src_register.h:190-310`

**Reference:** `integration/rdl/sep/entropy_source.rdl:107-259`

The model implements bits 0, 4, 8, and 12 (`0x00001111`). RDL also defines
persistent failure, autotune failure, BIW overflow, and noise overflow at bits
16, 20, 24, and 28. Those sources cannot be enabled, injected, observed, or
cleared.

Recommended core mask:

```cpp
static constexpr uint32_t kAllEntropyInterrupts = 0x11111111u;
INTR_STATUS = regmodel::apply_w1c(
    static_cast<uint32_t>(INTR_STATUS), value, kAllEntropyInterrupts);
```

Add named bitfields and tests for every source; a mask-only change is
insufficient.

### SEP-11 — High — Entropy FIFO has half the required depth

**Model:** `sep/peripherals/entropy_src/include/entropy_src.h:581`

**Reference:** `integration/rdl/sep/entropy_source.rdl:260-295`

The model has 32 entries and five-bit pointers. RDL specifies 64 entries and
six-bit pointers. Fill level, overflow point, and pointer wrap are wrong.

Recommended constants:

```cpp
static constexpr unsigned kFifoDepth = 64;
status = level
       | ((m_wptr & 0x3fu) << 8)
       | ((m_rptr & 0x3fu) << 16);
```

Update the coverage test that currently expects saturation at 32 entries.

### SEP-12 — High — Entropy health status is RW instead of W1C

**Model:** `sep/peripherals/entropy_src/include/entropy_src_register.h:610-633,1190-1529`

**Reference:** `integration/rdl/sep/entropy_source.rdl:348-357,420-553`

`HEALTH_TEST_STATUS` and generator health-status registers use ordinary
writable storage. Software can set arbitrary failures; RTL permits software
only to clear hardware-latched failures. `INTR_STATUS` already has a separate
W1C callback and is not part of this finding.

Recommended callback:

```cpp
memory.register_write_callback(
    [this](uint32_t incoming) {
        HEALTH_TEST_STATUS = regmodel::apply_w1c(
            static_cast<uint32_t>(HEALTH_TEST_STATUS), incoming, 0xffu);
        return true;
    },
    HEALTH_TEST_STATUS.offset);
```

Use each register's exact RDL mask instead of applying `0xff` indiscriminately.

### SEP-13 — High — `FIPS_LOCK` leaves certified configuration writable

**Model:** `sep/peripherals/entropy_src/src/entropy_src.cpp:190-486`

**Reference:** `integration/rdl/sep/entropy_source.rdl:322-344,556-620,910-950`

Only a subset of RDL fields marked `swwel=true` are lock-gated. Window size,
Markov/APT thresholds, oscillator tuning/control, decorrelator controls, and
sample-clock configuration remain mutable after lock.

Recommended pattern (add this local helper once, then register every governed
field through it):

```cpp
auto register_locked_write = [this](auto& reg) {
    memory.register_write_callback(
        [this, &reg](uint32_t incoming) {
            const uint32_t lock =
                static_cast<uint32_t>(FIPS_LOCK.LOCK) ? ~uint32_t{0} : 0;
            reg = regmodel::apply_lock_gated(
                static_cast<uint32_t>(reg),
                incoming & static_cast<uint32_t>(reg.write_bit_mask), lock);
            return true;
        },
        reg.offset);
};
```

Prefer one descriptor table to ensure new RDL-controlled registers cannot be
forgotten.

### SEP-14 — High — EDN command FIFO reset does not clear FIFOs

**Model:** `sep/peripherals/edn/src/edn.cpp:320-491`

**Reference:** `vendor/lowRISC/opentitan/upstream/hw/ip/edn/rtl/edn_core.sv:703,746,815-821`

Writing multibit-boolean true (`0x6`) is stored but neither command FIFO is
cleared. The current T11 test returns true without checking behavior.

Recommended change:

```cpp
if (cmd_fifo_rst_field == 0x6u) {
    std::queue<uint32_t>{}.swap(m_reseed_cmd_fifo);
    std::queue<uint32_t>{}.swap(m_generate_cmd_fifo);
}
```

The replacement test should load both FIFOs, issue reset, enable auto mode, and
observe the empty-FIFO path.

### SEP-15 — High — Key-manager mailbox suppresses RTL bus errors

**Model:** `sep/peripherals/key_manager/src/key_manager.cpp:345-486`

**Reference:** `hw/ip/key_manager/rtl/km_mailbox.sv:205-211,242-248`

RTL returns SLVERR for a full inbound FIFO write or empty outbound FIFO read
unless `MB_CTRL` selects OKAY. The model records flags but always returns
success.

Recommended logic requires the shared `regmodel::Memory` transport to propagate
callback rejection instead of unconditionally setting `TLM_OK_RESPONSE`:

```cpp
if (!operation_ok && !(static_cast<uint32_t>(MB_CTRL) & okay_override_mask))
    return false;
return true;
```

Extend `Memory::write_registers`/`b_transport` to map that `false` to a generic
or slave-error response, then add tests for both default-SLVERR and
override-OKAY modes.

### SEP-16 — Medium — eFuse offset `0x174` has the wrong register contract

**Model:** `sep/peripherals/efuse/include/efuse_register.h:1227-1267`

**Reference:** `hw/sys/sep/regs/blocks/sep_efuse_map/sep_efuse_map.rdl:817-829`

RTL defines read-only `SYSCLK_FREQ_MHZ[10:0]`. The model calls the location
`SEP_SPI_CTRL_FIELD_EN`, exposes 32 readable bits, and defines unrelated
fields through bit 18.

Recommended declaration:

```cpp
regmodel::Register32 sysclk_freq_mhz{0x000007ffu, 0u, reset_frequency_mhz};
```

Rename the parameter and tests so units and semantics are explicit.

### SEP-17 — Medium — Adams Bridge interrupt counters wrap

**Model:** `sep/peripherals/adams_bridge/src/adams_bridge.cpp:418-431`

**Reference:** `vendor/chipsalliance/adams-bridge/upstream/src/abr_top/rtl/abr_reg.rdl:624-634`

RDL specifies `incrsaturate=true`; model arithmetic wraps `UINT32_MAX` to zero.

Recommended change:

```cpp
const auto count = static_cast<uint32_t>(counter);
if (count != std::numeric_limits<uint32_t>::max())
    counter = count + 1u;
```

Add a backdoor setup or narrow helper test for the maximum-value boundary.

### SEP-18 — Medium — AES/HMAC/KMAC acknowledge unmapped KeyMgr writes

**Models:** `sep/peripherals/aes/src/aes.cpp:294-308`,
`sep/peripherals/hmac/src/hmac.cpp:1804-1817`,
`sep/peripherals/kmac/src/kmac.cpp:511-537`

**Reference:** `hw/ip/key_manager/regs/*_wrapper_key.rdl:49-60`

Invalid or unaligned offsets fall through and still receive
`TLM_OK_RESPONSE`. The RDL windows end at `KEY_CTRL` offset `0x40`.

Recommended common validation:

```cpp
if (trans.get_data_ptr() == nullptr || trans.get_data_length() != 4) {
    trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
} else if (!valid_aligned_offset(offset)) {
    trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
} else {
    decode_keymgr_write(offset, *reinterpret_cast<uint32_t*>(data));
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}
```

Avoid unaligned pointer casts in the real implementation; use `memcpy`.
Factor the repeated transport contract into one shared decoder.

## Test and verification findings

### SEP-19 — High — OTBN can print failure and still exit successfully

**Test:** `sep/peripherals/otbn/test/src/testbench.cpp:1353-1410,13276-13288`

The read-only-register test sets local `test_passed=false` but never calls
`report_test_result`, the only path that increments `m_tests_failed`. CI can
receive exit status zero after a printed failure.

Recommended change:

```cpp
report_test_result("Read-only registers", test_passed);
```

Audit all tests that accumulate a local result. Prefer assertion helpers whose
failure automatically updates the suite result.

### SEP-20 — Medium — CSRNG reseed test checks bit 7 instead of bit 15

**Test:** `sep/peripherals/csrng/test/src/csrng_func003_seed_life_management.cpp:234-246`

**Reference:** `vendor/lowRISC/opentitan/overlay/regs/csrng/regs/csrng.rdl:402-410`

The test warns if bit 7 is absent and then passes unconditionally. RDL defines
`CMD_STAGE_RESEED_CNT_ALERT` at bit 15.

Recommended check:

```cpp
constexpr uint32_t kReseedCountAlert = 1u << 15;
if ((alert_sts & kReseedCountAlert) == 0)
    throw std::runtime_error("CMD_STAGE_RESEED_CNT_ALERT[15] not set");
```

### SEP-21 — Medium — `sep_memory` has no executable tests

**Build:** `sep/peripherals/sep_memory/CMakeLists.txt:60-67`

The runner builds the library but does not test byte enables, bounds, DMI,
cross-page accesses, ROM writes, debug loading, invalid commands, or 64-bit
addresses.

Recommended minimum wiring:

```cmake
add_executable(sep_memory_tb test/sep_memory_tb.cpp)
target_link_libraries(sep_memory_tb PRIVATE sep_memory_model)
add_test(NAME sep_memory_tb COMMAND sep_memory_tb)
```

The test should be added to Release, ASan/UBSan, and coverage phases.

### SEP-22 — Low — CPU coverage collection fails before its gate

**Build:** `sep/cpu/CMakeLists.txt:256-260`

On the reviewed macOS toolchain, tests pass but lcov aborts on an out-of-range
SDK header line. The advertised wrapper coverage gate is therefore not run.

Recommended lcov invocation:

```cmake
COMMAND ${LCOV_EXECUTABLE} ${LCOV_IGNORE_FLAGS}
        --filter range
        --capture --directory ${CMAKE_BINARY_DIR}
        --output-file ${CMAKE_BINARY_DIR}/coverage/coverage.info
```

Keep source filtering after capture and verify the same behavior on CI's lcov
version.

## Style and efficiency observations

### SEP-23 — Medium — Register behavior is duplicated across large hand-written classes

The entropy model demonstrates the maintenance risk: interrupt masks, lock
coverage, and status access types drifted independently from RDL. Use compact
tables for repeated register policy and shared `regmodel` helpers for W1C,
lock-gated, and byte-enable-aware writes. This reduces both review surface and
the chance that newly added fields miss callbacks.

Use descriptor tables that register callbacks on the owning
`regmodel::Memory`; SEP's `regmodel::Reg` surface does not provide the
`Register::on_write` API.

### SEP-24 — Medium — Transport validation should be uniform

Several model-specific sockets validate reads but acknowledge invalid writes.
Adopt one transaction-validation helper for command, pointer, size, alignment,
streaming width, byte enables, and offset. This improves style and avoids
different error responses for equivalent invalid accesses.

## Areas reviewed without retained high-confidence findings

- Scratch cold/warm storage
- Local alias remap
- Filter matching and lock behavior
- Mailbox FIFO/error semantics outside the KeyMgr mailbox
- Secure DMA
- Lifecycle controller
- SPI controller and SPI flash
- HMAC/KMAC/AES main datapaths
- CSRNG main state machine
- OTBN production transport and register model

This does not prove full RTL equivalence. It means no additional issue met the
evidence threshold in this pass.

## Verification performed

- `sep/peripherals/el2_pic/run_tests.sh` — passed.
- `sep/peripherals/el2_pic/run_tests.sh --asan` — passed.
- `sep/peripherals/el2_pic/run_tests.sh --coverage` — passed; reported 95.5%.
- `sep/cpu/run_tests.sh` — passed, 23/23.
- `sep/cpu/run_tests.sh --asan` — passed, 23/23.
- `sep/cpu/run_tests.sh --coverage` — tests passed; lcov collection failed as
  documented in SEP-22.
- `sep/peripherals/sep_memory/run_tests.sh` — build passed; no executable tests.

The remaining IPs were reviewed statically against source, tests, and RTL/RDL.
A complete all-peripheral Release/ASan/Coverage run was not repeated during
this report.

## Recommended remediation order

1. Fix SEP-02, SEP-04 through SEP-07, SEP-10 through SEP-15, and SEP-19.
2. Correct tests that currently bless wrong behavior before changing models.
3. Reconcile register maps with one pinned RDL revision.
4. Add `sep_memory` tests and invalid-transport tests.
5. Run separate Release, ASan/UBSan, and coverage suites for every changed IP.

