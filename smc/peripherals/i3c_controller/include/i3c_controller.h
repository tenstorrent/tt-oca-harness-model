// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file i3c_controller.h
 * @brief SystemC/TLM-2.0 Loosely-Timed (LT) model of the OCA I3C Controller.
 *
 * This module is a **transaction-level, register-accurate model** of the
 * multi-instance OCA I3C Core Controller (`i3ccore_wrapper`) instantiated in
 * the System Management Controller (SMC) peripheral sub-system.  It is
 * intended for software bring-up, firmware development, and integration
 * testing — not for micro-architectural timing, bit-level SCL/SDA waveform,
 * or clock-domain-crossing (CDC) verification.
 *
 * The OCA I3C controller is a MIPI I3C Basic v1.0/v1.1.1 + HCI v1.2 compliant
 * communication controller (CHIPS-Alliance i3c-core fork with OCA
 * enhancements) supporting active-controller, secondary-controller, and
 * target operation with I2C backward compatibility.  The RTL wraps up to
 * `MAX_NUM_I3CS` (6) independent instances behind a single AXI4-Lite slave,
 * decoding the instance from the access address (`INSTANCE_SPACING = 0x1000`).
 *
 * ---
 * ## Authoritative references (tt-oca-hw)
 *
 * | Source | Role |
 * |--------|------|
 * | `hw/periph/i3ccore_wrap/rtl/i3ccore_wrapper.sv`        | **Multi-instance top** (AXI-Lite demux by address) |
 * | `hw/periph/i3ccore_wrap/rtl/i3c_wrapper.sv`            | Per-instance wrapper (CSR + bus signal routing) |
 * | `hw/periph/i3ccore_wrap/rtl/i3c.sv`                    | Main I3C module (HCI queues, FSMs, PHY) |
 * | `hw/periph/i3ccore_wrap/rtl/i3ccore_wrap_pkg.sv`       | `MAX_NUM_I3CS`, `I3C_INSTANCE_SPACING`, `I3C_REG_ADDR_WIDTH` |
 * | `hw/periph/i3ccore_wrap/data/registers/rdl/oca_i3c_wrap.rdl` | `I3CCSR` register block @ 0x0 |
 * | `hw/periph/i3ccore_wrap/doc/architecture.adoc`         | Block diagram, transaction flows |
 * | `hw/periph/i3ccore_wrap/doc/memmap.adoc`               | **Register map** (HCI v1.2 + OCA extensions) |
 * | `hw/smc/smc_peripherals/rtl/smc_peripherals.sv`        | `u_i3ccore_wrapper` instantiation (NUM_I3C=6) |
 * | `hw/smc/smc_config_pkg.sv`                             | `NUM_I3C = 6` |
 * | `hw/smc/data/registers/svh/smc_top_reg.svh`           | `OCA_I3C_WRAP_0_REG_MAP_BASE_ADDR = 0xC000_5000` |
 *
 * ---
 * ## Register map (per instance, 11-bit window 0x000..0x4FF)
 *
 * All registers are 32-bit and accept only 32-bit, naturally-aligned access.
 *
 * ```
 * Offset  Name                    SW    Reset        Notes
 * ─────────────────────────────────────────────────────────────────────────
 * 0x00    HCI_VERSION             r     0x00000120   HCI v1.2
 * 0x04    HC_CONTROL              rw    0x00000040   bit31 BUS_ENABLE, bit29 ABORT(self-clr),
 *                                                    bit30 RESUME(self-clr), bit8 HOT_JOIN_CTRL,
 *                                                    bit7 I2C_DEV_PRESENT, bit6 MODE_SELECTOR(ro=1),
 *                                                    bit0 IBA_INCLUDE
 * 0x08    CONTROLLER_DEVICE_ADDR  rw    0x00000000   [22:16] DYNAMIC_ADDR, bit31 DYNAMIC_ADDR_VALID
 * 0x0C    HC_CAPABILITIES         r     (cfg)        feature flags
 * 0x10    RESET_CONTROL           rw    0x00000000   self-clearing: bit0 SOFT_RST, bit1 CMD_QUEUE_RST,
 *                                                    bit2 RESP_QUEUE_RST, bit3 TX_FIFO_RST,
 *                                                    bit4 RX_FIFO_RST, bit5 IBI_QUEUE_RST
 * 0x14    PRESENT_STATE           r     (hw)         bit2 AC_CURRENT_OWN
 * 0x20    INTR_STATUS             rw1c  0x00000000   bits10..14 HC error/status
 * 0x24    INTR_STATUS_ENABLE      rw    0x00000000
 * 0x28    INTR_SIGNAL_ENABLE      rw    0x00000000
 * 0x2C    INTR_FORCE              w     0x00000000   write-1 sets matching INTR_STATUS bit
 * 0x30    DAT_SECTION_OFFSET      r     (cfg)        [11:0] offset=0x300, [18:12] size, [23:19] entry
 * 0x34    DCT_SECTION_OFFSET      rw    (cfg)        [11:0] offset=0x400, [18:12] size, [23:19] entry,
 *                                                    [31:19] ENTDAA index (rw)
 * 0x3C    PIO_SECTION_OFFSET      r     0x00000080
 * 0x80    COMMAND_PORT            w     —            64-bit descriptor (two 32-bit writes; 2nd enqueues)
 * 0x84    RESPONSE_PORT           r     —            pops a 32-bit response descriptor
 * 0x88    XFER_DATA_PORT          rw    —            write=TX FIFO push, read=RX FIFO pop
 * 0x8C    IBI_PORT                r     —            pops IBI status / data
 * 0x90    QUEUE_THLD_CTRL         rw    0x01010101   [7:0] CMD_EMPTY, [15:8] RESP_BUF,
 *                                                    [23:16] IBI_DATA_SEG, [31:24] IBI_STATUS
 * 0x94    DATA_BUFFER_THLD_CTRL   rw    0x01010101   [2:0] TX_BUF, [10:8] RX_BUF,
 *                                                    [18:16] TX_START, [26:24] RX_START
 * 0x98    QUEUE_SIZE              r     (cfg)        [7:0] CR_QUEUE, [15:8] IBI_STATUS,
 *                                                    [23:16] RX_DATA_BUF, [31:24] TX_DATA_BUF
 * 0x9C    ALT_QUEUE_SIZE         r     0x00000000
 * 0xA0    PIO_INTR_STATUS         rw1c  0x00000000   see @ref pio_intr
 * 0xA4    PIO_INTR_STATUS_ENABLE  rw    0x00000000
 * 0xA8    PIO_INTR_SIGNAL_ENABLE  rw    0x00000000
 * 0xAC    PIO_CONTROL             rw    0x00000003   bit0 ENABLE, bit1 RS, bit2 ABORT
 * 0x100   STBY_CR_EXTCAP_HEADER   r     0x00000012   CAP_ID=0x12
 * 0x104   STBY_CR_CONTROL         rw    0x00000000
 * 0x108   STBY_CR_DEVICE_ADDR     rw    0x00000000
 * 0x10C   STBY_CR_CAPABILITIES    r     (cfg)
 * 0x300   DAT_BASE  .. 0x3FC      rw    0x00000000   128 entries × 2 DWORDs (Device Address Table)
 * 0x400   DCT_BASE  .. 0x4FC      rw    0x00000000   128 entries × 4 DWORDs (Device Config Table)
 * ```
 *
 * Unimplemented offsets inside the window are RAZ/WI.  Accesses outside the
 * decoded `NUM_I3C × INSTANCE_SPACING` aperture → `TLM_ADDRESS_ERROR_RESPONSE`.
 *
 * ---
 * ## Functional model of the HCI transaction engine
 *
 * The RTL command/response/TX/RX/IBI FIFOs and the controller/PHY FSMs are
 * abstracted to a **descriptor-level functional engine** (the bit-level
 * SCL/SDA signalling, OD/PP timing parameters, and CDC synchronisers are not
 * modelled — they are firmware-irrelevant, mirroring the reset_unit's
 * abstraction of its de-glitch/extend counters):
 *
 * 1. Firmware pushes a 64-bit command descriptor (two writes to COMMAND_PORT)
 *    and, for writes, the payload DWORDs (TX FIFO via XFER_DATA_PORT).
 * 2. When `HC_CONTROL.BUS_ENABLE = 1`, completing a descriptor schedules a
 *    transaction `xfer_delay_ns` later.
 * 3. The engine resolves the target dynamic address from `DAT[dev_index]`,
 *    then calls the registered **bus model** (@ref bus_model_fn) — a
 *    test-bench-supplied callback that emulates the I3C bus / targets.  With
 *    no bus model attached the address is NACKed (ERROR_ADDRESS_NACK).
 * 4. Writes drain `data_length` bytes from the TX FIFO; reads push the
 *    returned bytes into the RX FIFO.  Under/overflow → ERROR.
 * 5. A 32-bit response descriptor (error / data-length / TID) is pushed to the
 *    response queue and `PIO_INTR_STATUS.RESP_READY_STAT` is raised.
 *
 * Targets raise In-Band Interrupts through @ref inject_ibi, which enqueues an
 * IBI status descriptor (+ optional payload) and raises
 * `PIO_INTR_STATUS.IBI_STATUS_THLD_STAT`.
 *
 * Per-instance `irq_o[i]` is the OR of the enabled-and-signalled INTR_STATUS
 * and PIO_INTR_STATUS bits.
 *
 * ---
 * ## CCI configuration parameters
 *
 * | Name              | Type     | Default | Mutability | Purpose |
 * |-------------------|----------|---------|------------|---------|
 * | `num_instances`   | unsigned | 6       | immutable  | I3C instances (1..6); sizes irq_o/bus ports. |
 * | `access_delay_ns` | double   | 2.0     | mutable    | TLM `b_transport` annotated delay (AXI-Lite latency). |
 * | `xfer_delay_ns`   | double   | 100.0   | mutable    | Modelled transaction-processing latency. |
 * | `cmd_fifo_depth`  | unsigned | 8       | immutable  | Command/response queue depth. |
 * | `rx_fifo_depth`   | unsigned | 64      | immutable  | RX data FIFO depth (DWORDs). |
 * | `tx_fifo_depth`   | unsigned | 64      | immutable  | TX data FIFO depth (DWORDs). |
 * | `ibi_fifo_depth`  | unsigned | 8       | immutable  | IBI status/data queue depth (DWORDs). |
 */

