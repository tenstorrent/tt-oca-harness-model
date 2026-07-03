// SPDX-License-Identifier: Apache-2.0
/**
 * @file uart.h
 * @brief SystemC/TLM-2.0 Loosely-Timed (LT) model of the SMC UART 16550.
 *
 * This module is a **loosely-timed, transaction-level functional model** of
 * the NS16550A-compatible UART instantiated in the SMC chiplet.  It is
 * intended for firmware bring-up, console/debug I/O, and integration testing
 * — not for bit-level serial-timing analysis.
 *
 * ---
 * ## Authoritative references
 *
 * | Source | Role |
 * |--------|------|
 * | `hw/comp/uart_16550/data/registers/rdl/uart_16550_main.rdl`    | **Ground-truth** register map (DLAB=0 read map) |
 * | `hw/comp/uart_16550/data/registers/rdl/uart_16550_main_wo.rdl` | **Ground-truth** write-only map (THR, FCR) |
 * | `hw/comp/uart_16550/data/registers/rdl/uart_16550_dl.rdl`      | **Ground-truth** divisor-latch map (DLL, DLM) |
 * | `hw/comp/uart_16550/rtl/uart_16550.sv` | Top-level reference RTL (component, port names) |
 * | `hw/comp/uart_16550/rtl/uart_core.sv`  | Functional reference RTL — registers, FIFOs, interrupt/DMA |
 * | `hw/comp/uart_16550/rtl/uart_tx.sv`    | Transmit-engine reference RTL |
 * | `hw/comp/uart_16550/rtl/uart_rx.sv`    | Receive-engine reference RTL |
 * | NS16550A data sheet (National Semiconductor) | Industry-standard UART baseline |
 * | `01_UART_Specification.md`     | Externally-observable behaviour |
 * | `02_UART_LowLevel_Design.md`   | TLM-2.0 interface contract and internals |
 *
 * ---
 * ## Register map (offsets relative to the UART base address)
 *
 * ```
 * Offset  DLAB=0 (R / W)      DLAB=1      Access  Description
 * ─────────────────────────────────────────────────────────────────────────
 * 0x00    RBR / THR           DLL         RO / WO RX buffer (read) / TX hold (write)
 * 0x04    IER                 DLM         RW      Interrupt Enable / Divisor MSB
 * 0x08    IIR / FCR           —           RO / WO Interrupt ID (read) / FIFO Control (write)
 * 0x0C    LCR                 —           RW      Line Control (LCR[7] = DLAB)
 * 0x10    MCR                 —           RW      Modem Control
 * 0x14    LSR                 —           RO      Line Status
 * 0x18    MSR                 —           RO      Modem Status
 * 0x1C    SCR                 —           RW      Scratch
 * 0x20    ECR                 —           RW      Extended Control (RX trigger MS-2-bits)
 * 0x24    ITR                 —           RW      Interrupt Test
 * ```
 *
 * ---
 * ## Interrupt priority chain (encoded into IIR.INTERRUPT_ID)
 *
 * | Priority | IIR.INTERRUPT_ID | Source |
 * |----------|------------------|--------|
 * | 0 (highest) | 0x7 | FIFO Error |
 * | 1 | 0x3 | Receiver Line Status (OE/PE/FE/BI) |
 * | 2 | 0x6 | Reception Timeout |
 * | 3 | 0x2 | Received Data Ready (RX FIFO watermark) |
 * | 4 | 0x1 | Transmitter Holding Register Empty |
 * | 5 (lowest) | 0x0 | Modem Status (DCTS/DDSR/TERI/DDCD) |
 *
 * `IIR.INTERRUPT_PENDING` (bit 0) is active-low: 0 = interrupt pending.
 *
 * ---
 * ## Modeling notes
 *
 * - **Functional / character-granularity.** TX writes capture whole bytes;
 *   RX characters are delivered through the `inject_rx_char()` back door or
 *   via system loopback.  Bit-level serial timing is not modelled.
 * - **Single-driver discipline.** `update_outputs()` is the only writer of
 *   every `sc_out` port; all state-changing paths call `schedule_recompute()`.
 * - **Loosely-timed.** The model never calls `wait()` in `b_transport`; it
 *   only adds `access_delay_ns` to the annotated `delay`.  Temporal
 *   decoupling (quantum keeper) is the initiator's responsibility.
 * - **DMI** is never granted (RBR/IIR/MSR reads have side effects).
 */

