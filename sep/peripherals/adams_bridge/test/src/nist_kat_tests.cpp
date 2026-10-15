// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file nist_kat_tests.cpp
 * @brief Drive the firmware NIST ACVP vectors through the default FIPS backend.
 *
 * Vectors: sw/sep-vp-tests/.../common/abr_nist_vectors.h (ACVP ML-DSA-87).
 * Packing is little-endian 32-bit words, matching SEP firmware.
 */

#include "abr_acvp_sigver_all.h"
#include "abr_nist_vectors.h"
#include "abr_signer_vectors.h"
#include "abr_testbench.h"

#include <string>
#include <vector>

namespace {

constexpr uint32_t CMD_KEYGEN = 0x1u;
constexpr uint32_t CMD_SIGN   = 0x2u;
constexpr uint32_t CMD_VERIFY = 0x3u;
constexpr uint32_t CTRL_EXTERNAL_MU = 1u << 5;

std::vector<uint32_t> words(const uint32_t *p, unsigned int n)
{
    return std::vector<uint32_t>(p, p + n);
}

int bytes_mismatch(const std::vector<uint32_t> &got, const uint32_t *exp, int nbytes)
{
    const int full = nbytes / 4;
    for (int i = 0; i < full; ++i) {
        if (got[static_cast<std::size_t>(i)] != exp[i]) {
            return i;
        }
    }
    const int rem = nbytes % 4;
    if (rem != 0) {
        const uint32_t mask = (1u << (rem * 8)) - 1u;
        if ((got[static_cast<std::size_t>(full)] & mask) != (exp[full] & mask)) {
            return full;
        }
    }
    return -1;
}

abr::bytes from_hex(const char *hex)
{
    abr::bytes out;
    for (const char *p = hex; p[0] != '\0' && p[1] != '\0'; p += 2) {
        out.push_back(static_cast<uint8_t>(std::stoul(std::string(p, 2), nullptr, 16)));
    }
    return out;
}

std::vector<uint32_t> le_words(const abr::bytes &b, std::size_t n)
{
    std::vector<uint32_t> out((n + 3u) / 4u, 0u);
    for (std::size_t i = 0; i < n; ++i) {
        out[i / 4u] |= static_cast<uint32_t>(b[i]) << ((i % 4u) * 8u);
    }
    return out;
}

std::vector<uint32_t> inverted(std::vector<uint32_t> v)
{
    for (auto &w : v) {
        w = ~w;
    }
    return v;
}

/// External-mu verify of @p sig; returns VERIFY_RES.
std::vector<uint32_t> verify_ext_mu(abr_testbench &tb, const abr::bytes &pk,
                                    const abr::bytes &mu, const abr::bytes &sig,
                                    const std::string &what)
{
    tb.check(tb.zeroize_mldsa(), what + ": zeroize");
    tb.wr_bytes(abr::OFF_MLDSA_PUBKEY, pk.data(), pk.size());
    tb.wr_bytes(abr::OFF_MLDSA_SIGNATURE, sig.data(), sig.size());
    tb.wr_bytes(abr::OFF_MLDSA_EXTERNAL_MU, mu.data(), mu.size());
    tb.check(tb.run_mldsa(CMD_VERIFY | CTRL_EXTERNAL_MU), what + ": VERIFY completes");
    return tb.rd_n(abr::OFF_MLDSA_VERIFY_RES, abr::N_VERIFY_RES);
}

const char *kind_name(abr_acvp_kind k)
{
    switch (k) {
    case ABR_ACVP_ACCEPT:
        return "accept";
    case ABR_ACVP_Z_REJECT:
        return "z reject";
    case ABR_ACVP_HINT_REJECT:
        return "hint reject";
    default:
        return "mismatch";
    }
}

} // namespace