#ifndef SMC_I3C_CONTROLLER_H_
#define SMC_I3C_CONTROLLER_H_

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include <cci_configuration>

#include "smc_axi_extension.h"

namespace smc {

// ---------------------------------------------------------------------------
// Bus-model transaction interface (functional abstraction of the I3C bus)
// ---------------------------------------------------------------------------

/// Transfer type carried in a command descriptor / presented to the bus model.
enum class i3c_xfer_kind : uint8_t {
    PrivateWrite = 0, ///< Controller → target private write (R/W = 0).
    PrivateRead  = 1, ///< Controller ← target private read  (R/W = 1).
    CccWrite     = 2, ///< Broadcast/direct CCC write.
    CccRead      = 3  ///< Direct CCC read.
};

/// Response-descriptor error codes (TCRI 6.4.1 Table 1 / RESPONSE_PORT [31:28]).
enum class i3c_err : uint8_t {
    Success                      = 0x0, ///< Completed without error.
    CrcError                     = 0x1, ///< CRC failure (HDR modes).
    ParityError                  = 0x2, ///< Parity error.
    FrameError                   = 0x3, ///< Framing error.
    AddrHeader                   = 0x4, ///< Address header error.
    Nack                         = 0x5, ///< Address or DAA was NACK'ed.
    AddressNack                  = Nack, ///< Alias of Nack (legacy name).
    Ovl                          = 0x6, ///< RX overflow or TX underflow.
    OverflowUnder                = Ovl, ///< Alias of Ovl (legacy name).
    I3cShortReadErr              = 0x7, ///< Short read not permitted.
    HcAborted                    = 0x8, ///< Terminated by host controller (Abort).
    TransferAbort                = HcAborted, ///< Alias of HcAborted (legacy name).
    I2cDataNackOrI3cBusAborted   = 0x9, ///< I2C write-data NACK or I3C bus abort.
    I2cWNack                     = I2cDataNackOrI3cBusAborted, ///< Alias (legacy name).
    I2cDataNack                  = I2cDataNackOrI3cBusAborted, ///< Alias (legacy name).
    NotSupported                 = 0xA, ///< Command not supported by the HC.
    AbortedWithCRC               = 0xB  ///< HDR-BT abort / default error status.
};

/**
 * @brief A transaction presented by the controller to the (modelled) I3C bus.
 *
 * The test bench supplies a @ref bus_model_fn that consumes this request and
 * fills in the result fields.  Addresses are 7-bit dynamic addresses resolved
 * from the Device Address Table.
 */
struct i3c_xfer {
    // ---- request (controller → bus) ----
    i3c_xfer_kind        kind        = i3c_xfer_kind::PrivateWrite;
    uint8_t              dynamic_addr = 0;   ///< 7-bit target dynamic address (from DAT).
    uint8_t              dev_index    = 0;   ///< DAT index from the command descriptor.
    uint8_t              ccc_code     = 0;   ///< CCC command byte (CccWrite/CccRead).
    uint16_t             data_length  = 0;   ///< Bytes to transfer (write: TX bytes, read: requested).
    std::vector<uint8_t> write_data;        ///< Payload for writes (data_length bytes).

