// SPDX-License-Identifier: Apache-2.0
/**
 * @file i2c_controller.h
 * @brief SystemC/TLM-2.0 Loosely-Timed (LT) model of the OCA I2C Controller.
 *
 * This module is a **transaction-level, register-accurate model** of a single
 * OCA I2C core instance (the `i2c` block instantiated by the `i2c_wrap`
 * peripheral wrapper in the SMC chiplet).  It is intended for firmware
 * bring-up, driver development, and integration testing — not for
 * micro-architectural timing, bit-level SCL/SDA waveform, or clock-domain
 * crossing verification.
 *
 * The OCA I2C core is an OpenTitan-derived controller/target that supports
 * Controller Mode, Target Mode, Hybrid Mode, and Monitor Mode with four
 * FIFOs (Controller TX "FMT", Controller RX, Target TX, Target RX "ACQ"),
 * SMBus features, and a 20-source interrupt block.  The `i2c_wrap` RTL packs
 * `NUM_I2CS` (3) such cores plus a wrapper enable register behind one AXI
 * slave; this model reproduces **one** core (the natural unit named
 * `i2c_controller`) presented in its own 0x200-byte register window, exactly
 * as `uart` models a single UART instance.
 *
 * ---
 * ## Authoritative references (tt-oca-hw)
 *
 * | Source | Role |
 * |--------|------|
 * | `hw/comp/i2c/data/registers/rdl/i2c.rdl`        | **Ground-truth** register map (this model) |
 * | `hw/comp/i2c/doc/architecture.adoc`             | Block diagram, FSMs, FIFOs |
 * | `hw/comp/i2c/doc/programming.adoc`              | Controller/Target programming sequences |
 * | `hw/comp/i2c/doc/memmap.adoc`                    | Register offsets |
 * | `hw/periph/i2c_wrap/data/registers/rdl/i2c_wrap.rdl` | Multi-instance packing (i2c[3] @0x0 += 0x200, i2c_ctrl @0xe00) |
 * | `hw/periph/i2c_wrap/data/registers/rdl/i2c_ctrl.rdl` | Wrapper enable register (out of scope here) |
 *
 * ---
 * ## Register map (per core, 0x200-byte window; all 32-bit, word-aligned)
 *
 * ```
 * Offset  Name                       SW   Notes
 * ─────────────────────────────────────────────────────────────────────────
 * 0x00    INTR_STATE                 rw   20 interrupt sources (mix of level + W1C)
 * 0x04    INTR_ENABLE                rw   per-source enable
 * 0x08    INTR_TEST                  wo   force interrupts (level bits held, pulse bits latched)
 * 0x0C    SMBUS_CTRL                 rw   SMBSUS / SMBALERT drive
 * 0x10    CTRL                       rw   ENABLEHOST/ENABLETARGET/LLPBK/… mode select
 * 0x14    STATUS                     ro   FIFO full/empty + idle flags (computed)
 * 0x18    RDATA                      ro   pop Controller RX FIFO
 * 0x1C    FDATA                      wo   push Controller TX (FMT) FIFO
 * 0x20    FIFO_CTRL                  wo   self-clearing FIFO resets
 * 0x24    HOST_FIFO_CONFIG           rw   RX_THRESH[11:0] / FMT_THRESH[27:16]
 * 0x28    TARGET_FIFO_CONFIG         rw   TX_THRESH[11:0] / ACQ_THRESH[27:16]
 * 0x2C    HOST_FIFO_STATUS           ro   FMTLVL[11:0] / RXLVL[27:16] (computed)
 * 0x30    TARGET_FIFO_STATUS         ro   TXLVL[11:0] / ACQLVL[27:16] (computed)
 * 0x34    OVRD                       rw   SDA/SCL override (line-level; abstracted)
 * 0x38    VAL                        ro   oversampled SCL/SDA (abstracted -> 0)
 * 0x3C    TIMING0                    rw   THIGH/TLOW
 * 0x40    TIMING1                    rw   T_R/T_F
 * 0x44    TIMING2                    rw   TSU_STA/THD_STA
 * 0x48    TIMING3                    rw   TSU_DAT/THD_DAT
 * 0x4C    TIMING4                    rw   TSU_STO/T_BUF
 * 0x50    TIMEOUT_CTRL               rw   VAL[29:0]/MODE/EN
 * 0x54    TARGET_ID                  rw   ADDRESS0/MASK0/ADDRESS1/MASK1
 * 0x58    ACQDATA                    ro   pop Target RX (ACQ) FIFO (ABYTE + SIGNAL)
 * 0x5C    TXDATA                     wo   push Target TX FIFO
 * 0x60    HOST_TIMEOUT_CTRL          rw   VAL[30:0]
 * 0x64    TARGET_TIMEOUT_CTRL        rw   VAL[30:0]/EN
 * 0x68    TARGET_NACK_COUNT          rw   saturating NACK counter, read-clear
 * 0x6C    TARGET_ACK_CTRL            rw   NBYTES[8:0] / NACK(pulse)
 * 0x70    ACQ_FIFO_NEXT_DATA         ro   next ACQ byte (computed)
 * 0x74    HOST_NACK_HANDLER_TIMEOUT  rw   VAL[30:0]/EN
 * 0x78    CONTROLLER_EVENTS          rw   NACK/…/ARBITRATION_LOST (W1C; gate CONTROLLER_HALT)
 * 0x7C    TARGET_EVENTS              rw   TX_PENDING/…/START_DETECT/STOP_DETECT (W1C)
 * 0x80    SMBUS_STATUS               ro   SMBSUS / SMBALERT input status (abstracted)
 * ```
 *
 * Offsets inside the window with no register decode to
 * `TLM_ADDRESS_ERROR_RESPONSE` (same policy as the UART model).
 *
 * ---
 * ## Functional model
 *
 * The bit-level SCL/SDA signalling, timing parameters (TIMING0..4, the various
 * timeouts), oversampling (VAL), and line overrides (OVRD) are **firmware
 * visible but functionally abstracted** — they are read/write storage with no
 * modelled waveform, mirroring how `i3c_controller` abstracts its OD/PP timing.
 *
 * ### Controller Mode
 * Firmware pushes format entries (`FDATA`: byte + START/STOP/READB/RCONT/NAKOK
 * flags) into the FMT FIFO.  When `CTRL.ENABLEHOST = 1`, the engine drains the
 * FMT FIFO `xfer_delay_ns` later, reassembling it into per-address **segments**
 * and presenting each to a test-bench-supplied @ref i2c_bus_model_fn (an
 * emulator of the I2C bus / remote targets).  Read data returned by the bus
 * model is pushed into the Controller RX FIFO (readable via `RDATA`); an
 * address NACK (without `NAKOK`) sets `CONTROLLER_EVENTS.NACK`, halts the
 * controller, and raises `CONTROLLER_HALT`.  Each STOP raises `CMD_COMPLETE`.
 *
 * ### Target Mode
 * When `CTRL.ENABLETARGET = 1`, an external controller is emulated through the
 * @ref target_write / @ref target_read back doors.  A matching address (per
 * `TARGET_ID`) fills the ACQ FIFO with signalled entries (Start/data/Stop) or
 * drains the Target TX FIFO for reads; a non-matching address is NACKed and
 * bumps `TARGET_NACK_COUNT`.
 *
 * ### Loosely-timed
 * `b_transport` never calls `wait()`; it only adds `access_delay_ns` to the
 * annotated `delay`.  Temporal decoupling (the quantum keeper) is the
 * initiator's responsibility — see the test bench driver.
 *
 * ---
 * ## CCI configuration parameters
 *
 * | Name              | Type     | Default | Mutability | Purpose |
 * |-------------------|----------|---------|------------|---------|
 * | `fmt_fifo_depth`  | unsigned | 64      | immutable  | Controller TX (FMT) FIFO depth (entries). |
 * | `rx_fifo_depth`   | unsigned | 64      | immutable  | Controller RX FIFO depth (bytes). |
 * | `tx_fifo_depth`   | unsigned | 64      | immutable  | Target TX FIFO depth (bytes). |
 * | `acq_fifo_depth`  | unsigned | 64      | immutable  | Target RX (ACQ) FIFO depth (entries). |
 * | `access_delay_ns` | double   | 2.0     | mutable    | TLM `b_transport` annotated delay (AXI-Lite latency). |
 * | `xfer_delay_ns`   | double   | 100.0   | mutable    | Modelled FMT-drain / transaction latency. |
 */

