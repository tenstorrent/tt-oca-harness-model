# SMC Code Review

Date: 2026-09-21

## Scope

This review covers hand-written production code and testbenches under `smc/`
and `aou/`. It excludes build/log output and
`smc/cpu_cluster/external/`. Functional comparisons use the RTL/RDL tree at:

`/Users/pdroy/Library/CloudStorage/GoogleDrive-pdroy@tenstorrent.com/My Drive/tt-oca-harness`

Severity definitions match `sep-code-review.md`. A **Critical** finding means
the model's primary firmware interface is incompatible with the RTL contract.

## Alignment update — 2026-09-22

The artifact-alignment branch resolves the integration portion of this review
against harness `main` at `3753f0ff`:

- I3C regular-transfer descriptors now use the RTL HCI field positions and the
  controller has an architectural reset input.
- SMC peripheral interrupts consistently include the 256-source external band;
  firmware PLIC IDs now match `tt_smc_interrupts.h`.
- The VP-only AOU IRQ no longer occupies the RTL SEP-WDT slot.
- DFX and NDM-reset apertures have named platform routes instead of being
  swallowed by broad catch-all decode.

SMC-01 and the architectural-reset portion of SMC-02 are resolved by this
branch. Cycle-accurate FSM, timing, surrogate-IP, byte-strobe, and CI findings
below remain the follow-up backlog.

## Executive summary

The most serious issue is that the I3C model uses a private command-descriptor
layout explicitly documented as incompatible with the HCI descriptor consumed
by RTL. Other recurring problems are missing architectural reset ports,
clock/FSM behavior collapsed into synchronous register callbacks, ignored AXI
byte strobes, current tests encoding model-only behavior, and CI omissions.

The canonical `smc_axi_extension` is used consistently in the reviewed
peripherals; no duplicate extension definition was found.

## Correctness findings

### SMC-01 — Critical — I3C descriptors are not HCI compatible

**Model:** `smc/peripherals/i3c_controller/src/i3c_controller.cpp:30-49`

**Reference:** `vendor/chipsalliance/i3c-core/upstream/src/ctrl/flow_active.sv:284,358-365`

The model decodes RNW at bit 7, device index at `[14:8]`, and CCC at `[39:32]`.
RTL uses RNW bit 29, device index `[20:16]`, and CCC `[14:7]`. Existing tests
construct the private model encoding. Production firmware descriptors will be
misinterpreted.

Recommended decoding:

```cpp
constexpr bool cmd_rnw(uint64_t d)       { return (d >> 29) & 1u; }
constexpr uint8_t cmd_devidx(uint64_t d) { return (d >> 16) & 0x1fu; }
constexpr uint8_t cmd_ccc(uint64_t d)    { return (d >> 7) & 0xffu; }
constexpr uint16_t cmd_length(uint64_t d){ return d >> 48; }
```

Build test descriptors from the HCI definition or firmware headers, not model
helpers.

### SMC-02 — High — I3C has no architectural reset input

**Model:** `smc/peripherals/i3c_controller/include/i3c_controller.h:421-432`

**Reference:** `hw/ip/i3ccore_wrap/rtl/i3ccore_wrapper.sv:27-30`

Platform reset cannot clear CSRs, queues, DAT/DCT state, pending transfers, or
IRQs. Software reset is not equivalent to the block reset.

Recommended shape:

```cpp
sc_core::sc_in<bool> rst_n_i{"rst_n_i"};

void reset_proc() {
    if (rst_n_i.read()) return;
    xfer_event_.cancel();
    xfer_pending_.clear();
    for (auto& state : inst_) state = make_reset_state();
    schedule_recompute();
}
```

Bind the port in the SMC platform and test reset during an active transfer.

### SMC-03 — High — Telemetry interrupt enable has reversed semantics

**Model:** `smc/peripherals/telemetry_receiver/src/telemetry_receiver.cpp:210-267`

**Reference:** `hw/ip/telemetry_receiver/rtl/telemetry_receiver.sv:289-295`

The model latches `MISSING_LAST` only while enabled, then drives IRQ directly
from the sticky status. RTL always latches status and applies enable only at
the IRQ output. Events while disabled disappear, and disabling a latched event
fails to lower IRQ.

Recommended split:

```cpp
void set_missing_last_status() {
    missing_last_status_ = true;
}

const bool missing_last_irq =
    missing_last_status_ &&
    (intr_enable_.raw() & cfg::INTR_MISSING_LAST);
```

Test status and IRQ independently before and after enable changes.

### SMC-04 — High — I2C target-mode controls are stored but not implemented