    // ---- result (bus → controller); filled by the bus model ----
    bool                 ack       = false; ///< Target ACKed its address.
    std::vector<uint8_t> read_data;         ///< Payload for reads (≤ data_length bytes).
    i3c_err              error     = i3c_err::AddressNack; ///< Final transaction status.
};

/// Test-bench-supplied bus model: emulate the I3C bus/targets for one transfer.
using bus_model_fn = std::function<void(i3c_xfer&)>;

// ---------------------------------------------------------------------------
// i3c_controller_cfg
// ---------------------------------------------------------------------------

/**
 * @brief Compile-time defaults and address-map constants for the I3C controller.
 *
 * `num_instances`, the FIFO depths, and the delays may be overridden via CCI
 * presets.  The register offsets are fixed by the I3CCSR map and must not be
 * changed.
 */
struct i3c_controller_cfg {
    /// Number of I3C instances (1..MAX_INSTANCES). Default 6 matches NUM_I3C.
    unsigned num_instances = 6;

    /// TLM `b_transport` annotated delay (ns) — approximates AXI-Lite latency.
    double access_delay_ns = 2.0;

    /// Modelled transaction-processing latency (ns) from enqueue to response.
    double xfer_delay_ns = 100.0;

    /// HCI command/response queue depth (entries).
    unsigned cmd_fifo_depth = 8;
    /// RX data FIFO depth (DWORDs).
    unsigned rx_fifo_depth = 64;
    /// TX data FIFO depth (DWORDs).
    unsigned tx_fifo_depth = 64;
    /// IBI status/data queue depth (DWORDs).
    unsigned ibi_fifo_depth = 8;