#ifndef SMC_I2C_CONTROLLER_H_
#define SMC_I2C_CONTROLLER_H_

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <vector>

#include <cci_configuration>

#include "reg_access.h"
#include "reg_map.h"
#include "smc_tlm_extensions.h"

namespace smc {

// ---------------------------------------------------------------------------
// Bus-model transaction interface (functional abstraction of the I2C bus)
// ---------------------------------------------------------------------------

/// Direction of an I2C address segment.
enum class i2c_dir : uint8_t {
    Write = 0, ///< Controller -> target write (R/W bit = 0).
    Read  = 1  ///< Controller <- target read  (R/W bit = 1).
};

/**
 * @brief One address segment presented by the controller to the (modelled) bus.
 *
 * A segment spans one addressed phase of a transaction: a START/repeated-START
 * with an address byte, followed by the write bytes or the requested read
 * length up to the next START or STOP.  The test bench supplies an
 * @ref i2c_bus_model_fn that consumes the request fields and fills the result
 * fields.
 */
struct i2c_xfer {
    // ---- request (controller -> bus) ----
    uint8_t              addr       = 0;                 ///< 7-bit target address.
    i2c_dir              dir        = i2c_dir::Write;    ///< Segment direction.
    std::vector<uint8_t> write_data;                     ///< Write payload (dir == Write).
    unsigned             read_len   = 0;                 ///< Requested byte count (dir == Read).