**Model:** `smc/peripherals/i2c_controller/src/i2c_controller.cpp:521-676`

**Reference:** `hw/ip/i2c/rtl/i2c_core.sv:749-758`

Start/Stop acquisition entries are always generated despite
`ACQ_START_STOP_EN`; ACK-count control, stretch, decrement, and software NACK
behavior are absent. Firmware sees a different acquisition stream in the
default configuration.

Recommended direction:

```cpp
const bool include_symbols =
    regmodel::bit(ctrl_.read(), i2c_ctrl::ACQ_START_STOP_EN);
if (include_symbols)
    acq_push(address_byte, i2c_acq_signal::Start);
```

Model ACK-control as explicit state and test the default-disabled mode first.

### SMC-05 — High — I2C executes incomplete controller transactions

**Model:** `smc/peripherals/i2c_controller/src/i2c_controller.cpp:369-372`

**Reference:** `hw/ip/i2c/rtl/i2c_controller_fsm.sv:383-685`

`drain_fmt()` executes a trailing open segment without STOP or repeated START.
RTL waits for more FMT entries. Separate software writes can therefore split
one transaction or create a premature NACK.

Recommended state handling:

```cpp
if (entry.start && segment_.open)
    execute_segment(segment_);
append_entry(segment_, entry);
if (entry.stop) {
    execute_segment(segment_);
    segment_.reset();
}
// Never execute a trailing open segment.
```

### SMC-06 — High — WDT combined FEED+KEY bypasses locking

**Model:** `smc/peripherals/wdt/src/wdt.cpp:461-470`

**Reference:** `hw/sys/smc/rtl/smc_cpu/chipyard_generated_files/4core/OCAH4CORECluster_WatchdogTimer.sv:140-145,194-199`

The model applies KEY before FEED in one 64-bit transaction. RTL authorizes
FEED from the pre-edge unlock state; simultaneous FEED prevents that KEY from
unlocking the same transaction. The existing test expects the wrong result.

Recommended logic:

```cpp
const bool was_unlocked = unlocked_;
if (addr == wdt_cfg::OFF_FEED && was_unlocked &&
    lo == wdt_cfg::FEED_MAGIC)
    do_feed(lo);
unlocked_ = false;
```

### SMC-07 — Medium — WDT bypasses the reset synchronizer

**Model:** `smc/peripherals/wdt/src/wdt.cpp:100-108`

**Reference:** `hw/sys/smc/rtl/smc_cpu/chipyard_generated_files/4core/OCAH4CORECluster_WatchdogTimer.sv:140-151`

`core_rst_i` is consumed directly while RTL uses two flip-flops. Start/stop
timing differs and short asynchronous pulses are not filtered.

```cpp
const bool synchronized_reset = core_rst_sync1_;
core_rst_sync1_ = core_rst_sync0_;
core_rst_sync0_ = core_rst_i.read();
```

### SMC-08 — Medium — BEU `ACCRUED_ENABLE` is modeled as clear-only

**Model:** `smc/peripherals/beu/src/beu.cpp:147-152`

**Reference:** `hw/sys/smc/rtl/smc_cpu/chipyard_generated_files/4core/OCAH4CORECluster_BusErrorUnit.sv:217-236`

RDL defines normal RW replacement. `accrued_ &= data` prevents setting a
previously clear bit.

```cpp
accrued_ = regmodel::apply_write_mask(
    accrued_, data, beu_cfg::VALID_MASK);
```

Replace the test that asserts software cannot set these bits.

### SMC-09 — Medium — CPU stage-2 watchdog asserts one cycle early

**Model:** `smc/peripherals/cpu_ctrl/src/cpu_ctrl.cpp:536-557`

**Reference:** `hw/sys/smc/rtl/smc_cpu/smc_cpu_ctrl_wrap.sv:156-180`

The model asserts when decrement reaches zero. RTL's output flop observes the
previous cycle's zero state, so assertion occurs on the following clock.

```cpp
const bool zero_before_tick = std::any_of(
    wdt_stage2_count_.begin(), wdt_stage2_count_.end(),
    [](uint32_t v) { return v == 0; });
decrement_counters();
wdt_second_timeout_ = zero_before_tick;
```

### SMC-10 — Medium — CPU-control masks are incomplete

**Model:** `smc/peripherals/cpu_ctrl/src/cpu_ctrl.cpp:390-401`

**Reference:** `hw/sys/smc/regs/blocks/cpu_ctrl/cpu_ctrl.rdl:219-268`

`WB_PC` drops valid bits 57:56, and unrestricted `SMC_ATTRIBUTES` backdoor
writes expose reserved bits.

