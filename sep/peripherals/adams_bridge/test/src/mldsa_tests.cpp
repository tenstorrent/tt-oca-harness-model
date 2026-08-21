/**
 * @file mldsa_tests.cpp
 * @brief ML-DSA behavioural tests: keygen, sign, verify, the modifier flags and
 *        the park-on-VALID completion contract firmware relies on.
 */

#include "abr_testbench.h"

#include <iostream>

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

    tb.section("ML-DSA: signing context");

    tb.check(tb.zeroize_mldsa(), "zeroize before context sign");
    tb.wr_n(abr::OFF_MLDSA_PRIVKEY_IN, privkey);
    tb.wr_n(abr::OFF_MLDSA_MSG, k_msg);
    tb.wr_n(abr::OFF_MLDSA_SIGN_RND, k_sign_rnd);
    tb.wr(abr::OFF_MLDSA_CTX_CONFIG, 8u); // 8-byte context
    tb.wr_n(abr::OFF_MLDSA_CTX, {0xDEADBEEFu, 0xCAFEF00Du});

    tb.check(tb.run_mldsa(CMD_SIGN), "SIGN with a non-empty context completes");
    const std::vector<uint32_t> ctx_sig =
        tb.rd_n(abr::OFF_MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE);
    tb.check(ctx_sig != signature, "a signing context changes the signature");

    tb.section("ML-DSA: streamed message");

    tb.check(tb.zeroize_mldsa(), "zeroize before streamed sign");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & (1u << 2), (1u << 2),
                "MSG_STREAM_READY set while idle");

    tb.wr_n(abr::OFF_MLDSA_PRIVKEY_IN, privkey);
    tb.wr_n(abr::OFF_MLDSA_SIGN_RND, k_sign_rnd);
    tb.wr(abr::OFF_MLDSA_MSG_STROBE, 0xFu);
    // Stream 8 words through the first MSG port.
    for (unsigned int i = 0; i < 8u; ++i) {
        tb.wr(abr::OFF_MLDSA_MSG, 0x01020304u + i);
    }

    tb.check(tb.run_mldsa(CMD_SIGN | CTRL_STREAM_MSG), "SIGN with STREAM_MSG completes");
    tb.check(any_nonzero(tb.rd_n(abr::OFF_MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE)),
             "streamed SIGN produced a signature");

    tb.section("ML-DSA: partial-word strobe");

    tb.check(tb.zeroize_mldsa(), "zeroize before partial-strobe sign");
    tb.wr_n(abr::OFF_MLDSA_PRIVKEY_IN, privkey);
    tb.wr_n(abr::OFF_MLDSA_SIGN_RND, k_sign_rnd);
    tb.wr(abr::OFF_MLDSA_MSG_STROBE, 0x3u); // only two bytes of the word are live
    tb.wr(abr::OFF_MLDSA_MSG, 0xAABBCCDDu);
    tb.check(tb.run_mldsa(CMD_SIGN | CTRL_STREAM_MSG),
             "SIGN with a partial MSG_STROBE completes");

    tb.section("ML-DSA: PCR_SIGN modifier");

    tb.check(tb.zeroize_mldsa(), "zeroize before PCR sign");
    tb.wr_n(abr::OFF_MLDSA_PRIVKEY_IN, privkey);
    tb.wr_n(abr::OFF_MLDSA_MSG, k_msg);
    tb.wr_n(abr::OFF_MLDSA_SIGN_RND, k_sign_rnd);
    tb.check(tb.run_mldsa(CMD_SIGN | CTRL_PCR_SIGN), "SIGN with PCR_SIGN completes");
    const std::vector<uint32_t> pcr_fallback_sig =
        tb.rd_n(abr::OFF_MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE);

    tb.check(tb.zeroize_mldsa(), "zeroize before PCR digest sign");
    tb.wr_n(abr::OFF_MLDSA_PRIVKEY_IN, privkey);
    tb.wr_n(abr::OFF_MLDSA_MSG, k_msg);
    tb.wr_n(abr::OFF_MLDSA_SIGN_RND, k_sign_rnd);
    tb.check(tb.run_mldsa(CMD_SIGN), "plain SIGN for PCR comparison");
    const std::vector<uint32_t> plain_sig =
        tb.rd_n(abr::OFF_MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE);
    tb.check(pcr_fallback_sig == plain_sig,
             "PCR_SIGN without a digest falls back to MSG");

    abr::bytes pcr_digest(abr::N_MLDSA_MSG * 4u, 0x5Au);
    tb.dut.set_pcr_digest(pcr_digest);
    tb.check(tb.zeroize_mldsa(), "zeroize before PCR digest sign");
    tb.wr_n(abr::OFF_MLDSA_PRIVKEY_IN, privkey);
    tb.wr_n(abr::OFF_MLDSA_MSG, k_msg);
    tb.wr_n(abr::OFF_MLDSA_SIGN_RND, k_sign_rnd);
    tb.check(tb.run_mldsa(CMD_SIGN | CTRL_PCR_SIGN), "SIGN with PCR digest completes");
    tb.check(tb.rd_n(abr::OFF_MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE) != plain_sig,
             "PCR_SIGN with a digest diverges from MSG");
    tb.dut.set_pcr_digest({});

    tb.section("ML-DSA: error and no-op paths");

    tb.check(tb.zeroize_mldsa(), "zeroize before invalid command");
    tb.wr(abr::OFF_MLDSA_CTRL, 0x5u); // 5..7 are not valid commands
    tb.check(tb.wait_mldsa_valid() == false || (tb.rd(abr::OFF_MLDSA_STATUS) & ST_ERROR) != 0u,
             "an invalid command raises STATUS.ERROR");

    tb.check(tb.zeroize_mldsa(), "zeroize recovers from the invalid command");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & ST_ERROR, 0u, "zeroize cleared ERROR");

    // CTRL=0 is a no-op: the engine must stay ready and idle.
    tb.wr(abr::OFF_MLDSA_CTRL, 0u);
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & ST_READY, ST_READY,
                "CTRL=NONE leaves the engine ready");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & ST_VALID, 0u,
                "CTRL=NONE does not assert VALID");

    tb.section("ML-DSA: zeroize clears key material");

    load_inputs(tb);
    tb.check(tb.run_mldsa(CMD_KEYGEN), "KEYGEN before zeroize check");
    tb.check(any_nonzero(tb.rd_n(abr::OFF_MLDSA_PUBKEY, 16u)), "public key present");
    tb.check(tb.zeroize_mldsa(), "zeroize completes");
    tb.check(!any_nonzero(tb.rd_n(abr::OFF_MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY)),
             "zeroize cleared MLDSA_PUBKEY");
    tb.check(!any_nonzero(tb.rd_n(abr::OFF_MLDSA_PRIVKEY_OUT, abr::N_MLDSA_PRIVKEY)),
             "zeroize cleared MLDSA_PRIVKEY_OUT");
    tb.check(!any_nonzero(tb.rd_n(abr::OFF_MLDSA_VERIFY_RES, abr::N_VERIFY_RES)),
             "zeroize cleared MLDSA_VERIFY_RES");

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