#ifndef SMC_UART_H_
#define SMC_UART_H_

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>
#include <deque>
#include <iostream>

#include <cci_configuration>

#include "smc_tlm_extensions.h"

namespace smc {

// ---------------------------------------------------------------------------
// uart_cfg
// ---------------------------------------------------------------------------

/**
 * @brief Sizing defaults and fixed address-map constants for the SMC UART.
 *
 * `tx_fifo_depth` / `rx_fifo_depth` supply the **default values** for the
 * corresponding CCI params; a broker preset set before construction wins.
 * The address-map constants are compile-time fixed (they must never vary at
 * run-time) and are taken from the `uart_16550_*.rdl` register maps.
 */

/**
 * Register bit layouts (byte [7:0]; 32-bit bus access, upper bits RAZ).
 * Offsets relative to UART base. When LCR.DLAB=1, 0x00 and 0x04 decode to
 * DLL/DLM instead of RBR/THR/IER (registers at 0x08..0x24 are unchanged).
 *
 * RBR @ 0x00 (RO, DLAB=0) — Receiver Buffer Register:
 *   7:0
 * +--------+
 * |  DATA  |
 * +--------+
 *   [7:0] DATA  Received character; read pops RX FIFO (FIFO mode) or RBR
 *               (non-FIFO). Reading when empty returns 0x00. Clears RX timeout.
 *
 * THR @ 0x00 (WO, DLAB=0) — Transmitter Holding Register:
 *   7:0
 * +--------+
 * |  DATA  |
 * +--------+
 *   [7:0] DATA  Write pushes byte into TX FIFO (FIFO mode) or THR (non-FIFO).
 *               Silently dropped when TX FIFO is full.
 *
 * DLL @ 0x00 (RW, DLAB=1) — Divisor Latch LSB:
 *   7:0
 * +--------+
 * |  DLL   |
 * +--------+
 *   [7:0] DLL   Baud divisor bits [7:0]. Divisor 0 disables TX and RX.
 *               baud = system_clock / (16 × {DLM, DLL})
 *
 * IER @ 0x04 (RW, DLAB=0) — Interrupt Enable Register:
 *   7:5    4     3     2     1     0
 * +-----+-----+-----+-----+-----+-----+
 * |  R  |EFEI |EDSSI| ELSI|ETBEI|ERBFI|
 * +-----+-----+-----+-----+-----+-----+
 *   [0]   ERBFI     Enable Received-Data-Ready / RX-timeout interrupt
 *   [1]   ETBEI     Enable Transmitter-Holding-Register-Empty interrupt
 *   [2]   ELSI      Enable Receiver-Line-Status interrupt (OE/PE/FE/BI)
 *   [3]   EDSSI     Enable Modem-Status interrupt (DCTS/DDSR/TERI/DDCD)
 *   [4]   EFEI      Enable FIFO-Error interrupt
 *   [7:5] —         Reserved (read 0, write ignored)
 *
 * DLM @ 0x04 (RW, DLAB=1) — Divisor Latch MSB:
 *   7:0
 * +--------+
 * |  DLM   |
 * +--------+
 *   [7:0] DLM   Baud divisor bits [15:8] (concatenated with DLL)
 *
 * IIR @ 0x08 (RO) — Interrupt Identification Register (same address as FCR):
 *   7:6           5:4    3:1            0
 * +-------------+------+--------------+-----------+
 * |FIFOS_ENABLED|  R   | INTERRUPT_ID | INT_PEND  |
 * +-------------+------+--------------+-----------+
 *   [0]   INT_PEND      Interrupt Pending (active-low: 0 = IRQ active, 1 = none)
 *   [3:1] INTERRUPT_ID  Highest-priority pending source (when INT_PEND=0):
 *                       0x0 Modem Status, 0x1 THRE, 0x2 Data Ready,
 *                       0x3 Line Status, 0x6 RX Timeout, 0x7 FIFO Error
 *   [5:4] —             Reserved (read 0)
 *   [7:6] FIFOS_ENABLED 2'b11 when FIFO mode enabled, 2'b00 otherwise
 *   Read clears THRE interrupt if THRE was the reported source.
 *
 * FCR @ 0x08 (WO) — FIFO Control Register (same address as IIR):
 *   7:6           5:4    3              2       1       0
 * +-------------+------+--------------+-------+-------+--------+
 * | RCVR_TRIGGER|  R   | DMA_MODE_SEL | XMIT_ | RCVR_ | FIFO_  |
 * |             |      |              | RESET | RESET | ENABLE |
 * +-------------+------+--------------+-------+-------+--------+
 *   [0]   FIFO_ENABLE   Enable TX and RX FIFOs; toggling resets both FIFOs
 *   [1]   RCVR_FIFO_RST Self-clearing: flush and reset RX FIFO
 *   [2]   XMIT_FIFO_RST Self-clearing: flush and reset TX FIFO
 *   [3]   DMA_MODE_SEL  0=DMA Mode 0 (single transfer), 1=DMA Mode 1 (burst)
 *   [5:4] —             Reserved (write 0)
 *   [7:6] RCVR_TRIGGER  RX interrupt/DMA trigger level LS-2-bits; concat with
 *                       ECR[1:0] to form 4-bit selector (1..4096 characters)
 *
 * LCR @ 0x0C (RW) — Line Control Register:
 *   7      6         5           4     3     2     1:0
 * +------+--------+-----------+-----+-----+-----+-----+
 * | DLAB | SET_BRK| STICK_PAR | EPS | PEN | STB | WLS |
 * +------+--------+-----------+-----+-----+-----+-----+
 *   [1:0] WLS       Word length select: 0=5, 1=6, 2=7, 3=8 data bits
 *   [2]   STB       Stop bits: 0=1 stop, 1=2 stop (1.5 stop when WLS=0)
 *   [3]   PEN       Parity enable (0=none, 1=parity bit added to frame)
 *   [4]   EPS       Even parity select when PEN=1 (0=odd, 1=even)
 *   [5]   STICK_PAR Stick parity: force parity bit to space/mark value
 *   [6]   SET_BRK   Set break: forces tx_o low regardless of transmitter
 *   [7]   DLAB      Divisor Latch Access: 1 maps 0x00/0x04 to DLL/DLM
 *
 * MCR @ 0x10 (RW) — Modem Control Register:
 *   7:6    5           4      3     2     1     0
 * +-----+-----------+------+-----+-----+-----+-----+
 * |  R  | LINE_LOOP | LOOP |OUT2 |OUT1 | RTS | DTR |
 * +-----+-----------+------+-----+-----+-----+-----+
 *   [0]   DTR         Data Terminal Ready; drives dtr_no = ~DTR (active-low pin)
 *   [1]   RTS         Request To Send; drives rts_no = ~RTS
 *   [2]   OUT1        User output 1; drives out1_no = ~OUT1
 *   [3]   OUT2        User output 2; drives out2_no = ~OUT2
 *   [4]   LOOP        System loopback: TX character feeds RX internally
 *   [5]   LINE_LOOP   Line loopback: rx_i feeds tx_o (takes precedence over LOOP)
 *   [7:6] —           Reserved (writes ignored)
 *
 * LSR @ 0x14 (RO) — Line Status Register:
 *   7       6     5     4     3     2     1     0
 * +-------+-----+-----+-----+-----+-----+-----+----+
 * | ERRF  |TEMT |THRE | BI  | FE  | PE  | OE  | DR |
 * +-------+-----+-----+-----+-----+-----+-----+----+
 *   Reset = 0x60 (THRE=1, TEMT=1). Bits [4:1] sticky; cleared on LSR read.
 *   [0]   DR        Data Ready: RX FIFO (or RBR) contains at least one byte
 *   [1]   OE        Overrun Error: new character lost because RX was full
 *   [2]   PE        Parity Error on character at top of RX FIFO
 *   [3]   FE        Framing Error on character at top of RX FIFO
 *   [4]   BI        Break Interrupt: break condition on received character
 *   [5]   THRE      Transmitter Holding Register Empty (TX FIFO can accept data)
 *   [6]   TEMT      Transmitter Empty: TX FIFO and shift register both empty
 *   [7]   ERRF      Error In Receiver FIFO: any PE/FE/BI present in FIFO contents
 *
 * MSR @ 0x18 (RO) — Modem Status Register:
 *   7     6    5     4     3     2     1     0
 * +-----+----+-----+-----+-----+-----+-----+-----+
 * | DCD | RI | DSR | CTS |DDCD |TERI |DDSR |DCTS |
 * +-----+----+-----+-----+-----+-----+-----+-----+
 *   Delta bits [3:0] sticky; cleared on MSR read. Levels [7:4] are live status.
 *   [0]   DCTS      Delta CTS: CTS input changed since last MSR read
 *   [1]   DDSR      Delta DSR: DSR input changed since last MSR read
 *   [2]   TERI      Trailing Edge RI: RI went high-to-low since last read
 *   [3]   DDCD      Delta DCD: DCD input changed since last MSR read
 *   [4]   CTS       Current CTS level (1 = asserted; active-high view of cts_ni)
 *   [5]   DSR       Current DSR level
 *   [6]   RI        Current Ring Indicator level
 *   [7]   DCD       Current Data Carrier Detect level
 *   System loopback: bits [7:4] reflect MCR[4:1]. Line loopback: bits [7:4] = 0.
 *
 * SCR @ 0x1C (RW) — Scratch Register:
 *   7:0
 * +--------+
 * |  SCR   |
 * +--------+
 *   [7:0] SCR   General-purpose read/write scratchpad; no hardware function
 *
 * ECR @ 0x20 (RW) — Extended Control Register:
 *   7:2              1:0
 * +----------------+-------------+
 * |       R        | RCVR_TRIG_MS|
 * +----------------+-------------+
 *   [1:0] RCVR_TRIG_MS  RX FIFO trigger level MS-2-bits; with FCR[7:6] forms
 *                       4-bit index into trigger table (0x0=1 char .. 0xB=4096)
 *   [7:2] —             Reserved (read 0, write ignored)
 *
 * ITR @ 0x24 (RW) — Interrupt Test Register:
 *   7:6    5     4     3     2     1     0
 * +-----+-----+-----+-----+-----+-----+-----+
 * |  R  | TRTI| TFEI|TDSSI| TLSI|TTBEI|TRBFI|
 * +-----+-----+-----+-----+-----+-----+-----+
 *   [0]   TRBFI     Force Received-Data-Ready interrupt (bypasses IER)
 *   [1]   TTBEI     Force THRE interrupt
 *   [2]   TLSI      Force Receiver-Line-Status interrupt
 *   [3]   TDSSI     Force Modem-Status interrupt
 *   [4]   TFEI      Force FIFO-Error interrupt
 *   [5]   TRTI      Force Reception-Timeout interrupt
 *   [7:6] —         Reserved (read 0, write ignored)
 */

struct uart_cfg {
    /// TX FIFO depth in bytes (1..4096). Maps to TX_FIFO_DEPTH in uart_16550.sv.
    unsigned tx_fifo_depth = 32;
    /// RX FIFO depth in bytes (1..4096). Maps to RX_FIFO_DEPTH in uart_16550.sv.
    unsigned rx_fifo_depth = 32;

