// SPDX-License-Identifier: Apache-2.0
/**
 * @file cpu_ctrl.h
 * @brief SystemC/TLM-2.0 Loosely-Timed (LT) model of the SMC CPU Control block.
 *
 * The CPU Control register block lives in the SMC fabric-control region
 * (`BASE + 0x001_0000`, 8 KiB).  Its **SCRATCH[16]** array at offset `+0x100`
 * is the authoritative SMC↔SEP inter-stage handoff mailbox used by the
 * production ROM (`smc_rom.adoc` § Scratch Registers).
 *
 * Authoritative register map: `hw/smc/smc_misc/data/registers/rdl/cpu_ctrl.rdl`
 *
 * ## Inter-stage handoff scratch indices (firmware contract)
 *
 * | Index | Offset | Symbol (smc_rom_defs.h)        | Role |
 * |-------|--------|--------------------------------|------|
 * | 8     | +0x140 | SMC_SCRATCH_MANIFEST_ADDR      | Manifest SRAM offset |
 * | 9     | +0x148 | SMC_SCRATCH_SMC_STATUS_TO_SEP  | Coordination status bits |
 * | 11    | +0x158 | SMC_SCRATCH_STATUS_BUFFER_ADDR | Status ring-buffer offset |
 * | 13    | +0x168 | SMC_SCRATCH_SEP_SAFE_SRAM_START| SEP safe SRAM start offset |
 * | 14    | +0x170 | SMC_SCRATCH_SEP_SAFE_SRAM_SIZE | SEP safe SRAM size |
 * | 15    | +0x178 | SMC_SCRATCH_MEM_REPAIR_STATUS  | Memory-repair magic status |
 *
 * Full system addresses use `SMC_SCRATCH_BASE_ADDR` (`0xC001_0100`) as the
 * base of index 0 when `LOCAL_BASE = 0xC000_0000`.
 */

#ifndef SMC_CPU_CTRL_H_
#define SMC_CPU_CTRL_H_

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <array>
#include <cstdint>
#include <iostream>

#include <cci_configuration>

#include "reg_access.h"
#include "smc_tlm_extensions.h"

namespace smc {

// ---------------------------------------------------------------------------
// Handoff constants (mirror smc_rom_defs.h / sep_smc_interface.h)
// ---------------------------------------------------------------------------

constexpr unsigned CPU_CTRL_SCRATCH_COUNT = 16;

constexpr unsigned CPU_CTRL_SCRATCH_MANIFEST_IDX       = 8;
constexpr unsigned CPU_CTRL_SCRATCH_STATUS_TO_SEP_IDX  = 9;
constexpr unsigned CPU_CTRL_SCRATCH_STATUS_BUFFER_IDX  = 11;
constexpr unsigned CPU_CTRL_SCRATCH_SEP_SRAM_OFF_IDX   = 13;
constexpr unsigned CPU_CTRL_SCRATCH_SEP_SRAM_SIZE_IDX  = 14;
constexpr unsigned CPU_CTRL_SCRATCH_MEM_REPAIR_IDX     = 15;

constexpr uint32_t CPU_CTRL_SEP_STATUS_SRAM_INIT_BIT         = 0;
constexpr uint32_t CPU_CTRL_SEP_STATUS_MANIFEST_READY_BIT    = 1;
constexpr uint32_t CPU_CTRL_SEP_STATUS_BUFFER_READY_BIT      = 2;
constexpr uint32_t CPU_CTRL_SEP_STATUS_SRAM_PROTECTED_BIT    = 3;

constexpr uint32_t CPU_CTRL_MEM_REPAIR_STATUS_PASSED   = 0x600DCAFEu;
constexpr uint32_t CPU_CTRL_MEM_REPAIR_STATUS_FAILED   = 0xBADC0FFEu;
constexpr uint32_t CPU_CTRL_MEM_REPAIR_STATUS_BYPASSED = 0x12340001u;

// ---------------------------------------------------------------------------
// cpu_ctrl_cfg — address map constants
// ---------------------------------------------------------------------------

struct cpu_ctrl_cfg {
    static constexpr uint64_t DEFAULT_BASE_ADDR = 0xC001'0000ULL;
    static constexpr uint64_t WINDOW_SIZE       = 0x2000ULL;

    static constexpr uint64_t OFF_RESET_VECTOR           = 0x000u;
    static constexpr uint64_t OFF_RESET_CTRL             = 0x020u;
    static constexpr uint64_t OFF_CORE_RESET_PULSE_COUNT = 0x028u;
    static constexpr uint64_t OFF_CLOCK_GATE_CONTROL     = 0x030u;
    static constexpr uint64_t OFF_GLOBAL_BASE            = 0x040u;
    static constexpr uint64_t OFF_LOCAL_BASE             = 0x048u;
    static constexpr uint64_t OFF_REGION_SIZE            = 0x050u;
    static constexpr uint64_t OFF_REFERENCE_COUNTER      = 0x060u;
    static constexpr uint64_t OFF_WDT_TIMEOUT            = 0x070u;
    static constexpr uint64_t OFF_WDT_TIMEOUT_RESET      = 0x078u;
    static constexpr uint64_t OFF_SCRATCH                = 0x100u;
    static constexpr uint64_t OFF_TEST_CTRL              = 0x200u;
    static constexpr uint64_t OFF_DEBUG_CTRL             = 0x208u;
    static constexpr uint64_t OFF_DEBUG_BUS_MUX          = 0x210u;
    static constexpr uint64_t OFF_WB_PC_CORE0            = 0x300u;
    static constexpr uint64_t OFF_WB_PC_CORE1            = 0x340u;
    static constexpr uint64_t OFF_WB_PC_CORE2            = 0x380u;
    static constexpr uint64_t OFF_WB_PC_CORE3            = 0x3C0u;
    static constexpr uint64_t OFF_SMC_ATTRIBUTES         = 0x1000u;
    static constexpr uint64_t OFF_MUTEX                  = 0x1040u;
    static constexpr uint64_t OFF_SEMA                   = 0x1060u;
    static constexpr uint64_t OFF_DUMMY_ROM_0            = 0x1180u;
    static constexpr uint64_t OFF_DUMMY_ROM_1            = 0x1188u;
    static constexpr uint64_t OFF_DUMMY_ROM_2            = 0x1190u;
    static constexpr uint64_t OFF_DUMMY_ROM_3            = 0x1198u;
    static constexpr uint64_t OFF_DUMMY_ROM_NULL         = 0x11A0u;