    // ------------------------------------------------------------------
    // Per-instance register byte offsets (I3CCSR map, memmap.adoc).
    // ------------------------------------------------------------------
    static constexpr uint64_t HCI_VERSION            = 0x00;
    static constexpr uint64_t HC_CONTROL             = 0x04;
    static constexpr uint64_t CONTROLLER_DEVICE_ADDR = 0x08;
    static constexpr uint64_t HC_CAPABILITIES        = 0x0C;
    static constexpr uint64_t RESET_CONTROL          = 0x10;
    static constexpr uint64_t PRESENT_STATE          = 0x14;
    static constexpr uint64_t INTR_STATUS            = 0x20;
    static constexpr uint64_t INTR_STATUS_ENABLE     = 0x24;
    static constexpr uint64_t INTR_SIGNAL_ENABLE     = 0x28;
    static constexpr uint64_t INTR_FORCE             = 0x2C;
    static constexpr uint64_t DAT_SECTION_OFFSET     = 0x30;
    static constexpr uint64_t DCT_SECTION_OFFSET     = 0x34;
    static constexpr uint64_t PIO_SECTION_OFFSET     = 0x3C;
    static constexpr uint64_t COMMAND_PORT           = 0x80;
    static constexpr uint64_t RESPONSE_PORT          = 0x84;
    static constexpr uint64_t XFER_DATA_PORT         = 0x88;
    static constexpr uint64_t IBI_PORT               = 0x8C;
    static constexpr uint64_t QUEUE_THLD_CTRL        = 0x90;
    static constexpr uint64_t DATA_BUFFER_THLD_CTRL  = 0x94;
    static constexpr uint64_t QUEUE_SIZE             = 0x98;
    static constexpr uint64_t ALT_QUEUE_SIZE         = 0x9C;
    static constexpr uint64_t PIO_INTR_STATUS        = 0xA0;
    static constexpr uint64_t PIO_INTR_STATUS_ENABLE = 0xA4;
    static constexpr uint64_t PIO_INTR_SIGNAL_ENABLE = 0xA8;
    static constexpr uint64_t PIO_CONTROL            = 0xAC;
    static constexpr uint64_t STBY_CR_EXTCAP_HEADER  = 0x100;
    static constexpr uint64_t STBY_CR_CONTROL        = 0x104;
    static constexpr uint64_t STBY_CR_DEVICE_ADDR    = 0x108;
    static constexpr uint64_t STBY_CR_CAPABILITIES   = 0x10C;

