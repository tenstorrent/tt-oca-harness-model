// SPDX-License-Identifier: Apache-2.0
/**
 * @file avsbus_controller.h
 * @brief SystemC/TLM-2.0 Loosely-Timed (LT) model of the SMC AVSBus Controller.
 *
 * Register-accurate functional model of the single-target AVSBus 1.3.1
 * controller used by the SMC (`hw/ip/avsbus_controller`).  Mapped at
 * `0xC000_8000` (4 KiB window); interrupt feeds peripheral bit 22 / PLIC 278.
 *
 * Authoritative references
 * ------------------------
 * - `hw/ip/avsbus_controller/data/registers/rdl/avsbus_controller.rdl`
 * - `hw/ip/avsbus_controller/doc/{index,architecture,interface,memmap}.adoc`
 * - `hw/smc/data/registers/rdl/smc_top.rdl` (`smc_avsbus_controller @ +0x8000`)
 * - OCAH documentation § AVSBus Controller
 *
 * Abstraction
 * -----------
 * Loosely-timed: software writes `AVS_CMD` into an 8-deep command FIFO; a
 * timed background process synthesizes (or queries a slave model for) a
 * slave subframe and pushes it into an 8-deep readback FIFO.  Bit-serial
 * AVS wire timing, clock mux/divider, and GPIO pad OE are not modelled at
 * cycle accuracy — `AVS_CFG_1` / `AVS_CONFIG` are stored and returned.
 */

#ifndef SMC_AVSBUS_CONTROLLER_H_
#define SMC_AVSBUS_CONTROLLER_H_

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cci_configuration>

#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <string>

#include "reg_access.h"
#include "reg_map.h"
#include "smc_axi_extension.h"

