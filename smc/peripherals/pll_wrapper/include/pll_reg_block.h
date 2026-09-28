// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file pll_reg_block.h
 * @brief Shared table-driven register-file base for the SMC PLL wrapper models.
 *
 * The PLL wrapper is a pure register block: the RDL (pll_wrap.rdl and the
 * included cgm.rdl / awm.rdl / pll_cntl.rdl) describes address maps of RW /
 * RO / self-clearing registers with no datapath.  Rather than re-implement the
 * same offset-decode + mask-merge + reset plumbing in each of the four models
 * (`cgm`, `awm`, `pll_cntl`, `pll_wrapper`), they all derive from this base and
 * simply hand it a static table of @ref reg_spec entries.
 *
 * The base is intentionally behaviour-free (per the modelling scope): it stores
 * register state and enforces the software RO/RW/WO contract via the shared
 * `regmodel` helpers (reg_access.h / reg_map.h).  Every register is given its
 * own dedicated read and write callback (installed in build(), parameterised
 * by that register's masks); self-clearing / single-pulse strobes are handled
 * inside their write callback.  A model can replace any single register's
 * callback with bespoke behaviour via set_write_callback()/set_read_callback()
 * — the hook exists per register.  Hardware-driven status bits (lock_detect,
 * monitor counters, ...) are left at their reset value and can be driven from a
 * test or an enclosing model via the @ref poke back door.
 *
 * Bus contract: registers are stored as 32-bit words on 4-byte-aligned
 * offsets, but the SMC PLL firmware accesses them with mixed widths — 16-bit
 * (`uint16_t`) loads/stores for cgm/awm and the pll_cntl status/config
 * registers, and 32-bit stores for the wide pll_cntl registers.  This block
 * therefore accepts naturally aligned 1/2/4-byte accesses that fall within a
 * single 32-bit register, performing a read-modify-write on the containing
 * word.  16-bit RDL registers (cgm/awm) live in the low half of their word.
 *
 * Because every accepted access is naturally aligned and at most 4 bytes, and
 * every window size is a multiple of 4, an access can never start inside the
 * window and end outside it — the start-address check alone is sufficient once
 * size/alignment have been validated.  The bound is nevertheless expressed in
 * an overflow-safe form (`len > window_size_ - adr`) so that a wild address
 * near `UINT64_MAX` cannot wrap into the window.
 *
 * Byte enables: rejected outright (`TLM_BYTE_ENABLE_ERROR_RESPONSE`) whenever
 * `get_byte_enable_ptr()` is non-null, independent of the byte-enable length.
 * The block models register lanes through the address/length pair only, so
 * honouring a partially-enabled pattern would need a second, redundant lane
 * merge; refusing is the TLM-2.0 sanctioned response for a target that does
 * not implement byte enables.
 *
 * Streaming width: a register access is a single beat, so TLM-2.0 requires
 * `streaming_width >= data_length`.  Anything smaller (including the
 * frequently mis-set 0) is a burst error; anything larger is accepted and
 * ignored, as the standard prescribes for non-streaming targets.
 *
 * Debug and DMI: `transport_dbg` is a raw back door with `peek`/`poke`
 * semantics — no read/write masks, no self-clearing strobes, no write
 * observers, and no annotated delay.  DMI is always denied: register reads and
 * writes have side effects (strobes and lock observers) that a direct memory
 * pointer would bypass.
 */

#ifndef SMC_PLL_REG_BLOCK_H_
#define SMC_PLL_REG_BLOCK_H_

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cci_configuration>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "reg_access.h"
#include "reg_map.h"

namespace smc {
namespace pll {

/**
 * @brief One register's software-access contract, as extracted from the RDL.
 *
 * Masks are 32-bit even for 16-bit RDL registers (bus width is 32-bit).
 *  - @p rmask : bits software can read (0 elsewhere => RAZ).
 *  - @p wmask : bits software can write (0 => read-only to software).
 *  - @p self_clear_mask : bits that auto-return to 0 after any write
 *        (RDL `onwrite = wclr/woclr` self-clearing strobes such as REG_UPDATE /
 *        SAMPLE_STROBE, and `singlepulse` fields like postdiv_update_div).
 */
struct reg_spec {
    uint32_t    offset;
    const char* name;
    uint32_t    reset;
    uint32_t    rmask;
    uint32_t    wmask;
    uint32_t    self_clear_mask;
};

/**
 * @brief Table-driven 32-bit register-file SC_MODULE.
 *
 * Ports:
 *  - `reg_socket` : 32-bit TLM target (MMIO register access).
 *  - `rst_n_i`    : active-low reset; restores every register to its RDL reset.
 */
class reg_block : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<reg_block> reg_socket;
    sc_core::sc_in<bool>                        rst_n_i;

    SC_HAS_PROCESS(reg_block);

    /**
     * @param name             SystemC instance name.
     * @param specs            Register table (must outlive the module; use a
     *                         `static constexpr` array in the derived model).
     * @param n                Number of entries in @p specs.
     * @param window_size      Addressable byte span of this block.
     * @param base_addr        System base address (informational / logging).
     * @param access_delay_ns  Default annotated per-access delay (CCI-mutable).
     */
    reg_block(sc_core::sc_module_name name,
              const reg_spec* specs, std::size_t n,
              uint64_t window_size,
              uint64_t base_addr,
              double   access_delay_ns);

    // ---- Test / hardware back doors (no bus side effects) -----------------

    /// Read raw backing storage at @p offset (bypasses read mask). false=miss.
    bool peek(uint64_t offset, uint32_t& out) const;
    /// Overwrite raw backing storage at @p offset (models a HW-driven update).
    bool poke(uint64_t offset, uint32_t value);

    /// Callback invoked after a software write to @p offset, receiving the
    /// full 32-bit value presented to the register (post byte-lane merge,
    /// pre self-clear).  Used by an enclosing model to react to a strobe write
    /// — e.g. a REG_UPDATE that must assert lock status in another sub-block.
    using WriteObserver = std::function<void(uint32_t /*written*/)>;
    /// Register a side-effect observer on a register.  Multiple observers on
    /// one offset fire in registration order.
    void observe_write(uint64_t offset, WriteObserver fn);

    uint64_t window_size() const { return window_size_; }
    uint64_t base_addr()   const { return base_addr_; }
    std::size_t num_registers() const { return map_.size(); }

    /// Dump the register image (offset-ordered) to @p os.
    void dump_state(std::ostream& os = std::cout) const;

    /// Replace the (per-register) write callback at @p offset with bespoke
    /// behaviour — e.g. W1C on a *_clear field, or a hardware side effect.
    /// @return false if @p offset is not a registered register.
    bool set_write_callback(uint64_t offset, regmodel::Register32::WriteFn fn);
    /// Replace the (per-register) read callback at @p offset (read side
    /// effects / hardware-computed value).  @return false on unknown offset.
    bool set_read_callback(uint64_t offset, regmodel::Register32::ReadFn fn);

protected:
    void b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay);
    /// Raw back-door debug access (see the file header): naturally aligned
    /// 1/2/4-byte reads/writes of the backing store, no masks, no strobes, no
    /// observers, no delay.  @return bytes transferred, 0 if refused.
    unsigned int transport_dbg(tlm::tlm_generic_payload& gp);
    /// Always denies DMI — register accesses have side effects.
    bool get_direct_mem_ptr(tlm::tlm_generic_payload& gp,
                            tlm::tlm_dmi& dmi_data);
    /// Shared bus-contract check for b_transport / transport_dbg.  On failure
    /// sets @p status to the TLM response the caller should report.
    bool check_access(const tlm::tlm_generic_payload& gp,
                      tlm::tlm_response_status& status) const;

    /// A validated access mapped onto its containing 32-bit register.
    struct lane_view {
        uint64_t reg_off;    ///< 4-byte-aligned offset of the register
        unsigned lane_bits;  ///< bit position of the addressed lane
        uint32_t lane_mask;  ///< bits of the register the access covers
    };
    /// Decode a bus address/length pair that has already passed check_access().
    static lane_view lane_of(uint64_t adr, unsigned len);
    void reset_proc();
    void do_reset();
    void build(const reg_spec* specs, std::size_t n);

    cci::cci_param<double> access_delay_ns_p_;

    uint64_t window_size_;
    uint64_t base_addr_;

    // deque keeps element pointers stable as the map is populated.
    std::deque<regmodel::Register32> regs_;
    std::vector<uint32_t>            resets_;
    regmodel::RegisterMap32          map_;
    std::multimap<uint64_t, WriteObserver> write_observers_;
};

}  // namespace pll
}  // namespace smc

#endif  // SMC_PLL_REG_BLOCK_H_
