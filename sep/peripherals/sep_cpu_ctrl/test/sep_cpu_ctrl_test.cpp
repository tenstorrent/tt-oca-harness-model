// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// =============================================================================
// sep_cpu_ctrl_test.cpp  —  smoke test for sep_cpu_ctrl_ip (regmodel style)
// Addresses are register offsets — SimpleBus strips the base address.
// =============================================================================

#include "sep_cpu_ctrl.h"
#include "reg_param.h"
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
#include <cassert>
#include <cstring>

// gcov coverage data flushing (GCC 11+)
// Required when using std::quick_exit() to ensure .gcda files are written
#ifdef __GNUC__
#ifdef __COVERAGE__
extern "C" void __gcov_dump(void);
#endif
#endif

// ---------------------------------------------------------------------------
// Testbench — owns an initiator socket bound to dut.target_socket
// ---------------------------------------------------------------------------
SC_MODULE(Tb) {
    sep_cpu_ctrl_ip                       dut;
    tlm_utils::simple_initiator_socket<Tb, 32> isock;
    sc_core::sc_signal<bool>              rst_n_sig;
    sc_core::sc_signal<uint32_t, sc_core::SC_MANY_WRITERS> nmi_vec_sig;
    // Plain (single-writer) signals: publish_window_process is the only driver,
    // and keeping the default policy makes an accidental second one an error.
    sc_core::sc_signal<uint64_t> global_base_sig;
    sc_core::sc_signal<uint64_t> region_size_sig;

    SC_CTOR(Tb)
        : dut("dut")
        , isock("isock")
        , rst_n_sig("rst_n_sig")
        , nmi_vec_sig("nmi_vec_sig")
        , global_base_sig("global_base_sig")
        , region_size_sig("region_size_sig")
    {
        isock.bind(dut.target_socket);
        dut.rst_ni(rst_n_sig);
        dut.nmi_vec_o(nmi_vec_sig);
        dut.sep_global_base_addr_o(global_base_sig);
        dut.sep_region_size_o(region_size_sig);
        SC_THREAD(run);
    }

    void do_write(uint64_t offset, uint64_t value) {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(offset);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(8);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_streaming_width(8);
        isock->b_transport(trans, delay);
        assert(trans.get_response_status() == tlm::TLM_OK_RESPONSE);
    }

    uint64_t do_read(uint64_t offset) {
        uint64_t value = 0;
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(offset);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(8);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_streaming_width(8);
        isock->b_transport(trans, delay);
        assert(trans.get_response_status() == tlm::TLM_OK_RESPONSE);
        return value;
    }

    void run() {
        assert(nmi_vec_sig.read() == 0xC0000100u); // 0x60000080 << 1
        std::cout << "[PASS] T0: nmi_vec_o initialized before any reset edge\n";

        // Prime to high, then assert (falling edge fires reset_handler), then deassert
        rst_n_sig.write(true);
        wait(1, sc_core::SC_NS);
        rst_n_sig.write(false);
        wait(1, sc_core::SC_NS);
        rst_n_sig.write(true);
        wait(1, sc_core::SC_NS);

        std::cout << "\n=== sep_cpu_ctrl_ip smoke tests ===\n";

        // ------------------------------------------------------------------
        // T1: Reset values
        // ------------------------------------------------------------------
        uint64_t v = do_read(0x008); // CLOCK_GATE_CTRL
        assert((v & 0xFFFFFFFFULL) == 0x0ULL); // reserved placeholder, resets to 0
        std::cout << "[PASS] T1: CLOCK_GATE_CTRL reset=0x0\n";

        v = do_read(0x1000); // SEP_VERSION_ID
        assert(v == 0xDEADBEEFULL);
        std::cout << "[PASS] T1: SEP_VERSION_ID = 0xDEADBEEF\n";

        v = do_read(0x0C8); // SEP_LOCAL_BASE_ADDR
        assert(v == 0xD0000000ULL);
        std::cout << "[PASS] T1: SEP_LOCAL_BASE_ADDR reset=0xD0000000\n";

        v = do_read(0x190); // EXT_TRNG_SRC_SEL
        assert(v == 0x7); // sel[2:0]: bit2 = entropy pool stream
        std::cout << "[PASS] T1: EXT_TRNG_SRC_SEL reset=0x7\n";

        v = do_read(0x020); // PKA_CTRL
        assert(v == 0x0);
        std::cout << "[PASS] T1: PKA_CTRL reset=0x0\n";

        assert(nmi_vec_sig.read() == 0xC0000100u); // 0x60000080 << 1
        std::cout << "[PASS] T1: nmi_vec_o reset=0xC0000100\n";

        v = do_read(0x0C0); // SEP_GLOBAL_BASE_ADDR
        assert(v == 0x0ULL);
        v = do_read(0x0D0); // SEP_REGION_SIZE
        assert(v == 0x01000000ULL);
        assert(global_base_sig.read() == 0x0ULL);
        assert(region_size_sig.read() == 0x01000000ULL);
        std::cout << "[PASS] T1: inbound window resets to [0x0,+16 MiB) and is exported\n";

        // ------------------------------------------------------------------
        // T2: Basic SW read/write (CLOCK_GATE_CTRL)
        // ------------------------------------------------------------------
        do_write(0x008, 0x0000000000000004ULL); // dma_cg_enable=1
        v = do_read(0x008);
        assert((v & 0x4) == 0x4);
        std::cout << "[PASS] T2: CLOCK_GATE_CTRL write/readback\n";

        // pka_noise_src / pka_noise_src_valid live at bits 1/2, not 8/16.
        do_write(0x020, 0x7ULL);
        v = do_read(0x020);
        assert(v == 0x7);
        do_write(0x020, 0x10100ULL); // former bit8/bit16 positions are reserved
        v = do_read(0x020);
        assert(v == 0x0);
        std::cout << "[PASS] T2: PKA_CTRL fields at bits 0/1/2\n";

        // ------------------------------------------------------------------
        // T3: REFERENCE_COUNTER reloads on SW write (wr_swacc path); other
        // read-only registers (SEP_VERSION_ID below) silently ignore writes.
        // ------------------------------------------------------------------
        dut.hwif_in.reference_counter_rc = 0x123456789ABCULL;
        do_write(0x010, 0xDEADDEADDEADDEADULL); // reloads the counter
        v = do_read(0x010); // pre-increments to 0xDEADDEADDEADDEAE
        assert(v == 0xDEADDEADDEADDEAEULL);
        std::cout << "[PASS] T3: REFERENCE_COUNTER reloads on SW write, then keeps counting\n";

        dut.hwif_in.sys_in_timeout_int = true;
        dut.hwif_in.alias_remap_timeout_int = true;
        do_write(0x018, 0x0ULL); // sw=r — writes have no effect
        v = do_read(0x018);
        assert((v & 0x1) == 1); // sys_in
        assert((v & 0x4) == 4); // alias_remap
        std::cout << "[PASS] T3: TIMEOUT_INTERRUPT returns hwif_in bits\n";

        do_write(0x1000, 0xCAFEBABEULL);
        v = do_read(0x1000);
        assert(v == 0xDEADBEEFULL);
        std::cout << "[PASS] T3: SEP_VERSION_ID ignores writes\n";

        // ------------------------------------------------------------------
        // T4: NMI vector lock (woset + lock-gated write + nmi_vec_o)
        // ------------------------------------------------------------------
        do_write(0x180, 0x0000000000001000ULL);
        wait(sc_core::SC_ZERO_TIME); // sc_signal update phase must commit before reading
        v = do_read(0x180);
        assert((v & ~0x1ULL) == 0x1000ULL);
        assert(nmi_vec_sig.read() == 0x1000u);
        std::cout << "[PASS] T4: SEP_NMI_VEC write before lock + nmi_vec_o driven\n";

        do_write(0x188, 0x1ULL); // set lock
        v = do_read(0x188);
        assert((v & 0x1) == 1);
        std::cout << "[PASS] T4: SEP_NMI_VEC_LOCK set\n";

        do_write(0x180, 0xFFFFFFFFFFFFFFFFULL); // write after lock — must be ignored
        v = do_read(0x180);
        assert((v & ~0x1ULL) == 0x1000ULL);
        std::cout << "[PASS] T4: SEP_NMI_VEC write silently ignored after lock\n";

        do_write(0x188, 0x0ULL); // try to clear lock — must stay set (woset)
        v = do_read(0x188);
        assert((v & 0x1) == 1);
        std::cout << "[PASS] T4: SEP_NMI_VEC_LOCK cannot be cleared (woset)\n";

        // ------------------------------------------------------------------
        // T5: EXT_TRNG_SRC_SEL lock (same pattern)
        // ------------------------------------------------------------------
        do_write(0x190, 0x5ULL); // bit2 (entropy pool) must survive: sel is [2:0]
        v = do_read(0x190);
        assert(v == 0x5);

        do_write(0x198, 0x1ULL); // set lock
        v = do_read(0x198);
        assert(v == 0x1); // lock is readable
        do_write(0x190, 0x7ULL); // should be ignored
        v = do_read(0x190);
        assert(v == 0x5); // unchanged
        std::cout << "[PASS] T5: EXT_TRNG_SRC_SEL locked correctly\n";

        // ------------------------------------------------------------------
        // T6: TIMEOUT_CLEAR reads back 0 (singlepulse, write-only to HW)
        // ------------------------------------------------------------------
        v = do_read(0x070);
        assert(v == 0);
        std::cout << "[PASS] T6: TIMEOUT_CLEAR reads back 0\n";

        // ------------------------------------------------------------------
        // T7: TIMEOUT_MODE reads back 0 (write-only from SW perspective)
        // ------------------------------------------------------------------
        do_write(0x078, 0x5ULL);
        v = do_read(0x078);
        assert(v == 0);
        std::cout << "[PASS] T7: TIMEOUT_MODE reads back 0 (write-only)\n";

        // ------------------------------------------------------------------
        // T8: TIMEOUT_COUNT 48-bit masking (base handles via write_mask)
        // ------------------------------------------------------------------
        do_write(0x028, 0xFFFFFFFFFFFFFFFFULL);
        v = do_read(0x028);
        assert(v == 0x0000FFFFFFFFFFFFULL);
        std::cout << "[PASS] T8: TIMEOUT_COUNT_DMA 48-bit mask\n";

        // ------------------------------------------------------------------
        // T9: SEP_GLOBAL_BASE_ADDR 56-bit mask
        // ------------------------------------------------------------------
        do_write(0x0C0, 0xFFFFFFFFFFFFFFFFULL);
        v = do_read(0x0C0);
        assert(v == 0x00FFFFFFFFFFFFFFULL);
        std::cout << "[PASS] T9: SEP_GLOBAL_BASE_ADDR 56-bit mask\n";

        // ------------------------------------------------------------------
        // T10: Fuse sense status from hwif_in (boot-critical)
        // ------------------------------------------------------------------
        dut.hwif_in.smc_fuse_sense_done = true;
        dut.hwif_in.sep_fuse_sense_done = true;
        v = do_read(0x140);
        assert(v == 1);
        v = do_read(0x150);
        assert(v == 1);
        std::cout << "[PASS] T10: Fuse sense status reads from hwif_in\n";

        // ------------------------------------------------------------------
        // T11: TIMEOUT_CLEAR write actually clears hwif_in.*_timeout_int bits
        // ------------------------------------------------------------------
        dut.hwif_in.sys_in_timeout_int          = true;
        dut.hwif_in.dma_data_timeout_int        = true;
        dut.hwif_in.alias_remap_timeout_int     = true;
        dut.hwif_in.filter_out_timeout_int      = true;
        dut.hwif_in.entropy_read_timeout_int    = true;
        dut.hwif_in.entropy_write_timeout_int   = true;
        dut.hwif_in.inbound_mailbox_timeout_int = true;
        dut.hwif_in.outbound_mailbox_timeout_int = true;
        v = do_read(0x018); // TIMEOUT_INTERRUPT
        assert((v & 0xFFULL) == 0xFFULL);

        do_write(0x070, 0x05ULL); // clear bit0 (sys_in) and bit2 (alias_remap) only
        v = do_read(0x018);
        assert((v & 0x01ULL) == 0);      // sys_in cleared
        assert((v & 0x04ULL) == 0);      // alias_remap cleared
        assert((v & 0x02ULL) == 0x02ULL); // dma_data untouched
        assert((v & 0xF8ULL) == 0xF8ULL); // remaining 5 flags untouched

        do_write(0x070, 0xFFULL); // clear the rest
        v = do_read(0x018);
        assert(v == 0);
        v = do_read(0x070); // singlepulse — self-clears back to 0
        assert(v == 0);
        std::cout << "[PASS] T11: TIMEOUT_CLEAR write clears targeted hwif_in bits\n";

        // ------------------------------------------------------------------
        // T12: SEP_TEST_CTRL reads live hwif_in.fast_*_en / sep_standalone bits
        // ------------------------------------------------------------------
        dut.hwif_in.fast_spi_en    = true;
        dut.hwif_in.fast_iccm_en   = true;
        dut.hwif_in.fast_dccm_en   = true;
        dut.hwif_in.fast_sram_en   = true;
        dut.hwif_in.fast_pka_en    = true;
        dut.hwif_in.sep_standalone = true;
        v = do_read(0x0B0); // SEP_TEST_CTRL
        assert(v == (0x1ULL << 31 | 0x1ULL << 30 | 0x1ULL << 29 |
                     0x1ULL << 28 | 0x1ULL << 27 | 0x1ULL << 26));
        std::cout << "[PASS] T12: SEP_TEST_CTRL reflects hwif_in fast_*_en/sep_standalone\n";

        // ------------------------------------------------------------------
        // T13: SEP_STRAPS reads live hwif_in.test_en / bypass_mem_repair bits
        // ------------------------------------------------------------------
        dut.hwif_in.test_en           = true;
        dut.hwif_in.bypass_mem_repair = true;
        v = do_read(0x160); // SEP_STRAPS
        assert((v & 0x3ULL) == 0x3ULL);
        std::cout << "[PASS] T13: SEP_STRAPS reflects hwif_in test_en/bypass_mem_repair\n";

        // ------------------------------------------------------------------
        // T14: inbound-window CSR writes drive sep_global_base_addr_o /
        // sep_region_size_o. The SMU interconnect sizes its SEP aperture from
        // these, so a firmware reprogram has to be visible on the ports and not
        // only in the register file. addr is [55:0], size is [31:0]: the bits
        // above each field are reserved and must not reach the export.
        // ------------------------------------------------------------------
        // Two zero-time waits throughout: the callback only requests a
        // republish, so the driving process runs one delta later and its signal
        // write commits the delta after that.
        do_write(0x0C0, 0x5000'0000ULL);
        do_write(0x0D0, 0x2000'0000ULL);
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);
        assert(do_read(0x0C0) == 0x5000'0000ULL);
        assert(do_read(0x0D0) == 0x2000'0000ULL);
        assert(global_base_sig.read() == 0x5000'0000ULL);
        assert(region_size_sig.read() == 0x2000'0000ULL);

        do_write(0x0C0, 0xFFFF'FFFF'FFFF'FFFFULL);
        do_write(0x0D0, 0xFFFF'FFFF'FFFF'FFFFULL);
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);
        assert(global_base_sig.read() == 0x00FF'FFFF'FFFF'FFFFULL); // [55:0]
        assert(region_size_sig.read() == 0xFFFF'FFFFULL);           // [31:0]
        std::cout << "[PASS] T14: inbound window CSR writes drive the exports, reserved bits masked\n";

        // Assigning the registers directly bypasses the write callbacks, which
        // is how a platform seeds the window in place of firmware. The exports
        // go stale until publish_inbound_window() asks for a re-sample.
        dut.SEP_GLOBAL_BASE_ADDR = 0x4000'0000ULL;
        dut.SEP_REGION_SIZE      = 0x0800'0000ULL;
        wait(sc_core::SC_ZERO_TIME);
        assert(global_base_sig.read() == 0x00FF'FFFF'FFFF'FFFFULL); // still stale
        dut.publish_inbound_window();
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME); // notify(SC_ZERO_TIME) + signal update
        assert(global_base_sig.read() == 0x4000'0000ULL);
        assert(region_size_sig.read() == 0x0800'0000ULL);
        std::cout << "[PASS] T14: publish_inbound_window() re-samples the register file\n";

        // ------------------------------------------------------------------
        // T15: DMA/PERIPH bus-error status is RO; CLEAR is singlepulse
        // ------------------------------------------------------------------
        assert(do_read(0x1A8) == 0);
        assert(do_read(0x1B8) == 0);
        do_write(0x1A8, 0x3ULL);
        do_write(0x1B8, 0x7FULL);
        assert(do_read(0x1A8) == 0);
        assert(do_read(0x1B8) == 0);

        dut.DMA_BUS_ERR_STATUS = 0x3ULL;
        dut.PERIPH_BUS_ERR_STATUS = 0x15ULL; // aes + kmac + csrng
        assert(do_read(0x1A8) == 0x3ULL);
        assert(do_read(0x1B8) == 0x15ULL);

        do_write(0x1B0, 0x1ULL);
        assert(do_read(0x1A8) == 0);
        assert(do_read(0x1B0) == 0);

        do_write(0x1C0, 0x01ULL); // clear aes only
        assert((do_read(0x1B8) & 0x01ULL) == 0);
        assert((do_read(0x1B8) & 0x14ULL) == 0x14ULL);
        do_write(0x1C0, 0x7FULL);
        assert(do_read(0x1B8) == 0);
        assert(do_read(0x1C0) == 0);
        std::cout << "[PASS] T15: DMA/PERIPH_BUS_ERR status/clear match RTL\n";

        std::cout << "\n=== All tests PASSED ===\n";
        sc_core::sc_stop();
    }
};

int sc_main(int argc, char** argv) {
    regmodel::load_config_file(argc > 1 ? argv[1] : nullptr);
    Tb tb("tb");
    sc_core::sc_start();

#ifdef __COVERAGE__
    __gcov_dump();  // Flush coverage data before quick_exit
#endif
    // quick_exit bypasses CCI broker destructor, which crashes on cleanup
    std::quick_exit(0);
    return 0;
}