namespace smc {

// ---------------------------------------------------------------------------
// Interrupt bit indices (AVS_INTERRUPT / MASK / CLEAR)
// ---------------------------------------------------------------------------

namespace avs_irq {
constexpr unsigned SLAVE_ISSUED        = 0;
constexpr unsigned CMD_FIFO_FULL       = 1;
constexpr unsigned READBACK_FIFO_FULL  = 2;
constexpr unsigned READBACK_HAS_DATA   = 3;
constexpr unsigned SLAVE_UNRESPONSIVE  = 4;
constexpr unsigned MAX_RETRIES         = 5;
constexpr unsigned CMD_FIFO_OVERFLOW   = 6;
constexpr unsigned READBACK_UNDERFLOW  = 7;
constexpr unsigned READBACK_OVERFLOW   = 8;
constexpr uint32_t ALL_MASK            = 0x1FFu;
} // namespace avs_irq

// ---------------------------------------------------------------------------
// AVS_CMD / AVS_READBACK field helpers
// ---------------------------------------------------------------------------

namespace avs_cmd {
constexpr unsigned R_OR_W_SHIFT    = 28;
constexpr unsigned CMD_GRP_SHIFT   = 27;
constexpr unsigned CMD_CODE_SHIFT  = 23;
constexpr unsigned RAIL_SEL_SHIFT  = 19;
constexpr unsigned CMD_DATA_SHIFT  = 3;

constexpr uint32_t R_OR_W_MASK     = 0x3u;
constexpr uint32_t CMD_GRP_MASK    = 0x1u;
constexpr uint32_t CMD_CODE_MASK   = 0xFu;
constexpr uint32_t RAIL_SEL_MASK   = 0xFu;
constexpr uint32_t CMD_DATA_MASK   = 0xFFFFu;

/// R_OR_W encodings (RDL).
constexpr uint32_t COMMIT_WRITE = 0x0;
constexpr uint32_t HOLD_WRITE   = 0x1;
constexpr uint32_t READ         = 0x2;

inline uint32_t pack(uint32_t r_or_w, uint32_t cmd_grp, uint32_t cmd_code,
                     uint32_t rail_sel, uint32_t cmd_data)
{
    return ((r_or_w   & R_OR_W_MASK)    << R_OR_W_SHIFT)  |
           ((cmd_grp  & CMD_GRP_MASK)   << CMD_GRP_SHIFT) |
           ((cmd_code & CMD_CODE_MASK)  << CMD_CODE_SHIFT)|
           ((rail_sel & RAIL_SEL_MASK)  << RAIL_SEL_SHIFT)|
           ((cmd_data & CMD_DATA_MASK)  << CMD_DATA_SHIFT);
}

inline uint32_t r_or_w   (uint32_t w) { return (w >> R_OR_W_SHIFT)   & R_OR_W_MASK; }
inline uint32_t cmd_grp  (uint32_t w) { return (w >> CMD_GRP_SHIFT)  & CMD_GRP_MASK; }
inline uint32_t cmd_code (uint32_t w) { return (w >> CMD_CODE_SHIFT) & CMD_CODE_MASK; }
inline uint32_t rail_sel (uint32_t w) { return (w >> RAIL_SEL_SHIFT) & RAIL_SEL_MASK; }
inline uint32_t cmd_data (uint32_t w) { return (w >> CMD_DATA_SHIFT) & CMD_DATA_MASK; }
} // namespace avs_cmd

namespace avs_rb {
constexpr unsigned SLAVE_ACK_SHIFT        = 30;
constexpr unsigned CONST0_SHIFT           = 29;
constexpr unsigned STATUS_RESPONSE_SHIFT  = 24;
constexpr unsigned CMD_DATA_SHIFT         = 8;
constexpr unsigned CRC_SHIFT              = 0;

constexpr uint32_t SLAVE_ACK_MASK         = 0x3u;
constexpr uint32_t STATUS_RESPONSE_MASK   = 0x1Fu;
constexpr uint32_t CMD_DATA_MASK          = 0xFFFFu;
constexpr uint32_t CRC_MASK               = 0x7u;

/// SlaveAck encodings (RDL).
constexpr uint32_t ACK_OK          = 0x0;
constexpr uint32_t ACK_BUSY        = 0x1; ///< triggers HW retry
constexpr uint32_t ACK_BAD_CRC     = 0x2; ///< triggers HW retry
constexpr uint32_t ACK_BAD_DATA    = 0x3; ///< no HW retry

inline uint32_t pack(uint32_t slave_ack, uint32_t status_resp,
                     uint32_t cmd_data, uint32_t crc)
{
    return ((slave_ack   & SLAVE_ACK_MASK)       << SLAVE_ACK_SHIFT)       |
           ((status_resp & STATUS_RESPONSE_MASK) << STATUS_RESPONSE_SHIFT) |
           ((cmd_data    & CMD_DATA_MASK)        << CMD_DATA_SHIFT)        |
           ((crc         & CRC_MASK)             << CRC_SHIFT);
}

inline uint32_t slave_ack     (uint32_t w) { return (w >> SLAVE_ACK_SHIFT)       & SLAVE_ACK_MASK; }
inline uint32_t status_resp   (uint32_t w) { return (w >> STATUS_RESPONSE_SHIFT) & STATUS_RESPONSE_MASK; }
inline uint32_t cmd_data      (uint32_t w) { return (w >> CMD_DATA_SHIFT)        & CMD_DATA_MASK; }
inline uint32_t crc           (uint32_t w) { return (w >> CRC_SHIFT)             & CRC_MASK; }
} // namespace avs_rb

namespace avs_status {
constexpr unsigned TOTAL_RETRIES_SHIFT      = 0;
constexpr unsigned MASTER_IS_RETRYING_SHIFT  = 16;
constexpr unsigned CMD_FIFO_EMPTY_SHIFT      = 17;
constexpr unsigned CMD_FIFO_FULL_SHIFT       = 18;
constexpr unsigned READBACK_FIFO_FULL_SHIFT  = 19;
constexpr unsigned READBACK_HAS_DATA_SHIFT   = 20;
constexpr unsigned BUS_IS_IDLE_SHIFT         = 21;
constexpr unsigned SLAVE_IN_RESYNC_SHIFT     = 22;
} // namespace avs_status

// ---------------------------------------------------------------------------
// Configuration / address map
// ---------------------------------------------------------------------------

/**
 * @brief Timing defaults and fixed RDL address-map constants.
 */
struct avsbus_controller_cfg {
    double   access_delay_ns     = 2.0;   ///< TLM annotated AXI-Lite latency.
    double   xfer_delay_ns       = 50.0;  ///< Delay from cmd launch to response.
    double   resync_delay_ns     = 20.0;  ///< Duration of modelled resync pulse.
    unsigned command_fifo_depth  = 8;
    unsigned readback_fifo_depth = 8;