```cpp
constexpr uint64_t kWbPcMask = (uint64_t{1} << 58) - 1;
constexpr uint64_t kAttributesMask = 0x0000'3fff'ffff'ff01ULL;
wb_pc_[core][slot].set_raw(pc & kWbPcMask);
smc_attributes_ = value & kAttributesMask;
```

### SMC-11 — High — DMA lacks reset and completion IRQ interfaces

**Model:** `smc/peripherals/dma/include/dma.h:163-174`

**Reference:** `hw/ip/idma_wrapper/rtl/idma_wrapper.sv:45-50,160-173`

The model exposes only TLM sockets. RTL has `rst_ni`, busy, and a one-cycle
completion pulse. Work cannot be reset/aborted and firmware cannot observe
completion IRQ.

Add the ports and drive completion on the busy falling edge:

```cpp
const bool completion = previous_busy_ && !busy_;
dma_intp_o.write(completion);
previous_busy_ = busy_;
```

### SMC-12 — Medium — DMA ignores register masks and byte strobes

**Model:** `smc/peripherals/dma/src/dma.cpp:94-139,300-303`

**Reference:** `vendor/pulp-platform/idma/overlay/rdl/dma_ctrl.rdl:58-99`

Only CONFIG bits `[10:0]` exist. The model stores all 32 bits and never applies
byte enables; its test expects full `0x12345678` readback.

```cpp
uint32_t merged = config_;
for (unsigned lane = 0; lane < 4; ++lane) {
    if (byte_enable_mask & (1u << lane)) {
        const uint32_t mask = 0xffu << (lane * 8);
        merged = (merged & ~mask) | (data & mask);
    }
}
config_ = merged & 0x7ffu;
```

### SMC-13 — High — Memory zeroer collapses its asynchronous FSM

**Model:** `smc/peripherals/memory_zeroer/src/memory_zeroer.cpp:115-139`

**Reference:** `hw/ip/zeroer/rtl/zeroer.sv:268-275,390-401`

The full operation runs inside the control-register write. Busy is not
observable, reset cannot interrupt work, and IRQ latches high instead of
pulsing after outstanding responses complete.

Recommended structure:

```cpp
void start_zero() {
    state_ = state::issue;
    busy_o.write(true);
    work_event_.notify(sc_core::SC_ZERO_TIME);
}

void finish_zero() {
    busy_o.write(false);
    irq_o.write(interrupt_enabled());
    irq_clear_event_.notify(clock_period_);
}
```

Use an `SC_THREAD` or scheduled state steps; do not perform the transfer from
the register callback.

### SMC-14 — High — Cold reset does not cancel pending FLR events

**Model:** `smc/peripherals/reset_unit/src/reset_unit.cpp:245-286`

**Reference:** `hw/sys/smc/rtl/smc_reset_unit/rtl/smc_cool_reset_wrap.sv:493-510`

Register state is cleared, but scheduled assert/deassert events survive and
`flr_cool_n_` is not restored. A stale event can assert reset after cold-reset
release.

```cpp
flr_assert_event_.cancel();
flr_deassert_event_.cancel();
flr_cool_n_ = true;
```

Test cold reset during both FLR delay and active pulse.

### SMC-15 — Medium — Reset-unit rejects byte strobes supported by RTL

**Model:** `smc/peripherals/reset_unit/src/reset_unit.cpp:500-504`

**Reference:** `hw/sys/smc/rtl/smc_reset_unit/rtl/smc_subsystem_resets.sv:76-89`

RTL merges `wr_biten`; the model returns `TLM_BYTE_ENABLE_ERROR_RESPONSE`.
Implement strobe merge through `regmodel` and change the negative test.

### SMC-16 — Medium — Boot ROM omits runtime endian flipping

**Model:** `smc/peripherals/bootrom/src/bootrom.cpp:222-225`

**Reference:** `hw/sys/smc/rtl/smc_cpu/mem_swaps/4core/tilelink_to_rom_mem.sv:85-106`

The model always copies bytes unchanged. RTL can reverse each 64-bit word under
`rom_flip_endianness_i`.

```cpp
uint64_t word;
std::memcpy(&word, source, sizeof word);
if (rom_flip_endianness_i.read()) {
    uint64_t swapped = 0;
    for (unsigned byte = 0; byte < 8; ++byte)
        swapped |= ((word >> (byte * 8)) & 0xffu) << ((7 - byte) * 8);
    word = swapped;
}
std::memcpy(destination, &word, sizeof word);
```

Handle partial and unaligned reads explicitly rather than assuming full words.

### SMC-17 — High — PVT and PLL models do not match current harness placeholders

