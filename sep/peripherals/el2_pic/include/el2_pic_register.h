// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file el2_pic_register.h
 * @brief Register type definitions for the VeeR EL2 PIC peripheral.
 *
 * Mirrors the bitfield layout described in
 *   riscv-vp-plusplus/sw/tt-oca-hw-main/meta/registers/rdl/el2_pic.rdl
 *
 * The six register types match the RDL declarations:
 *   - MPICCFG_type       (priord)
 *   - MEIPL_type         (intpriority[3:0])
 *   - MEIP_type          (intpend[31:0])
 *   - MEIE_type          (inten[0])
 *   - MEIGWCTRL_type     (polarity[0], irq_type[1])
 *   - MEIGWCLR_type      (clear[0], write-only)
 */
#pragma once
#include <iostream>
#include <systemc.h>
#include "csml_register.h"

namespace el2_pic {

/// PIC Configuration Register (mpiccfg) — selects priority-order convention.
template <unsigned int N>
class MPICCFG_type : public csml_reg<N> {
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    MPICCFG_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : csml_reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000001, 0x0),
          priord(reg_name + ".priord", *this, 0, 1),
          reserved0(reg_name + ".reserved0", *this, 1, 31)
    {}
    using csml_reg<N>::operator=;
    csml_bitfield<N> priord;       ///< 0 = standard (15=highest), 1 = reverse (0=highest)
    csml_bitfield<N> reserved0;
};

/// External Interrupt Priority Level Register (meipl[S]).
template <unsigned int N>
class MEIPL_type : public csml_reg<N> {
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    MEIPL_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : csml_reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x0000000F, 0x0),
          intpriority(reg_name + ".intpriority", *this, 0, 4),
          reserved0(reg_name + ".reserved0", *this, 4, 28)
    {}
    using csml_reg<N>::operator=;
    csml_bitfield<N> intpriority;  ///< 4-bit priority (0=disabled, 1..15)
    csml_bitfield<N> reserved0;
};

/// External Interrupt Pending Register (meip[0..1]) — RO to SW, HW-updated.
template <unsigned int N>
class MEIP_type : public csml_reg<N> {
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    MEIP_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : csml_reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000000, 0x0),
          intpend(reg_name + ".intpend", *this, 0, 32)
    {}
    using csml_reg<N>::operator=;
    csml_bitfield<N> intpend;      ///< bit Y = source X*32+Y pending
};

/// External Interrupt Enable Register (meie[S]).
template <unsigned int N>
class MEIE_type : public csml_reg<N> {
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    MEIE_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : csml_reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000001, 0x0),
          inten(reg_name + ".inten", *this, 0, 1),
          reserved0(reg_name + ".reserved0", *this, 1, 31)
    {}
    using csml_reg<N>::operator=;
    csml_bitfield<N> inten;        ///< 1 = source enabled
    csml_bitfield<N> reserved0;
};

/// External Interrupt Gateway Configuration Register (meigwctrl[S]).
template <unsigned int N>
class MEIGWCTRL_type : public csml_reg<N> {
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    MEIGWCTRL_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : csml_reg<N>(reg_name, memory, offset, 0xFFFFFFFF, 0x00000003, 0x0),
          polarity(reg_name + ".polarity", *this, 0, 1),
          irq_type(reg_name + ".irq_type", *this, 1, 1),
          reserved0(reg_name + ".reserved0", *this, 2, 30)
    {}
    using csml_reg<N>::operator=;
    csml_bitfield<N> polarity;     ///< 0 = active-high, 1 = active-low
    csml_bitfield<N> irq_type;     ///< 0 = level, 1 = edge
    csml_bitfield<N> reserved0;
};

/// External Interrupt Gateway Clear Register (meigwclr[S]) — W-only.
template <unsigned int N>
class MEIGWCLR_type : public csml_reg<N> {
public:
    using typename csml_reg<N>::memory_type;
    typedef typename csml_word<N>::wordtype DT;
    MEIGWCLR_type(std::string reg_name, memory_type &memory, unsigned int offset)
        : csml_reg<N>(reg_name, memory, offset, 0x00000000, 0x00000001, 0x0),
          clear(reg_name + ".clear", *this, 0, 1),
          reserved0(reg_name + ".reserved0", *this, 1, 31)
    {}
    using csml_reg<N>::operator=;
    csml_bitfield<N> clear;        ///< Any write clears the gateway's latched pending bit
    csml_bitfield<N> reserved0;
};

} // namespace el2_pic
