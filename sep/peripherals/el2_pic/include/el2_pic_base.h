/**
 * @file el2_pic_base.h
 * @brief Register infrastructure base class for the VeeR EL2 PIC model.
 *
 * Provides:
 *   - The CSML memory block backing the 0xC008_0000..0xC008_53FF MMIO range
 *   - Storage for mpiccfg, meipl[0..255], meip[0..7], meie[0..255],
 *     meigwctrl[0..255], meigwclr[0..255]  (index 0 is reserved per spec)
 *   - target_socket: bound directly to VeeRISSTlm's dedicated pic_isock_
 *     (NOT on the system bus — PIC is internal to the VeeR EL2 core)
 *   - reset_all_registers()
 *
 * Behavioural logic (gateway sensitivity, arbitration, hart hand-off)
 * lives in the derived el2_pic class.
 */
#pragma once
#include "el2_pic_register.h"
#include <string.h>

namespace el2_pic {

/// Total number of interrupt slots, matching this SEP VeeR build's
/// `RV_PIC_TOTAL_INT_PLUS1 = 256`
/// (`vendor/chipsalliance/Cores-VeeR-EL2/overlay/snapshots/sep/common_defines.vh:185`).
/// Source 0 is reserved per VeeR EL2 spec, so valid IDs are 1..255 —
/// `RV_PIC_TOTAL_INT = 255`. Note this is the build parameter, not the
/// `PIC_TOTAL_INT = 32` *default* carried by `meta/registers/rdl/el2_pic.rdl`;
/// el2_pic_ctrl.sv sizes every array off PIC_TOTAL_INT_PLUS1.
static constexpr unsigned NUM_INTERRUPTS = 256;

/// Number of 32-bit meip pending words, one bit per source. Matches
/// `RV_PIC_INT_WORDS = 8` (common_defines.vh:159) and el2_pic_ctrl.sv's
/// INTPEND_SIZE/32 for PIC_TOTAL_INT_PLUS1 = 256. The RDL hardcodes `meip[2]`,
/// which only covers 64 sources; the RTL's own address comment (X = 0 ..
/// (PIC_TOTAL_INT+31)/32-1) agrees with 8 here, and el2_pic_ctrl.sv:491 decodes
/// the word index from address bits [5:2], leaving room for up to 16.
static constexpr unsigned NUM_PEND_WORDS = NUM_INTERRUPTS / 32;

/// Backing memory size in bytes. The PIC's MMIO surface ends at
/// meigwclr[255] = 0x53FC..0x53FF, so 0x6000 gives comfortable padding.
static constexpr unsigned MEM_SIZE_BYTES = 0x6000;

// Register-array byte offsets (per RDL).
static constexpr unsigned OFFS_MEIPL_BASE       = 0x0000;  // meipl[0]    (reserved)
static constexpr unsigned OFFS_MEIP_BASE        = 0x1000;  // meip[0]
static constexpr unsigned OFFS_MEIE_BASE        = 0x2000;  // meie[0]     (reserved)
static constexpr unsigned OFFS_MPICCFG          = 0x3000;
static constexpr unsigned OFFS_MEIGWCTRL_BASE   = 0x4000;  // meigwctrl[0] (reserved)
static constexpr unsigned OFFS_MEIGWCLR_BASE    = 0x5000;  // meigwclr[0]  (reserved)

class el2_pic_base : public sc_module {
public:
    typedef typename csml_reg<32>::DT DT;

    el2_pic_base(sc_module_name name)
        : sc_module(name),
          memory(std::string(name) + ".Memory", MEM_SIZE_BYTES / sizeof(unsigned int)),
          MPICCFG(std::string(name) + ".MPICCFG", memory, OFFS_MPICCFG / sizeof(unsigned int)),
          MEIPL(std::string(name) + ".MEIPL", memory,
                OFFS_MEIPL_BASE / sizeof(unsigned int), 1),
          MEIP(std::string(name) + ".MEIP", memory,
               OFFS_MEIP_BASE / sizeof(unsigned int), 1),
          MEIE(std::string(name) + ".MEIE", memory,
               OFFS_MEIE_BASE / sizeof(unsigned int), 1),
          MEIGWCTRL(std::string(name) + ".MEIGWCTRL", memory,
                    OFFS_MEIGWCTRL_BASE / sizeof(unsigned int), 1),
          MEIGWCLR(std::string(name) + ".MEIGWCLR", memory,
                   OFFS_MEIGWCLR_BASE / sizeof(unsigned int), 1)
    {
        memory.bind_to_socket(target_socket);
    }

    csml_memory<32> memory;
    tlm_utils::simple_target_socket<csml_memory<32>, 32> target_socket;

    MPICCFG_type<32> MPICCFG;

    /// meipl[0] is reserved; meipl[1..255] hold the per-source priority.
    csml_reg_vector<MEIPL_type<32>, NUM_INTERRUPTS> MEIPL;

    /// Pending bitmaps; bit Y of meip[X] is source X*32+Y.
    csml_reg_vector<MEIP_type<32>, NUM_PEND_WORDS> MEIP;

    /// meie[0] reserved; meie[1..255] hold the per-source enable.
    csml_reg_vector<MEIE_type<32>, NUM_INTERRUPTS> MEIE;

    /// meigwctrl[0] reserved; meigwctrl[1..255] hold gateway type/polarity.
    csml_reg_vector<MEIGWCTRL_type<32>, NUM_INTERRUPTS> MEIGWCTRL;

    /// meigwclr[0] reserved; writes to meigwclr[1..255] clear edge pending state.
    csml_reg_vector<MEIGWCLR_type<32>, NUM_INTERRUPTS> MEIGWCLR;

    /// Reset all storage to power-on defaults (zeros).
    void reset_all_registers();
};

} // namespace el2_pic