**Models:** `smc/peripherals/pvt_wrap/src/pvt_wrap.cpp:190-209`,
`smc/peripherals/pll_wrapper/include/pll_wrapper.h:5-17`

**Reference:** `hw/sys/smc/dv/models/regs/{pvt_wrap,pll_wrap}.rdl`

The current harness implements all-zero OKAY placeholders. SystemC invents
writable sensor/control CSRs, interrupts, PLL blocks, and synthetic lock
completion. Tests require those extra behaviors.

This needs an explicit product decision, not a silent patch:

```cpp
enum class adopter_model_mode { harness_placeholder, functional_surrogate };
```

Default to the harness contract for equivalence testing. If the functional
surrogate is intentionally retained, expose an immutable CCI mode and document
that it is not RTL-equivalent.

### SMC-18 — High — Fabric ignores programmable `REGION_SIZE`

**Model:** `smc/smc_fabric/src/smc_fabric.cpp:231-246`

**Reference:** `hw/sys/smc/rtl/smc_fabric/smc_input_fabric/rtl/smc_input_fabric.sv:237-248`

Routing and local rebasing use hard-coded 16/32 MiB masks instead of the
programmed aperture. Tests verify CSR readback but not routing changes.

```cpp
bool in_window(uint64_t addr, uint64_t base, uint64_t size) {
    return size != 0 && addr >= base && addr - base < size;
}
```

Validate power-of-two sizes, avoid `base + size` overflow, and derive the
rebase mask from the active size.

### SMC-19 — High — `hart_id_base` has no effect

**Model:** `smc/cpu_cluster/src/smc_cpu_cluster.cpp:147-180`

The parameter is copied into the backend configuration, but the Whisper
backend does not use it when assigning `MHARTID`; harts still start at zero.
Until the backend supports the offset, fail elaboration instead of accepting a
setting that cannot work:

```cpp
if (hart_id_base_p_.get_value() != 0)
    SC_REPORT_FATAL(name(), "nonzero hart_id_base is not implemented");
```

### SMC-20 — High — CPU-control accepts `TLM_IGNORE_COMMAND` as write

**Model:** `smc/cpu_cluster/src/smc_cpu_cluster.cpp:560-612`

The decoder treats every non-read as a write, allowing an invalid command to
mutate control state.

```cpp
if (!trans.is_read() && !trans.is_write()) {
    trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
    trans.set_dmi_allowed(false);
    return;
}
```

Validate alignment, streaming width, and byte enables before decode.

### SMC-21 — High — Primary reset leaves CPU-control state dirty

**Model:** `smc/cpu_cluster/src/smc_cpu_cluster.cpp:129-136,560-648,793-799`

**Reference:** `hw/sys/smc/rtl/smc_cpu/smc_cpu_ctrl_wrap.sv:235-260,493-500`

The method sensitive to primary reset updates only watchdog state; no
corresponding reset of the CPU-control register image, mutexes, or semaphores
is present in the decoder state. RTL resets those structures. Test reset with
dirty state in each class.

### SMC-22 — High — CPU-cluster register image differs from RDL

**Model:** `smc/cpu_cluster/src/smc_cpu_cluster.cpp:640-648`

**Reference:** `hw/sys/smc/regs/blocks/cpu_ctrl/cpu_ctrl.rdl:259-268,369-373`

RDL allocates eight WB-PC slots per core. The model flattens addresses directly
to hart indices, so core-1's first slot is mapped incorrectly.

```cpp
const unsigned rel = unsigned(offset - 0x100);
const unsigned core = rel / 0x40;
const unsigned slot = (rel % 0x40) / 8;
```

Also populate all implemented `SMC_ATTRIBUTES` strap fields, not only hart
count.

### SMC-23 — High — AOU activation IRQ has opposite semantics

**Model:** `aou/src/aou_core.cpp:147-173`

**Reference:** `AOU_ACTIVATION_CTRL.sv:440-441`

The model raises `activate_start` IRQ after software activation succeeds. RTL
raises it when activation is needed and software has not yet started it.

```cpp
enabled_ = true;
activate_start_ = false;
// Successful software activation does not create an activation request IRQ.
update_irq();
```

Add a distinct external/requested-activation path.

### SMC-24 — High — AOU has no hardware reset port

**Model:** `aou/include/aou_core.h:55-61`

**Reference:** `AOU_ACTIVATION_CTRL.sv:446-470`

Only software reset exists, so platform reset cannot restore disabled state.
Add `rst_n_i`, bind it in the platform, cancel pending events, and test reset
from enabled state.

### SMC-25 — Medium — UART drains TX FIFO synchronously