    /// Decoded register window size in bytes (256-byte window).
    static constexpr uint64_t WINDOW_SIZE = 0x100;
    /// Register access stride in bytes (32-bit registers).
    static constexpr unsigned REG_WIDTH   = 4;

    // Register byte offsets (DLAB=0 view; 0x00/0x04 remap under DLAB=1).
    static constexpr uint64_t OFF_RBR_THR = 0x00; ///< RBR (R) / THR (W) / DLL (DLAB=1)
    static constexpr uint64_t OFF_IER_DLM = 0x04; ///< IER (RW) / DLM (DLAB=1)
    static constexpr uint64_t OFF_IIR_FCR = 0x08; ///< IIR (R) / FCR (W)
    static constexpr uint64_t OFF_LCR     = 0x0C; ///< Line Control
    static constexpr uint64_t OFF_MCR     = 0x10; ///< Modem Control
    static constexpr uint64_t OFF_LSR     = 0x14; ///< Line Status
    static constexpr uint64_t OFF_MSR     = 0x18; ///< Modem Status
    static constexpr uint64_t OFF_SCR     = 0x1C; ///< Scratch
    static constexpr uint64_t OFF_ECR     = 0x20; ///< Extended Control
    static constexpr uint64_t OFF_ITR     = 0x24; ///< Interrupt Test
};

/**
 * @brief IIR.INTERRUPT_ID values (3-bit IDs from uart_16550_pkg::interrupt_id_e).
 *
 * Priority order (highest first): FIFO_ERROR, RECEIVER_LINE_STATUS,
 * RECEPTION_TIMEOUT, RECEIVED_DATA_READY, TRANSMITTER_HOLDING_REGISTER_EMPTY,
 * MODEM_STATUS. NONE means no enabled/forced interrupt is active.
 */
enum class uart_intr_id : uint8_t {
    MODEM_STATUS                       = 0x0,
    TRANSMITTER_HOLDING_REGISTER_EMPTY = 0x1,
    RECEIVED_DATA_READY                = 0x2,
    RECEIVER_LINE_STATUS               = 0x3,
    RECEPTION_TIMEOUT                  = 0x6,
    FIFO_ERROR                         = 0x7,
    NONE                               = 0xFF,
};

/** Encode @p id into IIR bits [3:1] with INT_PEND (bit 0); does not set FIFOS_ENABLED. */
inline uint8_t uart_iir_byte(uart_intr_id id)
{
    if (id == uart_intr_id::NONE) return 0x01; // INT_PEND = 1 (no interrupt)
    return static_cast<uint8_t>((static_cast<uint8_t>(id) << 1) & 0x0E);
}

// ---------------------------------------------------------------------------
// uart
// ---------------------------------------------------------------------------

/**
 * @brief SystemC/TLM-2.0 LT model of the SMC UART 16550.
 *
 * Port names follow the `uart_16550.sv` component top level.  In the
 * `uart_wrap` platform these are exposed per-instance with a `uart_` prefix
 * (e.g. `uart_irq_o[i]`).
 */
class uart : public sc_core::sc_module {
protected:
    // ------------------------------------------------------------------
    // CCI configuration parameters.
    //
    // Declared BEFORE any sized member / port so they are initialised first
    // in the member-initialiser list.  The FIFO depths are immutable after
    // construction; access_delay_ns is mutable and read on every transaction.
    // ------------------------------------------------------------------

