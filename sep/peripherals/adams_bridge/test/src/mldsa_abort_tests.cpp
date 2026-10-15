// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file mldsa_abort_tests.cpp
 * @brief Zeroize and reset abort a running ML-DSA operation.
 *
 * Zeroize sends the RTL sequencer to its zeroize state from any state, clears the
 * engine's registers and memories, and returns it to RESET with READY set once the
 * memory sweep is done (abr_ctrl.sv:1617-1619, 1708-1712). Nothing the aborted
 * operation computed is published: VALID stays clear and no command-done event is
 * raised. Reset does the same through rst_ni.
 */

#include "abr_signer_vectors.h"
#include "abr_testbench.h"

#include <cci_configuration>

#include <string>
#include <vector>

namespace {

constexpr uint32_t CMD_KEYGEN       = 0x1u;
constexpr uint32_t CMD_SIGN         = 0x2u;
constexpr uint32_t CMD_KEYGEN_SIGN  = 0x4u;
constexpr uint32_t CMD_VERIFY       = 0x3u;
constexpr uint32_t CTRL_ZEROIZE     = 1u << 3;
constexpr uint32_t CTRL_EXTERNAL_MU = 1u << 5;
constexpr uint32_t CTRL_STREAM_MSG  = 1u << 6;

constexpr uint32_t ST_READY  = 1u << 0;
constexpr uint32_t ST_VALID  = 1u << 1;
constexpr uint32_t ST_STREAM = 1u << 2;

using bytes = std::vector<uint8_t>;

enum class abort_by { ZEROIZE, RESET };

const char *name(abort_by a)
{
    return a == abort_by::ZEROIZE ? "zeroize" : "reset";
}

bytes signer_pk()
{
    return bytes(abr_signer_pk, abr_signer_pk + sizeof(abr_signer_pk));
}

bytes genuine_msg()
{
    return bytes(abr_signer_cases[0].msg, abr_signer_cases[0].msg + abr_signer_cases[0].msg_len);
}

void load_genuine(abr_testbench &tb)
{
    const bytes pk = signer_pk();
    tb.wr_bytes(abr::OFF_MLDSA_PUBKEY, pk.data(), pk.size());
    tb.wr_bytes(abr::OFF_MLDSA_SIGNATURE, abr_signer_cases[0].sig, 4627u);
}

bool genuine_accepted(abr_testbench &tb)
{
    return tb.rd_bytes(abr::OFF_MLDSA_VERIFY_RES, 64u) ==
           bytes(abr_signer_cases[0].sig, abr_signer_cases[0].sig + 64u);
}

void do_abort(abr_testbench &tb, abort_by a)
{
    if (a == abort_by::ZEROIZE) {
        tb.wr(abr::OFF_MLDSA_CTRL, CTRL_ZEROIZE);
    } else {
        tb.reset_dut();
    }
}

/// After an abort: READY returns, nothing is published, then a clean verify passes.
void check_aborted(abr_testbench &tb, const std::string &what, uint32_t done_before)
{
    tb.check(tb.wait_mldsa_ready(), what + ": READY returns");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS), ST_READY,
                what + ": STATUS is READY alone (VALID and MSG_STREAM_READY clear)");
    tb.check_eq(tb.rd(abr::OFF_NOTIF_INTR_COUNT), done_before,
                what + ": the aborted operation raised no done event");
    tb.check(tb.rd_n(abr::OFF_MLDSA_VERIFY_RES, abr::N_VERIFY_RES) ==
                 std::vector<uint32_t>(abr::N_VERIFY_RES, 0u),
             what + ": VERIFY_RES cleared");

    load_genuine(tb);
    tb.check(tb.run_mldsa_stream(CMD_VERIFY, genuine_msg()), what + ": a clean verify completes");
    tb.check(genuine_accepted(tb), what + ": the clean verify accepts");
    tb.check(tb.zeroize_mldsa(), what + ": zeroize after the clean verify");
}

