#pragma once
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include "keymgr_tt_base.h"  // for KPVLP_KEY_BASE etc.

/**
 * @brief Base test class — TLM initiator socket + register offset/mask/reset enums.
 *
 * Offsets marked PLACEHOLDER must be updated once customer confirms the
 * register layout.
 */
class keymgr_tt_basetest : public sc_module
{
public:
    // Mailbox registers → binds to DUT mailbox_socket
    tlm_utils::simple_initiator_socket<keymgr_tt_basetest, 32> initiator_socket;
    // KPVLP registers → binds to DUT kpvlp_socket
    tlm_utils::simple_initiator_socket<keymgr_tt_basetest, 32> kpvlp_initiator_socket;

    // -------------------------------------------------------------------
    // Register byte offsets  (confirmed from km_mailbox_sep_regs.h and km_kpv_kpvlp_regs.h)
    // -------------------------------------------------------------------
    enum Register_Offset
    {
        // Mailbox  (test-side offsets; sent directly to mailbox_socket)
        MB_WDATA_OFFSET    = 0x00,
        MB_WSEP_OFFSET     = 0x04,
        MB_RDATA_OFFSET    = 0x08,
        MB_STATUS_OFFSET   = 0x0C,
        MB_IRQS_OFFSET     = 0x10,
        MB_IRQEN_OFFSET    = 0x14,
        MB_CTRL_OFFSET     = 0x18,

        // KPVLP  (test-side namespace — 0x1000 base is stripped before dispatch to kpvlp_socket)
        KPVLP_KEY_BASE     = 0x1000,
        KPVLP_CTRL_BASE    = 0x1800,
        KPVLP_STATUS_OFFSET= 0x1880
    };

    /// Byte offset of KPVLP key word: slot [0..31], word [0..15]
    static constexpr unsigned int kpvlp_key_offset(unsigned int slot, unsigned int word)
    {
        return KPVLP_KEY_BASE + slot * (16 * 4) + word * 4;
    }

    /// Byte offset of KPVLP ctrl register: slot [0..31]
    static constexpr unsigned int kpvlp_ctrl_offset(unsigned int slot)
    {
        return KPVLP_CTRL_BASE + slot * 4;
    }

    // -------------------------------------------------------------------
    // Read masks (bits readable by SEP)
    // -------------------------------------------------------------------
    enum Register_Read_Mask
    {
        MB_WDATA_READ    = 0x00000000,   // WO
        MB_WSEP_READ     = 0x00000001,   // RW bit[0] only
        MB_RDATA_READ    = 0xFFFFFFFF,   // RO
        MB_STATUS_READ   = 0x03FFFFFF,   // RO bits[25:0]
        MB_IRQS_READ     = 0x0000001F,   // bits[1:0] RO (level), bits[4:2] W1C
        MB_IRQEN_READ    = 0x0000001F,   // RW  5 sources
        MB_CTRL_READ     = 0x00000007,   // RW  3 bits
        KPVLP_KEY_READ   = 0x00000000,   // WO
        KPVLP_CTRL_READ  = 0x00000000,   // WO
        KPVLP_STATUS_READ= 0xFFFFFFFF    // RO
    };

    // -------------------------------------------------------------------
    // Write masks (bits writable by SEP)
    // -------------------------------------------------------------------
    enum Register_Write_Mask
    {
        MB_WDATA_WRITE    = 0xFFFFFFFF,
        MB_WSEP_WRITE     = 0x00000001,
        MB_RDATA_WRITE    = 0x00000000,  // RO
        MB_STATUS_WRITE   = 0x00F00000,  // bits[23:20] W1C (overflow/underflow flags)
        MB_IRQS_WRITE     = 0x0000001C,  // bits[4:2] W1C; bits[1:0] are level-sensitive RO
        MB_IRQEN_WRITE    = 0x0000001F,  // RW  5 sources
        MB_CTRL_WRITE     = 0x00000007,  // RW  3 bits
        KPVLP_KEY_WRITE   = 0xFFFFFFFF,
        KPVLP_CTRL_WRITE  = 0x001FFE70,  // EXTEND[6:4] | DEST_VALID[16:9] | LAST_DWORD[20:17]
        KPVLP_STATUS_WRITE= 0x00000000   // RO
    };

    // -------------------------------------------------------------------
    // Reset values  (confirmed from km_mailbox_sep_regs.h)
    // -------------------------------------------------------------------
    enum Register_Reset_Val
    {
        MB_WDATA_RESET    = 0x00000000,
        MB_WSEP_RESET     = 0x00000000,
        MB_RDATA_RESET    = 0x00000000,
        MB_STATUS_RESET   = 0x00000005,  // inbound_empty[0]=1, outbound_empty[2]=1
        MB_IRQS_RESET     = 0x00000002,  // bit[1]=INBOUND_WRITE_SPACE_AVAIL set after flush
        MB_IRQEN_RESET    = 0x00000000,
        MB_CTRL_RESET     = 0x00000000,
        KPVLP_KEY_RESET   = 0x00000000,
        KPVLP_CTRL_RESET  = 0x00000000,
        KPVLP_STATUS_RESET= 0x00000000
    };

    keymgr_tt_basetest(sc_module_name name)
      : sc_module(name),
        initiator_socket("initiator_socket"),
        kpvlp_initiator_socket("kpvlp_initiator_socket")
    {}
};
