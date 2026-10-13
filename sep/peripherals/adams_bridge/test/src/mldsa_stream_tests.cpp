// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file mldsa_stream_tests.cpp
 * @brief The ML-DSA streamed-message handshake follows the RTL.
 *
 * With CTRL.STREAM_MSG set, the RTL raises MSG_STREAM_READY only once the
 * sequencer reaches the message hash: after the command, and for VERIFY after the
 * z range check. The streamed message is 0 || |ctx| || ctx followed by the bytes
 * written to MSG[0] while MSG_STREAM_READY is set, each word filtered by
 * MSG_STROBE; the first write whose strobe is not 0xF ends the stream
 * (abr_ctrl.sv:1838-1900). MSG and MSG_STROBE writes outside that window are
 * stored or dropped but never streamed, MSG_STROBE resets to 0xF and zeroize
 * clears it to 0.
 */

#include "abr_acvp_sigver_all.h"
#include "abr_signer_vectors.h"
#include "abr_testbench.h"

#include <cci_configuration>

#include <string>
#include <vector>

namespace {

constexpr uint32_t CMD_KEYGEN       = 0x1u;
constexpr uint32_t CMD_SIGN         = 0x2u;
constexpr uint32_t CMD_VERIFY       = 0x3u;
constexpr uint32_t CMD_KEYGEN_SIGN  = 0x4u;
constexpr uint32_t CTRL_EXTERNAL_MU = 1u << 5;
constexpr uint32_t CTRL_STREAM_MSG  = 1u << 6;

constexpr uint32_t ST_READY  = 1u << 0;
constexpr uint32_t ST_STREAM = 1u << 2;

constexpr std::size_t SIG_BYTES = 4627u;

using bytes = std::vector<uint8_t>;

bytes signer_pk()
{
    return bytes(abr_signer_pk, abr_signer_pk + sizeof(abr_signer_pk));
}

bytes signer_msg(std::size_t i)
{
    return bytes(abr_signer_cases[i].msg, abr_signer_cases[i].msg + abr_signer_cases[i].msg_len);
}

bytes signer_sig(std::size_t i)
{
    return bytes(abr_signer_cases[i].sig, abr_signer_cases[i].sig + SIG_BYTES);
}

bytes pattern(std::size_t n, uint8_t salt)
{
    bytes out(n);
    for (std::size_t i = 0; i < n; ++i) {
        out[i] = static_cast<uint8_t>(i * 13u + salt);
    }
    return out;
}

bytes from_hex(const char *hex)
{
    bytes out;
    for (const char *p = hex; p[0] != '\0' && p[1] != '\0'; p += 2) {
        out.push_back(static_cast<uint8_t>(std::stoul(std::string(p, 2), nullptr, 16)));
    }
    return out;
}

bytes inverted(bytes v)
{
    for (auto &b : v) {
        b = static_cast<uint8_t>(~b);
    }
    return v;
}

void load_verify(abr_testbench &tb, const bytes &pk, const bytes &sig, const bytes &ctx)
{
    tb.check(tb.zeroize_mldsa(), "zeroize before loading a streamed verify");
    tb.wr_bytes(abr::OFF_MLDSA_PUBKEY, pk.data(), pk.size());
    tb.wr_bytes(abr::OFF_MLDSA_SIGNATURE, sig.data(), sig.size());
    tb.load_ctx(ctx);
}

bytes verify_res(abr_testbench &tb)
{
    return tb.rd_bytes(abr::OFF_MLDSA_VERIFY_RES, 64u);
}

/// Wait for MSG_STREAM_READY, write MSG[0] words with no MSG_STROBE writes, then VALID.
bool stream_without_strobe(abr_testbench &tb, const std::vector<uint32_t> &words)
{
    if (!tb.wait_stream_ready_or_valid()) {
        return false;
    }
    for (uint32_t w : words) {
        tb.wr(abr::OFF_MLDSA_MSG, w);
    }
    return tb.wait_mldsa_valid();
}

void set_int_param(regmodel::Param<int> &p, int value)
{
    auto broker = cci::cci_get_broker();
    cci::cci_param_handle h = broker.get_param_handle(p.get_Name());
    if (h.is_valid()) {
        h.set_cci_value(cci::cci_value(value));
    }
}

} // namespace

