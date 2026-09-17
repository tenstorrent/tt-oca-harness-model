// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file uart.cpp
 * @brief SMC UART 16550 — SystemC/TLM-2.0 LT implementation.
 *
 * See `include/uart.h` for the full design description, register map, and
 * compliance notes; `doc/02_UART_LowLevel_Design.md` for the implementation
 * contract.
 */

#include "uart.h"

#include "reg_access.h"
#include "sim_log.h"

#include <cstring>
#include <iomanip>
#include <string>

namespace smc {

namespace {

// ---- LSR bit masks (offset 0x14) ------------------------------------------
// LSR bits are set when the corresponding condition is true
constexpr uint8_t LSR_DR   = 0x01; ///< Data Ready
constexpr uint8_t LSR_OE   = 0x02; ///< Overrun Error (sticky)
constexpr uint8_t LSR_PE   = 0x04; ///< Parity Error (front-of-FIFO)
constexpr uint8_t LSR_FE   = 0x08; ///< Framing Error (front-of-FIFO)
constexpr uint8_t LSR_BI   = 0x10; ///< Break Interrupt (front-of-FIFO)
constexpr uint8_t LSR_THRE = 0x20; ///< TX holding/FIFO empty
constexpr uint8_t LSR_TEMT = 0x40; ///< TX holding + shift register empty
constexpr uint8_t LSR_ERRF = 0x80; ///< Error in RX FIFO
constexpr uint8_t LSR_RX_ERRS = LSR_OE | LSR_PE | LSR_FE | LSR_BI;

// ---- IER bit masks (offset 0x04) ------------------------------------------
constexpr uint8_t IER_ERBFI = 0x01; ///< Received-data-ready / timeout enable
constexpr uint8_t IER_ETBEI = 0x02; ///< THRE enable
constexpr uint8_t IER_ELSI  = 0x04; ///< Line-status enable
constexpr uint8_t IER_EDSSI = 0x08; ///< Modem-status enable
constexpr uint8_t IER_EFEI  = 0x10; ///< FIFO-error enable
constexpr uint8_t IER_MASK  = 0x1F;

// ---- LCR bit masks (offset 0x0C) ------------------------------------------
constexpr uint8_t LCR_BREAK = 0x40; ///< Set-break
constexpr uint8_t LCR_DLAB  = 0x80; ///< Divisor Latch Access Bit

// ---- MCR bit masks (offset 0x10) ------------------------------------------
constexpr uint8_t MCR_DTR       = 0x01; ///< Data Terminal Ready (drives dtr_no = ~DTR)
constexpr uint8_t MCR_RTS       = 0x02; ///< Request To Send (drives rts_no = ~RTS)
constexpr uint8_t MCR_OUT1      = 0x04; ///< User Output 1 (drives out1_no = ~OUT1)
constexpr uint8_t MCR_OUT2      = 0x08; ///< User Output 2 (drives out2_no = ~OUT2)
constexpr uint8_t MCR_LOOP      = 0x10; ///< System loopback (TX feeds RX internally)
constexpr uint8_t MCR_LINE_LOOP = 0x20; ///< Line loopback (rx_i feeds tx_o)
constexpr uint8_t MCR_MASK      = 0x3F; ///< Writable MCR bits [5:0]

// ---- MSR bit masks (offset 0x18) ------------------------------------------
constexpr uint8_t MSR_DCTS = 0x01; ///< Delta CTS — CTS changed since last read (sticky)
constexpr uint8_t MSR_DDSR = 0x02; ///< Delta DSR — DSR changed since last read (sticky)
constexpr uint8_t MSR_TERI = 0x04; ///< Trailing Edge RI — RI went 1->0 (sticky)
constexpr uint8_t MSR_DDCD = 0x08; ///< Delta DCD — DCD changed since last read (sticky)
constexpr uint8_t MSR_CTS  = 0x10; ///< Current CTS level (active-high view of cts_ni)
constexpr uint8_t MSR_DSR  = 0x20; ///< Current DSR level (active-high view of dsr_ni)
constexpr uint8_t MSR_RI   = 0x40; ///< Current RI level (active-high view of ri_ni)
constexpr uint8_t MSR_DCD  = 0x80; ///< Current DCD level (active-high view of dcd_ni)
constexpr uint8_t MSR_DELTAS = MSR_DCTS | MSR_DDSR | MSR_TERI | MSR_DDCD; ///< All four delta bits (cleared on MSR read)

// ---- FCR bit masks (offset 0x08, write) -----------------------------------
constexpr uint8_t FCR_FIFO_EN = 0x01; ///< FIFO enable (toggling resets both FIFOs)
constexpr uint8_t FCR_RX_RST  = 0x02; ///< RX FIFO reset (self-clearing)
constexpr uint8_t FCR_TX_RST  = 0x04; ///< TX FIFO reset (self-clearing)
constexpr uint8_t FCR_DMA     = 0x08; ///< DMA mode select (0 = Mode 0, 1 = Mode 1)

// ---- ITR bit masks (offset 0x24) ------------------------------------------
constexpr uint8_t ITR_TRBFI = 0x01; ///< Force data-ready
constexpr uint8_t ITR_TTBEI = 0x02; ///< Force THRE
constexpr uint8_t ITR_TLSI  = 0x04; ///< Force line-status
constexpr uint8_t ITR_TDSSI = 0x08; ///< Force modem-status
constexpr uint8_t ITR_TFEI  = 0x10; ///< Force FIFO-error
constexpr uint8_t ITR_TRTI  = 0x20; ///< Force RX-timeout
// ITR write mask (0x3F) is enforced by the typed `itr_` register's write_mask.

} // anonymous namespace

// ===========================================================================
// Constructor
// ===========================================================================

uart::uart(sc_core::sc_module_name name, uart_cfg cfg)
    : sc_core::sc_module(name)
    // CCI params first (declared before ports/sized members in uart.h).
    , tx_fifo_depth_p_(
          "tx_fifo_depth", cfg.tx_fifo_depth,
          "TX FIFO depth in bytes (1..4096). "
          "Maps to TX_FIFO_DEPTH in uart_16550.sv.")
    , rx_fifo_depth_p_(
          "rx_fifo_depth", cfg.rx_fifo_depth,
          "RX FIFO depth in bytes (1..4096). "
          "Maps to RX_FIFO_DEPTH in uart_16550.sv.")
    , access_delay_ns_p_(
          "access_delay_ns", 2.0,
          "TLM register-access annotated delay in nanoseconds. "
          "Approximates AXI4-Lite bus latency. Mutable at run-time.")
    , reg_socket("reg_socket")
    , rst_n_i("rst_n_i")
    , tx_o("tx_o")
    , rx_i("rx_i")
    , cts_ni("cts_ni"), dsr_ni("dsr_ni"), ri_ni("ri_ni"), dcd_ni("dcd_ni")
    , rts_no("rts_no"), dtr_no("dtr_no"), out1_no("out1_no"), out2_no("out2_no")
    , rxrdy_o("rxrdy_o"), txrdy_o("txrdy_o"), err_o("err_o"), irq_o("irq_o")
    , cfg_(cfg)
{
    cfg_.tx_fifo_depth = tx_fifo_depth_p_.get_value();
    cfg_.rx_fifo_depth = rx_fifo_depth_p_.get_value();

    // Provenance metadata for tooling/introspection.
    tx_fifo_depth_p_.add_metadata("rtl_param",   cci::cci_value(std::string("TX_FIFO_DEPTH")));
    rx_fifo_depth_p_.add_metadata("rtl_param",   cci::cci_value(std::string("RX_FIFO_DEPTH")));
    tx_fifo_depth_p_.add_metadata("valid_range", cci::cci_value(std::string("1..4096")));
    rx_fifo_depth_p_.add_metadata("valid_range", cci::cci_value(std::string("1..4096")));
    access_delay_ns_p_.add_metadata("unit",      cci::cci_value(std::string("nanoseconds")));
    access_delay_ns_p_.add_metadata("tlm_phase", cci::cci_value(std::string("annotated_delay")));

    if (cfg_.tx_fifo_depth == 0 || cfg_.tx_fifo_depth > 4096)
        SC_REPORT_FATAL(name, "UART tx_fifo_depth must be in 1..4096");
    if (cfg_.rx_fifo_depth == 0 || cfg_.rx_fifo_depth > 4096)
        SC_REPORT_FATAL(name, "UART rx_fifo_depth must be in 1..4096");

    // Register the plain SW-only registers in the offset->register dispatch
    // table (common/include/reg_map.h). Reads/writes to these offsets are then
    // served by the map instead of a hand-written switch case.
    regmap_.add(uart_cfg::OFF_SCR, "SCR", scr_)
           .add(uart_cfg::OFF_ECR, "ECR", ecr_)
           .add(uart_cfg::OFF_ITR, "ITR", itr_);

    SIM_LOG_INFO(this,
        "CCI config resolved:"
        << "  tx_fifo_depth=" << cfg_.tx_fifo_depth
        << (tx_fifo_depth_p_.is_preset_value()   ? " [preset]"  : " [default]")
        << "  rx_fifo_depth=" << cfg_.rx_fifo_depth
        << (rx_fifo_depth_p_.is_preset_value()   ? " [preset]"  : " [default]")
        << "  access_delay_ns=" << access_delay_ns_p_.get_value()
        << (access_delay_ns_p_.is_preset_value() ? " [preset]"  : " [default]")
        << "  regmap_registers=" << regmap_.size());

    // Coarse functional RX character-timeout period (not bit-accurate).
    rx_timeout_delay_ = sc_core::sc_time(1.0, sc_core::SC_US);

    update_rx_trigger();

    reg_socket.register_b_transport  (this, &uart::b_transport);
    reg_socket.register_transport_dbg(this, &uart::transport_dbg);

    SC_METHOD(reset_proc);
    sensitive << rst_n_i;
    dont_initialize();

    SC_METHOD(modem_method);
    sensitive << cts_ni << dsr_ni << ri_ni << dcd_ni;
    dont_initialize();

    // recompute_method is the SOLE driver of every sc_out port.
    SC_METHOD(recompute_method);
    sensitive << recompute_event_;
    dont_initialize();

    SC_METHOD(rx_timeout_method);
    sensitive << rx_timeout_event_;
    dont_initialize();
}

// ===========================================================================
// SC_METHOD processes
// ===========================================================================

void uart::schedule_recompute()
{
    recompute_event_.notify(sc_core::SC_ZERO_TIME);
}

void uart::reset_proc()
{
    if (rst_n_i.read()) return; // act only on assertion (low)

    regs_ = uart_regs{};        // LSR back to 0x60 (THRE|TEMT), all else 0
    scr_.reset(0x00u);          // typed registers reset separately from regs_
    ecr_.reset(0x00u);
    itr_.reset(0x00u);
    rx_fifo_.clear();
    tx_fifo_.clear();
    tx_history_.clear();

    fifo_en_   = false;
    dma_mode_  = 0;
    thre_intr_ = false;
    rx_timeout_pending_ = false;
    dma1_rx_ready_    = false;
    dma1_tx_notready_ = false;
    rx_timeout_event_.cancel();
    update_rx_trigger();

    // Re-baseline the modem delta detector against the current input levels
    // so reset does not manufacture spurious delta bits.
    bool cts, dsr, ri, dcd;
    compute_modem_levels(cts, dsr, ri, dcd);
    prev_cts_ = cts; prev_dsr_ = dsr; prev_ri_ = ri; prev_dcd_ = dcd;

    schedule_recompute();
}

void uart::modem_method()
{
    detect_modem_deltas();
    schedule_recompute();
}

void uart::recompute_method()
{
    update_outputs();
}

void uart::rx_timeout_method()
{
    if (!rx_fifo_.empty()) {
        rx_timeout_pending_ = true;
        schedule_recompute();
    }
}

// ===========================================================================
// Datapath helpers
// ===========================================================================

bool uart::tx_enabled() const
{
    return ((static_cast<unsigned>(regs_.dlm) << 8) | regs_.dll) != 0;
}

bool uart::rx_enabled() const
{
    return tx_enabled(); // same divisor gates both engines
}

bool uart::any_rx_error() const
{
    for (const auto& e : rx_fifo_)
        if (e.parity_err || e.framing_err || e.break_err) return true;
    return false;
}

void uart::update_rx_trigger()
{
    const uint8_t sel = static_cast<uint8_t>(((ecr_.read() & 0x3u) << 2)
                                           | ((regs_.fcr >> 6) & 0x3u));
    static const unsigned table[12] =
        {1, 4, 8, 14, 32, 64, 128, 256, 512, 1024, 2048, 4096};
    unsigned t = (sel < 12) ? table[sel] : 1;
    // Clamp so the watermark is reachable in a FIFO of the configured depth.
    if (t > cfg_.rx_fifo_depth) t = cfg_.rx_fifo_depth;
    rx_trigger_ = t;
}

void uart::update_rx_front_status()
{
    //clear the parity, framing, and break error bits
    regs_.lsr &= ~(LSR_PE | LSR_FE | LSR_BI);
    if (!rx_fifo_.empty()) {
        //set the data ready bit
        regs_.lsr |= LSR_DR;
        const auto& f = rx_fifo_.front();
        //set the parity error bit
        if (f.parity_err)  regs_.lsr |= LSR_PE;
        //set the framing error bit
        if (f.framing_err) regs_.lsr |= LSR_FE;
        //set the break error bit
        if (f.break_err)   regs_.lsr |= LSR_BI;
    } else {
        // no data waiting in the FIFO, clear the data ready bit
        regs_.lsr &= ~LSR_DR;
    }
}

void uart::rearm_rx_timeout()
{
    rx_timeout_event_.cancel();
    rx_timeout_event_.notify(rx_timeout_delay_);
}

void uart::drain_tx()
{
    const bool sys_loop  = (regs_.mcr & MCR_LOOP)      != 0;
    const bool line_loop = (regs_.mcr & MCR_LINE_LOOP) != 0;

    while (!tx_fifo_.empty()) {
        const uint8_t b = tx_fifo_.front();
        tx_fifo_.pop_front();
        if (sys_loop && !line_loop) {
            // System loopback: the transmitted character feeds the RX path.
            deliver_rx_char(b, false, false, false);
        } else {
            tx_history_.push_back(b);
        }
    }
    regs_.lsr |= (LSR_THRE | LSR_TEMT);
    thre_intr_ = true; // THR became empty -> THRE interrupt edge
}

void uart::deliver_rx_char(uint8_t ch, bool perr, bool ferr, bool berr)
{
    if (!rx_enabled()) return; // divisor 0 disables the receiver

    const unsigned cap = fifo_en_ ? cfg_.rx_fifo_depth : 1u;
    if (rx_fifo_.size() >= cap) {
        regs_.lsr |= LSR_OE; // overrun
        if (!fifo_en_ && !rx_fifo_.empty()) {
            // Non-FIFO mode overwrites the held byte.
            rx_fifo_.front() = rx_entry{ch, perr, ferr, berr};
        }
        // FIFO mode: drop the new character.
    } else {
        rx_fifo_.push_back(rx_entry{ch, perr, ferr, berr});
    }
    update_rx_front_status();
    rearm_rx_timeout();
    schedule_recompute();
}

// ===========================================================================
// Per-register read accessors (read side effects)
// ===========================================================================

uint8_t uart::rbr_read()
{
    uint8_t ch = 0;
    if (!rx_fifo_.empty()) {
        ch = rx_fifo_.front().character;
        rx_fifo_.pop_front();
    }
    rx_timeout_pending_ = false; // reading RBR clears the timeout
    update_rx_front_status();    // reveal the next character's status
    schedule_recompute();
    return ch;
}

uint8_t uart::iir_read()
{
    // format of the IIR register is as follows:
    // 7:6 - FIFOS_ENABLED
    // 5:1 - INTERRUPT_ID
    // 0 - INTERRUPT_PENDING
    const uart_intr_id id = interrupt_id();
    uint8_t v = uart_iir_byte(id);
    if (fifo_en_) v |= 0xC0; // FIFOS_ENABLED = 2'b11

    // Reading IIR clears the THRE interrupt if it was the reported source.
    // THRE interrupt purpose is to indicate that the THR is empty and can accept data.
    // THR is the transmitter holding register.
    if (id == uart_intr_id::TRANSMITTER_HOLDING_REGISTER_EMPTY) {
        //clear the THRE interrupt
        thre_intr_ = false;
        schedule_recompute();
    }   
    return v;
}

uint8_t uart::lsr_read()
{
    uint8_t v = regs_.lsr & ~LSR_ERRF; //clear the error in receiver FIFO bit
    //check if there is any error in the receiver FIFO
    const bool errf = fifo_en_ ? any_rx_error()
                               : ((regs_.lsr & (LSR_PE | LSR_FE | LSR_BI)) != 0);
    if (errf) v |= LSR_ERRF;
    //set the error in receiver FIFO bit
    //clear the sticky RX error bits
    regs_.lsr &= ~LSR_RX_ERRS;
    schedule_recompute();
    return v;
}

uint8_t uart::msr_read()
{
    bool cts, dsr, ri, dcd;
    compute_modem_levels(cts, dsr, ri, dcd);

    uint8_t v = regs_.msr & MSR_DELTAS; //clear the delta bits
    //set the CTS bit
    if (cts) v |= MSR_CTS;
    if (dsr) v |= MSR_DSR;
    if (ri)  v |= MSR_RI;
    if (dcd) v |= MSR_DCD;

    // Delta bits clear on read; snapshot the current levels.
    regs_.msr &= ~MSR_DELTAS;
    prev_cts_ = cts; prev_dsr_ = dsr; prev_ri_ = ri; prev_dcd_ = dcd;
    schedule_recompute();
    return v;
}

// ===========================================================================
// Per-register write actions (write side effects)
// ===========================================================================

void uart::thr_write(uint8_t byte)
{
    const unsigned cap = fifo_en_ ? cfg_.tx_fifo_depth : 1u;
    if (tx_fifo_.size() < cap) tx_fifo_.push_back(byte); // else drop silently

    regs_.lsr &= ~(LSR_THRE | LSR_TEMT);
    thre_intr_ = false;

    if (tx_enabled()) drain_tx(); // LT: the whole FIFO transmits at once
    schedule_recompute();
}

void uart::fcr_write(uint8_t data)
{
    const bool new_fifo_en = (data & FCR_FIFO_EN) != 0;

    if (new_fifo_en != fifo_en_) {
        // Toggling FIFO enable resets both FIFOs and the RBR/THR.
        rx_fifo_.clear();
        tx_fifo_.clear();
        regs_.lsr &= ~(LSR_DR | LSR_RX_ERRS);
        regs_.lsr |= (LSR_THRE | LSR_TEMT);
        rx_timeout_pending_ = false;
        rx_timeout_event_.cancel();
    }
    if (data & FCR_RX_RST) {
        rx_fifo_.clear();
        regs_.lsr &= ~(LSR_DR | LSR_RX_ERRS);
        rx_timeout_pending_ = false;
        rx_timeout_event_.cancel();
    }
    if (data & FCR_TX_RST) {
        tx_fifo_.clear();
        regs_.lsr |= (LSR_THRE | LSR_TEMT);
    }

    fifo_en_  = new_fifo_en;
    dma_mode_ = (data & FCR_DMA) ? 1 : 0;
    // Store shadow with the self-clearing reset bits read back as 0.
    regs_.fcr = static_cast<uint8_t>(data & ~(FCR_RX_RST | FCR_TX_RST));

    update_rx_trigger();
    schedule_recompute();
}

// ===========================================================================
// Register decode
// ===========================================================================

bool uart::reg_read(uint64_t off, uint32_t& data)
{
    // Plain SW-only registers (SCR/ECR/ITR) are owned by the RegisterMap and
    // have no read side effects, so the map fully serves them. These offsets
    // are never DLAB-banked, so the lookup is safe regardless of LCR.DLAB.
    if (uint8_t rv = 0; regmap_.read(off, rv)) { data = rv; return true; }

    const bool dlab = (regs_.lcr & LCR_DLAB) != 0;
    switch (off) {
    case uart_cfg::OFF_RBR_THR: data = dlab ? regs_.dll : rbr_read(); break;
    case uart_cfg::OFF_IER_DLM: data = dlab ? regs_.dlm : regs_.ier;  break;
    case uart_cfg::OFF_IIR_FCR: data = iir_read();                    break;
    case uart_cfg::OFF_LCR:     data = regs_.lcr;                     break;
    case uart_cfg::OFF_MCR:     data = regs_.mcr;                     break;
    case uart_cfg::OFF_LSR:     data = lsr_read();                    break;
    case uart_cfg::OFF_MSR:     data = msr_read();                    break;
    default: return false; // decode miss inside window -> ADDRESS_ERROR
    }
    return true;
}

bool uart::reg_write(uint64_t off, uint32_t data)
{
    const bool    dlab = (regs_.lcr & LCR_DLAB) != 0;
    //get the byte from the data that represents the register value
    const uint8_t b    = static_cast<uint8_t>(data & 0xFFu);
    switch (off) {
    case uart_cfg::OFF_RBR_THR:
        if (dlab) { //if the DLAB bit is set, write the byte to the DLL register
            regs_.dll = b;
            if (tx_enabled() && !tx_fifo_.empty()) drain_tx();
            schedule_recompute();
        } else {
            //if the DLAB bit is not set, write the byte to the THR register
            thr_write(b);
        }
        break;
    case uart_cfg::OFF_IER_DLM:
        if (dlab) {
            regs_.dlm = b;
            if (tx_enabled() && !tx_fifo_.empty()) drain_tx();
            schedule_recompute();
        } else {
            const uint8_t old = regs_.ier;
            // Reserved IER bits [7:5] are RAZ/WI: drop them via the shared
            // masking helper instead of a hand-rolled `& MASK`.
            regs_.ier = regmodel::apply_write_mask<uint8_t>(regs_.ier, b, IER_MASK);
            // Enabling ETBEI while THR is already empty raises the THRE int.
            if ((regs_.ier & IER_ETBEI) && !(old & IER_ETBEI)
                && (regs_.lsr & LSR_THRE)) {
                //set the THRE interrupt
                thre_intr_ = true;
            }
            schedule_recompute();
        }
        break;
    case uart_cfg::OFF_IIR_FCR: fcr_write(b);                      break;
    case uart_cfg::OFF_LCR:     regs_.lcr = b; schedule_recompute(); break;
    case uart_cfg::OFF_MCR:
        // Reserved MCR bits [7:6] are RAZ/WI (shared masking helper).
        regs_.mcr = regmodel::apply_write_mask<uint8_t>(regs_.mcr, b, MCR_MASK);
        detect_modem_deltas();   // loopback changes can move MSR levels
        schedule_recompute();
        break;
    case uart_cfg::OFF_LSR:     /* read-only: ignore */            break;
    case uart_cfg::OFF_MSR:     /* read-only: ignore */            break;
    // SCR/ECR/ITR storage is owned by the RegisterMap (write_mask drops
    // reserved bits); ECR/ITR additionally need a model-side recompute.
    case uart_cfg::OFF_SCR:     regmap_.write(off, b);             break;
    case uart_cfg::OFF_ECR:
        regmap_.write(off, b);
        update_rx_trigger();
        schedule_recompute();
        break;
    case uart_cfg::OFF_ITR:
        regmap_.write(off, b);
        schedule_recompute();
        break;
    default: return false; // decode miss inside window -> ADDRESS_ERROR
    }
    return true;
}

// ===========================================================================
// Modem helpers
// ===========================================================================

void uart::compute_modem_levels(bool& cts, bool& dsr, bool& ri, bool& dcd) const
{
    const bool sys_loop  = (regs_.mcr & MCR_LOOP)      != 0;
    const bool line_loop = (regs_.mcr & MCR_LINE_LOOP) != 0;

    if (line_loop) {
        // Line loopback takes precedence: MSR inputs are deasserted.
        cts = dsr = ri = dcd = false;
    } else if (sys_loop) {
        // System loopback: MSR levels reflect the MCR control bits.
        cts = (regs_.mcr & MCR_RTS)  != 0;
        dsr = (regs_.mcr & MCR_DTR)  != 0;
        ri  = (regs_.mcr & MCR_OUT1) != 0;
        dcd = (regs_.mcr & MCR_OUT2) != 0;
    } else {
        // Normal: active-high level = inverted active-low input.
        cts = !cts_ni.read();
        dsr = !dsr_ni.read();
        ri  = !ri_ni.read();
        dcd = !dcd_ni.read();
    }
}

void uart::detect_modem_deltas()
{
    bool cts, dsr, ri, dcd;
    compute_modem_levels(cts, dsr, ri, dcd);

    if (cts != prev_cts_) regs_.msr |= MSR_DCTS;
    if (dsr != prev_dsr_) regs_.msr |= MSR_DDSR;
    if (dcd != prev_dcd_) regs_.msr |= MSR_DDCD;
    if (prev_ri_ && !ri)  regs_.msr |= MSR_TERI; // trailing edge of RI

    prev_cts_ = cts; prev_dsr_ = dsr; prev_ri_ = ri; prev_dcd_ = dcd;
}

// ===========================================================================
// Interrupt arbitration
// ===========================================================================

uart_intr_id uart::interrupt_id() const
{
    const uint8_t ier = regs_.ier;
    const uint8_t itr = itr_.read();

    const bool fifo_error  = fifo_en_ && any_rx_error();
    const bool line_status = (regs_.lsr & LSR_RX_ERRS) != 0;
    const bool rx_timeout  = rx_timeout_pending_;
    const bool data_ready  = fifo_en_ ? (rx_fifo_.size() >= rx_trigger_)
                                      : !rx_fifo_.empty();
    const bool thre        = thre_intr_;
    const bool modem       = (regs_.msr & MSR_DELTAS) != 0;

    // ITR test bits force the corresponding source active (bypassing both
    // the real condition and the IER enable); otherwise a source is active
    // only when its condition holds AND its IER enable is set.
    if ((itr & ITR_TFEI)  || (fifo_error  && (ier & IER_EFEI)))
        return uart_intr_id::FIFO_ERROR;
    if ((itr & ITR_TLSI)  || (line_status && (ier & IER_ELSI)))
        return uart_intr_id::RECEIVER_LINE_STATUS;
    if ((itr & ITR_TRTI)  || (rx_timeout  && (ier & IER_ERBFI)))
        return uart_intr_id::RECEPTION_TIMEOUT;
    if ((itr & ITR_TRBFI) || (data_ready  && (ier & IER_ERBFI)))
        return uart_intr_id::RECEIVED_DATA_READY;
    if ((itr & ITR_TTBEI) || (thre        && (ier & IER_ETBEI)))
        return uart_intr_id::TRANSMITTER_HOLDING_REGISTER_EMPTY;
    if ((itr & ITR_TDSSI) || (modem       && (ier & IER_EDSSI)))
        return uart_intr_id::MODEM_STATUS;
    return uart_intr_id::NONE;
}

bool uart::any_interrupt() const
{
    return interrupt_id() != uart_intr_id::NONE;
}

// ===========================================================================
// Single-driver output point
// ===========================================================================

/*
             Registers
             FIFOs
             Inputs
                │
                ▼
         update_outputs()
                │
 ┌──────────────┼──────────────┐
 │              │              │
 ▼              ▼              ▼
Modem        TX Line        IRQ/DMA
Outputs                     Status
 │              │              │
 ▼              ▼              ▼
RTS_N        TX_O         IRQ_O
DTR_N                     RXRDY_O
OUT1_N                    TXRDY_O
OUT2_N                    ERR_O


*/


void uart::update_outputs()
{
    const bool sys_loop  = (regs_.mcr & MCR_LOOP)      != 0;
    const bool line_loop = (regs_.mcr & MCR_LINE_LOOP) != 0;

    // ---- Modem outputs (active-low) --------------------------------------
    bool rts, dtr, out1, out2;
    if (line_loop) {
        // Line loopback: modem outputs follow the (active-low) inputs.
        rts  = cts_ni.read();
        dtr  = dsr_ni.read();
        out1 = ri_ni.read();
        out2 = dcd_ni.read();
    } else if (sys_loop) {
        // System loopback: modem outputs deasserted (active-low high).
        rts = dtr = out1 = out2 = true;
    } else {
        rts  = !(regs_.mcr & MCR_RTS);
        dtr  = !(regs_.mcr & MCR_DTR);
        out1 = !(regs_.mcr & MCR_OUT1);
        out2 = !(regs_.mcr & MCR_OUT2);
    }

    // ---- TX line ---------------------------------------------------------
    bool tx;
    if (line_loop)                      tx = rx_i.read();
    else if (regs_.lcr & LCR_BREAK)     tx = false; // break forces low
    else                                tx = true;  // idle high

    // ---- Interrupt -------------------------------------------------------
    const bool irq = (interrupt_id() != uart_intr_id::NONE);

    // ---- DMA handshake ---------------------------------------------------
    // Unlike normal mode dma mode does data transfer either at trigger level or timeout.
    const unsigned rx_occ = static_cast<unsigned>(rx_fifo_.size());
    const unsigned tx_cap = fifo_en_ ? cfg_.tx_fifo_depth : 1u;
    bool rxrdy, txrdy;
    if (dma_mode_ == 1 && fifo_en_) {
        if (rx_occ == 0)
            dma1_rx_ready_ = false;
        else if (rx_occ >= rx_trigger_ || rx_timeout_pending_)
            dma1_rx_ready_ = true;
        rxrdy = dma1_rx_ready_;

        if (tx_fifo_.empty())
            dma1_tx_notready_ = false;
        else if (tx_fifo_.size() >= tx_cap)
            dma1_tx_notready_ = true;
        txrdy = !dma1_tx_notready_;
    } else {
        rxrdy = (rx_occ != 0);
        txrdy = (tx_fifo_.size() < tx_cap);
    }

    // ---- Aggregate error -------------------------------------------------
    const bool err = ((regs_.lsr & LSR_RX_ERRS) != 0) || (fifo_en_ && any_rx_error());

    // ---- Drive, suppressing redundant sc_signal writes -------------------
    if (!outputs_valid_ || tx   != out_tx_)   { out_tx_   = tx;   tx_o.write(tx);     }
    if (!outputs_valid_ || rts  != out_rts_)  { out_rts_  = rts;  rts_no.write(rts);  }
    if (!outputs_valid_ || dtr  != out_dtr_)  { out_dtr_  = dtr;  dtr_no.write(dtr);  }
    if (!outputs_valid_ || out1 != out_out1_) { out_out1_ = out1; out1_no.write(out1);}
    if (!outputs_valid_ || out2 != out_out2_) { out_out2_ = out2; out2_no.write(out2);}
    if (!outputs_valid_ || irq  != out_irq_)  { out_irq_  = irq;  irq_o.write(irq);   }
    if (!outputs_valid_ || rxrdy!= out_rxrdy_){ out_rxrdy_= rxrdy;rxrdy_o.write(rxrdy);}
    if (!outputs_valid_ || txrdy!= out_txrdy_){ out_txrdy_= txrdy;txrdy_o.write(txrdy);}
    if (!outputs_valid_ || err  != out_err_)  { out_err_  = err;  err_o.write(err);   }
    outputs_valid_ = true;
}

// ===========================================================================
// TLM-2.0 callbacks
// ===========================================================================

void uart::b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay)
{
    const tlm::tlm_command cmd = gp.get_command();
    const uint64_t         adr = gp.get_address();
    const uint32_t         len = gp.get_data_length();
    uint8_t* const         buf = gp.get_data_ptr();

    if (cmd != tlm::TLM_READ_COMMAND && cmd != tlm::TLM_WRITE_COMMAND) {
        gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    if (len != 4) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (adr >= uart_cfg::WINDOW_SIZE || (adr & 0x3u) != 0) {
        gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    // Optional AXI sideband extension is forwarded but not enforced here.
    smc::smc_axi_extension* ext = nullptr;
    gp.get_extension(ext);
    (void)ext;

    bool ok;
    if (cmd == tlm::TLM_READ_COMMAND) {
        uint32_t v = 0;
        ok = reg_read(adr, v);
        if (ok) std::memcpy(buf, &v, 4);
        SIM_LOG_TRACE(this, "read  off=0x" << std::hex << adr
                            << " data=0x" << v << (ok ? "" : " [decode-miss]"));
    } else {
        uint32_t v = 0;
        std::memcpy(&v, buf, 4);
        ok = reg_write(adr, v);
        SIM_LOG_TRACE(this, "write off=0x" << std::hex << adr
                            << " data=0x" << v << (ok ? "" : " [decode-miss]"));
    }
    if (!ok) SIM_LOG_DEBUG(this, "TLM decode miss at off=0x" << std::hex << adr);

    delay += sc_core::sc_time(access_delay_ns_p_.get_value(), sc_core::SC_NS);
    gp.set_response_status(ok ? tlm::TLM_OK_RESPONSE
                              : tlm::TLM_ADDRESS_ERROR_RESPONSE);
    gp.set_dmi_allowed(false); // RBR/IIR/MSR reads have side effects
}

unsigned int uart::transport_dbg(tlm::tlm_generic_payload& gp)
{
    const tlm::tlm_command cmd = gp.get_command();
    const uint64_t         adr = gp.get_address();
    const uint32_t         len = gp.get_data_length();
    uint8_t* const         buf = gp.get_data_ptr();

    if (len != 4 || (adr & 0x3u) != 0 || adr >= uart_cfg::WINDOW_SIZE) return 0;

    if (cmd == tlm::TLM_READ_COMMAND) {
        uint32_t v = dbg_reg(adr); // side-effect-free peek
        std::memcpy(buf, &v, 4);
        return 4;
    }
    if (cmd == tlm::TLM_WRITE_COMMAND) {
        uint32_t v = 0;
        std::memcpy(&v, buf, 4);
        if (!reg_write(adr, v)) return 0;
        return 4;
    }
    return 0;
}

// ===========================================================================
// Test-bench back door
// ===========================================================================

void uart::inject_rx_char(uint8_t ch, bool parity_err, bool framing_err,
                          bool break_err)
{
    deliver_rx_char(ch, parity_err, framing_err, break_err);
}

bool uart::dbg_tx_pop(uint8_t& ch)
{
    if (tx_history_.empty()) return false;
    ch = tx_history_.front();
    tx_history_.pop_front();
    return true;
}

unsigned uart::dbg_tx_count() const
{
    return static_cast<unsigned>(tx_fifo_.size());
}

unsigned uart::dbg_rx_count() const
{
    return static_cast<unsigned>(rx_fifo_.size());
}

uint32_t uart::dbg_reg(uint64_t off) const
{
    // Plain SW-only registers (SCR/ECR/ITR) via the RegisterMap peek.
    if (uint8_t rv = 0; regmap_.read(off, rv)) return rv;

    const bool dlab = (regs_.lcr & LCR_DLAB) != 0;
    switch (off) {
    case uart_cfg::OFF_RBR_THR:
        if (dlab) return regs_.dll;
        return rx_fifo_.empty() ? 0u : rx_fifo_.front().character;
    case uart_cfg::OFF_IER_DLM:
        return dlab ? regs_.dlm : regs_.ier;
    case uart_cfg::OFF_IIR_FCR: {
        uint8_t v = uart_iir_byte(interrupt_id());
        if (fifo_en_) v |= 0xC0u;
        return v;
    }
    case uart_cfg::OFF_LCR: return regs_.lcr;
    case uart_cfg::OFF_MCR: return regs_.mcr;
    case uart_cfg::OFF_LSR: {
        uint8_t v = regs_.lsr & ~LSR_ERRF;
        const bool errf = fifo_en_ ? any_rx_error()
                                   : ((regs_.lsr & (LSR_PE | LSR_FE | LSR_BI)) != 0);
        if (errf) v |= LSR_ERRF;
        return v;
    }
    case uart_cfg::OFF_MSR: {
        bool cts, dsr, ri, dcd;
        compute_modem_levels(cts, dsr, ri, dcd);
        uint8_t v = regs_.msr & MSR_DELTAS;
        if (cts) v |= MSR_CTS;
        if (dsr) v |= MSR_DSR;
        if (ri)  v |= MSR_RI;
        if (dcd) v |= MSR_DCD;
        return v;
    }
    default: return 0u;
    }
}

void uart::dump_state(std::ostream& os) const
{
    os << "[" << name() << "] UART state dump\n";
    os << "  CCI parameters:\n"
       << "    tx_fifo_depth="   << tx_fifo_depth_p_.get_value()
       << (tx_fifo_depth_p_.is_preset_value()   ? " [preset]"  : " [default]") << "\n"
       << "    rx_fifo_depth="   << rx_fifo_depth_p_.get_value()
       << (rx_fifo_depth_p_.is_preset_value()   ? " [preset]"  : " [default]") << "\n"
       << "    access_delay_ns=" << access_delay_ns_p_.get_value()
       << (access_delay_ns_p_.is_preset_value() ? " [preset]"  : " [default]") << "\n";
    os << "  fifo_en=" << fifo_en_
       << " dma_mode=" << static_cast<unsigned>(dma_mode_)
       << " rx_trigger=" << rx_trigger_
       << " tx_enabled=" << tx_enabled() << "\n";
    os << "  tx_fifo=" << tx_fifo_.size()
       << " rx_fifo=" << rx_fifo_.size()
       << " tx_history=" << tx_history_.size() << "\n";
    os << std::hex << std::setfill('0');
    os << "  IER=0x" << std::setw(2) << unsigned(regs_.ier)
       << " LCR=0x"  << std::setw(2) << unsigned(regs_.lcr)
       << " MCR=0x"  << std::setw(2) << unsigned(regs_.mcr)
       << " LSR=0x"  << std::setw(2) << unsigned(regs_.lsr)
       << " MSR=0x"  << std::setw(2) << unsigned(regs_.msr) << "\n";
    os << "  IIR(id)=0x" << std::setw(2)
       << unsigned(static_cast<uint8_t>(interrupt_id()))
       << " irq=" << std::dec << any_interrupt() << "\n";
    os << std::dec << std::setfill(' ');
}

// ---------------------------------------------------------------------------
// uart_wrap
// ---------------------------------------------------------------------------

uart_wrap::uart_wrap(sc_core::sc_module_name name)
    : sc_core::sc_module(name)
    , access_delay_ns_p_("access_delay_ns", 2.0,
          "TLM register-access annotated delay (ns). Mutable at run-time.")
    , reg_socket("reg_socket")
    , rst_n_i("rst_n_i")
{
    access_delay_ns_p_.add_metadata("unit", cci::cci_value(std::string("nanoseconds")));
    access_delay_ns_p_.add_metadata("tlm_phase", cci::cci_value(std::string("annotated_delay")));

    log_intr_status_.on_write([](uint32_t cur, uint32_t in) {
        return regmodel::apply_w1c(cur, in, uart_wrap_cfg::LOG_INTR_MASK);
    });
    log_intr_test_.on_write([this](uint32_t, uint32_t in) {
        const uint32_t bits = in & uart_wrap_cfg::LOG_INTR_MASK;
        log_intr_status_.set_raw(log_intr_status_.raw() | bits);
        return 0u;
    });
    log_en_.on_write([this](uint32_t cur, uint32_t in) {
        const uint32_t next = (cur & ~uart_wrap_cfg::LOG_EN_MASK)
                            | (in & uart_wrap_cfg::LOG_EN_MASK);
        if ((cur & 1u) && !(next & 1u))
            reset_log_engine();
        return next;
    });

    for (auto& e : log_entry_)
        e = regmodel::Register32{uart_wrap_cfg::LOG_LEN_MASK,
                                 uart_wrap_cfg::LOG_LEN_MASK, 0};

    regmap_.add(uart_wrap_cfg::OFF_WRAP_CTRL, "UART_EN", wrap_ctrl_)
           .add(uart_wrap_cfg::OFF_LOG_CTRL, "LOG_EN", log_en_)
           .add(uart_wrap_cfg::OFF_LOG_REGION_SIZE, "LOG_REGION_SIZE", log_region_size_)
           .add(uart_wrap_cfg::OFF_LOG_REGION_LO, "LOG_REGION_ADDR_LO", log_region_lo_)
           .add(uart_wrap_cfg::OFF_LOG_REGION_HI, "LOG_REGION_ADDR_HI", log_region_hi_)
           .add(uart_wrap_cfg::OFF_LOG_WRITE_ADDR, "LOG_WRITE_ADDR", log_write_addr_)
           .add(uart_wrap_cfg::OFF_LOG_INTR_STATUS, "LOG_INTR_STATUS", log_intr_status_)
           .add(uart_wrap_cfg::OFF_LOG_INTR_ENABLE, "LOG_INTR_ENABLE", log_intr_enable_)
           .add(uart_wrap_cfg::OFF_LOG_INTR_TEST, "LOG_INTR_TEST", log_intr_test_);
    for (unsigned i = 0; i < uart_wrap_cfg::NUM_LOG_ENTRIES; ++i) {
        regmap_.add(uart_wrap_cfg::OFF_LOG_ENTRY + i * 4u,
                    std::string("LOG_CTRL") + std::to_string(i), log_entry_[i]);
    }

    reg_socket.register_b_transport(this, &uart_wrap::b_transport);
    reg_socket.register_transport_dbg(this, &uart_wrap::transport_dbg);
    SC_METHOD(reset_proc);
    sensitive << rst_n_i;
    dont_initialize();
    SIM_LOG_INFO(this, "uart_wrap instantiated (ctrl@0x0 log_engine@0x200)");
}

void uart_wrap::reset_log_engine()
{
    log_en_.reset(0);
    log_region_size_.reset(0);
    log_region_lo_.reset(0);
    log_region_hi_.reset(0);
    log_write_addr_.reset(0);
    log_intr_status_.reset(0);
    log_intr_enable_.reset(0);
    log_intr_test_.reset(0);
    for (auto& e : log_entry_) e.reset(0);
}

void uart_wrap::reset_proc()
{
    if (!rst_n_i.read()) {
        wrap_ctrl_.reset(0);
        reset_log_engine();
    }
}

void uart_wrap::b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay)
{
    const auto cmd = gp.get_command();
    const uint64_t adr = gp.get_address();
    const uint32_t len = gp.get_data_length();
    uint8_t* const buf = gp.get_data_ptr();

    if (cmd != tlm::TLM_READ_COMMAND && cmd != tlm::TLM_WRITE_COMMAND) {
        gp.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    if (len != 4) {
        gp.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (adr >= uart_wrap_cfg::WINDOW_SIZE || (adr & 0x3u) != 0) {
        gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (cmd == tlm::TLM_READ_COMMAND) {
        uint32_t v = 0;
        (void)regmap_.read(adr, v); // miss → RAZ
        std::memcpy(buf, &v, 4);
        SIM_LOG_TRACE(this, "read  off=0x" << std::hex << adr << " data=0x" << v);
    } else {
        uint32_t v = 0;
        std::memcpy(&v, buf, 4);
        (void)regmap_.write(adr, v); // miss → WI
        SIM_LOG_TRACE(this, "write off=0x" << std::hex << adr << " data=0x" << v);
    }
    delay += sc_core::sc_time(access_delay_ns_p_.get_value(), sc_core::SC_NS);
    gp.set_response_status(tlm::TLM_OK_RESPONSE);
}

unsigned int uart_wrap::transport_dbg(tlm::tlm_generic_payload& gp)
{
    const uint64_t adr = gp.get_address();
    const uint32_t len = gp.get_data_length();
    if (len != 4 || adr >= uart_wrap_cfg::WINDOW_SIZE || (adr & 0x3u) != 0)
        return 0;
    uint32_t v = 0;
    if (gp.is_read()) {
        (void)regmap_.read(adr, v);
        std::memcpy(gp.get_data_ptr(), &v, 4);
    } else {
        std::memcpy(&v, gp.get_data_ptr(), 4);
        (void)regmap_.write(adr, v);
    }
    gp.set_response_status(tlm::TLM_OK_RESPONSE);
    return 4;
}

} // namespace smc
