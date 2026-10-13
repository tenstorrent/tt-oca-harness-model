// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file mldsa_tests.cpp
 * @brief ML-DSA behavioural tests: keygen, sign, verify, the modifier flags and
 *        the park-on-VALID completion contract firmware relies on.
 */

#include "abr_testbench.h"

#include <iostream>
#include <string>

namespace {

constexpr uint32_t CMD_KEYGEN      = 0x1u;
constexpr uint32_t CMD_SIGN        = 0x2u;
constexpr uint32_t CMD_VERIFY      = 0x3u;
constexpr uint32_t CMD_KEYGEN_SIGN = 0x4u;

constexpr uint32_t CTRL_ZEROIZE     = 1u << 3;
constexpr uint32_t CTRL_PCR_SIGN    = 1u << 4;
constexpr uint32_t CTRL_EXTERNAL_MU = 1u << 5;
constexpr uint32_t CTRL_STREAM_MSG  = 1u << 6;

constexpr uint32_t ST_READY = 1u << 0;
constexpr uint32_t ST_VALID = 1u << 1;
constexpr uint32_t ST_ERROR = 1u << 3;

/// Deterministic inputs, matching the style of the SEP firmware KAT tests.
const std::vector<uint32_t> k_seed = {
    0x00010203u, 0x04050607u, 0x08090a0bu, 0x0c0d0e0fu,
    0x10111213u, 0x14151617u, 0x18191a1bu, 0x1c1d1e1fu};

const std::vector<uint32_t> k_entropy = {
    0xa5a5a5a5u, 0x5a5a5a5au, 0xdeadbeefu, 0xcafef00du,
    0x01234567u, 0x89abcdefu, 0xfedcba98u, 0x76543210u,
    0x0f0f0f0fu, 0xf0f0f0f0u, 0x33333333u, 0xccccccccu,
    0xa5a5a5a5u, 0x5a5a5a5au, 0xdeadc0deu, 0xfeedfaceu};

const std::vector<uint32_t> k_msg = {
    0x41425220u, 0x4b415420u, 0x6d736720u, 0x76303031u,
    0x11223344u, 0x55667788u, 0x99aabbccu, 0xddeeff00u,
    0x0badf00du, 0x8badf00du, 0xfaceb00cu, 0x1ceb00dau,
    0x00000000u, 0x11111111u, 0x22222222u, 0x33333333u};

const std::vector<uint32_t> k_sign_rnd(8, 0u);

bool any_nonzero(const std::vector<uint32_t> &v)
{
    for (uint32_t w : v) {
        if (w != 0u) {
            return true;
        }
    }
    return false;
}

/// Load the standard ML-DSA inputs.
void load_inputs(abr_testbench &tb)
{
    tb.wr_n(abr::OFF_MLDSA_SEED, k_seed);
    tb.wr_n(abr::OFF_ABR_ENTROPY, k_entropy);
    tb.wr_n(abr::OFF_MLDSA_MSG, k_msg);
    tb.wr_n(abr::OFF_MLDSA_SIGN_RND, k_sign_rnd);
}

} // namespace

