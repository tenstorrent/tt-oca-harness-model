// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file mldsa_verify_reject_tests.cpp
 * @brief ML-DSA verify fails closed the way the RTL does.
 *
 * The RTL preloads VERIFY_RES with the inverted signature challenge (~c~) when it
 * accepts VERIFY, checks ||z|| before it asks for the message, checks the hint
 * encoding after it, and leaves ~c~ in place when either check fails
 * (abr_ctrl.sv:808-811, abr_seq.sv:521-537 and 649). A malformed signature can
 * therefore never leave a VERIFY_RES that equals its own c~, whatever c~ is.
 */

#include "abr_signer_vectors.h"
#include "abr_testbench.h"

#include <string>
#include <vector>

namespace {

constexpr uint32_t CMD_VERIFY       = 0x3u;
constexpr uint32_t CTRL_EXTERNAL_MU = 1u << 5;
constexpr uint32_t STATUS_VALID     = 1u << 1;
constexpr uint32_t STATUS_ERROR     = 1u << 3;

constexpr std::size_t SIG_BYTES   = 4627u;
constexpr std::size_t CTILDE      = 64u;
constexpr std::size_t Z_BYTES     = 7u * 640u;
constexpr std::size_t HINT_OFF    = CTILDE + Z_BYTES;
constexpr unsigned int OMEGA      = 75u;
constexpr unsigned int K          = 8u;
constexpr int32_t GAMMA1          = 1 << 19;
constexpr int32_t BETA            = 120;

using bytes = std::vector<uint8_t>;

bytes genuine_sig()
{
    return bytes(abr_signer_cases[0].sig, abr_signer_cases[0].sig + SIG_BYTES);
}

bytes genuine_msg()
{
    return bytes(abr_signer_cases[0].msg, abr_signer_cases[0].msg + abr_signer_cases[0].msg_len);
}

bytes signer_pk()
{
    return bytes(abr_signer_pk, abr_signer_pk + sizeof(abr_signer_pk));
}

/// Overwrite z[0] coefficient 0 with @p value (FIPS 204 BitPack, gamma1 = 2^19).
void set_z0(bytes &sig, int32_t value)
{
    const uint32_t t = static_cast<uint32_t>(GAMMA1 - value) & 0xFFFFFu;
    sig[CTILDE + 0] = static_cast<uint8_t>(t);
    sig[CTILDE + 1] = static_cast<uint8_t>(t >> 8);
    sig[CTILDE + 2] = static_cast<uint8_t>((sig[CTILDE + 2] & 0xF0u) | ((t >> 16) & 0x0Fu));
}

enum class hint_fault { COUNT_ABOVE_OMEGA, COUNT_DECREASES, INDEX_NOT_INCREASING, TRAILING_INDEX };

const char *hint_fault_name(hint_fault f)
{
    switch (f) {
    case hint_fault::COUNT_ABOVE_OMEGA:
        return "hint count above omega";
    case hint_fault::COUNT_DECREASES:
        return "hint counts decrease";
    case hint_fault::INDEX_NOT_INCREASING:
        return "hint indices not increasing";
    default:
        return "non-zero index past the last hint";
    }
}

/// Break the hint encoding (FIPS 204 Algorithm 21) in the way named by @p f.
bool break_hint(bytes &sig, hint_fault f)
{
    uint8_t *h = sig.data() + HINT_OFF;
    switch (f) {
    case hint_fault::COUNT_ABOVE_OMEGA:
        h[OMEGA + K - 1u] = OMEGA + 1u;
        return true;
    case hint_fault::COUNT_DECREASES:
        for (unsigned int i = 0; i + 1u < K; ++i) {
            if (h[OMEGA + i] > 0u) {
                h[OMEGA + i + 1u] = static_cast<uint8_t>(h[OMEGA + i] - 1u);
                return true;
            }
        }
        return false;
    case hint_fault::INDEX_NOT_INCREASING: {
        unsigned int k = 0;
        for (unsigned int i = 0; i < K; ++i) {
            const unsigned int end = h[OMEGA + i];
            if (end >= k + 2u) {
                h[k + 1u] = h[k];
                return true;
            }
            k = end;
        }
        return false;
    }
    default: {
        const unsigned int last = h[OMEGA + K - 1u];
        if (last >= OMEGA) {
            return false;
        }
        h[last] = 1u;
        return true;
    }
    }
}

std::vector<uint32_t> le_words(const uint8_t *p, std::size_t n)
{
    std::vector<uint32_t> out((n + 3u) / 4u, 0u);
    for (std::size_t i = 0; i < n; ++i) {
        out[i / 4u] |= static_cast<uint32_t>(p[i]) << ((i % 4u) * 8u);
    }
    return out;
}

std::vector<uint32_t> inverted(const std::vector<uint32_t> &v)
{
    std::vector<uint32_t> out(v);
    for (auto &w : out) {
        w = ~w;
    }
    return out;
}

/// Load pk, signature and external mu; leaves the core READY with no command issued.
void load_ext_mu(abr_testbench &tb, const bytes &sig)
{
    const bytes pk = signer_pk();
    const abr::bytes mu = abr_testbench::expected_mu(pk, {}, genuine_msg());
    tb.check(tb.zeroize_mldsa(), "zeroize before loading the verify inputs");
    tb.wr_bytes(abr::OFF_MLDSA_PUBKEY, pk.data(), pk.size());
    tb.wr_bytes(abr::OFF_MLDSA_SIGNATURE, sig.data(), sig.size());
    tb.wr_bytes(abr::OFF_MLDSA_EXTERNAL_MU, mu.data(), mu.size());
}

/// Run an external-mu verify of @p sig and return VERIFY_RES.
std::vector<uint32_t> verify_ext_mu(abr_testbench &tb, const bytes &sig, const std::string &what)
{
    load_ext_mu(tb, sig);
    tb.check(tb.run_mldsa(CMD_VERIFY | CTRL_EXTERNAL_MU), what + ": VERIFY completes");
    return tb.rd_n(abr::OFF_MLDSA_VERIFY_RES, abr::N_VERIFY_RES);
}

void expect_inverted(abr_testbench &tb, const bytes &sig, const std::string &what)
{
    const std::vector<uint32_t> res = verify_ext_mu(tb, sig, what);
    const std::vector<uint32_t> ct = le_words(sig.data(), CTILDE);
    tb.check(res == inverted(ct), what + ": VERIFY_RES keeps the preloaded ~c~");
    tb.check(res != ct, what + ": VERIFY_RES does not match c~");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & (STATUS_VALID | STATUS_ERROR), STATUS_VALID,
                what + ": a rejection is a completed verify, VALID without ERROR");
}

} // namespace

