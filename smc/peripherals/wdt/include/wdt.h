// SPDX-License-Identifier: Apache-2.0
/**
 * @file wdt.h
 * @brief SystemC/TLM-2.0 Loosely-Timed (LT) model of one SMC SiFive TLWDT.
 *
 * One instance models a single per-core Chipyard/SiFive watchdog timer
 * (stage 1).  Stage-2 countdown lives in `cpu_ctrl` / `smc_cpu_cluster`.
 *
 * ---
 * ## Authoritative references
 *
 * | Source | Role |
 * |--------|------|
 * | `hw/smc/smc_cpu/data/registers/rdl/wdt.rdl` | Ground-truth register map |
 * | `hw/smc/smc_cpu/chipyard_generated_files/{1,4}core/OCAH*WatchdogTimer.sv` | Functional RTL |
 * | `hw/smc/smc_cpu/chipyard_generated_files/{1,4}core/OCAH*TLWDT.sv` | TileLink wrapper |
 * | `fw/smc/common/drivers/sifive_wdog0.c` | Firmware access patterns |
 *
 * ---
 * ## Register map (offsets relative to instance base)
 *
 * ```
 * Offset  Reg            Description
 * ──────────────────────────────────────────────────────────────────
 * 0x00    CTRL           scale, rsten, zerocmp, always, awake, ip
 * 0x08    COUNT          31-bit counter (unlocked write loads value)
 * 0x10    SCALED_COUNT   RO count >> scale; write locks only
 * 0x18    FEED           write 0xD09F00D clears count (+ clears sticky)
 * 0x1C    KEY            write 0x51F15E unlocks; read returns unlocked
 * 0x20    CMP            compare vs scaled count (reset 0x1000)
 * Window  0x400 (1 KiB).  Holes RAZ/WI; out-of-window → address error.
 * ```
 *
 * Absolute bases: `0xC000_0000 + N×0x400` for core N.
 */

#ifndef SMC_WDT_H_
#define SMC_WDT_H_

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>
#include <iostream>

#include <cci_configuration>

#include "smc_axi_extension.h"

namespace smc {

// ---------------------------------------------------------------------------
// wdt_cfg
// ---------------------------------------------------------------------------

struct wdt_cfg {
    /// Absolute base of core-0 WDT (informational; model uses relative offsets).
    static constexpr uint64_t SMC_BASE_ADDR = 0xC000'0000ULL;
    static constexpr uint64_t WINDOW_SIZE   = 0x400ULL;
    static constexpr uint64_t INSTANCE_STRIDE = 0x400ULL;

    static constexpr uint64_t OFF_CTRL         = 0x00;
    static constexpr uint64_t OFF_COUNT        = 0x08;
    static constexpr uint64_t OFF_SCALED_COUNT = 0x10;
    static constexpr uint64_t OFF_FEED         = 0x18;
    static constexpr uint64_t OFF_KEY          = 0x1C;
    static constexpr uint64_t OFF_CMP          = 0x20;

    static constexpr uint32_t KEY_MAGIC  = 0x0051F15Eu;
    static constexpr uint32_t FEED_MAGIC = 0x0D09F00Du;

    static constexpr uint32_t CTRL_SCALE_MASK      = 0xFu;
    static constexpr unsigned CTRL_SCALE_SHIFT     = 0;
    static constexpr uint32_t CTRL_RSTEN_BIT       = 1u << 8;
    static constexpr uint32_t CTRL_ZEROCMP_BIT     = 1u << 9;
    static constexpr uint32_t CTRL_ALWAYS_BIT      = 1u << 12;
    static constexpr uint32_t CTRL_AWAKE_BIT       = 1u << 13;
    static constexpr uint32_t CTRL_IP_BIT          = 1u << 28;
    static constexpr uint32_t CTRL_RW_MASK =
        CTRL_SCALE_MASK | CTRL_RSTEN_BIT | CTRL_ZEROCMP_BIT |
        CTRL_ALWAYS_BIT | CTRL_AWAKE_BIT | CTRL_IP_BIT;

    static constexpr uint32_t CMP_RESET = 0x1000u;

    /// Counter tick period in ns.  0 disables auto-tick (tests use dbg_tick).
    double tick_period_ns = 100.0;
    /// TLM annotated access delay in ns.
    double access_delay_ns = 2.0;
};

// ---------------------------------------------------------------------------
// wdt
// ---------------------------------------------------------------------------

/**
 * @brief SystemC/TLM-2.0 LT model of one SiFive / Chipyard TLWDT (stage 1).
 *
 * ### Ports
 *
 * | Port           | Dir    | Description |
 * |----------------|--------|-------------|
 * | `reg_socket`   | target | Register MMIO |
 * | `rst_n_i`      | in     | Active-low module reset |
 * | `core_rst_i`   | in     | Core-in-reset (active-high); gates `wdogcoreawake` |
 * | `irq_o`        | out    | Level IRQ from `wdogip0` → PLIC |
 * | `rst_sticky_o` | out    | Sticky rst when `wdogrsten` && elapsed → stage 2 |
 */
class wdt : public sc_core::sc_module {
protected:
    cci::cci_param<double, cci::CCI_IMMUTABLE_PARAM> tick_period_ns_p_;
    cci::cci_param<double>                           access_delay_ns_p_;

public:
    SC_HAS_PROCESS(wdt);

    tlm_utils::simple_target_socket<wdt> reg_socket;
    sc_core::sc_in<bool>  rst_n_i;
    sc_core::sc_in<bool>  core_rst_i;
    sc_core::sc_out<bool> irq_o;
    sc_core::sc_out<bool> rst_sticky_o;

    explicit wdt(sc_core::sc_module_name name, wdt_cfg cfg = wdt_cfg{});

    // ---- Debug / test back-doors ------------------------------------------
    uint32_t dbg_count() const { return count_; }
    uint16_t dbg_scaled() const;
    uint16_t dbg_cmp() const { return cmp_; }
    bool     dbg_unlocked() const { return unlocked_; }
    bool     dbg_ip() const { return ip_; }
    bool     dbg_elapsed() const;
    bool     dbg_rst_sticky() const { return rst_sticky_; }
    void     dbg_tick(unsigned n = 1);
    void     dbg_set_count(uint32_t v);
    void     dump_state(std::ostream& os = std::cout) const;

private:
    void         b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& gp);

    void reset_proc();
    void tick_method();
    void output_method();
    void schedule_recompute();

    bool reg_read(uint64_t off, unsigned access_size, uint32_t& data) const;
    bool reg_write(uint64_t off, unsigned access_size, uint32_t data);

    void do_unlock_write_key(uint32_t data);
    void do_feed(uint32_t data);
    void apply_elapsed_side_effects(bool fed_this_cycle);
    bool counting_enabled() const;
    uint32_t ctrl_read_value() const;

    wdt_cfg cfg_;

    uint32_t count_     = 0;       ///< 31-bit counter
    uint16_t cmp_       = wdt_cfg::CMP_RESET;
    uint8_t  scale_     = 0;
    bool     rsten_     = false;
    bool     zerocmp_   = false;
    bool     always_    = false;
    bool     awake_     = false;
    bool     ip_        = false;
    bool     unlocked_  = false;
    bool     rst_sticky_ = false;

    bool irq_cache_        = false;
    bool rst_sticky_cache_ = false;

    sc_core::sc_event recompute_event_;
    sc_core::sc_event tick_event_;
    sc_core::sc_time  access_delay_;
    sc_core::sc_time  tick_period_;
};

}  // namespace smc

#endif  // SMC_WDT_H_