    // ---- result (bus -> controller); filled by the bus model ----
    bool                 ack        = false;             ///< Target ACKed its address.
    std::vector<uint8_t> read_data;                      ///< Read payload (<= read_len bytes).
};

/// Test-bench-supplied bus model: emulate the I2C bus/targets for one segment.
using i2c_bus_model_fn = std::function<void(i2c_xfer&)>;

// ---------------------------------------------------------------------------
// i2c_controller_cfg
// ---------------------------------------------------------------------------

/**
 * @brief Sizing / timing defaults and fixed address-map constants.
 *
 * The FIFO depths and delays supply the default values for the CCI params; a
 * broker preset set before construction wins.  The register offsets, field
 * masks, and bit positions are compile-time fixed by `i2c.rdl`.
 */
struct i2c_controller_cfg {
    unsigned fmt_fifo_depth = 64; ///< Controller TX (FMT) FIFO depth (entries).
    unsigned rx_fifo_depth  = 64; ///< Controller RX FIFO depth (bytes).
    unsigned tx_fifo_depth  = 64; ///< Target TX FIFO depth (bytes).
    unsigned acq_fifo_depth = 64; ///< Target RX (ACQ) FIFO depth (entries).

    double access_delay_ns = 2.0;   ///< TLM annotated delay (AXI-Lite latency).
    double xfer_delay_ns   = 100.0; ///< FMT-drain / transaction latency.

    /// Decoded register window size in bytes (per-core stride from i2c_wrap.rdl).
    static constexpr uint64_t WINDOW_SIZE = 0x200;
    /// Register access stride in bytes (32-bit registers).
    static constexpr unsigned REG_WIDTH = 4;