    /// SMC allocates 4 KiB (`0xC000_8000`); used footprint is `0x00..0x5B`.
    static constexpr uint64_t WINDOW_SIZE = 0x1000;
    static constexpr unsigned REG_WIDTH   = 4;

    // ---- Register byte offsets (avsbus_controller.rdl) -----------------
    static constexpr uint64_t AVS_CMD                   = 0x00;
    static constexpr uint64_t AVS_READBACK              = 0x04;
    static constexpr uint64_t AVS_DEBUG_READBACK        = 0x08;
    static constexpr uint64_t AVS_LATEST_SLAVE_SUBFRAME = 0x0C;
    static constexpr uint64_t AVS_NORMAL_STATUS         = 0x20;
    static constexpr uint64_t AVS_SLAVE_STATUS          = 0x24;
    static constexpr uint64_t AVS_FIFOS_STATUS          = 0x28;
    static constexpr uint64_t AVS_INTERRUPT             = 0x30;
    static constexpr uint64_t AVS_INTERRUPT_MASK        = 0x34;
    static constexpr uint64_t AVS_INTERRUPT_CLEAR       = 0x38;
    static constexpr uint64_t AVS_CFG_0                 = 0x50;
    static constexpr uint64_t AVS_CFG_1                 = 0x54;
    static constexpr uint64_t AVS_CONFIG                = 0x58;

    // ---- Field masks / resets ------------------------------------------
    static constexpr uint32_t CFG_0_MASK   = 0x00FFFFFFu; ///< [23:0]
    static constexpr uint32_t CFG_0_RESET  = 0x00051000u; ///< max_retries=5, resync=0x1000
    static constexpr uint32_t CFG_1_MASK   = 0xFFFF0703u; ///< duty/div + ctrl bits
    static constexpr uint32_t CFG_1_RESET  = 0x00000003u; ///< clock_select=3
    static constexpr uint32_t CONFIG_MASK  = 0x1u;
    static constexpr uint32_t CONFIG_RESET = 0x1u;        ///< GPIO enable
    static constexpr uint32_t IRQ_MASK_RST = 0x1FFu;      ///< all disabled
};

/**
 * @brief Slave-model callback: given the launched command word, return the
 *        32-bit slave subframe that should be pushed into the readback FIFO.
 *
 * Returning a subframe with `SLAVE_ACK` ∈ {1,2} triggers automatic retries
 * (up to `AVS_CFG_0.MAX_RETRIES`).  A null / unset model uses the built-in
 * default responder (ACK=OK, echo/synthetic data).
 */
using avs_slave_model_fn = std::function<uint32_t(uint32_t cmd_word)>;

// ---------------------------------------------------------------------------
// Module
// ---------------------------------------------------------------------------

/**
 * @brief SystemC/TLM-2.0 LT model of the SMC AVSBus Controller.
 *
 * ### Ports
 * | Port         | Dir | Description |
 * |--------------|-----|-------------|
 * | `reg_socket` | tgt | AXI4-Lite-style TLM-2.0 target (32-bit). |
 * | `rst_n_i`    | in  | Active-low asynchronous reset. |
 * | `irq_o`      | out | Aggregated interrupt (active-high). |
 * | `avs_gpio_enable_o` | out | Mirror of `AVS_CONFIG.AVS_GPIO_ENABLE`. |
 *
 * `recompute_method` is the sole driver of `irq_o` / `avs_gpio_enable_o`.
 */
class avsbus_controller : public sc_core::sc_module {
protected:
    // CCI parameters (declared before ports so sizes resolve first).
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> command_fifo_depth_p_;
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> readback_fifo_depth_p_;
    cci::cci_param<double> access_delay_ns_p_;
    cci::cci_param<double> xfer_delay_ns_p_;
    cci::cci_param<double> resync_delay_ns_p_;

public:
    SC_HAS_PROCESS(avsbus_controller);

    tlm_utils::simple_target_socket<avsbus_controller> reg_socket;

    sc_core::sc_in<bool>  rst_n_i;
    sc_core::sc_out<bool> irq_o;
    sc_core::sc_out<bool> avs_gpio_enable_o;

    explicit avsbus_controller(sc_core::sc_module_name name,
                               avsbus_controller_cfg cfg = avsbus_controller_cfg{});

    // ------------------------------------------------------------------
    // Test-bench / platform back doors
    // ------------------------------------------------------------------