void mldsa_verify_reject_tests(abr_testbench &tb)
{
    const bytes sig = genuine_sig();
    const std::vector<uint32_t> ct = le_words(sig.data(), CTILDE);

    tb.section("ML-DSA verify: VERIFY_RES is preloaded with ~c~");

    load_ext_mu(tb, sig);
    tb.check(tb.wait_mldsa_ready(), "READY before VERIFY");
    tb.wr(abr::OFF_MLDSA_CTRL, CMD_VERIFY | CTRL_EXTERNAL_MU);
    tb.check(tb.rd_n(abr::OFF_MLDSA_VERIFY_RES, abr::N_VERIFY_RES) == inverted(ct),
             "VERIFY_RES reads ~c~ right after the command is accepted");
    tb.check(tb.wait_mldsa_valid(), "genuine external-mu VERIFY completes");
    tb.check(tb.rd_n(abr::OFF_MLDSA_VERIFY_RES, abr::N_VERIFY_RES) == ct,
             "a genuine signature replaces the preload with c~");

    tb.section("ML-DSA verify: forged all-zero c~");

    bytes forged = sig;
    std::fill(forged.begin(), forged.begin() + CTILDE, 0u);
    bytes forged_z = forged;
    set_z0(forged_z, GAMMA1 - BETA);
    expect_inverted(tb, forged_z, "c~ = 0, z out of range");
    for (hint_fault f : {hint_fault::COUNT_ABOVE_OMEGA, hint_fault::COUNT_DECREASES,
                         hint_fault::INDEX_NOT_INCREASING, hint_fault::TRAILING_INDEX}) {
        bytes forged_h = forged;
        tb.check(break_hint(forged_h, f), std::string("genuine hint admits: ") + hint_fault_name(f));
        expect_inverted(tb, forged_h, std::string("c~ = 0, ") + hint_fault_name(f));
    }

    tb.section("ML-DSA verify: forged all-ones c~");

    bytes ones = sig;
    std::fill(ones.begin(), ones.begin() + CTILDE, 0xFFu);
    bytes ones_z = ones;
    set_z0(ones_z, GAMMA1 - BETA);
    expect_inverted(tb, ones_z, "c~ = all-ones, z out of range");
    bytes ones_h = ones;
    tb.check(break_hint(ones_h, hint_fault::COUNT_ABOVE_OMEGA), "all-ones c~ hint broken");
    expect_inverted(tb, ones_h, "c~ = all-ones, hint malformed");

    tb.section("ML-DSA verify: genuine c~ with a malformed body");

    bytes bad_z = sig;
    set_z0(bad_z, GAMMA1 - BETA);
    expect_inverted(tb, bad_z, "genuine c~, z out of range");
    bytes bad_z_neg = sig;
    set_z0(bad_z_neg, -(GAMMA1 - BETA));
    expect_inverted(tb, bad_z_neg, "genuine c~, z out of range (negative)");
    for (hint_fault f : {hint_fault::COUNT_ABOVE_OMEGA, hint_fault::COUNT_DECREASES,
                         hint_fault::INDEX_NOT_INCREASING, hint_fault::TRAILING_INDEX}) {
        bytes bad_h = sig;
        tb.check(break_hint(bad_h, f), std::string("genuine hint admits: ") + hint_fault_name(f));
        expect_inverted(tb, bad_h, std::string("genuine c~, ") + hint_fault_name(f));
    }

    tb.section("ML-DSA verify: z range boundary");

    bytes z_inside = sig;
    set_z0(z_inside, GAMMA1 - BETA - 1);
    const std::vector<uint32_t> res_inside = verify_ext_mu(tb, z_inside, "z = gamma1 - beta - 1");
    tb.check(res_inside != ct && res_inside != inverted(ct),
             "z = gamma1 - beta - 1 passes the range check and computes a non-matching c~'");

    tb.section("ML-DSA verify: non-streamed message, z out of range");

    tb.check(tb.zeroize_mldsa(), "zeroize before non-streamed bad-z VERIFY");
    const bytes pk = signer_pk();
    tb.wr_bytes(abr::OFF_MLDSA_PUBKEY, pk.data(), pk.size());
    tb.wr_bytes(abr::OFF_MLDSA_SIGNATURE, bad_z.data(), bad_z.size());
    tb.wr_bytes(abr::OFF_MLDSA_MSG, abr_signer_cases[8].msg, abr_signer_cases[8].msg_len);
    tb.check(tb.run_mldsa(CMD_VERIFY), "non-streamed bad-z VERIFY completes");
    tb.check(tb.rd_n(abr::OFF_MLDSA_VERIFY_RES, abr::N_VERIFY_RES) == inverted(ct),
             "non-streamed bad-z VERIFY keeps ~c~");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & (STATUS_VALID | STATUS_ERROR), STATUS_VALID,
                "non-streamed bad-z VERIFY ends VALID without ERROR");

    tb.section("ML-DSA verify: zeroize clears VERIFY_RES");

    tb.check(tb.zeroize_mldsa(), "zeroize after the reject cases");
    tb.check(tb.rd_n(abr::OFF_MLDSA_VERIFY_RES, abr::N_VERIFY_RES) ==
                 std::vector<uint32_t>(abr::N_VERIFY_RES, 0u),
             "zeroize leaves VERIFY_RES all-zero");
}
