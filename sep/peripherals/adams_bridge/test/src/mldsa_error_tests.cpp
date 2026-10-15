// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file mldsa_error_tests.cpp
 * @brief The ML-DSA controller error path parks the core until zeroize.
 *
 * With the Caliptra key-vault interface built in, PCR_SIGN on KEYGEN, SIGN or
 * VERIFY sets the controller's error flag (abr_ctrl.sv pcr_sign_input_invalid).
 * The sequencer then sits in its error state: ERROR=1, READY=0 and VALID never
 * set, until zeroize (abr_ctrl.sv:1617-1619). The error raises the error event,
 * never the command-done event.
 */

#include "abr_testbench.h"

#include <string>
#include <vector>

namespace {

constexpr uint32_t CMD_KEYGEN    = 0x1u;
constexpr uint32_t CMD_SIGN      = 0x2u;
constexpr uint32_t CMD_VERIFY    = 0x3u;
constexpr uint32_t CTRL_PCR_SIGN = 1u << 4;

constexpr uint32_t ST_READY = 1u << 0;
constexpr uint32_t ST_VALID = 1u << 1;
constexpr uint32_t ST_ERROR = 1u << 3;

} // namespace

void mldsa_error_tests(abr_testbench &tb)
{
    tb.section("ML-DSA error path: PCR_SIGN with VERIFY");

    tb.check(tb.zeroize_mldsa(), "zeroize before the error case");
    const std::vector<uint32_t> ct = {0x01020304u, 0x05060708u, 0x090A0B0Cu, 0x0D0E0F10u,
                                      0x11121314u, 0x15161718u, 0x191A1B1Cu, 0x1D1E1F20u,
                                      0x21222324u, 0x25262728u, 0x292A2B2Cu, 0x2D2E2F30u,
                                      0x31323334u, 0x35363738u, 0x393A3B3Cu, 0x3D3E3F40u};
    tb.wr_n(abr::OFF_MLDSA_SIGNATURE, ct);
    tb.wr(abr::OFF_ERROR_INTERNAL_INTR, 1u);
    tb.wr(abr::OFF_NOTIF_INTERNAL_INTR, 1u);
    const uint32_t err_before = tb.rd(abr::OFF_ERROR_INTR_COUNT);
    const uint32_t done_before = tb.rd(abr::OFF_NOTIF_INTR_COUNT);

    tb.wr(abr::OFF_MLDSA_CTRL, CMD_VERIFY | CTRL_PCR_SIGN);
    sc_core::wait(sc_core::sc_time(2, sc_core::SC_MS));

    const uint32_t st = tb.rd(abr::OFF_MLDSA_STATUS);
    tb.check_eq(st & ST_ERROR, ST_ERROR, "the error sets STATUS.ERROR");
    tb.check_eq(st & ST_VALID, 0u, "the error never sets VALID");
    tb.check_eq(st & ST_READY, 0u, "the error parks READY low");
    tb.check_eq(tb.rd(abr::OFF_ERROR_INTERNAL_INTR) & 1u, 1u, "the error raises the error event");
    tb.check_eq(tb.rd(abr::OFF_ERROR_INTR_COUNT), err_before + 1u,
                "the error event counter advances by one");
    tb.check_eq(tb.rd(abr::OFF_NOTIF_INTERNAL_INTR) & 1u, 0u, "the error raises no done event");
    tb.check_eq(tb.rd(abr::OFF_NOTIF_INTR_COUNT), done_before, "the done counter does not move");

    std::vector<uint32_t> inv(ct);
    for (auto &w : inv) {
        w = ~w;
    }
    tb.check(tb.rd_n(abr::OFF_MLDSA_VERIFY_RES, abr::N_VERIFY_RES) == inv,
             "the accepted VERIFY preloaded ~c~ before the error");

    tb.wr(abr::OFF_MLDSA_CTRL, CMD_KEYGEN);
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS), st, "a command written in the error state is dropped");

    tb.check(tb.zeroize_mldsa(), "zeroize recovers from the error");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS), ST_READY, "zeroize leaves READY alone set");
    tb.check_eq(tb.rd(abr::OFF_ERROR_INTERNAL_INTR) & 1u, 1u,
                "zeroize does not clear the error event status");
    tb.check_eq(tb.rd(abr::OFF_ERROR_INTR_COUNT), err_before + 1u,
                "zeroize does not clear the error event counter");

    tb.section("ML-DSA error path: PCR_SIGN with KEYGEN and SIGN");

    for (uint32_t cmd : {CMD_KEYGEN, CMD_SIGN}) {
        const std::string name = cmd == CMD_KEYGEN ? "KEYGEN" : "SIGN";
        tb.check(tb.zeroize_mldsa(), "zeroize before PCR_SIGN with " + name);
        tb.wr(abr::OFF_MLDSA_CTRL, cmd | CTRL_PCR_SIGN);
        sc_core::wait(sc_core::sc_time(2, sc_core::SC_MS));
        tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS), ST_ERROR,
                    "PCR_SIGN with " + name + " parks with ERROR set and READY, VALID clear");
    }
    tb.check(tb.zeroize_mldsa(), "zeroize after the error cases");
    tb.wr(abr::OFF_ERROR_INTERNAL_INTR, 1u);
}