    /// CSR-visible Device Address Table direct-access window (0x300..0x3FF):
    /// 64 DWORDs = 32 entries × 2 DWORDs.  (The architectural 128-entry table
    /// lives in external memory exported via the RTL `dat_mem` interface, which
    /// is abstracted away in this LT model — only the CSR window is modelled.)
    static constexpr uint64_t DAT_BASE    = 0x300;
    static constexpr uint64_t DAT_END     = 0x400; ///< exclusive
    static constexpr unsigned DAT_WORDS   = 64;    ///< (DAT_END-DAT_BASE)/4
    static constexpr unsigned DAT_ENTRIES = 32;    ///< 2 DWORDs per entry
    static constexpr unsigned DAT_DWORDS  = 2;

    /// CSR-visible Device Configuration Table window (0x400..0x4FF):
    /// 64 DWORDs = 16 entries × 4 DWORDs.
    static constexpr uint64_t DCT_BASE    = 0x400;
    static constexpr uint64_t DCT_END     = 0x500; ///< exclusive
    static constexpr unsigned DCT_WORDS   = 64;    ///< (DCT_END-DCT_BASE)/4
    static constexpr unsigned DCT_ENTRIES = 16;    ///< 4 DWORDs per entry
    static constexpr unsigned DCT_DWORDS  = 4;

    /// Per-instance decoded window (i3ccore_wrap_pkg::I3C_INSTANCE_SPACING
    /// / I3C_REG_ADDR_WIDTH = 0x1000 / 12 bits). The live HCI map ends at
    /// 0x4FF; 0x500..0xFFF is RAZ/WI inside the instance window.
    static constexpr uint64_t INSTANCE_SPACING = 0x1000;

    /// Reset values.
    static constexpr uint32_t HCI_VERSION_VALUE      = 0x0000'0120;
    static constexpr uint32_t HC_CONTROL_RESET       = 0x0000'0040; ///< MODE_SELECTOR=1.
    static constexpr uint32_t THLD_CTRL_RESET        = 0x0101'0101;
    static constexpr uint32_t PIO_CONTROL_RESET      = 0x0000'0003; ///< ENABLE|RS.
    static constexpr uint32_t STBY_CR_EXTCAP_HDR     = 0x0000'0012; ///< CAP_ID=0x12.

    /// Per-IP base address inside the SMC fabric (informational; the model
    /// exposes offsets relative to its own aperture). smc_top.rdl:
    /// `oca_i3c_wrap_0 @ 0xC003_A000`.
    static constexpr uint64_t SMC_BASE_ADDR = 0xC003'A000ULL;