uint32_t done_count_after_clear(abr_testbench &tb, abort_by a)
{
    // Reset clears the event counter, zeroize does not; sample after the reset path
    // has had its effect so both compare against what the aborted operation saw.
    return a == abort_by::RESET ? 0u : tb.rd(abr::OFF_NOTIF_INTR_COUNT);
}

} // namespace

void mldsa_abort_tests(abr_testbench &tb)
{
    for (abort_by a : {abort_by::ZEROIZE, abort_by::RESET}) {
        const std::string by = name(a);

        tb.section("ML-DSA abort by " + by + ": during KEYGEN");
        tb.check(tb.zeroize_mldsa(), by + " KEYGEN: start from READY");
        uint32_t done_before = tb.rd(abr::OFF_NOTIF_INTR_COUNT);
        tb.wr_bytes(abr::OFF_MLDSA_SEED, abr_signer_seed, sizeof(abr_signer_seed));
        tb.wr(abr::OFF_MLDSA_CTRL, CMD_KEYGEN);
        sc_core::wait(sc_core::sc_time(20, sc_core::SC_US));
        tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & (ST_READY | ST_VALID), 0u,
                    by + " KEYGEN: still running when aborted");
        do_abort(tb, a);
        check_aborted(tb, by + " during KEYGEN", a == abort_by::RESET ? 0u : done_before);

        tb.section("ML-DSA abort by " + by + ": VERIFY waiting for MSG_STREAM_READY");
        done_before = done_count_after_clear(tb, a);
        load_genuine(tb);
        tb.wr(abr::OFF_MLDSA_CTRL, CMD_VERIFY | CTRL_STREAM_MSG);
        sc_core::wait(sc_core::sc_time(5, sc_core::SC_US));
        tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & ST_STREAM, 0u,
                    by + " pre-stream: the core has not asked for the message yet");
        do_abort(tb, a);
        check_aborted(tb, by + " before MSG_STREAM_READY", done_before);

        tb.section("ML-DSA abort by " + by + ": VERIFY mid-stream");
        done_before = done_count_after_clear(tb, a);
        load_genuine(tb);
        tb.wr(abr::OFF_MLDSA_CTRL, CMD_VERIFY | CTRL_STREAM_MSG);
        tb.check(tb.wait_stream_ready_or_valid(), by + " mid-stream: the core asks for the message");
        const bytes msg = genuine_msg();
        for (std::size_t w = 0; w < 737u; ++w) {
            uint32_t word = 0u;
            for (unsigned int lane = 0; lane < 4u; ++lane) {
                word |= static_cast<uint32_t>(msg[w * 4u + lane]) << (lane * 8u);
            }
            tb.wr(abr::OFF_MLDSA_MSG_STROBE, 0xFu);
            tb.wr(abr::OFF_MLDSA_MSG, word);
        }
        tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & ST_STREAM, ST_STREAM,
                    by + " mid-stream: still streaming after half the message");
        do_abort(tb, a);
        check_aborted(tb, by + " mid-stream", done_before);

        tb.section("ML-DSA abort by " + by + ": VERIFY computing after the last strobe");
        done_before = done_count_after_clear(tb, a);
        load_genuine(tb);
        tb.wr(abr::OFF_MLDSA_CTRL, CMD_VERIFY | CTRL_STREAM_MSG);
        tb.check(tb.wait_stream_ready_or_valid(), by + " computing: the core asks for the message");
        tb.stream_message(msg);
        tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & (ST_STREAM | ST_VALID | ST_READY), 0u,
                    by + " computing: stream closed, result not yet published");
        do_abort(tb, a);
        check_aborted(tb, by + " while computing", done_before);
    }

    tb.section("ML-DSA zeroize: SIGN and KEYGEN+SIGN phases");

    struct phase {
        const char *what;
        uint32_t ctrl;
        bool stream_first;
        int wait_us;
    };
    for (const phase &ph : {phase{"KEYGEN+SIGN during its keygen", CMD_KEYGEN_SIGN, false, 20},
                            phase{"SIGN waiting for its message", CMD_SIGN | CTRL_STREAM_MSG, false, 30},
                            phase{"SIGN computing the signature", CMD_SIGN | CTRL_STREAM_MSG, true, 50}}) {
        tb.check(tb.zeroize_mldsa(), std::string(ph.what) + ": start from READY");
        const uint32_t done_before = tb.rd(abr::OFF_NOTIF_INTR_COUNT);
        tb.wr_bytes(abr::OFF_MLDSA_SEED, abr_signer_seed, sizeof(abr_signer_seed));
        tb.wr(abr::OFF_MLDSA_CTRL, ph.ctrl);
        if (ph.stream_first) {
            tb.check(tb.wait_stream_ready_or_valid(), std::string(ph.what) + ": asked for the message");
            tb.stream_message({0x01u, 0x02u, 0x03u});
        }
        sc_core::wait(sc_core::sc_time(ph.wait_us, sc_core::SC_US));
        tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & (ST_READY | ST_VALID), 0u,
                    std::string(ph.what) + ": still running when aborted");
        tb.wr(abr::OFF_MLDSA_CTRL, CTRL_ZEROIZE);
        check_aborted(tb, std::string("zeroize ") + ph.what, done_before);
    }

    tb.section("ML-KEM zeroize during an ML-DSA operation");

    tb.check(tb.zeroize_mldsa(), "ML-KEM zeroize: start from READY");
    load_genuine(tb);
    {
        const uint32_t done_before = tb.rd(abr::OFF_NOTIF_INTR_COUNT);
        tb.wr(abr::OFF_MLDSA_CTRL, CMD_VERIFY | CTRL_STREAM_MSG);
        tb.check(tb.wait_stream_ready_or_valid(), "ML-KEM zeroize: the core asks for the message");
        tb.wr(abr::OFF_MLKEM_CTRL, CTRL_ZEROIZE);
        check_aborted(tb, "ML-KEM zeroize while ML-DSA waits for its message", done_before);
    }
    tb.check(tb.zeroize_mlkem(), "ML-KEM zeroize: ML-KEM returns to READY");

    tb.section("ML-DSA zeroize during a zeroize");

    tb.wr(abr::OFF_MLDSA_CTRL, CTRL_ZEROIZE);
    sc_core::wait(sc_core::sc_time(5, sc_core::SC_US));
    tb.wr(abr::OFF_MLDSA_CTRL, CTRL_ZEROIZE);
    tb.check(tb.wait_mldsa_ready(), "a zeroize during a zeroize still ends with READY");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS), ST_READY, "the restarted zeroize leaves READY alone");

    tb.section("ML-DSA zeroize latency");

    const int zc = tb.dut.zeroize_cycles.get_param_value();
    tb.check_eq(static_cast<uint32_t>(zc), 1538u, "zeroize_cycles defaults to the RTL memory sweep");
    tb.wr(abr::OFF_MLDSA_CTRL, CTRL_ZEROIZE);
    // 100 MHz: one cycle is 10 ns.
    sc_core::wait(sc_core::sc_time(10.0 * (zc - 16), sc_core::SC_NS));
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & ST_READY, 0u, "READY is still low before the sweep ends");
    sc_core::wait(sc_core::sc_time(10.0 * 32, sc_core::SC_NS));
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & ST_READY, ST_READY, "READY returns once the sweep ends");

    tb.section("ML-DSA zeroize while an external-mu VERIFY computes");

    tb.check(tb.zeroize_mldsa(), "external-mu abort: start from READY");
    load_genuine(tb);
    const abr::bytes mu = abr_testbench::expected_mu(signer_pk(), {}, genuine_msg());
    tb.wr_bytes(abr::OFF_MLDSA_EXTERNAL_MU, mu.data(), mu.size());
    const uint32_t done_before = tb.rd(abr::OFF_NOTIF_INTR_COUNT);
    tb.wr(abr::OFF_MLDSA_CTRL, CMD_VERIFY | CTRL_EXTERNAL_MU);
    sc_core::wait(sc_core::sc_time(50, sc_core::SC_US));
    tb.wr(abr::OFF_MLDSA_CTRL, CTRL_ZEROIZE);
    check_aborted(tb, "zeroize during an external-mu VERIFY", done_before);
}