void mldsa_stream_tests(abr_testbench &tb)
{
    const bytes pk = signer_pk();
    const bytes genuine = signer_sig(0);
    const bytes genuine_ct(genuine.begin(), genuine.begin() + 64);

    tb.section("ML-DSA stream: idle and protocol order");

    tb.check(tb.zeroize_mldsa(), "zeroize before the stream cases");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS), ST_READY, "an idle core does not ask for a message");

    load_verify(tb, pk, genuine, {});
    tb.wr(abr::OFF_MLDSA_MSG_STROBE, 0xFu);
    for (unsigned int i = 0; i < abr::N_MLDSA_MSG; ++i) {
        tb.wr(abr::OFF_MLDSA_MSG, 0xBAD00000u + i);
    }
    tb.check(tb.run_mldsa_stream(CMD_VERIFY, signer_msg(0)),
             "VERIFY streams after MSG writes made while READY");
    tb.check(verify_res(tb) == genuine_ct, "MSG writes made before the command are not streamed");

    // CTRL is written whole, so a command written without STREAM_MSG clears a
    // STREAM_MSG written on its own before it.
    load_verify(tb, pk, genuine, {});
    tb.wr(abr::OFF_MLDSA_CTRL, CTRL_STREAM_MSG);
    tb.wr(abr::OFF_MLDSA_CTRL, CMD_VERIFY);
    tb.check(!tb.wait_stream_ready_or_valid(),
             "a command written without STREAM_MSG does not stream");

    load_verify(tb, pk, genuine, {});
    tb.wr(abr::OFF_MLDSA_CTRL, CMD_VERIFY | CTRL_STREAM_MSG);
    tb.check(tb.wait_stream_ready_or_valid(), "the core asks for the message");
    const bytes msg0 = signer_msg(0);
    for (std::size_t w = 0; w + 4u <= msg0.size(); w += 4u) {
        uint32_t word = 0u;
        for (unsigned int lane = 0; lane < 4u; ++lane) {
            word |= static_cast<uint32_t>(msg0[w + lane]) << (lane * 8u);
        }
        tb.wr(abr::OFF_MLDSA_MSG_STROBE, 0xFu);
        tb.wr(abr::OFF_MLDSA_MSG + 4u, 0xDEADBEEFu);
        tb.wr(abr::OFF_MLDSA_MSG, word);
    }
    uint32_t tail = 0u;
    for (std::size_t lane = 0; lane < msg0.size() % 4u; ++lane) {
        tail |= static_cast<uint32_t>(msg0[msg0.size() - msg0.size() % 4u + lane]) << (lane * 8u);
    }
    tb.wr(abr::OFF_MLDSA_MSG_STROBE, (1u << (msg0.size() % 4u)) - 1u);
    tb.wr(abr::OFF_MLDSA_MSG, tail);
    tb.check(tb.wait_mldsa_valid(), "the streamed VERIFY completes");
    tb.check(verify_res(tb) == genuine_ct, "only MSG[0] writes are streamed; MSG[1..15] are ignored");

    tb.section("ML-DSA stream: MSG_STROBE reset and zeroize values");

    // Reset leaves MSG_STROBE at 0xF, so MSG[0] writes stream whole words without
    // any strobe write; 5900 bytes then need the strobe-0x0 dummy word to end.
    tb.reset_dut();
    tb.wr_bytes(abr::OFF_MLDSA_PUBKEY, pk.data(), pk.size());
    const bytes sig3 = signer_sig(3);
    tb.wr_bytes(abr::OFF_MLDSA_SIGNATURE, sig3.data(), sig3.size());
    tb.wr(abr::OFF_MLDSA_CTRL, CMD_VERIFY | CTRL_STREAM_MSG);
    tb.check(tb.wait_stream_ready_or_valid(), "after reset the core asks for the message");
    const bytes msg3 = signer_msg(3);
    for (std::size_t w = 0; w < msg3.size(); w += 4u) {
        uint32_t word = 0u;
        for (unsigned int lane = 0; lane < 4u; ++lane) {
            word |= static_cast<uint32_t>(msg3[w + lane]) << (lane * 8u);
        }
        tb.wr(abr::OFF_MLDSA_MSG, word);
    }
    tb.wr(abr::OFF_MLDSA_MSG_STROBE, 0u);
    tb.wr(abr::OFF_MLDSA_MSG, 0u);
    tb.check(tb.wait_mldsa_valid(), "the reset-strobe stream completes");
    tb.check(verify_res(tb) == bytes(sig3.begin(), sig3.begin() + 64),
             "MSG_STROBE resets to 0xF");

    // Zeroize clears MSG_STROBE to 0, so the first MSG[0] write ends an empty stream.
    const bytes sig5 = signer_sig(5);
    load_verify(tb, pk, sig5, {});
    tb.wr(abr::OFF_MLDSA_CTRL, CMD_VERIFY | CTRL_STREAM_MSG);
    tb.check(stream_without_strobe(tb, {0x12345678u}), "the zeroize-strobe stream completes");
    tb.check(verify_res(tb) == bytes(sig5.begin(), sig5.begin() + 64),
             "zeroize clears MSG_STROBE to 0");

    // A strobe written while READY is dropped, so the stream is still empty.
    load_verify(tb, pk, sig5, {});
    tb.wr(abr::OFF_MLDSA_MSG_STROBE, 0xFu);
    tb.wr(abr::OFF_MLDSA_CTRL, CMD_VERIFY | CTRL_STREAM_MSG);
    tb.check(stream_without_strobe(tb, {0x12345678u}), "the dropped-strobe stream completes");
    tb.check(verify_res(tb) == bytes(sig5.begin(), sig5.begin() + 64),
             "MSG_STROBE written outside the stream window is dropped");

    tb.section("ML-DSA stream: z and hint checks around the stream");

    bytes bad_z = genuine;
    bad_z[64] = 120u; // z[0][0] = gamma1 - beta
    bad_z[65] = 0u;
    bad_z[66] = static_cast<uint8_t>(bad_z[66] & 0xF0u);
    load_verify(tb, pk, bad_z, {});
    tb.wr(abr::OFF_MLDSA_CTRL, CMD_VERIFY | CTRL_STREAM_MSG);
    tb.check(!tb.wait_stream_ready_or_valid(),
             "z out of range: VALID without MSG_STREAM_READY ever rising");
    tb.check(verify_res(tb) == inverted(genuine_ct), "z out of range: VERIFY_RES keeps ~c~");

    bytes bad_hint = genuine;
    bad_hint[4627u - 1u] = 76u; // last hint count above omega
    load_verify(tb, pk, bad_hint, {});
    tb.check(tb.run_mldsa_stream(CMD_VERIFY, signer_msg(0)),
             "malformed hint: the core still asks for and consumes the message");
    tb.check(verify_res(tb) == inverted(genuine_ct), "malformed hint: VERIFY_RES keeps ~c~");

    tb.section("ML-DSA stream: EXTERNAL_MU never streams");

    load_verify(tb, pk, genuine, {});
    const abr::bytes mu = abr_testbench::expected_mu(pk, {}, signer_msg(0));
    tb.wr_bytes(abr::OFF_MLDSA_EXTERNAL_MU, mu.data(), mu.size());
    tb.wr(abr::OFF_MLDSA_CTRL, CMD_VERIFY | CTRL_EXTERNAL_MU | CTRL_STREAM_MSG);
    tb.check(!tb.wait_stream_ready_or_valid(), "EXTERNAL_MU with STREAM_MSG completes without a stream");
    tb.check(verify_res(tb) == genuine_ct, "EXTERNAL_MU with STREAM_MSG verifies from mu");

    tb.section("ML-DSA stream: every final-word strobe and context length");

    tb.check(tb.zeroize_mldsa(), "zeroize before the matrix keygen");
    tb.wr_bytes(abr::OFF_MLDSA_SEED, abr_signer_seed, sizeof(abr_signer_seed));
    tb.check(tb.run_mldsa(CMD_KEYGEN), "keygen for the matrix");
    const std::vector<uint32_t> sk = tb.rd_n(abr::OFF_MLDSA_PRIVKEY_OUT, abr::N_MLDSA_PRIVKEY);

    bytes keygen_sign_sig;
    for (std::size_t mlen : {0u, 1u, 2u, 3u, 4u, 5u, 63u, 64u, 65u, 5900u, 5901u, 5902u, 5903u}) {
        for (std::size_t clen : {0u, 1u, 3u, 4u, 5u, 255u}) {
            const bytes msg = pattern(mlen, static_cast<uint8_t>(clen));
            const bytes ctx = pattern(clen, 0xA0u);
            const std::string what = "msg " + std::to_string(mlen) + " B, ctx " + std::to_string(clen) + " B";

            tb.check(tb.zeroize_mldsa(), what + ": zeroize before streamed SIGN");
            tb.wr_n(abr::OFF_MLDSA_PRIVKEY_IN, sk);
            tb.load_ctx(ctx);
            tb.check(tb.run_mldsa_stream(CMD_SIGN, msg), what + ": streamed SIGN completes");
            const bytes streamed = tb.rd_bytes(abr::OFF_MLDSA_SIGNATURE, SIG_BYTES);

            tb.check(tb.zeroize_mldsa(), what + ": zeroize before external-mu SIGN");
            tb.wr_n(abr::OFF_MLDSA_PRIVKEY_IN, sk);
            const abr::bytes m = abr_testbench::expected_mu(pk, ctx, msg);
            tb.wr_bytes(abr::OFF_MLDSA_EXTERNAL_MU, m.data(), m.size());
            tb.check(tb.run_mldsa(CMD_SIGN | CTRL_EXTERNAL_MU), what + ": external-mu SIGN completes");
            tb.check(tb.rd_bytes(abr::OFF_MLDSA_SIGNATURE, SIG_BYTES) == streamed,
                     what + ": streamed SIGN equals SIGN over the independent mu");

            load_verify(tb, pk, streamed, ctx);
            tb.check(tb.run_mldsa_stream(CMD_VERIFY, msg), what + ": streamed VERIFY completes");
            tb.check(verify_res(tb) == bytes(streamed.begin(), streamed.begin() + 64),
                     what + ": streamed VERIFY accepts");

            if (mlen == 5903u && clen == 0u) {
                keygen_sign_sig = streamed;
            }
        }
    }

    tb.check(tb.zeroize_mldsa(), "zeroize before streamed KEYGEN+SIGN");
    tb.wr_bytes(abr::OFF_MLDSA_SEED, abr_signer_seed, sizeof(abr_signer_seed));
    tb.check(tb.run_mldsa_stream(CMD_KEYGEN_SIGN, pattern(5903u, 0u)),
             "KEYGEN+SIGN streams its message after the keygen");
    tb.check(tb.rd_bytes(abr::OFF_MLDSA_SIGNATURE, SIG_BYTES) == keygen_sign_sig,
             "streamed KEYGEN+SIGN equals the separate streamed SIGN");

    tb.section("ML-DSA stream: ACVP pure sigVer cases with their contexts");

    for (const abr_acvp_case &c : abr_acvp_cases) {
        if (c.external_mu) {
            continue;
        }
        const bytes sig = from_hex(c.sig);
        const bytes ct(sig.begin(), sig.begin() + 64);
        load_verify(tb, from_hex(c.pk), sig, from_hex(c.ctx));
        const std::string what = std::string(c.origin) + " streamed";
        tb.check(tb.run_mldsa_stream(CMD_VERIFY, from_hex(c.msg)), what + ": completes");
        const bytes res = verify_res(tb);
        if (c.kind == ABR_ACVP_ACCEPT) {
            tb.check(res == ct, what + ": accepts");
        } else if (c.kind == ABR_ACVP_HINT_REJECT || c.kind == ABR_ACVP_Z_REJECT) {
            tb.check(res == inverted(ct), what + ": keeps ~c~");
        } else {
            tb.check(res != ct && res != inverted(ct), what + ": computes a non-matching c~'");
        }
    }

    tb.section("ML-DSA stream: message length cap");

    const int saved_cap = tb.dut.max_msg_stream_bytes.get_param_value();
    set_int_param(tb.dut.max_msg_stream_bytes, 8);
    const bytes sig4 = signer_sig(4);
    load_verify(tb, pk, sig4, {});
    tb.check(tb.run_mldsa_stream(CMD_VERIFY, pattern(16u, 0u)),
             "a stream longer than the cap still completes");
    tb.check(verify_res(tb) != bytes(sig4.begin(), sig4.begin() + 64),
             "bytes past the cap are dropped, so the result does not match");
    set_int_param(tb.dut.max_msg_stream_bytes, saved_cap);
    tb.check(tb.zeroize_mldsa(), "zeroize after the stream cases");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & ST_STREAM, 0u, "zeroize leaves MSG_STREAM_READY clear");
}