    /// Hard upper bound on instances (i3ccore_wrap_pkg::MAX_NUM_I3CS).
    static constexpr unsigned MAX_INSTANCES = 6;
};

// ---------------------------------------------------------------------------
// HC_CONTROL bit positions
// ---------------------------------------------------------------------------
namespace hc_control {
constexpr unsigned BUS_ENABLE    = 31;
constexpr unsigned RESUME        = 30;
constexpr unsigned ABORT         = 29;
constexpr unsigned HALT_ON_TO    = 12;
constexpr unsigned HOT_JOIN_CTRL = 8;
constexpr unsigned I2C_DEV_PRES  = 7;
constexpr unsigned MODE_SELECTOR = 6; ///< Read-only, always 1 (PIO mode).
constexpr unsigned IBA_INCLUDE   = 0;
constexpr uint32_t WMASK = (1u << BUS_ENABLE) | (1u << RESUME) | (1u << ABORT) |
                           (1u << HALT_ON_TO) | (1u << HOT_JOIN_CTRL) |
                           (1u << I2C_DEV_PRES) | (1u << IBA_INCLUDE);
} // namespace hc_control

// ---------------------------------------------------------------------------
// RESET_CONTROL bit positions (all self-clearing)
// ---------------------------------------------------------------------------
namespace reset_control {
constexpr unsigned SOFT_RST       = 0;
constexpr unsigned CMD_QUEUE_RST  = 1;
constexpr unsigned RESP_QUEUE_RST = 2;
constexpr unsigned TX_FIFO_RST    = 3;
constexpr unsigned RX_FIFO_RST    = 4;
constexpr unsigned IBI_QUEUE_RST  = 5;
constexpr uint32_t MASK = 0x3F;
} // namespace reset_control

// ---------------------------------------------------------------------------
// PIO_INTR_STATUS bit positions
// ---------------------------------------------------------------------------
/// @anchor pio_intr PIO interrupt-status bit layout (write-1-to-clear).
namespace pio_intr {
constexpr unsigned TX_THLD_STAT         = 0; ///< TX FIFO free ≥ threshold.
constexpr unsigned RX_THLD_STAT         = 1; ///< RX FIFO level ≥ threshold.
constexpr unsigned IBI_STATUS_THLD_STAT = 2; ///< IBI queue level ≥ threshold.
constexpr unsigned CMD_QUEUE_READY_STAT = 3; ///< Command queue free ≥ threshold.
constexpr unsigned RESP_READY_STAT      = 4; ///< Response queue level ≥ threshold.
constexpr unsigned TRANSFER_ABORT_STAT  = 5; ///< Transfer aborted.
constexpr unsigned TRANSFER_ERR_STAT    = 9; ///< Transfer error (NACK/over/underflow).
constexpr uint32_t W1C_MASK = (1u << TX_THLD_STAT) | (1u << RX_THLD_STAT) |
                              (1u << IBI_STATUS_THLD_STAT) |
                              (1u << CMD_QUEUE_READY_STAT) |
                              (1u << RESP_READY_STAT) |
                              (1u << TRANSFER_ABORT_STAT) |
                              (1u << TRANSFER_ERR_STAT);
} // namespace pio_intr

/// INTR_STATUS bit positions (HC-level; write-1-to-clear).
namespace hc_intr {
constexpr unsigned HC_INTERNAL_ERR_STAT       = 10;
constexpr unsigned HC_SEQ_CANCEL_STAT         = 11;
constexpr unsigned HC_WARN_CMD_SEQ_STALL_STAT = 12;
constexpr unsigned HC_ERR_CMD_SEQ_TIMEOUT     = 13;
constexpr unsigned SCHED_CMD_MISSED_TICK      = 14;
constexpr uint32_t W1C_MASK = (1u << HC_INTERNAL_ERR_STAT) |
                              (1u << HC_SEQ_CANCEL_STAT) |
                              (1u << HC_WARN_CMD_SEQ_STALL_STAT) |
                              (1u << HC_ERR_CMD_SEQ_TIMEOUT) |
                              (1u << SCHED_CMD_MISSED_TICK);
} // namespace hc_intr

// ---------------------------------------------------------------------------
// i3c_controller
// ---------------------------------------------------------------------------

/**
 * @brief SystemC/TLM-2.0 LT model of the OCA multi-instance I3C controller.
 *
 * ### Ports
 *
 * | Port           | Dir | Description |
 * |----------------|-----|-------------|
 * | `reg_socket`   | tgt | AXI4-Lite-style TLM-2.0 target socket (multi-instance, 32-bit). |
 * | `irq_o[i]`     | out | Per-instance interrupt request (active-high). |
 * | `scl_o[i]`     | out | Per-instance SCL drive (idle high; bit-level abstracted). |
 * | `sda_o[i]`     | out | Per-instance SDA drive (idle high; bit-level abstracted). |
 * | `scl_oe_o[i]`  | out | Per-instance SCL output-enable (idle 0). |
 * | `sda_oe_o[i]`  | out | Per-instance SDA output-enable (idle 0). |
 * | `sel_od_pp_o[i]`| out| Per-instance open-drain(0)/push-pull(1) select. |
 * | `recovery_payload_available_o[i]` | out | TCRI recovery payload available. |
 * | `recovery_image_activated_o[i]`   | out | TCRI recovery image activated. |
 *
 * The single `reg_socket` carries the whole `NUM_I3C × INSTANCE_SPACING`
 * aperture; the instance is decoded from the access offset, exactly as the
 * RTL's `axi_lite_demux`.
 *
 * `output_method` is the sole driver of every output signal (single-driver
 * discipline, as in the reset_unit/PLIC/CLINT models); all other code paths
 * mutate internal state and call `schedule_recompute()`.
 */
class i3c_controller : public sc_core::sc_module {
protected:
    // CCI parameters (declared before ports so sizes resolve first).
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> num_instances_p_;
    cci::cci_param<double>                             access_delay_ns_p_;
    cci::cci_param<double>                             xfer_delay_ns_p_;
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> cmd_fifo_depth_p_;
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> rx_fifo_depth_p_;
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> tx_fifo_depth_p_;
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> ibi_fifo_depth_p_;

public:
    SC_HAS_PROCESS(i3c_controller);