**Model:** `smc/peripherals/uart/src/uart.cpp:290-306,404-412`

**Reference:** `hw/ip/uart/rtl/uart_core.sv:150-176`

Every enabled write empties the complete FIFO. Firmware cannot observe normal
FIFO fullness, `THRE/TEMT` transitions, DMA backpressure, or overflow.

```cpp
if (tx_enabled() && !tx_busy_)
    tx_event_.notify(character_time());
```

Pop one character when the scheduled serializer event completes.

### SMC-26 — Medium — AVSBus response CRC uses command bits

**Model:** `smc/peripherals/avsbus_controller/src/avsbus_controller.cpp:305-314`

**Reference:** `hw/ip/avsbus/rtl/avsbus_controller.sv:760-768`

Default response CRC is calculated from `cmd`, not the packed response.

```cpp
uint32_t frame = avs_rb::pack(avs_rb::ACK_OK, status, data, 0);
frame |= crc3(frame & ~uint32_t{0x7});
```

### SMC-27 — Medium — AVSBus reset leaves events armed

**Model:** `smc/peripherals/avsbus_controller/src/avsbus_controller.cpp:140-162`

Stale transfer/resync events can complete a post-reset command too early.

```cpp
xfer_event_.cancel();
resync_done_event_.cancel();
xfer_pending_ = false;
slave_in_resync_ = false;
```

### SMC-28 — Medium — Memory transport validation is overflow-prone

**Models:** `smc/peripherals/bootrom/src/bootrom.cpp:194-219`,
`smc/peripherals/scratchpad_ram/src/scratchpad_ram.cpp:242-264`

Both models use overflow-prone `addr + length > size` checks, and their normal
blocking paths do not consistently reject null data pointers before use.

```cpp
if (buf == nullptr || addr > size || length > size - addr) {
    gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    return;
}
```

Use `TLM_GENERIC_ERROR_RESPONSE` for null pointers and address error for bounds.

## Style, efficiency, and verification findings

### SMC-29 — Medium — TLM payload policy is inconsistent

I2C, UART, AVSBus, and telemetry ignore streaming width and byte enables while
I3C rejects them. Centralize command, width, alignment, strobe, and pointer
validation. Where RTL supports WSTRB, merge enabled lanes rather than rejecting
them.

### SMC-30 — High — CI omits I2C, UART, and AVSBus

The local orchestrator lists all three, but `.github/workflows/ci.yml` and
`ci-rhel8.yml` invoke only I3C and telemetry from this serial group. Add the
missing targets to Release, ASan/UBSan, and coverage phases.

### SMC-31 — High — Sanitizer builds omit UBSan

`smc_fabric`, `cpu_cluster`, and `aou` configure only AddressSanitizer. The
repository gate requires both address and undefined-behavior checks.

```cmake
add_compile_options(-fsanitize=address,undefined
                    -fno-omit-frame-pointer -g)
add_link_options(-fsanitize=address,undefined)
```

Keep sanitizer and coverage builds separate.

### SMC-32 — Medium — Fabric configuration bypasses CCI

Routing size/base and timing remain constructor-only configuration. Structural
parameters should be immutable CCI values and access delay should be mutable,
re-read at each transaction.

### SMC-33 — Low — Scratchpad standalone tests use 64 KiB

The architectural capacity is 1 MiB. Platform presets correct it, but unit
tests never exercise the upper architectural range.

### SMC-34 — Low — Safe DMI opportunities are unused

Boot ROM and scratchpad force `dmi_allowed(false)`. Read-only ROM DMI could
materially reduce fetch overhead. Scratchpad DMI requires careful invalidation
or restricted access to preserve ECC behavior.

## Areas reviewed without retained high-confidence findings

- CLINT, PLIC, and OCTS timer core paths
- Canonical AXI extension declaration and inclusion
- Existing use of shared register-access helpers
- Normal-path scratchpad ECC behavior

## Verification status

This report is based on static source/test review and direct RTL/RDL
comparison. No complete SMC Release/ASan/Coverage sweep was run during this
pass. Existing tests were inspected where they intersect each finding; several
currently codify the behavior identified as divergent.

## Recommended remediation order

1. Replace the private I3C descriptor ABI and add architectural I3C reset.
2. Correct reset/interrupt/FSM behavior in telemetry, DMA, zeroer, AOU, and
   reset unit.
3. Fix fabric routing and CPU-cluster register/reset contracts.
4. Reconcile PVT/PLL placeholder versus surrogate intent explicitly.
5. Centralize TLM validation and byte-strobe handling.
6. Add omitted CI targets and UBSan, then run separate Release, sanitizer, and
   coverage phases.