    /// TX FIFO depth in bytes (1..4096).  Immutable after construction.
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> tx_fifo_depth_p_;

    /// RX FIFO depth in bytes (1..4096).  Immutable after construction.
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> rx_fifo_depth_p_;

    /// TLM register-access annotated delay in nanoseconds.  Mutable.
    cci::cci_param<double> access_delay_ns_p_;

public:
    SC_HAS_PROCESS(uart);

    /// TLM-2.0 target socket for register access (AXI4-Lite-style).
    tlm_utils::simple_target_socket<uart> reg_socket;

    /// Active-low asynchronous reset.
    sc_core::sc_in<bool>  rst_n_i;

    /// Serial transmit line (idle high).  Driven low only during break.
    sc_core::sc_out<bool> tx_o;
    /// Serial receive line (idle high).  Sampled in line-loopback mode.
    sc_core::sc_in<bool>  rx_i;

    // Modem inputs (active-low).
    sc_core::sc_in<bool>  cts_ni;  ///< Clear To Send (active-low)
    sc_core::sc_in<bool>  dsr_ni;  ///< Data Set Ready (active-low)
    sc_core::sc_in<bool>  ri_ni;   ///< Ring Indicator (active-low)
    sc_core::sc_in<bool>  dcd_ni;  ///< Data Carrier Detect (active-low)