    /// AXI4-Lite-style TLM-2.0 target socket (multi-instance, 32-bit access).
    tlm_utils::simple_target_socket<i3c_controller> reg_socket;
    sc_core::sc_in<bool> rst_n_i{"rst_n_i"};

    // ---- Outputs (per instance) -----------------------------------------
    sc_core::sc_vector<sc_core::sc_out<bool>> irq_o;
    sc_core::sc_vector<sc_core::sc_out<bool>> scl_o;
    sc_core::sc_vector<sc_core::sc_out<bool>> sda_o;
    sc_core::sc_vector<sc_core::sc_out<bool>> scl_oe_o;
    sc_core::sc_vector<sc_core::sc_out<bool>> sda_oe_o;
    sc_core::sc_vector<sc_core::sc_out<bool>> sel_od_pp_o;
    sc_core::sc_vector<sc_core::sc_out<bool>> recovery_payload_available_o;
    sc_core::sc_vector<sc_core::sc_out<bool>> recovery_image_activated_o;

    /**
     * @brief Construct the I3C controller model.
     * @param name SystemC module name.
     * @param cfg  Sizing / timing defaults.  CCI presets take priority.
     */
    explicit i3c_controller(sc_core::sc_module_name name,
                            i3c_controller_cfg cfg = i3c_controller_cfg{});

    // ------------------------------------------------------------------
    // Configuration / debug API
    // ------------------------------------------------------------------

    /// Attach a bus model (target emulator) to instance @p inst.
    void set_bus_model(unsigned inst, bus_model_fn fn);

    /// Inject an In-Band Interrupt from a target into instance @p inst:
    /// enqueues an IBI status descriptor (source @p addr, payload size) plus
    /// the payload DWORDs and raises IBI_STATUS_THLD_STAT.
    /// @returns false if the IBI queue cannot hold the descriptor + payload.
    bool inject_ibi(unsigned inst, uint8_t addr,
                    const std::vector<uint8_t>& payload = {});

    /// Back-door read of a register by aperture offset (no side effects, no delay).
    uint32_t dbg_read(uint64_t off) const;

    /// Number of instances (CCI-resolved value).
    unsigned num_instances() const { return cfg_.num_instances; }

    /// Print a human-readable snapshot of instance @p inst.
    void dump_state(unsigned inst, std::ostream& os = std::cout) const;

private:
    // ------------------------------------------------------------------
    // Per-instance state
    // ------------------------------------------------------------------
    struct inst_state {
        // CSR storage.
        uint32_t hc_control             = i3c_controller_cfg::HC_CONTROL_RESET;
        uint32_t controller_device_addr = 0;
        uint32_t intr_status            = 0;
        uint32_t intr_status_enable     = 0;
        uint32_t intr_signal_enable     = 0;
        uint32_t dct_section_offset     = 0; ///< ENTDAA index field is RW.
        uint32_t queue_thld_ctrl        = i3c_controller_cfg::THLD_CTRL_RESET;
        uint32_t data_buffer_thld_ctrl  = i3c_controller_cfg::THLD_CTRL_RESET;
        uint32_t pio_intr_status        = 0;
        uint32_t pio_intr_status_enable = 0;
        uint32_t pio_intr_signal_enable = 0;
        uint32_t pio_control            = i3c_controller_cfg::PIO_CONTROL_RESET;
        uint32_t stby_cr_control        = 0;
        uint32_t stby_cr_device_addr    = 0;