    // ---- Register byte offsets (i2c.rdl address map) --------------------
    static constexpr uint64_t INTR_STATE                = 0x00;
    static constexpr uint64_t INTR_ENABLE               = 0x04;
    static constexpr uint64_t INTR_TEST                 = 0x08;
    static constexpr uint64_t SMBUS_CTRL                = 0x0C;
    static constexpr uint64_t CTRL                      = 0x10;
    static constexpr uint64_t STATUS                    = 0x14;
    static constexpr uint64_t RDATA                     = 0x18;
    static constexpr uint64_t FDATA                     = 0x1C;
    static constexpr uint64_t FIFO_CTRL                 = 0x20;
    static constexpr uint64_t HOST_FIFO_CONFIG          = 0x24;
    static constexpr uint64_t TARGET_FIFO_CONFIG        = 0x28;
    static constexpr uint64_t HOST_FIFO_STATUS          = 0x2C;
    static constexpr uint64_t TARGET_FIFO_STATUS        = 0x30;
    static constexpr uint64_t OVRD                      = 0x34;
    static constexpr uint64_t VAL                       = 0x38;
    static constexpr uint64_t TIMING0                   = 0x3C;
    static constexpr uint64_t TIMING1                   = 0x40;
    static constexpr uint64_t TIMING2                   = 0x44;
    static constexpr uint64_t TIMING3                   = 0x48;
    static constexpr uint64_t TIMING4                   = 0x4C;
    static constexpr uint64_t TIMEOUT_CTRL              = 0x50;
    static constexpr uint64_t TARGET_ID                 = 0x54;
    static constexpr uint64_t ACQDATA                   = 0x58;
    static constexpr uint64_t TXDATA                    = 0x5C;
    static constexpr uint64_t HOST_TIMEOUT_CTRL         = 0x60;
    static constexpr uint64_t TARGET_TIMEOUT_CTRL       = 0x64;
    static constexpr uint64_t TARGET_NACK_COUNT         = 0x68;
    static constexpr uint64_t TARGET_ACK_CTRL           = 0x6C;
    static constexpr uint64_t ACQ_FIFO_NEXT_DATA        = 0x70;
    static constexpr uint64_t HOST_NACK_HANDLER_TIMEOUT = 0x74;
    static constexpr uint64_t CONTROLLER_EVENTS         = 0x78;
    static constexpr uint64_t TARGET_EVENTS             = 0x7C;
    static constexpr uint64_t SMBUS_STATUS              = 0x80;

