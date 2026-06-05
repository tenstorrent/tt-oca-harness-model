#pragma once
#include "key_manager_register.h"
#include <string.h>

/**
 * @brief Base class for the Key Manager TT TLM model.
 *
 * Owns two csml_memory instances and two TLM target sockets — one per
 * hardware slave port:
 *   mailbox_socket : Mailbox SEP-side registers
 *   kpvlp_socket   : KPVLP key provisioning registers
 *
 * Address layout within each socket (base addresses deferred to VP integration):
 *   mailbox_socket:
 *     0x0000 – 0x001B  (7 registers × 4 bytes)
 *   kpvlp_socket:
 *     0x0000 – 0x07FC  key data  (32 slots × 16 words × 4 bytes)
 *     0x0800 – 0x087C  ctrl regs (32 slots × 4 bytes)
 *     0x0880           status
 */
class key_manager_base : public sc_module
{
public:
    typedef typename csml_reg<32>::DT DT;

    key_manager_base(sc_module_name name)
      : sc_module(name),
        mb_memory   (std::string(name) + ".MB_Memory",    MB_MEM_SIZE    / sizeof(unsigned int)),
        kpvlp_memory(std::string(name) + ".KPVLP_Memory", KPVLP_MEM_SIZE / sizeof(unsigned int)),
        // ---- Mailbox registers ----
        MB_WDATA    (std::string(name) + ".MB_WDATA",    mb_memory, keymgr_tt::MB_WDATA_OFFSET   / sizeof(unsigned int)),
        MB_WSEP     (std::string(name) + ".MB_WSEP",     mb_memory, keymgr_tt::MB_WSEP_OFFSET    / sizeof(unsigned int)),
        MB_RDATA    (std::string(name) + ".MB_RDATA",    mb_memory, keymgr_tt::MB_RDATA_OFFSET   / sizeof(unsigned int)),
        MB_STATUS   (std::string(name) + ".MB_STATUS",   mb_memory, keymgr_tt::MB_STATUS_OFFSET  / sizeof(unsigned int)),
        MB_IRQS     (std::string(name) + ".MB_IRQS",     mb_memory, keymgr_tt::MB_IRQS_OFFSET    / sizeof(unsigned int)),
        MB_IRQEN    (std::string(name) + ".MB_IRQEN",    mb_memory, keymgr_tt::MB_IRQEN_OFFSET   / sizeof(unsigned int)),
        MB_CTRL     (std::string(name) + ".MB_CTRL",     mb_memory, keymgr_tt::MB_CTRL_OFFSET    / sizeof(unsigned int)),
        // ---- KPVLP registers ----
        KPVLP_KEY   (std::string(name) + ".KPVLP_KEY",    kpvlp_memory, KPVLP_KEY_BASE    / sizeof(unsigned int), 1),
        KPVLP_CTRL  (std::string(name) + ".KPVLP_CTRL",   kpvlp_memory, KPVLP_CTRL_BASE   / sizeof(unsigned int), 1),
        KPVLP_STATUS(std::string(name) + ".KPVLP_STATUS", kpvlp_memory, KPVLP_STATUS_BASE / sizeof(unsigned int))
    {
        mb_memory.bind_to_socket(mailbox_socket);
        kpvlp_memory.bind_to_socket(kpvlp_socket);
    }

    // -----------------------------------------------------------------------
    // TLM interface — two sockets, one per hardware slave port
    // -----------------------------------------------------------------------
    csml_memory<32> mb_memory;
    csml_memory<32> kpvlp_memory;
    tlm_utils::simple_target_socket<csml_memory<32>, 32> mailbox_socket;
    tlm_utils::simple_target_socket<csml_memory<32>, 32> kpvlp_socket;

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

    // -----------------------------------------------------------------------
    // KPVLP registers
    //   KPVLP_KEY  [512] : 32 slots × 16 words, WO key data
    //   KPVLP_CTRL [ 32] : per-slot policy (EXTEND, DEST_VALID, LAST_DWORD)
    //   KPVLP_STATUS      : RO unlock_sep bitmask for all 32 slots
    // -----------------------------------------------------------------------
    csml_reg_vector<keymgr_tt::KPVLP_KEY_type   <32>, 512> KPVLP_KEY;
    csml_reg_vector<keymgr_tt::KPVLP_CTRL_type  <32>,  32> KPVLP_CTRL;
    keymgr_tt::KPVLP_STATUS_type<32>                        KPVLP_STATUS;

    void reset_all_registers();

protected:
    // KPVLP byte-address base offsets within kpvlp_memory
    // (match key_manager_register.h KPVLP_*_OFFSET definitions exactly)
    static constexpr unsigned int KPVLP_KEY_BASE    = 0x000;
    static constexpr unsigned int KPVLP_CTRL_BASE   = 0x800;
    static constexpr unsigned int KPVLP_STATUS_BASE = 0x880;

    // Memory sizes in bytes
    static constexpr unsigned int MB_MEM_SIZE    = 0x020;   // 7 regs × 4B, padded to 32B
    static constexpr unsigned int KPVLP_MEM_SIZE = 0x884;   // KEY(0x800)+CTRL(0x80)+STATUS(0x4)
};
