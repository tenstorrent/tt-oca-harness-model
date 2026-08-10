// SPDX-License-Identifier: Apache-2.0
/**
 * @file aou_core_tb.cpp
 * @brief Unit test for AOU_CORE LT model: CSR reset, activate, AXI loopback.
 */

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <cci_configuration>
#include <cci/utils/consuming_broker.h>

#include <cstring>
#include <iostream>
#include <vector>

#include "aou_core.h"

namespace {

int g_failures = 0;

#define EXPECT_EQ(a, e)                                                            \
    do {                                                                           \
        if ((a) != (e)) {                                                          \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #a     \
                      << " got " << (a) << " expected " << (e) << "\n";             \
            ++g_failures;                                                          \
        }                                                                          \
    } while (0)

#define EXPECT_TRUE(expr)                                                          \
    do {                                                                           \
        if (!(expr)) {                                                             \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #expr  \
                      << "\n";                                                     \
            ++g_failures;                                                          \
        }                                                                          \
    } while (0)

template <unsigned W>
struct mem : sc_core::sc_module {
    tlm_utils::simple_target_socket<mem, W> sock;
    std::vector<unsigned char> data;

    explicit mem(sc_core::sc_module_name n, size_t size)
        : sc_core::sc_module(n), sock("sock"), data(size, 0)
    {
        sock.register_b_transport(this, &mem::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay)
    {
        const uint64_t addr = gp.get_address();
        auto* ptr = gp.get_data_ptr();
        const unsigned len = gp.get_data_length();
        if (addr + len > data.size()) {
            gp.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        if (gp.is_read()) std::memcpy(ptr, &data[addr], len);
        else if (gp.is_write()) std::memcpy(&data[addr], ptr, len);
        gp.set_response_status(tlm::TLM_OK_RESPONSE);
        delay += sc_core::sc_time(1.0, sc_core::SC_NS);
    }
};

template <unsigned W>
struct drv : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<drv, W> sock;
    explicit drv(sc_core::sc_module_name n) : sc_core::sc_module(n), sock("sock") {}

    tlm::tlm_response_status xfer(tlm::tlm_command cmd, uint64_t addr,
                                  unsigned char* data, unsigned len)
    {
        tlm::tlm_generic_payload gp;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        gp.set_command(cmd);
        gp.set_address(addr);
        gp.set_data_ptr(data);
        gp.set_data_length(len);
        gp.set_streaming_width(len);
        gp.set_byte_enable_ptr(nullptr);
        gp.set_dmi_allowed(false);
        gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(gp, delay);
        return gp.get_response_status();
    }

    uint32_t read32(uint64_t addr)
    {
        uint32_t d = 0;
        EXPECT_EQ(xfer(tlm::TLM_READ_COMMAND, addr,
                       reinterpret_cast<unsigned char*>(&d), 4),
                  tlm::TLM_OK_RESPONSE);
        return d;
    }

    void write32(uint64_t addr, uint32_t data)
    {
        EXPECT_EQ(xfer(tlm::TLM_WRITE_COMMAND, addr,
                       reinterpret_cast<unsigned char*>(&data), 4),
                  tlm::TLM_OK_RESPONSE);
    }
};

} // namespace

SC_MODULE(tb) {
    aou::aou_core dut_a;
    aou::aou_core dut_b;
    mem<64> mem_a;
    mem<64> mem_b;
    drv<32> apb_a;
    drv<32> apb_b;
    drv<64> axi_a;
    drv<64> axi_b;
    sc_core::sc_signal<bool> fdi_a;
    sc_core::sc_signal<bool> fdi_b;
    sc_core::sc_signal<bool> irq_a;
    sc_core::sc_signal<bool> irq_b;

    SC_CTOR(tb)
        : dut_a("dut_a")
        , dut_b("dut_b")
        , mem_a("mem_a", 64)
        , mem_b("mem_b", 4096)
        , apb_a("apb_a")
        , apb_b("apb_b")
        , axi_a("axi_a")
        , axi_b("axi_b")
        , fdi_a("fdi_a")
        , fdi_b("fdi_b")
        , irq_a("irq_a")
        , irq_b("irq_b")
    {
        apb_a.sock.bind(dut_a.apb_socket);
        apb_b.sock.bind(dut_b.apb_socket);
        axi_a.sock.bind(dut_a.axi_s[0]);
        axi_b.sock.bind(dut_b.axi_s[0]);
        dut_a.axi_m[0].bind(mem_a.sock);
        dut_b.axi_m[0].bind(mem_b.sock);
        dut_a.fdi_active_i.bind(fdi_a);
        dut_b.fdi_active_i.bind(fdi_b);
        dut_a.irq_o.bind(irq_a);
        dut_b.irq_o.bind(irq_b);
        dut_a.connect_peer(&dut_b);
        dut_b.connect_peer(&dut_a);
        SC_THREAD(run);
    }

    void run()
    {
        // Tie both dies' FDI (UCIe die-to-die link) active before anything
        // else -- activation is gated on fdi_active() for both the local
        // core and its peer. wait(SC_ZERO_TIME) lets the sc_signal writes
        // settle (a delta cycle) before the DUTs' fdi_active_i ports read
        // the new value.
        fdi_a.write(true);
        fdi_b.write(true);
        wait(sc_core::SC_ZERO_TIME);

        // Case 1: CSR reset -- ip_version, aou_con0, and aou_init's
        // activate_state_disabled bit should all read back at their RTL
        // reset values before any CSR writes happen.
        EXPECT_EQ(apb_a.read32(aou::aou_cfg::OFF_IP_VERSION), 0x00010000u);
        EXPECT_EQ(apb_a.read32(aou::aou_cfg::OFF_AOU_CON0), 0x00004008u);
        EXPECT_TRUE((apb_a.read32(aou::aou_cfg::OFF_AOU_INIT) & (1u << 3)) != 0);
        std::cout << "  [PASS] CSR reset\n";

        // Case 2: APB decode errors -- b_transport_apb() must reject a
        // non-4-byte transfer and a misaligned address with
        // TLM_GENERIC_ERROR_RESPONSE (before the offset is even looked at),
        // and an in-range-length-but-out-of-window offset with
        // TLM_ADDRESS_ERROR_RESPONSE.
        {
            uint32_t d = 0;
            EXPECT_EQ(apb_a.xfer(tlm::TLM_READ_COMMAND, aou::aou_cfg::OFF_IP_VERSION,
                                 reinterpret_cast<unsigned char*>(&d), 2),
                      tlm::TLM_GENERIC_ERROR_RESPONSE);
            EXPECT_EQ(apb_a.xfer(tlm::TLM_READ_COMMAND, 0x01,
                                 reinterpret_cast<unsigned char*>(&d), 4),
                      tlm::TLM_GENERIC_ERROR_RESPONSE);
            EXPECT_EQ(apb_a.xfer(tlm::TLM_READ_COMMAND, aou::aou_cfg::kWindowSize,
                                 reinterpret_cast<unsigned char*>(&d), 4),
                      tlm::TLM_ADDRESS_ERROR_RESPONSE);
        }
        std::cout << "  [PASS] APB decode errors\n";

        // Case 3: Named CSR read/write round trip -- aou_interrupt_mask,
        // lp_linkreset, prior_rp_axi and prior_timer are plain masked
        // storage (Register32); writing all-ones must read back exactly
        // each register's write mask merged with its (all-zero-outside-
        // mask) reset value. dest_rp_ is deliberately left untouched here
        // (Case 9 below relies on its identity-mapped reset value first).
        apb_a.write32(aou::aou_cfg::OFF_AOU_INTERRUPT_MASK, 0xFFFFFFFFu);
        EXPECT_EQ(apb_a.read32(aou::aou_cfg::OFF_AOU_INTERRUPT_MASK), 0x000001FCu);
        apb_a.write32(aou::aou_cfg::OFF_LP_LINKRESET, 0xFFFFFFFFu);
        EXPECT_EQ(apb_a.read32(aou::aou_cfg::OFF_LP_LINKRESET), 0x00003FFFu);
        apb_a.write32(aou::aou_cfg::OFF_PRIOR_RP_AXI, 0xFFFFFFFFu);
        EXPECT_EQ(apb_a.read32(aou::aou_cfg::OFF_PRIOR_RP_AXI), 0x0FF3FFF3u);
        apb_a.write32(aou::aou_cfg::OFF_PRIOR_TIMER, 0xFFFFFFFFu);
        EXPECT_EQ(apb_a.read32(aou::aou_cfg::OFF_PRIOR_TIMER), 0xFFFFFFFFu);
        std::cout << "  [PASS] named CSR read/write round trip\n";

        // Case 4: Per-RP CSR bank read/write via APB -- offset OFF_RP0_BASE
        // (RP0's axi_split_tr) falls outside every named switch case, so it
        // exercises the reg_read()/reg_write() fallthrough into the
        // [OFF_RP0_BASE, kWindowSize) array-indexed path. Reset value is
        // 0x0F0F (rp_reset(local_idx==0)); writing all-ones must read back
        // exactly the register's 16-bit mask.
        EXPECT_EQ(apb_a.read32(aou::aou_cfg::OFF_RP0_BASE), 0x00000F0Fu);
        apb_a.write32(aou::aou_cfg::OFF_RP0_BASE, 0xFFFFFFFFu);
        EXPECT_EQ(apb_a.read32(aou::aou_cfg::OFF_RP0_BASE), 0x0000FFFFu);
        std::cout << "  [PASS] per-RP CSR bank read/write\n";

        // Case 5: AXI deny when disabled -- dut_a is still DISABLED (no
        // activate_start written yet), so a write on axi_s[0] must be
        // rejected with TLM_COMMAND_ERROR_RESPONSE rather than reaching
        // the peer/mem at all.
        uint32_t w = 0xA5A5A5A5u;
        EXPECT_EQ(axi_a.xfer(tlm::TLM_WRITE_COMMAND, 0x10,
                             reinterpret_cast<unsigned char*>(&w), 4),
                  tlm::TLM_COMMAND_ERROR_RESPONSE);
        std::cout << "  [PASS] AXI deny when disabled\n";

        // Case 6: Activate -- writing activate_start on dut_a's aou_init
        // should enable dut_a (via try_activate()) *and* dut_b (via the
        // peer_activate_ack() auto-ack, since both FDIs are already up),
        // and raise dut_a's irq_o (int_activate_start). Writing back
        // int_activate_start (bit 8) then W1C-clears the interrupt.
        // wait(SC_ZERO_TIME) is needed both times because irq_o is driven
        // via update_irq()'s sc_signal write, which is only visible to
        // .read() after the delta-cycle settles.
        apb_a.write32(aou::aou_cfg::OFF_AOU_INIT, 0x1u);
        EXPECT_TRUE(dut_a.is_enabled());
        EXPECT_TRUE(dut_b.is_enabled());
        wait(sc_core::SC_ZERO_TIME);
        EXPECT_TRUE(irq_a.read());
        apb_a.write32(aou::aou_cfg::OFF_AOU_INIT, 1u << 8);
        wait(sc_core::SC_ZERO_TIME);
        EXPECT_TRUE(!irq_a.read());
        std::cout << "  [PASS] activate\n";

        // Case 7: Soft reset while enabled -- writing aou_con0 with bit4
        // (aou_sw_reset) set must call soft_reset() as a write side effect:
        // every CSR (including the ones Case 3 just dirtied) drops back to
        // its RTL reset value and dut_a returns to DISABLED, independently
        // of dut_b (soft_reset() is per-instance, so dut_b -- never soft
        // reset -- stays ENABLED). aou_con0 itself reads back to its own
        // reset value (0x00004008, bit4 clear again) because soft_reset()
        // resets aou_con0_ too. force (bit0) is 0 in this write, so the
        // try_deactivate(true) side effect is *not* taken here (Case 11
        // below exercises that branch instead).
        apb_a.write32(aou::aou_cfg::OFF_AOU_CON0, 0x00004008u | (1u << 4));
        EXPECT_TRUE(!dut_a.is_enabled());
        EXPECT_TRUE(dut_b.is_enabled());
        EXPECT_EQ(apb_a.read32(aou::aou_cfg::OFF_AOU_CON0), 0x00004008u);
        EXPECT_EQ(apb_a.read32(aou::aou_cfg::OFF_AOU_INTERRUPT_MASK), 0u);
        EXPECT_EQ(apb_a.read32(aou::aou_cfg::OFF_DEST_RP), 0x00003210u);
        std::cout << "  [PASS] soft reset while enabled\n";

        // Re-activate dut_a so the loopback case below has both dies
        // ENABLED again. dut_b is already enabled from Case 6, so its
        // peer_activate_ack() call here is a harmless no-op (guarded by
        // `if (enabled_ ...) return;`).
        apb_a.write32(aou::aou_cfg::OFF_AOU_INIT, 0x1u);
        EXPECT_TRUE(dut_a.is_enabled());
        wait(sc_core::SC_ZERO_TIME);

        // Case 8: AXI loopback A->B -- with both dies ENABLED, a write on
        // dut_a.axi_s[0] must bridge across the peer_ link to dut_b's
        // axi_m[dest] (dest_rp_ defaults to an identity RP mapping) and
        // land in mem_b; reading the same address back must return what
        // was written.
        w = 0xDEADBEEFu;
        EXPECT_EQ(axi_a.xfer(tlm::TLM_WRITE_COMMAND, 0x100,
                             reinterpret_cast<unsigned char*>(&w), 4),
                  tlm::TLM_OK_RESPONSE);

        // Independently confirm the write really landed in mem_b's backing
        // storage (bypassing dut_a/dut_b entirely) rather than trusting the
        // read-back below, which travels back through the same dut_a ->
        // dut_b forwarding path as the write and so couldn't by itself
        // distinguish "the bridge actually stored it in mem_b" from a
        // hypothetical short-circuit that never reached mem_b at all.
        uint32_t stored_in_b = 0;
        std::memcpy(&stored_in_b, &mem_b.data[0x100], 4);
        EXPECT_EQ(stored_in_b, 0xDEADBEEFu);

        uint32_t r = 0;
        EXPECT_EQ(axi_a.xfer(tlm::TLM_READ_COMMAND, 0x100,
                             reinterpret_cast<unsigned char*>(&r), 4),
                  tlm::TLM_OK_RESPONSE);
        EXPECT_EQ(r, 0xDEADBEEFu);
        std::cout << "  [PASS] AXI loopback A->B\n";

        // Case 9: AXI bridge dest_rp out-of-range -- writing all-ones to
        // dest_rp_ sets every 2-bit rp*_dest field to 3 (masked by its
        // 0x3333 write mask), so RP0's traffic now targets peer RP index 3.
        // dut_b only has 1 RP (rp_count defaults to 1), so
        // b_transport_axi_s() must reject the transfer with
        // TLM_ADDRESS_ERROR_RESPONSE *before* ever reaching peer_->axi_m[].
        apb_a.write32(aou::aou_cfg::OFF_DEST_RP, 0xFFFFFFFFu);
        EXPECT_EQ(apb_a.read32(aou::aou_cfg::OFF_DEST_RP), 0x00003333u);
        w = 0x11111111u;
        EXPECT_EQ(axi_a.xfer(tlm::TLM_WRITE_COMMAND, 0x200,
                             reinterpret_cast<unsigned char*>(&w), 4),
                  tlm::TLM_ADDRESS_ERROR_RESPONSE);
        std::cout << "  [PASS] AXI bridge dest_rp out-of-range\n";

        // Case 10: Deactivate -- writing deactivate_start on dut_a's
        // aou_init must return it to DISABLED via try_deactivate(false).
        apb_a.write32(aou::aou_cfg::OFF_AOU_INIT, 0x2u);
        EXPECT_TRUE(!dut_a.is_enabled());
        std::cout << "  [PASS] deactivate\n";

        // Case 11: aou_con0 write-only paths -- a write to OFF_IP_VERSION
        // is a documented no-op (ip_version is read-only silicon ID), and
        // deactivate_force (aou_con0 bit0) must force ENABLED -> DISABLED
        // unconditionally, bypassing the deactivate_start_/enabled_ gating
        // that the normal aou_init path requires. Re-activate dut_a first
        // so there is something to force-deactivate.
        apb_a.write32(aou::aou_cfg::OFF_IP_VERSION, 0x12345678u);
        EXPECT_EQ(apb_a.read32(aou::aou_cfg::OFF_IP_VERSION), 0x00010000u);

        apb_a.write32(aou::aou_cfg::OFF_AOU_INIT, 0x1u);
        EXPECT_TRUE(dut_a.is_enabled());
        wait(sc_core::SC_ZERO_TIME);

        apb_a.write32(aou::aou_cfg::OFF_AOU_CON0, 0x1u);
        EXPECT_TRUE(!dut_a.is_enabled());
        std::cout << "  [PASS] aou_con0 IP_VERSION no-op + deactivate_force\n";

        if (g_failures == 0) std::cout << "\nALL TESTS PASSED\n";
        else std::cout << "\n" << g_failures << " FAILURE(S)\n";
        sc_core::sc_stop();
    }
};

int sc_main(int, char**)
{
    cci::cci_register_broker(new cci_utils::consuming_broker("GlobalBroker"));
    tb top("tb");
    sc_core::sc_start();
    return g_failures == 0 ? 0 : 1;
}