    // Modem outputs (active-low).
    sc_core::sc_out<bool> rts_no;  ///< Request To Send (active-low)
    sc_core::sc_out<bool> dtr_no;  ///< Data Terminal Ready (active-low)
    sc_core::sc_out<bool> out1_no; ///< User Output 1 (active-low)
    sc_core::sc_out<bool> out2_no; ///< User Output 2 (active-low)

    // DMA / status / interrupt.
    sc_core::sc_out<bool> rxrdy_o; ///< DMA RX ready
    sc_core::sc_out<bool> txrdy_o; ///< DMA TX ready
    sc_core::sc_out<bool> err_o;   ///< Aggregate RX error flag
    sc_core::sc_out<bool> irq_o;   ///< Interrupt output (active-high)

    /**
     * @brief Construct the UART module.
     * @param name SystemC module name.
     * @param cfg  Sizing defaults (FIFO depths); CCI presets take priority.
     */
    explicit uart(sc_core::sc_module_name name, uart_cfg cfg = uart_cfg{});

    // ------------------------------------------------------------------
    // Test-bench back door (no socket, no bus side effects)
    // ------------------------------------------------------------------

    /**
     * @brief Deliver a received character into the RX path (back door).
     *
     * Models a character assembled by the (unmodelled) serial receiver.  The
     * optional error flags travel with the character to the top of the RX
     * FIFO, exactly as in hardware.  A push to a full FIFO sets `LSR.OE` and
     * drops the new character (FIFO mode) or overwrites the held byte
     * (non-FIFO mode).
     *
     * @param ch          Received data byte.
     * @param parity_err  Set the parity-error flag for this character.
     * @param framing_err Set the framing-error flag for this character.
     * @param break_err   Set the break-interrupt flag for this character.
     */
    void inject_rx_char(uint8_t ch, bool parity_err = false,
                        bool framing_err = false, bool break_err = false);

