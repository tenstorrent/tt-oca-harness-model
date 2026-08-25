// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <string>

// Register byte offsets — computed from el2_pic_base.h constants:
//   OFFS_*_BASE / 4 gives the csml word offset; byte addr = word_offset * 4.
//
//   MEIPL[s]       @ byte  s*4              (base 0x0000)
//   MEIP[w]        @ byte  0x1000 + w*4
//   MEIE[s]        @ byte  0x2000 + s*4
//   MPICCFG        @ byte  0x3000
//   MEIGWCTRL[s]   @ byte  0x4000 + s*4
//   MEIGWCLR[s]    @ byte  0x5000 + s*4

class el2_pic_basetest : public sc_module {
public:
    tlm_utils::simple_initiator_socket<el2_pic_basetest, 32> initiator_socket;

    // Byte-address helpers
    static constexpr unsigned meipl_offset(unsigned s)     { return s * 4; }
    static constexpr unsigned meip_offset(unsigned w)      { return 0x1000 + w * 4; }
    static constexpr unsigned meie_offset(unsigned s)      { return 0x2000 + s * 4; }
    static constexpr unsigned mpiccfg_offset()             { return 0x3000; }
    static constexpr unsigned meigwctrl_offset(unsigned s) { return 0x4000 + s * 4; }
    static constexpr unsigned meigwclr_offset(unsigned s)  { return 0x5000 + s * 4; }

    // Write / read masks per register type
    static constexpr uint32_t MPICCFG_WRITE_MASK   = 0x00000001u;
    static constexpr uint32_t MPICCFG_READ_MASK    = 0x00000001u;
    static constexpr uint32_t MEIPL_WRITE_MASK     = 0x0000000Fu;
    static constexpr uint32_t MEIPL_READ_MASK      = 0x0000000Fu;
    static constexpr uint32_t MEIP_WRITE_MASK      = 0x00000000u;  // RO
    static constexpr uint32_t MEIP_READ_MASK       = 0xFFFFFFFFu;
    static constexpr uint32_t MEIE_WRITE_MASK      = 0x00000001u;
    static constexpr uint32_t MEIE_READ_MASK       = 0x00000001u;
    static constexpr uint32_t MEIGWCTRL_WRITE_MASK = 0x00000003u;
    static constexpr uint32_t MEIGWCTRL_READ_MASK  = 0x00000003u;
    static constexpr uint32_t MEIGWCLR_WRITE_MASK  = 0x00000001u;
    static constexpr uint32_t MEIGWCLR_READ_MASK   = 0x00000000u;  // W-only

    struct RegisterProperty {
        unsigned  byte_offset;
        uint32_t  read_mask;
        uint32_t  write_mask;
        uint32_t  reset_val;
        std::string name;
    };

    explicit el2_pic_basetest(sc_module_name name) : sc_module(name) {}
};
