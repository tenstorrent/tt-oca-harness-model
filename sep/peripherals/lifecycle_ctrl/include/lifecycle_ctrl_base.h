// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include "lifecycle_ctrl_register.h"
#include <string.h>

class lifecycle_ctrl_base : public sc_module
{
  public:
    typedef typename csml_reg<32>::DT DT;
    lifecycle_ctrl_base(sc_module_name name, unsigned int memory_size)
        : sc_module(name),
          memory  (std::string(name) + ".Memory",      memory_size/sizeof(unsigned int)),
          FEAT_CTRL_LO(std::string(name) + ".FEAT_CTRL_LO", memory, 0x0000/sizeof(unsigned int)),
          FEAT_CTRL_HI(std::string(name) + ".FEAT_CTRL_HI", memory, 0x0004/sizeof(unsigned int)),
          DEMOTE_1    (std::string(name) + ".DEMOTE_1",     memory, 0x0008/sizeof(unsigned int)),
          DEMOTE_1_HI (std::string(name) + ".DEMOTE_1_HI",  memory, 0x000C/sizeof(unsigned int)),
          DEMOTE_2    (std::string(name) + ".DEMOTE_2",     memory, 0x0010/sizeof(unsigned int)),
          DEMOTE_2_HI (std::string(name) + ".DEMOTE_2_HI",  memory, 0x0014/sizeof(unsigned int))
    {
        memory.bind_to_socket(target_socket);
    }

    csml_memory<32> memory;
    tlm_utils::simple_target_socket<csml_memory<32>, 32> target_socket;

    lc_ctrl::FEAT_CTRL_LO_type<32> FEAT_CTRL_LO;   // @ 0x0000  RO
    lc_ctrl::FEAT_CTRL_HI_type<32> FEAT_CTRL_HI;   // @ 0x0004  RO
    lc_ctrl::DEMOTE_type<32>       DEMOTE_1;        // @ 0x0008  W1S (BL1 / domain-1)
    lc_ctrl::DEMOTE_HI_type<32>    DEMOTE_1_HI;     // @ 0x000C  rsvd[63:32], W1S
    lc_ctrl::DEMOTE_type<32>       DEMOTE_2;        // @ 0x0010  W1S (BL2 / domain-2)
    lc_ctrl::DEMOTE_HI_type<32>    DEMOTE_2_HI;     // @ 0x0014  rsvd[63:32], W1S

    void reset_all_registers();
};
