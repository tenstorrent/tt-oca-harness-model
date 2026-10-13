// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file abr_aperture_tb.cpp
 * @brief The SEP crossbar's 64 KiB Adams Bridge window against the 0xC018 bytes
 *        Adams Bridge decodes.
 *
 * sep-vp maps the whole ABR aperture (inc/Args.hpp abr_start_addr..abr_end_addr)
 * onto abr_ip through SimpleBus. The RTL's crypto demux forwards only offsets below
 * the end of the register map and answers the rest with DECERR and 0xBADCAB1E, so
 * an access in the window past that extent must come back from the bus the same way.
 */

#include "adams_bridge.h"
#include "bus.h"
#include "reg_param.h"

#include <tlm_utils/simple_initiator_socket.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

// inc/Args.hpp abr_start_addr / abr_end_addr.
constexpr uint64_t ABR_WINDOW_START = 0x10940000u;
constexpr uint64_t ABR_WINDOW_END   = 0x1094FFFFu;

int g_failed = 0;

void check(bool ok, const char *what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        ++g_failed;
    }
}

struct cpu : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<cpu> sock;
    // Unused here, but abr_ip's key-manager destination sockets must be bound.
    tlm_utils::simple_initiator_socket<cpu> km[4];

    SC_HAS_PROCESS(cpu);
    explicit cpu(sc_core::sc_module_name n) : sc_core::sc_module(n), sock("sock")
    {
        SC_THREAD(run);
    }

    tlm::tlm_response_status xact(tlm::tlm_command cmd, uint64_t addr, uint32_t &data)
    {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(cmd);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char *>(&data));
        trans.set_data_length(4u);
        trans.set_streaming_width(4u);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sock->b_transport(trans, delay);
        return trans.get_response_status();
    }

    void run()
    {
        uint32_t name = 0u;
        check(xact(tlm::TLM_READ_COMMAND, ABR_WINDOW_START, name) == tlm::TLM_OK_RESPONSE &&
                  name == 0x44534D4Cu,
              "MLDSA_NAME at the window base reads through the bus");

        uint32_t last = 0u;
        check(xact(tlm::TLM_READ_COMMAND, ABR_WINDOW_START + abr::ABR_DECODED_EXTENT - 4u, last) ==
                  tlm::TLM_OK_RESPONSE,
              "the last decoded word answers OKAY");

        for (uint64_t off : {uint64_t(abr::ABR_DECODED_EXTENT), uint64_t(0xFFFCu)}) {
            uint32_t rd = 0u;
            check(xact(tlm::TLM_READ_COMMAND, ABR_WINDOW_START + off, rd) ==
                          tlm::TLM_ADDRESS_ERROR_RESPONSE &&
                      rd == abr::ABR_DECERR_DATA,
                  "a read in the window past the decoded extent is DECERR with 0xBADCAB1E");
            uint32_t wr = 0x12345678u;
            check(xact(tlm::TLM_WRITE_COMMAND, ABR_WINDOW_START + off, wr) ==
                      tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  "a write in the window past the decoded extent is DECERR");
        }
        check(ABR_WINDOW_END - ABR_WINDOW_START + 1u >= abr::ABR_DECODED_EXTENT,
              "the window covers the decoded extent");

        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char *[])
{
    regmodel::load_config_file(nullptr);

    cpu initiator("cpu");
    SimpleBus<1, 1> bus("bus", false);
    abr_ip abr("abr");

    sc_core::sc_signal<double> clk("clk");
    sc_core::sc_signal<bool> rst_n("rst_n");
    sc_core::sc_signal<bool> err("err");
    sc_core::sc_signal<bool> notif("notif");
    clk.write(100000000.0);
    rst_n.write(true);
    abr.clk_i(clk);
    abr.rst_ni(rst_n);
    abr.intr_abr_error(err);
    abr.intr_abr_notif(notif);

    bus.ports[0] = new PortMapping(ABR_WINDOW_START, ABR_WINDOW_END, abr);
    bus.tsocks[0].bind(initiator.sock);
    bus.isocks[0].bind(abr.target_socket);
    initiator.km[0].bind(abr.keymgr_mldsa_seed_socket);
    initiator.km[1].bind(abr.keymgr_mlkem_d_socket);
    initiator.km[2].bind(abr.keymgr_mlkem_z_socket);
    initiator.km[3].bind(abr.keymgr_mlkem_msg_socket);

    sc_core::sc_start();

    std::printf(g_failed != 0 ? "SOME TESTS FAILED\n" : "ALL TESTS PASSED\n");
    std::fflush(stdout);
    std::quick_exit(g_failed != 0 ? 1 : 0);
}