    /// Install (or clear with `{}`) a slave-response callback.
    void set_slave_model(avs_slave_model_fn fn);

    /**
     * @brief Inject a sticky interrupt status bit (after mask aggregation).
     * Useful for testing SLAVE_ISSUED / SLAVE_UNRESPONSIVE without a wire model.
     */
    void inject_interrupt(unsigned bit);

    /**
     * @brief Push a synthetic slave subframe into the readback FIFO (or raise
     *        READBACK_OVERFLOW if full).  Optional retry-counter bump.
     */
    void inject_readback(uint32_t frame, bool count_as_retry = false);

    /// Side-effect-free register peek.
    uint32_t dbg_reg(uint64_t off) const;

    unsigned cmd_fifo_count() const { return static_cast<unsigned>(cmd_fifo_.size()); }
    unsigned rb_fifo_count()  const { return static_cast<unsigned>(rb_fifo_.size()); }

    void dump_state(std::ostream& os = std::cout) const;

    /// AVSBus CRC-3 over a full 32-bit wire message (matches `avsbus_crc3.sv`).
    static uint8_t crc3(uint32_t msg);

private:
    // TLM
    void         b_transport (tlm::tlm_generic_payload& gp, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& gp);

    // Processes
    void reset_proc();
    void recompute_method();
    void xfer_thread();       ///< SC_THREAD: drains cmd FIFO after xfer_delay.
    void resync_method();     ///< Ends modelled resync pulse.

    void schedule_recompute();
    void schedule_xfer();
    void start_resync();

    // Register decode
    bool reg_read (uint64_t off, uint32_t& data);
    bool reg_write(uint64_t off, uint32_t data);

    // Protocol helpers
    void push_cmd(uint32_t cmd);
    uint32_t pop_readback();          ///< May raise UNDERFLOW irq.
    uint32_t peek_readback() const;
    uint32_t default_slave_response(uint32_t cmd) const;
    void complete_one_xfer();
    void apply_response(uint32_t cmd, uint32_t resp, bool count_as_retry);

    void set_irq_bit(unsigned bit);
    void clear_irq_bits(uint32_t w1c_data);
    bool irq_active() const;

    uint32_t normal_status() const;
    uint32_t fifos_status() const;
    uint32_t slave_status() const;

    unsigned max_retries() const;
    unsigned cmd_depth() const { return command_fifo_depth_p_.get_value(); }
    unsigned rb_depth()  const { return readback_fifo_depth_p_.get_value(); }

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------
    avsbus_controller_cfg cfg_;

    regmodel::Register32 irq_mask_{avs_irq::ALL_MASK, avs_irq::ALL_MASK,
                                   avsbus_controller_cfg::IRQ_MASK_RST};
    regmodel::Register32 cfg0_{avsbus_controller_cfg::CFG_0_MASK,
                               avsbus_controller_cfg::CFG_0_MASK,
                               avsbus_controller_cfg::CFG_0_RESET};
    regmodel::Register32 cfg1_{avsbus_controller_cfg::CFG_1_MASK,
                               avsbus_controller_cfg::CFG_1_MASK,
                               avsbus_controller_cfg::CFG_1_RESET};
    regmodel::Register32 config_{avsbus_controller_cfg::CONFIG_MASK,
                                 avsbus_controller_cfg::CONFIG_MASK,
                                 avsbus_controller_cfg::CONFIG_RESET};

    regmodel::RegisterMap32 regmap_;

    std::deque<uint32_t> cmd_fifo_;
    std::deque<uint32_t> rb_fifo_;

    uint32_t irq_status_          = 0;
    uint32_t latest_slave_frame_  = 0x0000FFFFu;
    uint32_t total_retries_       = 0;
    bool     master_is_retrying_  = false;
    bool     bus_is_idle_         = true;
    bool     slave_in_resync_     = false;
    bool     xfer_pending_        = false;
    unsigned retries_this_cmd_    = 0;
    uint32_t current_cmd_         = 0;

    avs_slave_model_fn slave_model_;

    bool out_irq_     = false;
    bool out_gpio_en_ = true;
    bool outputs_valid_ = false;

    sc_core::sc_event recompute_event_;
    sc_core::sc_event xfer_event_;
    sc_core::sc_event resync_done_event_;
};

} // namespace smc

#endif // SMC_AVSBUS_CONTROLLER_H_