void mldsa_tests(abr_testbench &tb)
{
    tb.section("ML-DSA: keygen");

    load_inputs(tb);
    tb.check(tb.run_mldsa(CMD_KEYGEN), "KEYGEN completes (VALID asserted)");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & ST_ERROR, 0u, "KEYGEN leaves ERROR clear");

    // The sequencer parks at the end state: READY must NOT come back on its own.
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & ST_READY, 0u,
                "KEYGEN parks with READY low (zeroize required)");

    const std::vector<uint32_t> pubkey = tb.rd_n(abr::OFF_MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY);
    const std::vector<uint32_t> privkey =
        tb.rd_n(abr::OFF_MLDSA_PRIVKEY_OUT, abr::N_MLDSA_PRIVKEY);

    tb.check(any_nonzero(pubkey), "KEYGEN produced a non-zero public key");
    tb.check(any_nonzero(privkey), "KEYGEN produced a non-zero private key");

    tb.section("ML-DSA: keygen determinism");

    tb.check(tb.zeroize_mldsa(), "zeroize after KEYGEN restores READY");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & ST_VALID, 0u, "zeroize cleared VALID");

    load_inputs(tb);
    tb.check(tb.run_mldsa(CMD_KEYGEN), "second KEYGEN with same seed completes");
    tb.check(tb.rd_n(abr::OFF_MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY) == pubkey,
             "same seed reproduces the same public key");

    tb.section("ML-DSA: sign");

    tb.check(tb.zeroize_mldsa(), "zeroize before SIGN");
    tb.wr_n(abr::OFF_MLDSA_PRIVKEY_IN, privkey);
    tb.wr_n(abr::OFF_MLDSA_MSG, k_msg);
    tb.wr_n(abr::OFF_MLDSA_SIGN_RND, k_sign_rnd);

    tb.check(tb.run_mldsa(CMD_SIGN), "SIGN completes");
    const std::vector<uint32_t> signature =
        tb.rd_n(abr::OFF_MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE);
    tb.check(any_nonzero(signature), "SIGN produced a non-zero signature");

    tb.section("ML-DSA: verify accepts a valid signature");

    tb.check(tb.zeroize_mldsa(), "zeroize before VERIFY");
    tb.wr_n(abr::OFF_MLDSA_PUBKEY, pubkey);
    tb.wr_n(abr::OFF_MLDSA_SIGNATURE, signature);
    tb.wr_n(abr::OFF_MLDSA_MSG, k_msg);

    tb.check(tb.run_mldsa(CMD_VERIFY), "VERIFY completes");

    // Hardware reports the recomputed c-tilde; firmware compares it against the
    // first 16 words of the signature.
    std::vector<uint32_t> verify_res = tb.rd_n(abr::OFF_MLDSA_VERIFY_RES, abr::N_VERIFY_RES);
    std::vector<uint32_t> sig_ctilde(signature.begin(), signature.begin() + abr::N_VERIFY_RES);
    tb.check(verify_res == sig_ctilde, "VERIFY_RES matches signature c-tilde (valid sig)");

    tb.section("ML-DSA: verify rejects a tampered signature");

    tb.check(tb.zeroize_mldsa(), "zeroize before tampered VERIFY");
    std::vector<uint32_t> bad_signature = signature;
    bad_signature[100] ^= 0xFFFFFFFFu; // corrupt the signature body
    tb.wr_n(abr::OFF_MLDSA_PUBKEY, pubkey);
    tb.wr_n(abr::OFF_MLDSA_SIGNATURE, bad_signature);
    tb.wr_n(abr::OFF_MLDSA_MSG, k_msg);

    tb.check(tb.run_mldsa(CMD_VERIFY), "VERIFY completes on a tampered signature");
    verify_res = tb.rd_n(abr::OFF_MLDSA_VERIFY_RES, abr::N_VERIFY_RES);
    sig_ctilde.assign(bad_signature.begin(), bad_signature.begin() + abr::N_VERIFY_RES);
    tb.check(verify_res != sig_ctilde, "VERIFY_RES mismatches for a tampered signature");

    tb.section("ML-DSA: verify rejects a wrong message");

    tb.check(tb.zeroize_mldsa(), "zeroize before wrong-message VERIFY");
    std::vector<uint32_t> other_msg = k_msg;
    other_msg[0] ^= 0x1u;
    tb.wr_n(abr::OFF_MLDSA_PUBKEY, pubkey);
    tb.wr_n(abr::OFF_MLDSA_SIGNATURE, signature);
    tb.wr_n(abr::OFF_MLDSA_MSG, other_msg);

    tb.check(tb.run_mldsa(CMD_VERIFY), "VERIFY completes on a wrong message");
    verify_res = tb.rd_n(abr::OFF_MLDSA_VERIFY_RES, abr::N_VERIFY_RES);
    sig_ctilde.assign(signature.begin(), signature.begin() + abr::N_VERIFY_RES);
    tb.check(verify_res != sig_ctilde, "VERIFY_RES mismatches for a wrong message");

    tb.section("ML-DSA: KEYGEN+SIGN fused command");

    tb.check(tb.zeroize_mldsa(), "zeroize before KEYGEN+SIGN");
    load_inputs(tb);
    tb.check(tb.run_mldsa(CMD_KEYGEN_SIGN), "KEYGEN+SIGN completes");

    const std::vector<uint32_t> fused_pubkey =
        tb.rd_n(abr::OFF_MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY);
    const std::vector<uint32_t> fused_sig =
        tb.rd_n(abr::OFF_MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE);
    tb.check(fused_pubkey == pubkey, "KEYGEN+SIGN regenerated the same public key");
    tb.check(fused_sig == signature, "KEYGEN+SIGN matches the separate keygen+sign result");

    tb.section("ML-DSA: signature randomizer");

    tb.check(tb.zeroize_mldsa(), "zeroize before randomized SIGN");
    tb.wr_n(abr::OFF_MLDSA_PRIVKEY_IN, privkey);
    tb.wr_n(abr::OFF_MLDSA_MSG, k_msg);
    tb.wr_n(abr::OFF_MLDSA_SIGN_RND, std::vector<uint32_t>(8, 0xA5A5A5A5u));

    tb.check(tb.run_mldsa(CMD_SIGN), "randomized SIGN completes");
    const std::vector<uint32_t> rnd_sig =
        tb.rd_n(abr::OFF_MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE);
    tb.check(rnd_sig != signature, "a different SIGN_RND yields a different signature");

    // A randomized signature must still verify against the same public key.
    tb.check(tb.zeroize_mldsa(), "zeroize before verifying randomized signature");
    tb.wr_n(abr::OFF_MLDSA_PUBKEY, pubkey);
    tb.wr_n(abr::OFF_MLDSA_SIGNATURE, rnd_sig);
    tb.wr_n(abr::OFF_MLDSA_MSG, k_msg);
    tb.check(tb.run_mldsa(CMD_VERIFY), "VERIFY of randomized signature completes");
    verify_res = tb.rd_n(abr::OFF_MLDSA_VERIFY_RES, abr::N_VERIFY_RES);
    sig_ctilde.assign(rnd_sig.begin(), rnd_sig.begin() + abr::N_VERIFY_RES);
    tb.check(verify_res == sig_ctilde, "randomized signature verifies");

    tb.section("ML-DSA: EXTERNAL_MU");

    tb.check(tb.zeroize_mldsa(), "zeroize before EXTERNAL_MU sign");
    const std::vector<uint32_t> ext_mu(abr::N_EXTERNAL_MU, 0x5A5A5A5Au);
    tb.wr_n(abr::OFF_MLDSA_PRIVKEY_IN, privkey);
    tb.wr_n(abr::OFF_MLDSA_EXTERNAL_MU, ext_mu);
    tb.wr_n(abr::OFF_MLDSA_SIGN_RND, k_sign_rnd);

    tb.check(tb.run_mldsa(CMD_SIGN | CTRL_EXTERNAL_MU), "SIGN with EXTERNAL_MU completes");
    const std::vector<uint32_t> extmu_sig =
        tb.rd_n(abr::OFF_MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE);
    tb.check(extmu_sig != signature,
             "EXTERNAL_MU produces a different signature than message-derived mu");

    tb.check(tb.zeroize_mldsa(), "zeroize before EXTERNAL_MU verify");
    tb.wr_n(abr::OFF_MLDSA_PUBKEY, pubkey);
    tb.wr_n(abr::OFF_MLDSA_SIGNATURE, extmu_sig);
    tb.wr_n(abr::OFF_MLDSA_EXTERNAL_MU, ext_mu);
    tb.check(tb.run_mldsa(CMD_VERIFY | CTRL_EXTERNAL_MU),
             "VERIFY with EXTERNAL_MU completes");
    verify_res = tb.rd_n(abr::OFF_MLDSA_VERIFY_RES, abr::N_VERIFY_RES);
    sig_ctilde.assign(extmu_sig.begin(), extmu_sig.begin() + abr::N_VERIFY_RES);
    tb.check(verify_res == sig_ctilde, "EXTERNAL_MU signature verifies");

    tb.section("ML-DSA: context is ignored without streaming");

    // A non-streamed message is hashed as 0 || 0 || MSG, whatever CTX_CONFIG says
    // (abr_ctrl.sv msg_p_reg); the context only enters a streamed message.
    tb.check(tb.zeroize_mldsa(), "zeroize before context sign");
    tb.wr_n(abr::OFF_MLDSA_PRIVKEY_IN, privkey);
    tb.wr_n(abr::OFF_MLDSA_MSG, k_msg);
    tb.wr_n(abr::OFF_MLDSA_SIGN_RND, k_sign_rnd);
    tb.wr(abr::OFF_MLDSA_CTX_CONFIG, 8u);
    tb.wr_n(abr::OFF_MLDSA_CTX, {0xDEADBEEFu, 0xCAFEF00Du});
    tb.check(tb.run_mldsa(CMD_SIGN), "non-streamed SIGN with CTX_CONFIG set completes");
    tb.check(tb.rd_n(abr::OFF_MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE) == signature,
             "non-streamed SIGN ignores the context");

    tb.check(tb.zeroize_mldsa(), "zeroize before context verify");
    tb.wr_n(abr::OFF_MLDSA_PUBKEY, pubkey);
    tb.wr_n(abr::OFF_MLDSA_SIGNATURE, signature);
    tb.wr_n(abr::OFF_MLDSA_MSG, k_msg);
    tb.wr(abr::OFF_MLDSA_CTX_CONFIG, 8u);
    tb.wr_n(abr::OFF_MLDSA_CTX, {0xDEADBEEFu, 0xCAFEF00Du});
    tb.check(tb.run_mldsa(CMD_VERIFY), "non-streamed VERIFY with CTX_CONFIG set completes");
    verify_res = tb.rd_n(abr::OFF_MLDSA_VERIFY_RES, abr::N_VERIFY_RES);
    sig_ctilde.assign(signature.begin(), signature.begin() + abr::N_VERIFY_RES);
    tb.check(verify_res == sig_ctilde, "non-streamed VERIFY ignores the context");

    tb.section("ML-DSA: streamed message");

    // The 64-byte MSG block streamed through MSG[0] after the command gives the
    // same signature as the non-streamed block.
    tb.check(tb.zeroize_mldsa(), "zeroize before streamed sign");
    tb.wr_n(abr::OFF_MLDSA_PRIVKEY_IN, privkey);
    tb.wr_n(abr::OFF_MLDSA_SIGN_RND, k_sign_rnd);
    std::vector<uint8_t> msg_bytes;
    for (uint32_t word : k_msg) {
        for (unsigned int lane = 0; lane < 4u; ++lane) {
            msg_bytes.push_back(static_cast<uint8_t>(word >> (lane * 8u)));
        }
    }
    tb.check(tb.run_mldsa_stream(CMD_SIGN, msg_bytes), "SIGN with a streamed message completes");
    tb.check(tb.rd_n(abr::OFF_MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE) == signature,
             "streamed MSG bytes match the non-streamed signature");

    tb.section("ML-DSA: partial-word strobe");

    // Strobe 0x3 ends the stream after two bytes; one strobe-0x1 write followed by
    // another ends it after one, so the second form needs a full word first.
    tb.check(tb.zeroize_mldsa(), "zeroize before partial-strobe sign");
    tb.wr_n(abr::OFF_MLDSA_PRIVKEY_IN, privkey);
    tb.wr_n(abr::OFF_MLDSA_SIGN_RND, k_sign_rnd);
    tb.check(tb.run_mldsa_stream(CMD_SIGN, {0xDDu, 0xCCu}), "SIGN of a 2-byte stream completes");
    const std::vector<uint32_t> partial_sig =
        tb.rd_n(abr::OFF_MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE);

    tb.check(tb.zeroize_mldsa(), "zeroize before the raw strobe-0x3 stream");
    tb.wr_n(abr::OFF_MLDSA_PRIVKEY_IN, privkey);
    tb.wr_n(abr::OFF_MLDSA_SIGN_RND, k_sign_rnd);
    tb.wr(abr::OFF_MLDSA_CTRL, CMD_SIGN | CTRL_STREAM_MSG);
    tb.check(tb.wait_stream_ready_or_valid(), "the core asks for the message");
    tb.wr(abr::OFF_MLDSA_MSG_STROBE, 0x3u);
    tb.wr(abr::OFF_MLDSA_MSG, 0xAABBCCDDu);
    tb.check(tb.wait_mldsa_valid(), "strobe 0x3 ends the stream");
    tb.check(tb.rd_n(abr::OFF_MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE) == partial_sig,
             "disabled strobe lanes are excluded from the message");

    tb.section("ML-DSA: PCR_SIGN modifier");

    // PCR_SIGN is only legal with KEYGEN+SIGN; with KEYGEN, SIGN or VERIFY the RTL
    // raises its error flag (mldsa_error_tests). The VP has no PCR bank, so the
    // fused command signs the installed digest or, without one, MSG.
    tb.check(tb.zeroize_mldsa(), "zeroize before PCR KEYGEN+SIGN");
    load_inputs(tb);
    tb.check(tb.run_mldsa(CMD_KEYGEN_SIGN | CTRL_PCR_SIGN), "KEYGEN+SIGN with PCR_SIGN completes");
    const std::vector<uint32_t> pcr_fallback_sig =
        tb.rd_n(abr::OFF_MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE);
    tb.check(pcr_fallback_sig == fused_sig, "PCR_SIGN without a digest falls back to MSG");

    abr::bytes pcr_digest(abr::N_MLDSA_MSG * 4u, 0x5Au);
    tb.dut.set_pcr_digest(pcr_digest);
    tb.check(tb.zeroize_mldsa(), "zeroize before PCR digest sign");
    load_inputs(tb);
    tb.check(tb.run_mldsa(CMD_KEYGEN_SIGN | CTRL_PCR_SIGN), "KEYGEN+SIGN with a PCR digest completes");
    tb.check(tb.rd_n(abr::OFF_MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE) != fused_sig,
             "PCR_SIGN with a digest diverges from MSG");
    tb.dut.set_pcr_digest({});

    tb.section("ML-DSA: invalid commands are no-ops");

    for (uint32_t cmd = 5u; cmd <= 7u; ++cmd) {
        tb.check(tb.zeroize_mldsa(), "zeroize before invalid command " + std::to_string(cmd));
        const uint32_t err_count_before = tb.rd(abr::OFF_ERROR_INTR_COUNT);
        const uint32_t done_count_before = tb.rd(abr::OFF_NOTIF_INTR_COUNT);
        tb.wr(abr::OFF_MLDSA_CTRL, cmd);
        sc_core::wait(sc_core::sc_time(10, sc_core::SC_US));
        tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS), ST_READY,
                    "command " + std::to_string(cmd) + " leaves the core READY and idle");
        tb.check_eq(tb.rd(abr::OFF_ERROR_INTR_COUNT), err_count_before,
                    "command " + std::to_string(cmd) + " raises no error event");
        tb.check_eq(tb.rd(abr::OFF_NOTIF_INTR_COUNT), done_count_before,
                    "command " + std::to_string(cmd) + " raises no done event");
    }

    // CTRL=0 is a no-op: the engine must stay ready and idle.
    tb.wr(abr::OFF_MLDSA_CTRL, 0u);
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & ST_READY, ST_READY,
                "CTRL=NONE leaves the engine ready");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & ST_VALID, 0u,
                "CTRL=NONE does not assert VALID");

    tb.section("ML-DSA: writes are dropped while the core is busy");

    // Every ML-DSA input is written only while READY (abr_reg.rdl swwe = abr_ready).
    // Each case overwrites an input right after the command is accepted and checks
    // that the operation still used the value loaded before it.
    tb.check(tb.zeroize_mldsa(), "zeroize before busy-write KEYGEN");
    load_inputs(tb);
    tb.wr(abr::OFF_MLDSA_CTRL, CMD_KEYGEN);
    tb.wr_n(abr::OFF_MLDSA_SEED, std::vector<uint32_t>(abr::N_MLDSA_SEED, 0x77777777u));
    tb.wr(abr::OFF_KV_MLDSA_SEED_RD_CTRL, 0x1u);
    tb.check(tb.wait_mldsa_valid(), "KEYGEN with busy writes completes");
    tb.check(tb.rd_n(abr::OFF_MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY) == pubkey,
             "SEED written while busy is dropped");
    tb.check_eq(tb.rd(abr::OFF_KV_MLDSA_SEED_RD_STATUS) & 0x2u, 0u,
                "a Key Vault read requested while busy is dropped");

    tb.check(tb.zeroize_mldsa(), "zeroize before busy-write SIGN");
    tb.wr_n(abr::OFF_MLDSA_PRIVKEY_IN, privkey);
    tb.wr_n(abr::OFF_MLDSA_MSG, k_msg);
    tb.wr_n(abr::OFF_MLDSA_SIGN_RND, k_sign_rnd);
    tb.wr(abr::OFF_MLDSA_CTRL, CMD_SIGN);
    tb.wr_n(abr::OFF_MLDSA_PRIVKEY_IN, std::vector<uint32_t>(abr::N_MLDSA_PRIVKEY, 0x11111111u));
    tb.wr_n(abr::OFF_MLDSA_MSG, std::vector<uint32_t>(abr::N_MLDSA_MSG, 0x22222222u));
    tb.wr_n(abr::OFF_MLDSA_SIGN_RND, std::vector<uint32_t>(abr::N_SIGN_RND, 0x33333333u));
    tb.wr_n(abr::OFF_ABR_ENTROPY, std::vector<uint32_t>(abr::N_ENTROPY, 0x44444444u));
    tb.check(tb.wait_mldsa_valid(), "SIGN with busy writes completes");
    tb.check(tb.rd_n(abr::OFF_MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE) == signature,
             "PRIVKEY_IN, MSG and SIGN_RND written while busy are dropped");

    tb.check(tb.zeroize_mldsa(), "zeroize before busy-write VERIFY");
    tb.wr_n(abr::OFF_MLDSA_PUBKEY, pubkey);
    tb.wr_n(abr::OFF_MLDSA_SIGNATURE, extmu_sig);
    tb.wr_n(abr::OFF_MLDSA_EXTERNAL_MU, ext_mu);
    tb.wr(abr::OFF_MLDSA_CTRL, CMD_VERIFY | CTRL_EXTERNAL_MU);
    tb.wr_n(abr::OFF_MLDSA_PUBKEY, std::vector<uint32_t>(abr::N_MLDSA_PUBKEY, 0x55555555u));
    tb.wr_n(abr::OFF_MLDSA_SIGNATURE, std::vector<uint32_t>(abr::N_MLDSA_SIGNATURE, 0x66666666u));
    tb.wr_n(abr::OFF_MLDSA_EXTERNAL_MU, std::vector<uint32_t>(abr::N_EXTERNAL_MU, 0x77777777u));
    tb.wr(abr::OFF_MLDSA_CTX_CONFIG, 4u);
    tb.wr(abr::OFF_MLDSA_CTX, 0x88888888u);
    tb.wr(abr::OFF_MLDSA_CTRL, CMD_KEYGEN);
    tb.check(tb.wait_mldsa_valid(), "VERIFY with busy writes completes");
    verify_res = tb.rd_n(abr::OFF_MLDSA_VERIFY_RES, abr::N_VERIFY_RES);
    sig_ctilde.assign(extmu_sig.begin(), extmu_sig.begin() + abr::N_VERIFY_RES);
    tb.check(verify_res == sig_ctilde,
             "PUBKEY, SIGNATURE, EXTERNAL_MU, CTX and a second command written while busy are dropped");
    tb.check(tb.rd_n(abr::OFF_MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY) == pubkey,
             "PUBKEY reads back the value loaded before the command");

    tb.section("ML-DSA: PRIVKEY_OUT is readable only after a software-seeded KEYGEN");

    tb.check(tb.zeroize_mldsa(), "zeroize before PRIVKEY_OUT checks");
    load_inputs(tb);
    tb.check(tb.run_mldsa(CMD_KEYGEN_SIGN), "KEYGEN+SIGN for the PRIVKEY_OUT check");
    tb.check(!any_nonzero(tb.rd_n(abr::OFF_MLDSA_PRIVKEY_OUT, abr::N_MLDSA_PRIVKEY)),
             "PRIVKEY_OUT reads zero after KEYGEN+SIGN");
    tb.check(tb.zeroize_mldsa(), "zeroize before a software-seeded KEYGEN");
    load_inputs(tb);
    tb.wr(abr::OFF_MLDSA_CTRL, CMD_KEYGEN);
    tb.check(!any_nonzero(tb.rd_n(abr::OFF_MLDSA_PRIVKEY_OUT, 16u)),
             "PRIVKEY_OUT reads zero before VALID");
    tb.check(tb.wait_mldsa_valid(), "software-seeded KEYGEN completes");
    tb.check(tb.rd_n(abr::OFF_MLDSA_PRIVKEY_OUT, abr::N_MLDSA_PRIVKEY) == privkey,
             "PRIVKEY_OUT reads the private key after a software-seeded KEYGEN");

    tb.section("ML-DSA: zeroize clears key material");

    tb.check(tb.zeroize_mldsa(), "zeroize before the key-material check");
    load_inputs(tb);
    tb.check(tb.run_mldsa(CMD_KEYGEN), "KEYGEN before zeroize check");
    tb.check(any_nonzero(tb.rd_n(abr::OFF_MLDSA_PUBKEY, 16u)), "public key present");
    tb.check(tb.zeroize_mldsa(), "zeroize completes");
    tb.check(!any_nonzero(tb.rd_n(abr::OFF_MLDSA_VERIFY_RES, abr::N_VERIFY_RES)),
             "zeroize cleared MLDSA_VERIFY_RES");
    // PUBKEY and SIGNATURE read as zero until VALID anyway, so complete an
    // operation that loads nothing and read the storage back.
    tb.check(tb.run_mldsa(CMD_VERIFY | CTRL_EXTERNAL_MU), "VERIFY over the zeroized storage");
    tb.check(!any_nonzero(tb.rd_n(abr::OFF_MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY)),
             "zeroize cleared MLDSA_PUBKEY storage");
    tb.check(!any_nonzero(tb.rd_n(abr::OFF_MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE)),
             "zeroize cleared MLDSA_SIGNATURE storage");
    tb.check(!any_nonzero(tb.rd_n(abr::OFF_MLDSA_PRIVKEY_OUT, abr::N_MLDSA_PRIVKEY)),
             "MLDSA_PRIVKEY_OUT reads zero after a VERIFY");
    tb.check(tb.zeroize_mldsa(), "zeroize after the storage check");

    tb.section("ML-DSA: reset");

    load_inputs(tb);
    tb.check(tb.run_mldsa(CMD_KEYGEN), "KEYGEN before reset check");
    tb.reset_dut();
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & ST_READY, ST_READY,
                "reset restores READY");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & ST_VALID, 0u, "reset clears VALID");
    tb.check(!any_nonzero(tb.rd_n(abr::OFF_MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY)),
             "reset cleared MLDSA_PUBKEY");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_NAME), 0x44534D4Cu,
                "reset preserves the hardware-tied identity register");
}