    // ---- Field masks (union of defined field bits, from i2c.rdl) --------
    static constexpr uint32_t SMBUS_CTRL_MASK      = 0x00000011u; ///< SMBSUS[0], SMBALERT[4]
    static constexpr uint32_t CTRL_MASK            = 0x000000FFu; ///< [7:0]
    static constexpr uint32_t FIFO_CONFIG_MASK     = 0x0FFF0FFFu; ///< [11:0] | [27:16]
    static constexpr uint32_t OVRD_MASK            = 0x00000007u; ///< [2:0]
    static constexpr uint32_t TIMING0_MASK         = 0x1FFF1FFFu; ///< THIGH[12:0]|TLOW[28:16]
    static constexpr uint32_t TIMING1_MASK         = 0x01FF03FFu; ///< T_R[9:0]|T_F[24:16]
    static constexpr uint32_t TIMING2_MASK         = 0x1FFF1FFFu; ///< TSU_STA[12:0]|THD_STA[28:16]
    static constexpr uint32_t TIMING3_MASK         = 0x1FFF01FFu; ///< TSU_DAT[8:0]|THD_DAT[28:16]
    static constexpr uint32_t TIMING4_MASK         = 0x1FFF1FFFu; ///< TSU_STO[12:0]|T_BUF[28:16]
    static constexpr uint32_t TIMEOUT_CTRL_MASK    = 0xFFFFFFFFu; ///< VAL[29:0]|MODE[30]|EN[31]
    static constexpr uint32_t TARGET_ID_MASK       = 0x0FFFFFFFu; ///< ADDRESS0/MASK0/ADDRESS1/MASK1
    static constexpr uint32_t HOST_TIMEOUT_MASK    = 0x7FFFFFFFu; ///< VAL[30:0]
    static constexpr uint32_t TARGET_TIMEOUT_MASK  = 0xFFFFFFFFu; ///< VAL[30:0]|EN[31]
    static constexpr uint32_t NACK_HANDLER_MASK    = 0xFFFFFFFFu; ///< VAL[30:0]|EN[31]
    static constexpr uint32_t TARGET_ACK_NBYTES    = 0x000001FFu; ///< NBYTES[8:0]
    static constexpr uint32_t CONTROLLER_EVT_MASK  = 0x0000000Fu; ///< [3:0]
    static constexpr uint32_t TARGET_EVT_MASK      = 0x0000001Fu; ///< [4:0]
};

// ---------------------------------------------------------------------------
// Interrupt source bit positions (INTR_STATE / INTR_ENABLE / INTR_TEST)
// ---------------------------------------------------------------------------
namespace i2c_intr {
constexpr unsigned FMT_THRESHOLD            = 0;
constexpr unsigned RX_THRESHOLD             = 1;
constexpr unsigned ACQ_THRESHOLD            = 2;
constexpr unsigned RX_OVERFLOW              = 3;
constexpr unsigned CONTROLLER_HALT          = 4;
constexpr unsigned SCL_INTERFERENCE         = 5;
constexpr unsigned SDA_INTERFERENCE         = 6;
constexpr unsigned STRETCH_TIMEOUT          = 7;
constexpr unsigned SDA_UNSTABLE             = 8;
constexpr unsigned CMD_COMPLETE             = 9;
constexpr unsigned TX_STRETCH               = 10;
constexpr unsigned TX_THRESHOLD             = 11;
constexpr unsigned ACQ_STRETCH              = 12;
constexpr unsigned UNEXP_STOP               = 13;
constexpr unsigned HOST_TIMEOUT             = 14;
constexpr unsigned SMBALERT                 = 15;
constexpr unsigned CONTROLLER_TX_FIFO_ERROR = 16;
constexpr unsigned CONTROLLER_RX_FIFO_ERROR = 17;
constexpr unsigned TARGET_TX_FIFO_ERROR     = 18;
constexpr unsigned TARGET_RX_FIFO_ERROR     = 19;

/// All 20 defined interrupt sources.
constexpr uint32_t ALL_MASK = 0x000FFFFFu;
/// Sources whose value is a hardware-computed level (sw = r).
constexpr uint32_t LEVEL_MASK =
    (1u << FMT_THRESHOLD) | (1u << RX_THRESHOLD) | (1u << ACQ_THRESHOLD) |
    (1u << CONTROLLER_HALT) | (1u << TX_STRETCH) | (1u << TX_THRESHOLD) |
    (1u << ACQ_STRETCH);
/// Sources that are latched events cleared by writing 1 (sw = rw, woclr).
constexpr uint32_t W1C_MASK = ALL_MASK & ~LEVEL_MASK;
} // namespace i2c_intr

// ---------------------------------------------------------------------------
// CTRL bit positions
// ---------------------------------------------------------------------------
namespace i2c_ctrl {
constexpr unsigned ENABLEHOST                 = 0;
constexpr unsigned ENABLETARGET               = 1;
constexpr unsigned LLPBK                       = 2;
constexpr unsigned NACK_ADDR_AFTER_TIMEOUT    = 3;
constexpr unsigned ACK_CTRL_EN                = 4;
constexpr unsigned MULTI_CONTROLLER_MONITOR_EN = 5;
constexpr unsigned TX_STRETCH_CTRL_EN         = 6;
constexpr unsigned ACQ_START_STOP_EN          = 7;
} // namespace i2c_ctrl

// ---------------------------------------------------------------------------
// STATUS bit positions
// ---------------------------------------------------------------------------
namespace i2c_status {
constexpr unsigned FMTFULL          = 0;
constexpr unsigned RXFULL           = 1;
constexpr unsigned FMTEMPTY         = 2;
constexpr unsigned HOSTIDLE         = 3;
constexpr unsigned TARGETIDLE       = 4;
constexpr unsigned RXEMPTY          = 5;
constexpr unsigned TXFULL           = 6;
constexpr unsigned ACQFULL          = 7;
constexpr unsigned TXEMPTY          = 8;
constexpr unsigned ACQEMPTY         = 9;
constexpr unsigned ACK_CTRL_STRETCH = 10;
} // namespace i2c_status

// ---------------------------------------------------------------------------
// FDATA bit positions (Controller TX format entry)
// ---------------------------------------------------------------------------
namespace i2c_fdata {
constexpr unsigned FBYTE_LSB = 0;   ///< FBYTE[7:0]
constexpr unsigned START     = 8;
constexpr unsigned STOP      = 9;
constexpr unsigned READB     = 10;
constexpr unsigned RCONT     = 11;
constexpr unsigned NAKOK     = 12;
} // namespace i2c_fdata

// ---------------------------------------------------------------------------
// FIFO_CTRL bit positions (all self-clearing)
// ---------------------------------------------------------------------------
namespace i2c_fifo_ctrl {
constexpr unsigned RXRST  = 0;
constexpr unsigned FMTRST = 1;
constexpr unsigned ACQRST = 7;
constexpr unsigned TXRST  = 8;
} // namespace i2c_fifo_ctrl

/// ACQ FIFO entry SIGNAL codes (ACQDATA.SIGNAL[10:8]).
enum class i2c_acq_signal : uint8_t {
    Data       = 0x0, ///< Ordinary ACKed data byte.
    Start      = 0x1, ///< Address byte preceded by a START.
    Stop       = 0x2, ///< STOP after ACKed data.
    Restart    = 0x3, ///< Address byte preceded by a repeated START.
    NackData   = 0x4, ///< NACKed data byte.
    NackStart  = 0x5, ///< Address byte whose following data were NACKed.
    Error      = 0x6  ///< Abnormal transaction termination.
};

// ---------------------------------------------------------------------------
// i2c_controller
// ---------------------------------------------------------------------------

/**
 * @brief SystemC/TLM-2.0 LT model of a single OCA I2C core.
 *
 * ### Ports
 *
 * | Port         | Dir | Description |
 * |--------------|-----|-------------|
 * | `reg_socket` | tgt | AXI4-Lite-style TLM-2.0 target socket (32-bit access). |
 * | `rst_n_i`    | in  | Active-low asynchronous reset. |
 * | `irq_o`      | out | Interrupt request (active-high; OR of enabled INTR_STATE bits). |
 *
 * `recompute_method` is the sole driver of `irq_o` (single-driver discipline,
 * as in the UART / reset_unit / PLIC models); every state-changing path calls
 * `schedule_recompute()`.
 */
class i2c_controller : public sc_core::sc_module {
protected:
    // CCI parameters (declared before ports so sizes resolve first).
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> fmt_fifo_depth_p_;
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> rx_fifo_depth_p_;
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> tx_fifo_depth_p_;
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> acq_fifo_depth_p_;
    cci::cci_param<double>                             access_delay_ns_p_;
    cci::cci_param<double>                             xfer_delay_ns_p_;

public:
    SC_HAS_PROCESS(i2c_controller);

