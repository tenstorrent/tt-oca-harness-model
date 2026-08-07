#pragma once

#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <systemc>
#include <cassert>
#include <cstring>

// 64-to-32 bit bus adapter for DMA sys_initiator_socket (64-bit) → SimpleBus (32-bit).
class dma_sys_bus_adapter : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<dma_sys_bus_adapter, 64>    tgt;
    tlm_utils::simple_initiator_socket<dma_sys_bus_adapter, 32> ini;

    SC_HAS_PROCESS(dma_sys_bus_adapter);
    dma_sys_bus_adapter(sc_core::sc_module_name n)
        : sc_module(n), tgt("tgt"), ini("ini") {
        tgt.register_b_transport(this, &dma_sys_bus_adapter::b_transport);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
        ini->b_transport(trans, delay);
    }
};


/**
 * @brief 32-to-64 bit bus adapter for the OT Mailbox IP (Port 0, SEP side).
 *
 * The mailbox registers are 64-bit wide (regwidth=64, accesswidth=64 per RDL)
 * but the SEP RISC-V core uses a 32-bit data bus. This adapter sits between
 * the SEP SimpleBus and mailbox Port 0, transparently handling split 32-bit
 * accesses so software only needs to do normal 32-bit loads/stores.
 *
 * Split access rules
 * ------------------
 * WRITE_DATA (base+0x00 / base+0x04) — FIFO push must be atomic:
 *   32-bit write to base+0x00  → latch low word, do NOT push FIFO yet
 *   32-bit write to base+0x04  → combine with latch → single 64-bit push
 *
 * READ_DATA (base+0x08 / base+0x0C) — FIFO pop must be atomic:
 *   32-bit read  from base+0x08 → pop 64-bit from FIFO, return low word, latch high
 *   32-bit read  from base+0x0C → return latched high word (no second pop)
 *
 * All other registers have functional bits only in [31:0], so a 32-bit access
 * to the low word is zero-extended to 64-bit and forwarded immediately.
 */
struct MailboxBridge : public sc_core::sc_module {

    tlm_utils::simple_target_socket<MailboxBridge>    tsock;
    tlm_utils::simple_initiator_socket<MailboxBridge> isock;

    static constexpr uint64_t WRITE_DATA_OFFSET = 0x00;
    static constexpr uint64_t READ_DATA_OFFSET  = 0x08;

    SC_HAS_PROCESS(MailboxBridge);

    MailboxBridge(sc_core::sc_module_name n)
        : sc_module(n), tsock("tsock"), isock("isock")
        , m_write_latch(0), m_write_latch_valid(false)
        , m_read_latch(0),  m_read_latch_valid(false)
    {
        tsock.register_b_transport(this, &MailboxBridge::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload& txn, sc_core::sc_time& delay)
    {
        const uint64_t addr  = txn.get_address();
        const unsigned len   = txn.get_data_length();
        uint8_t*       dptr  = txn.get_data_ptr();

        if (len == 8) {
            isock->b_transport(txn, delay);
            return;
        }

        assert(len == 4 && "MailboxBridge: only 4-byte or 8-byte accesses supported");

        const uint64_t offset   = addr;
        const uint64_t reg_base = addr & ~static_cast<uint64_t>(0x7);
        const bool     is_high  = (offset & 0x4) != 0;

        if (txn.is_write()) {
            uint32_t word = 0;
            std::memcpy(&word, dptr, 4);

            if ((offset & ~static_cast<uint64_t>(0x7)) == WRITE_DATA_OFFSET) {
                if (!is_high) {
                    m_write_latch       = word;
                    m_write_latch_valid = true;
                    txn.set_response_status(tlm::TLM_OK_RESPONSE);
                } else {
                    const uint64_t combined = (static_cast<uint64_t>(word) << 32)
                                            |  static_cast<uint64_t>(m_write_latch);
                    m_write_latch_valid = false;
                    forward_write64(reg_base, combined, txn, delay);
                }
            } else {
                if (is_high) {
                    txn.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
                    return;
                }
                forward_write64(reg_base, static_cast<uint64_t>(word), txn, delay);
            }

        } else {
            if ((offset & ~static_cast<uint64_t>(0x7)) == READ_DATA_OFFSET) {
                if (!is_high) {
                    uint64_t val64 = 0;
                    forward_read64(reg_base, val64, txn, delay);
                    const uint32_t lo = static_cast<uint32_t>(val64 & 0xFFFFFFFFULL);
                    std::memcpy(dptr, &lo, 4);
                    m_read_latch       = static_cast<uint32_t>(val64 >> 32);
                    m_read_latch_valid = true;
                    txn.set_response_status(tlm::TLM_OK_RESPONSE);
                } else {
                    if (m_read_latch_valid) {
                        std::memcpy(dptr, &m_read_latch, 4);
                        m_read_latch_valid = false;
                        txn.set_response_status(tlm::TLM_OK_RESPONSE);
                    } else {
                        uint32_t zeroes = 0;
                        std::memcpy(dptr, &zeroes, 4);
                        txn.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
                    }
                }
            } else {
                if (is_high) {
                    uint32_t zeroes = 0;
                    std::memcpy(dptr, &zeroes, 4);
                    txn.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
                    return;
                }
                uint64_t val64 = 0;
                forward_read64(reg_base, val64, txn, delay);
                const uint32_t lo = static_cast<uint32_t>(val64 & 0xFFFFFFFFULL);
                std::memcpy(dptr, &lo, 4);
            }
        }
    }

private:
    uint32_t m_write_latch;
    bool     m_write_latch_valid;
    uint32_t m_read_latch;
    bool     m_read_latch_valid;

    void forward_write64(uint64_t addr, uint64_t value,
                         tlm::tlm_generic_payload& orig, sc_core::sc_time& delay)
    {
        uint8_t buf[8];
        std::memcpy(buf, &value, 8);
        tlm::tlm_generic_payload fwd;
        fwd.set_command(tlm::TLM_WRITE_COMMAND);
        fwd.set_address(addr);
        fwd.set_data_ptr(buf);
        fwd.set_data_length(8);
        fwd.set_streaming_width(8);
        fwd.set_byte_enable_ptr(nullptr);
        fwd.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        isock->b_transport(fwd, delay);
        orig.set_response_status(fwd.get_response_status());
    }

    void forward_read64(uint64_t addr, uint64_t& value,
                        tlm::tlm_generic_payload& orig, sc_core::sc_time& delay)
    {
        uint8_t buf[8] = {};
        tlm::tlm_generic_payload fwd;
        fwd.set_command(tlm::TLM_READ_COMMAND);
        fwd.set_address(addr);
        fwd.set_data_ptr(buf);
        fwd.set_data_length(8);
        fwd.set_streaming_width(8);
        fwd.set_byte_enable_ptr(nullptr);
        fwd.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        isock->b_transport(fwd, delay);
        std::memcpy(&value, buf, 8);
        orig.set_response_status(fwd.get_response_status());
    }
};