        // Device tables.
        std::vector<uint32_t> dat; ///< DAT_ENTRIES*DAT_DWORDS DWORDs.
        std::vector<uint32_t> dct; ///< DCT_ENTRIES*DCT_DWORDS DWORDs.

        // HCI queues.
        std::deque<uint64_t> cmd_q;  ///< Pending 64-bit command descriptors.
        std::deque<uint32_t> resp_q; ///< Response descriptors.
        std::deque<uint32_t> tx_q;   ///< TX data DWORDs.
        std::deque<uint32_t> rx_q;   ///< RX data DWORDs.
        std::deque<uint32_t> ibi_q;  ///< IBI status + data DWORDs.

        // Command-port assembly (64-bit descriptor = two 32-bit writes).
        uint32_t cmd_lo      = 0;
        bool     cmd_lo_seen = false;

        // Bus model (target emulator) for this instance.
        bus_model_fn bus_model;

        // Output idempotence caches.
        bool cache_irq = false;

        // Cached per-instance current values (avoid spurious events).
        bool valid = false;
    };

    std::vector<inst_state> inst_;

    // ------------------------------------------------------------------
    // TLM-2.0 callbacks
    // ------------------------------------------------------------------
    void         b_transport (tlm::tlm_generic_payload& gp, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& gp);
    /// Always denies DMI — FIFO ports pop on read and a CSR write can start a
    /// transfer, so a direct memory pointer would bypass real side effects.
    bool         get_direct_mem_ptr(tlm::tlm_generic_payload& gp,
                                    tlm::tlm_dmi& dmi_data);

    // ------------------------------------------------------------------
    // SC_METHOD processes
    // ------------------------------------------------------------------
    void output_method();
    void xfer_method();
    void reset_method();
    void schedule_recompute();
    void schedule_xfer();
    void start_of_simulation() override;

    // ------------------------------------------------------------------
    // Register decode helpers (operate on a resolved instance index)
    // ------------------------------------------------------------------
    // Unimplemented offsets inside an instance window are RAZ/WI, so these
    // always resolve; they return void rather than an ignorable bool.
    void reg_read (unsigned inst, uint64_t loff, uint32_t& data);
    void reg_write(unsigned inst, uint64_t loff, uint32_t data);

    // ------------------------------------------------------------------
    // HCI behaviour helpers
    // ------------------------------------------------------------------
    void apply_reset_control(inst_state& s, uint32_t data);
    void enqueue_command    (inst_state& s, uint64_t desc);
    void process_command    (unsigned inst);
    uint32_t compute_queue_size(const inst_state& s) const;
    uint32_t compute_present_state(const inst_state& s) const;
    uint32_t compute_dat_section_offset() const;
    uint32_t compute_dct_section_offset(const inst_state& s) const;
    uint32_t compute_hc_capabilities() const;
    bool     irq_level(const inst_state& s) const;

    // Threshold helpers.
    unsigned tx_buf_thld (const inst_state& s) const;
    unsigned rx_buf_thld (const inst_state& s) const;
    unsigned cmd_empty_thld(const inst_state& s) const;
    unsigned resp_buf_thld(const inst_state& s) const;
    unsigned ibi_status_thld(const inst_state& s) const;

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------
    i3c_controller_cfg cfg_;

    sc_core::sc_event recompute_event_; ///< Triggers output_method.
    sc_core::sc_event xfer_event_;      ///< Triggers transaction processing.

    /// Instances with a pending transaction to process (FIFO order).
    std::deque<unsigned> xfer_pending_;

    /// True while `xfer_event_` carries an outstanding timed notification.
    /// Guards against re-notifying, which would move an already-scheduled
    /// command's due time (see schedule_xfer()).
    bool xfer_scheduled_ = false;

    sc_core::sc_time access_delay_;
    sc_core::sc_time xfer_delay_;
};

} // namespace smc

#endif // SMC_I3C_CONTROLLER_H_