    /// AXI4-Lite-style TLM-2.0 target socket (32-bit access).
    tlm_utils::simple_target_socket<i2c_controller> reg_socket;

    /// Active-low asynchronous reset.
    sc_core::sc_in<bool>  rst_n_i;
    /// Interrupt request output (active-high).
    sc_core::sc_out<bool> irq_o;

    /**
     * @brief Construct the I2C controller model.
     * @param name SystemC module name.
     * @param cfg  Sizing / timing defaults.  CCI presets take priority.
     */
    explicit i2c_controller(sc_core::sc_module_name name,
                            i2c_controller_cfg cfg = i2c_controller_cfg{});

    // ------------------------------------------------------------------
    // Test-bench back door (no socket, no bus side effects unless noted)
    // ------------------------------------------------------------------

    /// Attach a Controller-Mode bus model (emulates the remote target(s)).
    void set_bus_model(i2c_bus_model_fn fn);

    /**
     * @brief Emulate an external controller writing to this device (Target Mode).
     *
     * If @p addr matches `TARGET_ID` and `CTRL.ENABLETARGET = 1`, pushes a
     * Start entry (address byte) plus one ACQ entry per data byte, and (if
     * @p stop) a Stop entry, into the ACQ FIFO.
     * @return true if addressed (ACKed); false if not matched (NACKed).
     */
    bool target_write(uint8_t addr, const std::vector<uint8_t>& data,
                      bool stop = true);

    /**
     * @brief Emulate an external controller reading from this device (Target Mode).
     *
     * If @p addr matches `TARGET_ID`, pushes a Start entry and drains up to
     * @p nbytes from the Target TX FIFO into @p out.
     * @return true if addressed (ACKed); false if not matched (NACKed).
     */
    bool target_read(uint8_t addr, unsigned nbytes, std::vector<uint8_t>& out,
                     bool stop = true);