void nist_kat_tests(abr_testbench &tb)
{
    tb.section("NIST ACVP: ML-DSA-87 keyGen");

    tb.check(std::string(tb.dut.crypto_backend_name()) == "pqclean-ml-dsa-87+ml-kem-1024",
             "default backend is PQClean FIPS 204/203");
    tb.check(tb.dut.crypto_backend_is_conformant(),
             "default backend reports standards-conformant");

    tb.wr_n(abr::OFF_MLDSA_SEED, words(nist_kg_seed, NIST_KG_SEED_WORDS));
    tb.check(tb.run_mldsa(CMD_KEYGEN), "NIST keyGen completes");
    const std::vector<uint32_t> pk =
        tb.rd_n(abr::OFF_MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY);
    tb.check(bytes_mismatch(pk, nist_kg_pk, NIST_KG_PK_BYTES) < 0,
             "NIST keyGen public key matches ACVP");

    tb.section("NIST ACVP: ML-DSA-87 sigGen (external mu, deterministic)");

    tb.check(tb.zeroize_mldsa(), "zeroize before NIST sigGen");
    tb.wr_n(abr::OFF_MLDSA_PRIVKEY_IN, words(nist_sg_sk, NIST_SG_SK_WORDS));
    tb.wr_n(abr::OFF_MLDSA_EXTERNAL_MU, words(nist_sg_mu, NIST_SG_MU_WORDS));
    tb.wr_n(abr::OFF_MLDSA_SIGN_RND, std::vector<uint32_t>(abr::N_SIGN_RND, 0u));
    tb.check(tb.run_mldsa(CMD_SIGN | CTRL_EXTERNAL_MU), "NIST sigGen completes");
    const std::vector<uint32_t> sig =
        tb.rd_n(abr::OFF_MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE);
    tb.check(bytes_mismatch(sig, nist_sg_sig, NIST_SG_SIG_BYTES) < 0,
             "NIST sigGen signature matches ACVP");

    tb.section("NIST ACVP: ML-DSA-87 sigVer");

    tb.check(tb.zeroize_mldsa(), "zeroize before valid sigVer");
    tb.wr_n(abr::OFF_MLDSA_PUBKEY, words(nist_sv_ok_pk, NIST_SV_PK_WORDS));
    tb.wr_n(abr::OFF_MLDSA_EXTERNAL_MU, words(nist_sv_ok_mu, NIST_SV_MU_WORDS));
    tb.wr_n(abr::OFF_MLDSA_SIGNATURE, words(nist_sv_ok_sig, NIST_SV_SIG_WORDS));
    tb.check(tb.run_mldsa(CMD_VERIFY | CTRL_EXTERNAL_MU), "valid sigVer completes");
    const std::vector<uint32_t> ctilde_ok =
        tb.rd_n(abr::OFF_MLDSA_VERIFY_RES, abr::N_VERIFY_RES);
    bool accept = true;
    for (unsigned int i = 0; i < abr::N_VERIFY_RES; ++i) {
        accept &= (ctilde_ok[i] == nist_sv_ok_sig[i]);
    }
    tb.check(accept == (NIST_SV_OK_VERDICT != 0), "valid signature is accepted");

    tb.check(tb.zeroize_mldsa(), "zeroize before invalid sigVer");
    tb.wr_n(abr::OFF_MLDSA_PUBKEY, words(nist_sv_bad_pk, NIST_SV_PK_WORDS));
    tb.wr_n(abr::OFF_MLDSA_EXTERNAL_MU, words(nist_sv_bad_mu, NIST_SV_MU_WORDS));
    tb.wr_n(abr::OFF_MLDSA_SIGNATURE, words(nist_sv_bad_sig, NIST_SV_SIG_WORDS));
    tb.check(tb.run_mldsa(CMD_VERIFY | CTRL_EXTERNAL_MU), "invalid sigVer completes");
    const std::vector<uint32_t> ctilde_bad =
        tb.rd_n(abr::OFF_MLDSA_VERIFY_RES, abr::N_VERIFY_RES);
    bool reject_ok = false;
    for (unsigned int i = 0; i < abr::N_VERIFY_RES; ++i) {
        if (ctilde_bad[i] != nist_sv_bad_sig[i]) {
            reject_ok = true;
            break;
        }
    }
    tb.check(reject_ok == (NIST_SV_BAD_VERDICT == 0), "invalid signature is rejected");

    tb.section("NIST ACVP: every ML-DSA-87 sigVer case and deterministic signature");

    for (const abr_acvp_case &c : abr_acvp_cases) {
        const std::string what = std::string(c.origin) + " (" + kind_name(c.kind) + ")";
        const abr::bytes pk = from_hex(c.pk);
        const abr::bytes sig = from_hex(c.sig);
        const abr::bytes mu = c.external_mu
                                  ? from_hex(c.mu)
                                  : abr_testbench::expected_mu(pk, from_hex(c.ctx), from_hex(c.msg));
        const std::vector<uint32_t> res = verify_ext_mu(tb, pk, mu, sig, what);
        const std::vector<uint32_t> ct = le_words(sig, 64u);
        switch (c.kind) {
        case ABR_ACVP_ACCEPT:
            tb.check(res == ct, what + ": VERIFY_RES equals c~");
            break;
        case ABR_ACVP_Z_REJECT:
        case ABR_ACVP_HINT_REJECT:
            tb.check(res == inverted(ct), what + ": VERIFY_RES keeps ~c~");
            break;
        default:
            tb.check(res != ct && res != inverted(ct), what + ": VERIFY_RES is a non-matching c~'");
            break;
        }
    }

    tb.section("Independent signer: keygen and every signature");

    tb.check(tb.zeroize_mldsa(), "zeroize before signer keygen");
    tb.wr_bytes(abr::OFF_MLDSA_SEED, abr_signer_seed, sizeof(abr_signer_seed));
    tb.check(tb.run_mldsa(CMD_KEYGEN), "keygen from the signer's seed completes");
    const abr::bytes signer_pk(abr_signer_pk, abr_signer_pk + sizeof(abr_signer_pk));
    tb.check(tb.rd_bytes(abr::OFF_MLDSA_PUBKEY, signer_pk.size()) == signer_pk,
             "keygen from the signer's seed reproduces the signer's public key");

    for (std::size_t i = 0; i < sizeof(abr_signer_cases) / sizeof(abr_signer_cases[0]); ++i) {
        const abr_signer_case &c = abr_signer_cases[i];
        const std::string what = "signer vector " + std::to_string(i) + " (msg " +
                                 std::to_string(c.msg_len) + " B, ctx " +
                                 std::to_string(c.ctx_len) + " B)";
        const abr::bytes sig(c.sig, c.sig + 4627u);
        const abr::bytes mu = abr_testbench::expected_mu(
            signer_pk, abr::bytes(c.ctx, c.ctx + c.ctx_len), abr::bytes(c.msg, c.msg + c.msg_len));
        tb.check(verify_ext_mu(tb, signer_pk, mu, sig, what) == le_words(sig, 64u),
                 what + ": verifies");
    }

    tb.section("NIST ACVP: backend helpers");

    abr::bytes empty;
    abr::shake256(empty, 0, empty);
    tb.check(empty.empty(), "SHAKE256 of length 0 is a no-op");

    abr::abr_fips_backend fips;
    abr::bytes short_sig(16, 0u);
    abr::bytes zero_pk(abr::MLDSA_PUBKEY_BYTES, 0u);
    abr::bytes mu_in(abr::MLDSA_MU_BYTES, 0u);
    abr::bytes ctilde;
    tb.check(!fips.mldsa_verify(zero_pk, mu_in, short_sig, ctilde),
             "a short signature computes no c-tilde");
    tb.check_eq(static_cast<uint32_t>(ctilde.size()),
                static_cast<uint32_t>(abr::MLDSA_CTILDE_BYTES),
                "a short signature still leaves a zeroed c-tilde buffer");
    tb.check(fips.mldsa_check_signature(short_sig) == abr::MldsaSigEncoding::HINT_MALFORMED,
             "a short signature is not well formed");
}