    /**
     * @brief Pop the next transmitted character (the model's serial output).
     * @param ch  Output: the popped character.
     * @return    True if a character was available; false if none.
     */
    bool dbg_tx_pop(uint8_t& ch);

    /// Number of bytes currently waiting (un-transmitted) in the TX FIFO.
    unsigned dbg_tx_count() const;

    /// Number of characters currently waiting in the RX FIFO.
    unsigned dbg_rx_count() const;

    /**
     * @brief Side-effect-free register peek.
     * @param off  Byte offset within the UART window.
     * @return     Register value (does NOT pop the RX FIFO or clear sticky bits).
     */
    uint32_t dbg_reg(uint64_t off) const;

    /// Print a human-readable snapshot of UART state to @p os.
    void dump_state(std::ostream& os = std::cout) const;

private:
    // ------------------------------------------------------------------
    // TLM-2.0 callbacks
    // ------------------------------------------------------------------

    /// TLM-2.0 blocking transport — main register-access entry point.
    void b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay);

    /// TLM-2.0 debug transport — back-door register access, no side effects.
    unsigned int transport_dbg(tlm::tlm_generic_payload& gp);

    // ------------------------------------------------------------------
    // SC_METHOD processes
    // ------------------------------------------------------------------

    /// SC_METHOD on rst_n_i: clears all state on active-low assertion.
    void reset_proc();
    /// SC_METHOD on cts_ni/dsr_ni/ri_ni/dcd_ni: detect modem deltas.
    void modem_method();
    /// SC_METHOD on recompute_event_: sole driver of all sc_out ports.
    void recompute_method();
    /// SC_METHOD on rx_timeout_event_: raise the RX timeout interrupt.
    void rx_timeout_method();

    /// Post recompute_event_ at SC_ZERO_TIME (after the current update).
    void schedule_recompute();

    // ------------------------------------------------------------------
    // Register decode helpers
    // ------------------------------------------------------------------

    /// DLAB-aware register read.  Returns false on a decode miss.
    bool reg_read(uint64_t off, uint32_t& data);
    /// DLAB-aware register write.  Returns false on a decode miss.
    bool reg_write(uint64_t off, uint32_t data);

    // Per-register read accessors (encapsulate read side effects).
    uint8_t rbr_read();   ///< Pop RX FIFO; update DR; clear timeout.
    uint8_t iir_read();   ///< Compose IIR; clear THRE interrupt if reported.
    uint8_t lsr_read();   ///< Compose LSR; clear sticky OE/PE/FE/BI.
    uint8_t msr_read();   ///< Compose MSR; clear delta bits; snapshot levels.

    // Per-register write actions (encapsulate write side effects).
    void thr_write(uint8_t byte); ///< Push + functionally transmit a byte.
    void fcr_write(uint8_t data); ///< FIFO enable/reset/DMA/trigger.

    // ------------------------------------------------------------------
    // Datapath helpers
    // ------------------------------------------------------------------

    /// Functionally drain the TX FIFO (LT: whole FIFO in one call).
    void drain_tx();
    /// Push a character into the RX FIFO (shared by inject and loopback).
    void deliver_rx_char(uint8_t ch, bool perr, bool ferr, bool berr);
    /// Recompute LSR.DR/PE/FE/BI from the current front-of-FIFO entry.
    void update_rx_front_status();
    /// (Re)arm the RX character-timeout timer.
    void rearm_rx_timeout();
    /// Decode the 4-bit {ECR,FCR} trigger selector into a character count.
    void update_rx_trigger();