    /// Side-effect-free register peek (does NOT pop FIFOs or clear sticky bits).
    uint32_t dbg_reg(uint64_t off) const;

    /// Controller RX FIFO occupancy (bytes).
    unsigned dbg_rx_count() const;
    /// Controller TX (FMT) FIFO occupancy (entries).
    unsigned dbg_fmt_count() const;
    /// Target TX FIFO occupancy (bytes).
    unsigned dbg_tx_count() const;
    /// Target RX (ACQ) FIFO occupancy (entries).
    unsigned dbg_acq_count() const;

    /// Print a human-readable snapshot to @p os.
    void dump_state(std::ostream& os = std::cout) const;

private:
    // ------------------------------------------------------------------
    // TLM-2.0 callbacks
    // ------------------------------------------------------------------
    void         b_transport (tlm::tlm_generic_payload& gp, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& gp);

    // ------------------------------------------------------------------
    // SC_METHOD processes
    // ------------------------------------------------------------------
    void reset_proc();       ///< SC_METHOD on rst_n_i: clear all state.
    void recompute_method(); ///< SC_METHOD on recompute_event_: drive irq_o.
    void xfer_method();      ///< SC_METHOD on xfer_event_: drain the FMT FIFO.

    void schedule_recompute();
    void schedule_xfer();

    // ------------------------------------------------------------------
    // Register decode helpers
    // ------------------------------------------------------------------
    bool reg_read (uint64_t off, uint32_t& data);
    bool reg_write(uint64_t off, uint32_t data);

    // ------------------------------------------------------------------
    // Controller / Target datapath
    // ------------------------------------------------------------------
    void     push_fmt(uint32_t fdata);   ///< FDATA write: enqueue a format entry.
    uint8_t  pop_rx();                    ///< RDATA read: pop Controller RX FIFO.
    uint32_t pop_acq();                   ///< ACQDATA read: pop ACQ FIFO (byte|signal<<8).
    void     apply_fifo_ctrl(uint32_t d); ///< FIFO_CTRL write: self-clearing resets.
    void     drain_fmt();                 ///< Reassemble + execute FMT segments.
    bool     target_match(uint8_t addr) const;
    void     acq_push(uint8_t byte, i2c_acq_signal sig);
    void     bump_target_nack();

    // ------------------------------------------------------------------
    // Status / interrupt computation
    // ------------------------------------------------------------------
    uint32_t compute_status() const;
    uint32_t compute_host_fifo_status() const;
    uint32_t compute_target_fifo_status() const;
    uint32_t level_status() const;   ///< Hardware-computed INTR_STATE bits.
    uint32_t intr_state_read() const;///< Full software-visible INTR_STATE.
    bool     irq_active() const;

    // ------------------------------------------------------------------
    // Internal state
    // ------------------------------------------------------------------

    /// One Controller TX (FMT) FIFO entry (decoded FDATA fields).
    struct fmt_entry {
        uint8_t byte  = 0;
        bool    start = false;
        bool    stop  = false;
        bool    readb = false;
        bool    rcont = false;
        bool    nakok = false;
    };

    /// One Target RX (ACQ) FIFO entry.
    struct acq_entry {
        uint8_t        byte = 0;
        i2c_acq_signal sig  = i2c_acq_signal::Data;
    };

    i2c_controller_cfg cfg_;