    static constexpr unsigned NUM_CORES       = 4;
    static constexpr unsigned WB_PC_PER_CORE  = 8;
    static constexpr unsigned NUM_MUTEX       = 4;
    static constexpr unsigned NUM_SEMA        = 4;
    static constexpr unsigned DUMMY_ROM_NULLS = 4;

    uint64_t base_addr       = DEFAULT_BASE_ADDR;
    double   access_delay_ns = 2.0;
};

// ---------------------------------------------------------------------------
// cpu_ctrl — SC_MODULE
// ---------------------------------------------------------------------------

class cpu_ctrl : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<cpu_ctrl> reg_socket{"reg_socket"};

    SC_HAS_PROCESS(cpu_ctrl);

    explicit cpu_ctrl(sc_core::sc_module_name name,
                      cpu_ctrl_cfg              cfg = {});

    // Side-effect-free peek (used by transport_dbg).
    uint64_t dbg_reg(uint64_t byte_off) const;

    // HW-modeled backdoors for sw=r / hw=w registers.
    void set_smc_attributes(uint64_t v);
    void set_test_ctrl(uint32_t v);
    void set_wb_pc(unsigned core, unsigned slot, uint64_t pc);

    uint32_t scratch(uint64_t idx) const;
    void     set_scratch(uint64_t idx, uint32_t v);

    void dump_state(std::ostream& os) const;

    double access_delay_ns() const { return access_delay_ns_p_.get_value(); }

private:
    cci::cci_param<uint64_t> base_addr_p_;
    cci::cci_param<double>   access_delay_ns_p_;

    cpu_ctrl_cfg cfg_;

    std::array<uint64_t, cpu_ctrl_cfg::NUM_CORES> reset_vector_{};
    uint64_t reset_ctrl_{};
    uint64_t core_reset_pulse_count_{};
    uint64_t clock_gate_control_{};
    uint64_t global_base_{};
    uint64_t local_base_{};
    uint64_t region_size_{};
    uint64_t reference_counter_{};
    uint64_t wdt_timeout_{};
    uint64_t wdt_timeout_reset_{};
    // SCRATCH[16]: plain RW32 mailbox words (regmodel-backed -- see
    // .cursor/rules/register-access-helpers.mdc). Masks are set once in the
    // constructor; reset_regs() only resets the stored value.
    std::array<regmodel::Register32, CPU_CTRL_SCRATCH_COUNT> scratch_{};
    uint64_t test_ctrl_{};
    uint64_t debug_ctrl_{};
    uint64_t debug_bus_mux_{};
    // WB_PC: SW read-only (write_mask=0); HW writes go through set_wb_pc()'s
    // set_raw() backdoor.
    std::array<std::array<regmodel::Register64, cpu_ctrl_cfg::WB_PC_PER_CORE>,
               cpu_ctrl_cfg::NUM_CORES>
        wb_pc_{};
    uint64_t smc_attributes_{};
    // MUTEX: 1-bit availability flag (RW32, read_mask/write_mask=0x1). SW
    // writes always force it back to available via an on_write callback; the
    // atomic test-and-set on SW read is a bus-level side effect handled
    // directly in b_transport() via raw()/set_raw() (distinct from the
    // side-effect-free dbg_reg()/transport_dbg() peek, which uses read()).
    std::array<regmodel::Register32, cpu_ctrl_cfg::NUM_MUTEX> mutex_{};
    std::array<uint16_t, cpu_ctrl_cfg::NUM_SEMA>  sema_{};
    // DUMMY_ROM_*: plain RW64, no side effects.
    regmodel::Register64 dummy_rom_0_{};
    regmodel::Register64 dummy_rom_1_{};
    regmodel::Register64 dummy_rom_2_{};
    regmodel::Register64 dummy_rom_3_{};
    std::array<regmodel::Register64, cpu_ctrl_cfg::DUMMY_ROM_NULLS> dummy_rom_null_{};

    void reset_regs();

    uint64_t normalize_addr(uint64_t addr) const;

    bool reg_read(uint64_t off, unsigned len, uint8_t* buf) const;
    bool reg_write(uint64_t off, unsigned len, const uint8_t* buf);

    uint64_t read_qword(uint64_t off) const;
    bool     write_qword(uint64_t off, uint64_t val, unsigned byte_off,
                         unsigned len);

    void b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& gp);
};

}  // namespace smc

#endif  // SMC_CPU_CTRL_H_