    /// True when TX is enabled (baud divisor != 0).
    bool tx_enabled() const;
    /// True when RX is enabled (baud divisor != 0).
    bool rx_enabled() const;
    /// True if any RX FIFO entry carries an error flag.
    bool any_rx_error() const;

    // ------------------------------------------------------------------
    // Interrupt / modem helpers
    // ------------------------------------------------------------------

    /// Compute the active modem levels (CTS/DSR/RI/DCD) honouring loopback.
    void compute_modem_levels(bool& cts, bool& dsr, bool& ri, bool& dcd) const;
    /// Edge-detect modem-input changes and set MSR delta bits.
    void detect_modem_deltas();

    /// Return the highest-priority pending interrupt ID, or NONE if inactive.
    uart_intr_id interrupt_id() const;
    /// True if any enabled/forced interrupt is currently active.
    bool    any_interrupt() const;

    /// Single-driver point: drive every sc_out port from internal state.
    void update_outputs();

    // ------------------------------------------------------------------
    // Internal state
    // ------------------------------------------------------------------

    /// Memory-mapped register file (status/control shadows).
    struct uart_regs {
        uint8_t ier = 0;     ///< Interrupt Enable
        uint8_t fcr = 0;     ///< FIFO Control (write-only shadow)
        uint8_t lcr = 0;     ///< Line Control
        uint8_t mcr = 0;     ///< Modem Control
        uint8_t lsr = 0x60;  ///< Line Status (THRE=1, TEMT=1 at reset)
        uint8_t msr = 0;     ///< Modem Status
        uint8_t scr = 0;     ///< Scratch
        uint8_t ecr = 0;     ///< Extended Control
        uint8_t itr = 0;     ///< Interrupt Test
        uint8_t dll = 0;     ///< Divisor Latch LSB
        uint8_t dlm = 0;     ///< Divisor Latch MSB
    };

    /// One RX FIFO entry: character plus the error flags that travel with it.
    struct rx_entry {
        uint8_t character   = 0;
        bool    parity_err  = false;
        bool    framing_err = false;
        bool    break_err   = false;
    };

    uart_cfg cfg_;                  ///< Configuration snapshot.

    uart_regs            regs_;     ///< Register file.
    std::deque<rx_entry> rx_fifo_;  ///< RX FIFO (limited by rx depth, or 1).
    std::deque<uint8_t>  tx_fifo_;  ///< TX FIFO (limited by tx depth, or 1).
    std::deque<uint8_t>  tx_history_; ///< Transmitted bytes (dbg_tx_pop()).

    bool     fifo_en_   = false;    ///< Current FCR.FIFO_ENABLE.
    unsigned rx_trigger_= 1;        ///< Decoded RX trigger level (characters).
    uint8_t  dma_mode_  = 0;        ///< 0 = single-transfer, 1 = burst.

    bool thre_intr_     = false;    ///< THRE interrupt latch (edge-set/clear).
    bool rx_timeout_pending_ = false; ///< RX character-timeout interrupt latch.

    // Modem previous-sample levels for sticky delta detection.
    bool prev_cts_ = false, prev_dsr_ = false, prev_ri_ = false, prev_dcd_ = false;

    // DMA Mode-1 FSM state.
    bool dma1_rx_ready_    = false; ///< Mode-1 RX FSM: asserted until FIFO empty.
    bool dma1_tx_notready_ = false; ///< Mode-1 TX FSM: deasserted until FIFO empty.

    // Output caches to suppress redundant sc_signal writes.
    bool out_tx_ = true,  out_rts_ = true, out_dtr_ = true,
         out_out1_ = true, out_out2_ = true;
    bool out_irq_ = false, out_rxrdy_ = false, out_txrdy_ = false, out_err_ = false;
    bool outputs_valid_ = false;    ///< False until first update_outputs().

    sc_core::sc_event recompute_event_;   ///< Triggers recompute_method().
    sc_core::sc_event rx_timeout_event_;  ///< Fires RX timeout.
    sc_core::sc_time  rx_timeout_delay_;  ///< Coarse functional timeout period.
};

} // namespace smc

#endif // SMC_UART_H_