    // Storage-backed registers owned by the offset->register dispatch table
    // (common/include/reg_map.h). Their mask contract is enforced by the type.
    regmodel::Register32 intr_enable_{i2c_intr::ALL_MASK, i2c_intr::ALL_MASK, 0};
    regmodel::Register32 smbus_ctrl_{i2c_controller_cfg::SMBUS_CTRL_MASK,
                                     i2c_controller_cfg::SMBUS_CTRL_MASK, 0};
    regmodel::Register32 ctrl_{i2c_controller_cfg::CTRL_MASK,
                               i2c_controller_cfg::CTRL_MASK, 0};
    regmodel::Register32 host_fifo_config_{i2c_controller_cfg::FIFO_CONFIG_MASK,
                                           i2c_controller_cfg::FIFO_CONFIG_MASK, 0};
    regmodel::Register32 target_fifo_config_{i2c_controller_cfg::FIFO_CONFIG_MASK,
                                             i2c_controller_cfg::FIFO_CONFIG_MASK, 0};
    regmodel::Register32 ovrd_{i2c_controller_cfg::OVRD_MASK,
                               i2c_controller_cfg::OVRD_MASK, 0};
    regmodel::Register32 val_{0xFFFFFFFFu, 0u, 0}; ///< RO (oversampled; abstracted to 0)
    regmodel::Register32 timing0_{i2c_controller_cfg::TIMING0_MASK,
                                  i2c_controller_cfg::TIMING0_MASK, 0};
    regmodel::Register32 timing1_{i2c_controller_cfg::TIMING1_MASK,
                                  i2c_controller_cfg::TIMING1_MASK, 0};
    regmodel::Register32 timing2_{i2c_controller_cfg::TIMING2_MASK,
                                  i2c_controller_cfg::TIMING2_MASK, 0};
    regmodel::Register32 timing3_{i2c_controller_cfg::TIMING3_MASK,
                                  i2c_controller_cfg::TIMING3_MASK, 0};
    regmodel::Register32 timing4_{i2c_controller_cfg::TIMING4_MASK,
                                  i2c_controller_cfg::TIMING4_MASK, 0};
    regmodel::Register32 timeout_ctrl_{i2c_controller_cfg::TIMEOUT_CTRL_MASK,
                                       i2c_controller_cfg::TIMEOUT_CTRL_MASK, 0};
    regmodel::Register32 target_id_{i2c_controller_cfg::TARGET_ID_MASK,
                                    i2c_controller_cfg::TARGET_ID_MASK, 0};
    regmodel::Register32 host_timeout_ctrl_{i2c_controller_cfg::HOST_TIMEOUT_MASK,
                                            i2c_controller_cfg::HOST_TIMEOUT_MASK, 0};
    regmodel::Register32 target_timeout_ctrl_{i2c_controller_cfg::TARGET_TIMEOUT_MASK,
                                              i2c_controller_cfg::TARGET_TIMEOUT_MASK, 0};
    regmodel::Register32 nack_handler_timeout_{i2c_controller_cfg::NACK_HANDLER_MASK,
                                               i2c_controller_cfg::NACK_HANDLER_MASK, 0};
    regmodel::Register32 smbus_status_{0xFFFFFFFFu, 0u, 0}; ///< RO (abstracted to 0)

    regmodel::RegisterMap32 regmap_; ///< offset -> storage register dispatch.

    // Registers with model-side behaviour (kept out of the map by design).
    uint32_t intr_latched_       = 0; ///< Latched W1C interrupt sources.
    uint32_t intr_force_         = 0; ///< INTR_TEST forced level sources.
    uint32_t controller_events_  = 0; ///< CONTROLLER_EVENTS (W1C).
    uint32_t target_events_      = 0; ///< TARGET_EVENTS (W1C).
    uint32_t target_ack_ctrl_    = 0; ///< TARGET_ACK_CTRL.NBYTES.
    uint32_t target_nack_count_  = 0; ///< TARGET_NACK_COUNT (saturating 8-bit).
    bool     halted_             = false; ///< Controller halted (needs event clear).

    std::deque<fmt_entry> fmt_; ///< Controller TX (FMT) FIFO.
    std::deque<uint8_t>   rx_;  ///< Controller RX FIFO.
    std::deque<uint8_t>   tx_;  ///< Target TX FIFO.
    std::deque<acq_entry> acq_; ///< Target RX (ACQ) FIFO.

    i2c_bus_model_fn bus_model_; ///< Controller-Mode remote-target emulator.

    // Output cache to suppress redundant sc_signal writes.
    bool out_irq_        = false;
    bool outputs_valid_  = false;

    sc_core::sc_event recompute_event_; ///< Triggers recompute_method().
    sc_core::sc_event xfer_event_;      ///< Triggers xfer_method().
    sc_core::sc_time  xfer_delay_;      ///< Modelled FMT-drain latency.
};

} // namespace smc

#endif // SMC_I2C_CONTROLLER_H_
