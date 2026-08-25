// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
#pragma once
#include "key_manager_register.h"
#include <string.h>

/**
 * @brief Base class for the Key Manager TT TLM model.
 *
 * Owns one csml_memory instance and one TLM target socket, matching the single
 * KM slave port the SEP host can reach:
 *   mailbox_socket : Mailbox SEP-side registers
 *
 * Address layout within the socket (base address deferred to VP integration):
 *   mailbox_socket:
 *     0x0000 – 0x001B  (7 registers × 4 bytes)
 */
class key_manager_base : public sc_module
{
public:
    typedef typename csml_reg<32>::DT DT;

    key_manager_base(sc_module_name name)
      : sc_module(name),
        mb_memory   (std::string(name) + ".MB_Memory",    MB_MEM_SIZE    / sizeof(unsigned int)),
        // ---- Mailbox registers ----
        MB_WDATA    (std::string(name) + ".MB_WDATA",    mb_memory, keymgr_tt::MB_WDATA_OFFSET   / sizeof(unsigned int)),
        MB_WSEP     (std::string(name) + ".MB_WSEP",     mb_memory, keymgr_tt::MB_WSEP_OFFSET    / sizeof(unsigned int)),
        MB_RDATA    (std::string(name) + ".MB_RDATA",    mb_memory, keymgr_tt::MB_RDATA_OFFSET   / sizeof(unsigned int)),
        MB_STATUS   (std::string(name) + ".MB_STATUS",   mb_memory, keymgr_tt::MB_STATUS_OFFSET  / sizeof(unsigned int)),
        MB_IRQS     (std::string(name) + ".MB_IRQS",     mb_memory, keymgr_tt::MB_IRQS_OFFSET    / sizeof(unsigned int)),
        MB_IRQEN    (std::string(name) + ".MB_IRQEN",    mb_memory, keymgr_tt::MB_IRQEN_OFFSET   / sizeof(unsigned int)),
        MB_CTRL     (std::string(name) + ".MB_CTRL",     mb_memory, keymgr_tt::MB_CTRL_OFFSET    / sizeof(unsigned int))
    {
        mb_memory.bind_to_socket(mailbox_socket);
    }

    // -----------------------------------------------------------------------
    // TLM interface — one socket for the single SEP-visible slave port
    // -----------------------------------------------------------------------
    csml_memory<32> mb_memory;
    tlm_utils::simple_target_socket<csml_memory<32>, 32> mailbox_socket;

    // -----------------------------------------------------------------------
    // Mailbox SEP-side registers
    // -----------------------------------------------------------------------
    keymgr_tt::MB_WDATA_type <32> MB_WDATA;   ///< WO  SEP → inbound queue push
    keymgr_tt::MB_WSEP_type  <32> MB_WSEP;    ///< RW  Write 1 to tag next WDATA as separator
    keymgr_tt::MB_RDATA_type <32> MB_RDATA;   ///< RO  SEP ← outbound queue pop
    keymgr_tt::MB_STATUS_type<32> MB_STATUS;  ///< RO  FIFO fill state and flags
    keymgr_tt::MB_IRQS_type  <32> MB_IRQS;    ///< W1C IRQ status
    keymgr_tt::MB_IRQEN_type <32> MB_IRQEN;   ///< RW  IRQ enable
    keymgr_tt::MB_CTRL_type  <32> MB_CTRL;    ///< RW  Overflow/underflow response and flush

    void reset_all_registers();

protected:
    // Memory size in bytes
    static constexpr unsigned int MB_MEM_SIZE = 0x020;   // 7 regs × 4B, padded to 32B
};
