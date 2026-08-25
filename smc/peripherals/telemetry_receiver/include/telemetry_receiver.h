// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file telemetry_receiver.h
 * @brief SystemC/TLM-2.0 Loosely-Timed (LT) model of the SMC Telemetry
 *        Receiver.
 *
 * A **transaction-level, register-accurate** model of one telemetry receiver
 * instance.  The receiver is the sink of the SoC telemetry path: a telemetry
 * transmitter streams counter samples over an **ATB** (AMBA Trace Bus) byte
 * interface, the receiver re-assembles them into *messages*, queues the
 * messages in a small circular buffer, and exposes the oldest queued message
 * to software through a read-only register aperture.
 *
 * Intended for firmware bring-up, telemetry driver development, and platform
 * integration testing — not for ATB cycle timing or transmitter modeling.
 *
 * ---
 * ## Authoritative references (tt-oca-hw)
 *
 * | Source | Role |
 * |--------|------|
 * | `hw/comp/telemetry_receiver/data/registers/rdl/telemetry_receiver.rdl` | **Ground-truth** register map |
 * | `hw/comp/telemetry_receiver/rtl/telemetry_receiver.sv` | Behaviour (assembly, decode, buffer, interrupts) |
 * | `hw/comp/telemetry_receiver/rtl/telemetry_receiver_pkg.sv` | ATB packet / block / counter widths |
 * | `hw/periph/telemetry_receiver_wrap/data/registers/rdl/telemetry_receiver_wrap.rdl` | `NUM_TELEMETRY_RECEIVERS = 3`, 0x100 stride |
 * | `hw/smc/data/registers/rdl/smc_top.rdl` | Placement of the wrapper in the SMC map |
 *
 * @note **Base address is deliberately not encoded in this model.**  The RTL
 *       `smc_top.rdl` places `telemetry_receiver_wrap` at `0xC000_9000`. The base
 *       address is assigned by the platform's address router (as it is for `beu`
 *       and `i3c_controller`), so this model decodes window-relative offsets only.
 *       See `doc/01_TELEMETRY_RECEIVER_Specification.md` §2.
 *
 * ---
 * ## Register map (one instance, 0x100 window, 32-bit registers)
 *
 * ```
 * Offset  Name                    SW   Notes
 * ────────────────────────────────────────────────────────────────────────────
 * 0x00    CTRL                    rw   [0] BUFFER_POP (wo, self-clearing pulse)
 *                                      [4] TELEMETRY_RX_FLUSH (wo, pulse)
 *                                      [8] TELEMETRY_TX_FLUSH (rw, HW-cleared)
 *                                      [23:12] BUFFER_THRESHOLD
 * 0x04    STATUS                  ro   [0] BUFFER_EMPTY (reset 1), [4] BUFFER_FULL
 * 0x08    INTR_STATUS             rw   [0] MISSING_LAST (W1C, sticky)
 *                                      [4] BUFFER_THRESHOLD (ro, level)
 * 0x0C    INTR_ENABLE             rw   [0] MISSING_LAST, [4] BUFFER_THRESHOLD
 * 0x10    INTR_TEST               rw   [0] MISSING_LAST (wo, pulse)
 *                                      [4] BUFFER_THRESHOLD (rw, level)
 * 0x14    TELEMETRY_PROBE_ID      ro   [4:0] PROBE_ID of the oldest message
 * 0x18    TELEMETRY_COUNTER_VLDS  ro   [31:0] per-counter valid bits
 * 0x80..  TELEMETRY_COUNTER[32]   ro   32 x 32-bit counter values (stride 4)
 * ```
 *
 * Offsets inside the window that decode to no register return
 * `TLM_ADDRESS_ERROR_RESPONSE`.  `TELEMETRY_COUNTER[i]` for
 * `i >= max_counters_per_message` reads 0 with its valid bit clear, exactly as
 * the RTL ties off the unused counter registers.
 *
 * ---
 * ## ATB message format
 *
 * The transmitter sends **byte beats**.  Eight beats form a 64-bit **packet**;
 * one or more packets form a **message**.  A packet is a bit-packed
 * `{last_packet, blocks[0..6]}` (see `telemetry_receiver_pkg.sv`):
 *
 * ```
 *  bit 63        62      61..54     53..45   ...        8..0
 *      last_packet  blk0.vld  blk0.data   blk1{vld,data}  ...  blk6{vld,data}
 * ```
 *
 * - A **block** is `{vld:1, data:8}` — one payload byte plus its valid bit.
 *   Seven blocks fit in a 64-bit packet (7 x 9 = 63 bits) alongside
 *   `last_packet`.
 * - Beat *i* of a packet carries packet bits `[8i+7 : 8i]`, so the first beat
 *   is the packet LSB and the eighth beat carries `last_packet`.
 * - `blocks[0]` of the **first** packet is the message **header**: the probe ID
 *   occupies packet bits `[60:56]`.
 * - The remaining blocks carry counter payload, **MSB byte first**, four blocks
 *   per 32-bit counter, running across packet boundaries.  A counter is valid
 *   only if **all four** of its blocks are valid; an invalid counter reads
 *   back as 0.
 *
 * Use @ref telemetry_encode_message to build a conforming beat stream (it is
 * the inverse of the model's decoder and mirrors the transmitter side).
 *
 * ---
 * ## Functional model
 *
 * There is no ATB initiator in this model, so beats are injected through the
 * test-bench back door @ref push_atb_beat — the same abstraction
 * `i2c_controller` uses for its bus and `beu` uses for error sources.
 *
 * 1. **Assembly buffer** — beats accumulate into a byte buffer sized
 *    `8 * ceil((1 + 4 * max_counters_per_message) / 7)`.  On each completed
 *    packet: if `last_packet` is set the message is decoded and queued and the
 *    buffer restarts; if the buffer is instead completely full without ever
 *    seeing `last_packet`, a **missing-last** event fires and the buffer
 *    restarts (the partial message is discarded).
 * 2. **Message buffer** — a circular queue of `buffer_depth` messages.  On
 *    overflow the **oldest** entry is dropped (RTL advances the read pointer),
 *    so the newest telemetry always survives.
 * 3. **Software view** — `TELEMETRY_PROBE_ID`, `TELEMETRY_COUNTER_VLDS` and
 *    `TELEMETRY_COUNTER[i]` always reflect the **oldest** queued message, and
 *    read as 0 while the queue is empty.  `CTRL.BUFFER_POP` dequeues it.
 * 4. **Interrupts** (`irq_o` is the OR of both sources):
 *    - `BUFFER_THRESHOLD`: level, `(fill_level > CTRL.BUFFER_THRESHOLD ||
 *      INTR_TEST.BUFFER_THRESHOLD) && INTR_ENABLE.BUFFER_THRESHOLD`.
 *      `INTR_STATUS.BUFFER_THRESHOLD` mirrors it and is read-only.
 *    - `MISSING_LAST`: sticky.  Set when a missing-last event (or
 *      `INTR_TEST.MISSING_LAST`) occurs **and** `INTR_ENABLE.MISSING_LAST` is
 *      set at that moment; cleared by writing 1 (W1C).
 * 5. **Transmitter flush** — writing `CTRL.TELEMETRY_TX_FLUSH` raises
 *    `afvalid_o`; the bit self-clears (and `afvalid_o` drops) once the
 *    transmitter acknowledges with `afready_i`.
 * 6. **Receiver flush** — writing `CTRL.TELEMETRY_RX_FLUSH` discards the
 *    assembly buffer and the whole message queue.
 *
 * ### Deviations from the RTL (and why)
 *
 * | RTL | Model | Rationale |
 * |-----|-------|-----------|
 * | `atready_o = !flush` (one-cycle dip) | `atready_o` tracks reset only | An RX flush completes inside the register write in an LT model, so the dip has no cycle to occupy. |
 * | `debug_o[3]`/`debug_o[0]` are single-cycle pulses | latched until RX flush or reset | An LT model has no clock edge on which a pulse would be observable; latching makes them checkable. |
 * | Registered pipeline (`end_of_packet_q`, …) | evaluated per injected beat | The observable result (which beat completes a message) is identical. |
 *
 * `CTRL.BUFFER_THRESHOLD` is a 12-bit field but the RTL casts it to the
 * message-buffer pointer width (`clog2(buffer_depth) + 1` bits) before
 * comparing, so high threshold values **truncate**.  The model reproduces that
 * truncation.
 *
 * ### Loosely-timed
 * `b_transport` never calls `wait()`; it only adds `access_delay_ns` to the
 * annotated delay.  Temporal decoupling is the initiator's responsibility.
 *
 * ---
 * ## CCI configuration parameters
 *
 * | Name | Type | Default | Mutability | Purpose |
 * |------|------|---------|------------|---------|
 * | `buffer_depth` | uint | 8 | immutable | Message-queue depth (>= 2). Sizes the queue and the threshold-compare width. |
 * | `max_counters_per_message` | uint | 4 | immutable | Counters per message (1..32). Sizes the assembly buffer. |
 * | `access_delay_ns` | double | 2.0 | mutable | TLM `b_transport` annotated delay (AXI4-Lite latency). |
 */

#ifndef SMC_TELEMETRY_RECEIVER_H_
#define SMC_TELEMETRY_RECEIVER_H_

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>
#include <iostream>
#include <vector>

#include <cci_configuration>

#include "reg_access.h"
#include "reg_map.h"
#include "smc_axi_extension.h"

namespace smc {

// ---------------------------------------------------------------------------
// telemetry_receiver_cfg
// ---------------------------------------------------------------------------

/**
 * @brief Construction defaults plus the fixed address-map / protocol constants.
 *
 * Register offsets and field positions are compile-time fixed by
 * `telemetry_receiver.rdl`; the ATB widths come from
 * `telemetry_receiver_pkg.sv`.
 */
struct telemetry_receiver_cfg {
    unsigned buffer_depth             = 8;   ///< Message-queue depth (>= 2).
    unsigned max_counters_per_message = 4;   ///< Counters per message (1..32).
    double   access_delay_ns          = 2.0; ///< TLM annotated delay.

    /// Per-instance register window (`telemetry_receiver_wrap.rdl` stride).
    static constexpr uint64_t WINDOW_SIZE = 0x100;
    /// Register access stride in bytes (32-bit registers).
    static constexpr unsigned REG_WIDTH = 4;

    // ---- Register byte offsets (telemetry_receiver.rdl) -------------------
    static constexpr uint64_t CTRL                   = 0x00;
    static constexpr uint64_t STATUS                 = 0x04;
    static constexpr uint64_t INTR_STATUS            = 0x08;
    static constexpr uint64_t INTR_ENABLE            = 0x0C;
    static constexpr uint64_t INTR_TEST              = 0x10;
    static constexpr uint64_t TELEMETRY_PROBE_ID     = 0x14;
    static constexpr uint64_t TELEMETRY_COUNTER_VLDS = 0x18;
    static constexpr uint64_t TELEMETRY_COUNTER0     = 0x80;
    /// `NUM_COUNTER_REGS` in `telemetry_receiver_pkg.sv`.
    static constexpr unsigned NUM_COUNTER_REGS = 32;

    // ---- CTRL fields -----------------------------------------------------
    static constexpr uint32_t CTRL_BUFFER_POP      = 1u << 0;
    static constexpr uint32_t CTRL_RX_FLUSH        = 1u << 4;
    static constexpr uint32_t CTRL_TX_FLUSH        = 1u << 8;
    static constexpr unsigned CTRL_THRESHOLD_SHIFT = 12;
    static constexpr uint32_t CTRL_THRESHOLD_MASK  = 0xFFFu << CTRL_THRESHOLD_SHIFT;
    /// Bits CTRL keeps in storage: the write-pulse bits read back as 0.
    static constexpr uint32_t CTRL_STORE_MASK = CTRL_TX_FLUSH | CTRL_THRESHOLD_MASK;

    // ---- STATUS fields ---------------------------------------------------
    static constexpr uint32_t STATUS_BUFFER_EMPTY = 1u << 0;
    static constexpr uint32_t STATUS_BUFFER_FULL  = 1u << 4;

    // ---- Interrupt bit layout (shared by STATUS / ENABLE / TEST) ---------
    static constexpr uint32_t INTR_MISSING_LAST     = 1u << 0;
    static constexpr uint32_t INTR_BUFFER_THRESHOLD = 1u << 4;
    static constexpr uint32_t INTR_MASK = INTR_MISSING_LAST | INTR_BUFFER_THRESHOLD;

    static constexpr uint32_t PROBE_ID_MASK = 0x1Fu;

    // ---- ATB protocol (telemetry_receiver_pkg.sv) ------------------------
    static constexpr unsigned BEATS_PER_PACKET  = 8;  ///< 64-bit packet / 8-bit beat.
    static constexpr unsigned BLOCKS_PER_PACKET = 7;  ///< 64 / 9 (block = vld + byte).
    static constexpr unsigned COUNTER_BYTES     = 4;  ///< 32-bit counters.
    /// Packet bit position of the `last_packet` marker.
    static constexpr unsigned PACKET_LAST_BIT = 63;
    /// LSB of `PROBE_ID` inside the first packet (`packet[60:56]`).
    static constexpr unsigned PROBE_ID_LSB = 56;
    /// LSB of `blocks[0].data`; block *k* sits `9 * k` bits below it.
    static constexpr unsigned BLOCK0_DATA_LSB = 54;
    static constexpr unsigned BLOCK_BITS      = 9;

    // ---- Debug output bit positions (telemetry_receiver.sv `debug_o`) ----
    static constexpr uint32_t DBG_MISSING_LAST     = 1u << 0;
    static constexpr uint32_t DBG_BUFFER_FULL      = 1u << 1;
    static constexpr uint32_t DBG_BUFFER_EMPTY     = 1u << 2;
    static constexpr uint32_t DBG_ASSEMBLY_FULL    = 1u << 3;
};

// ---------------------------------------------------------------------------
// ATB packet field accessors
// ---------------------------------------------------------------------------

/// One ATB block: a payload byte plus its valid bit.
struct telemetry_block {
    bool    vld  = false;
    uint8_t data = 0;
};

/// One decoded counter sample.
struct telemetry_counter_value {
    bool     vld   = false;
    uint32_t value = 0;
};

/// One decoded telemetry message: a probe ID plus its counter samples.
struct telemetry_message {
    uint8_t                              probe_id = 0;
    std::vector<telemetry_counter_value> counters;
};

/// True if @p packet carries the end-of-message marker.
constexpr bool telemetry_packet_last(uint64_t packet)
{
    return ((packet >> telemetry_receiver_cfg::PACKET_LAST_BIT) & 1u) != 0;
}

/// Probe ID carried by the first packet of a message (`packet[60:56]`).
constexpr uint8_t telemetry_packet_probe_id(uint64_t packet)
{
    return static_cast<uint8_t>((packet >> telemetry_receiver_cfg::PROBE_ID_LSB) &
                                telemetry_receiver_cfg::PROBE_ID_MASK);
}

/// Extract block @p k (0..6) from @p packet.
constexpr telemetry_block telemetry_packet_block(uint64_t packet, unsigned k)
{
    const unsigned data_lsb = telemetry_receiver_cfg::BLOCK0_DATA_LSB -
                              telemetry_receiver_cfg::BLOCK_BITS * k;
    return telemetry_block{((packet >> (data_lsb + 8)) & 1u) != 0,
                           static_cast<uint8_t>((packet >> data_lsb) & 0xFFu)};
}

/// Insert block @p k (0..6) into @p packet.
constexpr void telemetry_set_packet_block(uint64_t& packet, unsigned k,
                                          bool vld, uint8_t data)
{
    const unsigned data_lsb = telemetry_receiver_cfg::BLOCK0_DATA_LSB -
                              telemetry_receiver_cfg::BLOCK_BITS * k;
    packet |= (static_cast<uint64_t>(data) << data_lsb);
    if (vld) packet |= (uint64_t{1} << (data_lsb + 8));
}

/// Number of packets a message with @p max_counters counters occupies.
constexpr unsigned telemetry_packets_per_message(unsigned max_counters)
{
    const unsigned blocks = 1u + telemetry_receiver_cfg::COUNTER_BYTES * max_counters;
    return (blocks + telemetry_receiver_cfg::BLOCKS_PER_PACKET - 1u) /
           telemetry_receiver_cfg::BLOCKS_PER_PACKET;
}

/**
 * @brief Reference encoder: build the ATB beat stream for one message.
 *
 * The inverse of the model's decoder, mirroring what the telemetry transmitter
 * emits.  Provided for test benches and for a future platform-level ATB bridge
 * so the packing rules live in exactly one place.
 *
 * @param probe_id    Probe ID (low 5 bits used).
 * @param counters    Counter samples; entries past @p max_counters are ignored
 *                    and missing entries are encoded as invalid.
 * @param max_counters Counters per message the receiver is configured for.
 * @param set_last_packet Set the `last_packet` marker on the final packet.
 *                    Pass `false` to provoke a missing-last event.
 * @return Beat stream, `8 * telemetry_packets_per_message(max_counters)` bytes.
 */
std::vector<uint8_t> telemetry_encode_message(
    uint8_t probe_id,
    const std::vector<telemetry_counter_value>& counters,
    unsigned max_counters,
    bool set_last_packet = true);

// ---------------------------------------------------------------------------
// telemetry_receiver
// ---------------------------------------------------------------------------

/**
 * @brief SystemC/TLM-2.0 LT model of one SMC telemetry receiver.
 *
 * ### Ports
 *
 * | Port         | Dir | Description |
 * |--------------|-----|-------------|
 * | `reg_socket` | tgt | AXI4-Lite-style TLM-2.0 target socket (32-bit access). |
 * | `rst_n_i`    | in  | Active-low asynchronous reset. |
 * | `afready_i`  | in  | Transmitter flush acknowledge (ATB AF channel). |
 * | `irq_o`      | out | Interrupt: `MISSING_LAST || BUFFER_THRESHOLD`. |
 * | `afvalid_o`  | out | Transmitter flush request (`CTRL.TELEMETRY_TX_FLUSH`). |
 * | `atready_o`  | out | ATB ready (deasserted only while in reset). |
 * | `debug_o`    | out | 4-bit debug vector, see `DBG_*` in @ref telemetry_receiver_cfg. |
 *
 * `recompute_method` is the sole driver of every output (single-driver
 * discipline, as in the UART / PLIC / BEU models); state-changing paths call
 * `schedule_recompute()`.
 */
class telemetry_receiver : public sc_core::sc_module {
protected:
    // CCI params first: the immutable ones size the buffers below.
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> buffer_depth_p_;
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> max_counters_p_;
    cci::cci_param<double>                             access_delay_ns_p_;

public:
    SC_HAS_PROCESS(telemetry_receiver);

    /// AXI4-Lite-style TLM-2.0 target socket (32-bit access).
    tlm_utils::simple_target_socket<telemetry_receiver, 32> reg_socket;

    sc_core::sc_in<bool>  rst_n_i;   ///< Active-low asynchronous reset.
    sc_core::sc_in<bool>  afready_i; ///< Transmitter flush acknowledge.
    sc_core::sc_out<bool> irq_o;     ///< Interrupt request (active high).
    sc_core::sc_out<bool> afvalid_o; ///< Transmitter flush request.
    sc_core::sc_out<bool> atready_o; ///< ATB ready.
    sc_core::sc_out<uint32_t> debug_o; ///< 4-bit debug vector.

    /**
     * @brief Construct the telemetry receiver model.
     * @param name SystemC module name.
     * @param cfg  Construction defaults.  CCI presets take priority.
     */
    explicit telemetry_receiver(sc_core::sc_module_name name,
                               telemetry_receiver_cfg cfg = telemetry_receiver_cfg{});

    // ------------------------------------------------------------------
    // Test-bench back door (ATB ingress)
    // ------------------------------------------------------------------

    /**
     * @brief Inject one ATB byte beat (`atdata_i` with `atvalid_i` high).
     *
     * Completing a packet whose `last_packet` marker is set queues the decoded
     * message; filling the assembly buffer without ever seeing the marker
     * raises a missing-last event.
     *
     * @return false if the receiver is not accepting beats (`atready_o` low).
     */
    bool push_atb_beat(uint8_t beat);

    /// Inject a whole beat stream (see @ref telemetry_encode_message).
    /// @return number of beats accepted.
    unsigned push_atb_beats(const std::vector<uint8_t>& beats);

    /// Current message-queue fill level (`0 .. buffer_depth`).
    unsigned fill_level() const { return count_; }

    /// Side-effect-free register peek (no state change).
    uint32_t dbg_reg(uint64_t off) const;

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
    void reset_proc();          ///< SC_METHOD on rst_n_i: clear all state.
    void af_handshake_method(); ///< SC_METHOD on afready_i: retire TX flush.
    void recompute_method();    ///< SC_METHOD on recompute_event_: drive outputs.
    void start_of_simulation() override; ///< Force an initial output drive.

    void schedule_recompute();

    // ------------------------------------------------------------------
    // Register decode
    // ------------------------------------------------------------------
    bool reg_read (uint64_t off, uint32_t& data) const;
    bool reg_write(uint64_t off, uint32_t data);

    /// Counter-register index for @p off, or -1 if @p off is not in the array.
    int  counter_index(uint64_t off) const;

    // ------------------------------------------------------------------
    // Telemetry datapath
    // ------------------------------------------------------------------
    uint64_t          packet_word(unsigned packet_index) const;
    telemetry_message decode_message() const;
    void              queue_message(const telemetry_message& msg);
    void              pop_message();
    void              rx_flush();
    void              raise_missing_last();
    /// Apply the INTR_ENABLE.MISSING_LAST gate and set the sticky status bit.
    void              set_missing_last_status();

    /// Oldest queued message, or an all-zero message while the queue is empty.
    const telemetry_message& visible_message() const;

    bool     threshold_irq_active() const;
    unsigned threshold_compare_value() const;

    // ------------------------------------------------------------------
    // Internal state
    // ------------------------------------------------------------------
    telemetry_receiver_cfg cfg_;

    /// CTRL storage (TX_FLUSH + BUFFER_THRESHOLD; pulse bits read back 0).
    regmodel::Register32 ctrl_{telemetry_receiver_cfg::CTRL_STORE_MASK,
                               telemetry_receiver_cfg::CTRL_STORE_MASK, 0};
    /// INTR_ENABLE storage (plain RW).
    regmodel::Register32 intr_enable_{telemetry_receiver_cfg::INTR_MASK,
                                     telemetry_receiver_cfg::INTR_MASK, 0};
    /// INTR_TEST storage: only the level bit is held; the pulse bit reads 0.
    regmodel::Register32 intr_test_{telemetry_receiver_cfg::INTR_BUFFER_THRESHOLD,
                                   telemetry_receiver_cfg::INTR_BUFFER_THRESHOLD, 0};

    regmodel::RegisterMap32 regmap_; ///< offset -> storage register dispatch.

    /// Sticky `INTR_STATUS.MISSING_LAST` (set by HW, W1C by software).
    bool missing_last_status_ = false;

    /// `CTRL.BUFFER_THRESHOLD` is compared at message-buffer-pointer width
    /// (`clog2(buffer_depth) + 1` bits); wider values truncate, as in the RTL.
    unsigned threshold_wrap_mask_ = 0;

    /// ATB ingress accepted (drives `atready_o`); low while in reset.
    bool accepting_ = true;

    // Assembly buffer (ATB ingress).
    std::vector<uint8_t> assembly_;    ///< Sized 8 * packets-per-message.
    unsigned             beats_       = 0; ///< Beats accepted into assembly_.

    // Message buffer (circular, drop-oldest on overflow).
    std::vector<telemetry_message> queue_;
    unsigned                       rd_idx_ = 0;
    unsigned                       count_  = 0;

    /// Returned by visible_message() while the queue is empty.
    telemetry_message empty_message_;

    // Latched debug events (see the deviation table above).
    bool dbg_missing_last_  = false;
    bool dbg_assembly_full_ = false;

    // Output cache to suppress redundant sc_signal writes.
    bool     out_irq_       = false;
    bool     out_afvalid_   = false;
    bool     out_atready_   = false;
    uint32_t out_debug_     = 0;
    bool     outputs_valid_ = false;

    sc_core::sc_event recompute_event_; ///< Triggers recompute_method().
    sc_core::sc_event af_event_;        ///< Retries the TX-flush handshake.
};

} // namespace smc

#endif // SMC_TELEMETRY_RECEIVER_H_
