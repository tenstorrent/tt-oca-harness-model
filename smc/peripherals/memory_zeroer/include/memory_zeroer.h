// SPDX-License-Identifier: Apache-2.0
/**
 * @file memory_zeroer.h
 * @brief SystemC/TLM-2.0 Loosely-Timed (LT) model of the SMC memory zeroer.
 *
 * Functional model of the AXI memory-zeroer control block.  Software programs
 * a destination address and byte count, then writes CTRL_STATUS to start a
 * blocking (LT) burst of zero-fills over the DMA initiator socket.
 *
 * Authoritative register map: `axi_zeroer_ctrl.rdl` / Virtualizer AXI_zeroer.
 *
 * | Offset | Register     | Access | Description                          |
 * |--------|--------------|--------|--------------------------------------|
 * | 0x00   | DEST_ADDR    | RW     | Byte address to write zeros to       |
 * | 0x08   | SIZE         | RW     | Size in bytes of zeros to write      |
 * | 0x10   | CTRL_STATUS  | RW/RO  | int_en[0], busy status[32]; write    |
 * |        |              |        | triggers the zero job when SIZE!=0   |
 *
 * System window (docs): `0xC003_8200`, size `0x18`.  Fabric routes control
 * through `smc_fabric.to_data_accel_ctrl`; IRQ feeds internal interrupt 3.
 */

#ifndef SMC_MEMORY_ZEROER_H_
#define SMC_MEMORY_ZEROER_H_

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>
#include <iostream>
#include <string>

#include <cci_configuration>

#include "reg_access.h"
#include "reg_map.h"
#include "smc_tlm_extensions.h"

namespace smc {

// ---------------------------------------------------------------------------
// memory_zeroer_cfg — address-map / sizing constants + ctor defaults
// ---------------------------------------------------------------------------

struct memory_zeroer_cfg {
    /// SMC fabric decode window base (docs: data-accelerator zeroer ctrl).
    static constexpr uint64_t DEFAULT_BASE_ADDR = 0xC003'8200ULL;
    /// Register file span (DEST_ADDR + SIZE + CTRL_STATUS).
    static constexpr uint64_t WINDOW_SIZE       = 0x18ULL;

    static constexpr uint64_t OFF_DEST_ADDR   = 0x00u;
    static constexpr uint64_t OFF_SIZE        = 0x08u;
    static constexpr uint64_t OFF_CTRL_STATUS = 0x10u;

    /// CTRL_STATUS.int_en — RW, interrupt enable on completion.
    static constexpr uint64_t CTRL_INT_EN_BIT = 0u;
    /// CTRL_STATUS.status — RO to SW; 1 while a job is in progress.
    static constexpr uint64_t CTRL_STATUS_BIT = 32u;

    static constexpr uint64_t CTRL_INT_EN_MASK =
        UINT64_C(1) << CTRL_INT_EN_BIT;
    static constexpr uint64_t CTRL_STATUS_MASK =
        UINT64_C(1) << CTRL_STATUS_BIT;
    static constexpr uint64_t CTRL_RMASK =
        CTRL_INT_EN_MASK | CTRL_STATUS_MASK;
    static constexpr uint64_t CTRL_WMASK = CTRL_INT_EN_MASK;

    /// Default DMA burst chunk (matches Virtualizer AXI_zeroer).
    static constexpr unsigned DEFAULT_CHUNK_SIZE = 4096u;

    uint64_t base_addr       = DEFAULT_BASE_ADDR;
    unsigned chunk_size      = DEFAULT_CHUNK_SIZE;
    double   access_delay_ns = 2.0;
};

// ---------------------------------------------------------------------------
// memory_zeroer — SC_MODULE
// ---------------------------------------------------------------------------

/**
 * @brief SystemC/TLM-2.0 LT model of the SMC AXI memory zeroer.
 *
 * Ports:
 *  - `reg_socket`  — 64-bit register target (MMIO)
 *  - `dma_socket`  — initiator writing zero payloads into system memory
 *  - `rst_n_i`     — active-low reset
 *  - `irq_o`       — active-high completion interrupt (gated by int_en)
 */
class memory_zeroer : public sc_core::sc_module {
protected:
    // CCI params before ports / sized members so presets resolve first.
    cci::cci_param<unsigned, cci::CCI_IMMUTABLE_PARAM> chunk_size_p_;
    cci::cci_param<double> access_delay_ns_p_;

public:
    SC_HAS_PROCESS(memory_zeroer);

    tlm_utils::simple_target_socket<memory_zeroer>    reg_socket;
    tlm_utils::simple_initiator_socket<memory_zeroer> dma_socket;

    sc_core::sc_in<bool>  rst_n_i;
    sc_core::sc_out<bool> irq_o;

    explicit memory_zeroer(sc_core::sc_module_name name,
                           memory_zeroer_cfg cfg = memory_zeroer_cfg{});

    // ---- Test / debug back-doors (no bus side effects) --------------------

    uint64_t dbg_dest_addr() const { return dest_addr_.raw(); }
    uint64_t dbg_size() const { return size_.raw(); }
    uint64_t dbg_ctrl_status() const { return ctrl_status_.raw(); }
    bool     dbg_busy() const {
        return (ctrl_status_.raw() & memory_zeroer_cfg::CTRL_STATUS_MASK) != 0;
    }
    bool     dbg_int_en() const {
        return (ctrl_status_.raw() & memory_zeroer_cfg::CTRL_INT_EN_MASK) != 0;
    }

    /// Dump register image to @p os (debug / TB).
    void dump_state(std::ostream& os = std::cout) const;

private:
    void b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay);
    void reset_proc();

    bool reg_read(uint64_t offset, uint64_t& data);
    bool reg_write(uint64_t offset, uint64_t data);

    /// Apply hard reset to registers and irq.
    void do_reset();

    void end_of_elaboration() override;

    /// Write zeros via dma_socket from DEST_ADDR for SIZE bytes (LT, blocking).
    /// @return true if every chunk completed with TLM_OK_RESPONSE.
    bool perform_write_zeros(uint64_t addr, uint64_t nbytes);

    /// Start a job if SIZE != 0 (called on any CTRL_STATUS write).
    void trigger_job();

    void update_irq(bool asserted);

    memory_zeroer_cfg cfg_;

    regmodel::Register64 dest_addr_{~UINT64_C(0), ~UINT64_C(0), UINT64_C(0)};
    regmodel::Register64 size_{~UINT64_C(0), ~UINT64_C(0), UINT64_C(0)};
    /// CTRL_STATUS: SW may only write int_en; status is HW-driven busy bit.
    regmodel::Register64 ctrl_status_{memory_zeroer_cfg::CTRL_RMASK,
                                      memory_zeroer_cfg::CTRL_WMASK,
                                      UINT64_C(0)};

    regmodel::RegisterMap64 regmap_;

    bool outputs_valid_{false};
    bool out_irq_{false};
};

}  // namespace smc

#endif  // SMC_MEMORY_ZEROER_H_
