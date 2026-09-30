// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file interrupt_kv_tests.cpp
 * @brief Interrupt block, Key Vault sideload and crypto-backend plumbing tests.
 */

#include "abr_testbench.h"

#include <algorithm>
#include <memory>

namespace {

constexpr uint32_t CMD_KEYGEN = 0x1u;

constexpr uint32_t GLOBAL_ERROR_EN = 1u << 0;
constexpr uint32_t GLOBAL_NOTIF_EN = 1u << 1;
constexpr uint32_t INTR_EN         = 1u << 0;
constexpr uint32_t INTR_STS        = 1u << 0;

constexpr uint32_t KV_READ_EN      = 1u << 0;
constexpr uint32_t KV_ENTRY_SHIFT  = 1u;
constexpr uint32_t KV_STATUS_VALID = 1u << 1;
constexpr uint32_t KV_ERROR_SHIFT  = 2u;
constexpr uint32_t KV_ERROR_MASK   = 0xFFu;

/// 32 bytes of distinctive seed material for a Key Vault entry.
std::vector<uint8_t> kv_material(uint8_t base)
{
    std::vector<uint8_t> m(abr::MLDSA_SEED_BYTES);
    for (std::size_t i = 0; i < m.size(); ++i) {
        m[i] = static_cast<uint8_t>(base + i);
    }
    return m;
}

bool any_nonzero(const std::vector<uint32_t> &v)
{
    for (uint32_t w : v) {
        if (w != 0u) {
            return true;
        }
    }
    return false;
}

/// A backend that reports itself as conformant, used to prove the seam works.
class stub_backend : public abr::abr_crypto_backend
{
  public:
    const char *name() const override { return "unit-test-stub"; }
    bool is_standards_conformant() const override { return true; }

    void mldsa_keygen(const abr::bytes &, abr::bytes &pubkey,
                      abr::bytes &privkey) override
    {
        pubkey.assign(abr::MLDSA_PUBKEY_BYTES, 0xABu);
        privkey.assign(abr::MLDSA_PRIVKEY_BYTES, 0xCDu);
    }
    void mldsa_sign(const abr::bytes &, const abr::bytes &, const abr::bytes &,
                    abr::bytes &signature) override
    {
        signature.assign(abr::MLDSA_SIG_BYTES, 0xEFu);
    }
    void mldsa_verify(const abr::bytes &, const abr::bytes &, const abr::bytes &,
                      abr::bytes &ctilde_out) override
    {
        ctilde_out.assign(abr::MLDSA_CTILDE_BYTES, 0x11u);
    }
    void mldsa_compute_mu(const abr::bytes &, const abr::bytes &, const abr::bytes &,
                          abr::bytes &mu_out) override
    {
        mu_out.assign(abr::MLDSA_MU_BYTES, 0x22u);
    }
    void mldsa_compute_mu_from_sk(const abr::bytes &, const abr::bytes &,
                                  const abr::bytes &, abr::bytes &mu_out) override
    {
        mu_out.assign(abr::MLDSA_MU_BYTES, 0x22u);
    }
    void mlkem_keygen(const abr::bytes &, const abr::bytes &, abr::bytes &encaps_key,
                      abr::bytes &decaps_key) override
    {
        encaps_key.assign(abr::MLKEM_ENCAPS_KEY_BYTES, 0x33u);
        decaps_key.assign(abr::MLKEM_DECAPS_KEY_BYTES, 0x44u);
    }
    void mlkem_encaps(const abr::bytes &, const abr::bytes &, abr::bytes &ciphertext,
                      abr::bytes &shared_key) override
    {
        ciphertext.assign(abr::MLKEM_CIPHERTEXT_BYTES, 0x55u);
        shared_key.assign(abr::MLKEM_SHARED_KEY_BYTES, 0x66u);
    }
    void mlkem_decaps(const abr::bytes &, const abr::bytes &,
                      abr::bytes &shared_key) override
    {
        shared_key.assign(abr::MLKEM_SHARED_KEY_BYTES, 0x77u);
    }
};

} // namespace

void interrupt_kv_tests(abr_testbench &tb)
{
    tb.section("Interrupts: enables gate the output");

    tb.reset_dut();

    tb.check(!tb.notif_sig.read(), "notification output low out of reset");
    tb.check(!tb.err_sig.read(), "error output low out of reset");

    // With every enable clear, a completed command must not drive the pin.
    tb.wr_n(abr::OFF_MLDSA_SEED, std::vector<uint32_t>(8, 0x01020304u));
    tb.check(tb.run_mldsa(CMD_KEYGEN), "KEYGEN completes with interrupts disabled");
    tb.check_eq(tb.rd(abr::OFF_NOTIF_INTERNAL_INTR) & INTR_STS, INTR_STS,
                "notification status latches even when masked");
    tb.check(!tb.notif_sig.read(), "masked notification does not drive the output");

    tb.section("Interrupts: unmasking exposes a pending event");

    tb.wr(abr::OFF_NOTIF_INTR_EN, INTR_EN);
    tb.wr(abr::OFF_GLOBAL_INTR_EN, GLOBAL_ERROR_EN | GLOBAL_NOTIF_EN);
    sc_core::wait(sc_core::sc_time(1, sc_core::SC_US));
    tb.check(tb.notif_sig.read(), "unmasking an already-pending event drives the output");
    tb.check_eq(tb.rd(abr::OFF_NOTIF_GLOBAL_INTR) & INTR_STS, INTR_STS,
                "notif_global_intr aggregates the pending event");

    tb.section("Interrupts: write-1-to-clear");

    tb.wr(abr::OFF_NOTIF_INTERNAL_INTR, 0u);
    sc_core::wait(sc_core::sc_time(1, sc_core::SC_US));
    tb.check(tb.notif_sig.read(), "writing 0 does not clear the status bit");

    tb.wr(abr::OFF_NOTIF_INTERNAL_INTR, INTR_STS);
    sc_core::wait(sc_core::sc_time(1, sc_core::SC_US));
    tb.check_eq(tb.rd(abr::OFF_NOTIF_INTERNAL_INTR) & INTR_STS, 0u,
                "writing 1 clears the status bit");
    tb.check(!tb.notif_sig.read(), "clearing the status bit drops the output");

    tb.section("Interrupts: counters");

    const uint32_t notif_count_before = tb.rd(abr::OFF_NOTIF_INTR_COUNT);
    tb.check(tb.zeroize_mldsa(), "zeroize before counted command");
    tb.wr_n(abr::OFF_MLDSA_SEED, std::vector<uint32_t>(8, 0x05060708u));
    tb.check(tb.run_mldsa(CMD_KEYGEN), "counted KEYGEN completes");
    tb.check_eq(tb.rd(abr::OFF_NOTIF_INTR_COUNT), notif_count_before + 1u,
                "notification counter incremented");
    tb.check_eq(tb.rd(abr::OFF_NOTIF_INTR_COUNT_INCR) & INTR_STS, INTR_STS,
                "notification incrementor pulse is visible");

    tb.section("Interrupts: software trigger");

    tb.wr(abr::OFF_ERROR_INTR_EN, INTR_EN);
    const uint32_t err_count_before = tb.rd(abr::OFF_ERROR_INTR_COUNT);

    tb.wr(abr::OFF_ERROR_INTR_TRIG, INTR_STS);
    sc_core::wait(sc_core::sc_time(1, sc_core::SC_US));
    tb.check_eq(tb.rd(abr::OFF_ERROR_INTERNAL_INTR) & INTR_STS, INTR_STS,
                "error trigger sets the error status bit");
    tb.check(tb.err_sig.read(), "error trigger drives the error output");
    tb.check_eq(tb.rd(abr::OFF_ERROR_INTR_COUNT), err_count_before + 1u,
                "error counter incremented");
    tb.check_eq(tb.rd(abr::OFF_ERROR_GLOBAL_INTR) & INTR_STS, INTR_STS,
                "error_global_intr aggregates the error");

    tb.wr(abr::OFF_ERROR_INTERNAL_INTR, INTR_STS);
    sc_core::wait(sc_core::sc_time(1, sc_core::SC_US));
    tb.check(!tb.err_sig.read(), "clearing the error status drops the error output");

    tb.wr(abr::OFF_ERROR_INTR_TRIG, 0u);
    sc_core::wait(sc_core::sc_time(1, sc_core::SC_US));
    tb.check(!tb.err_sig.read(), "writing 0 to the trigger is a no-op");

    tb.wr(abr::OFF_NOTIF_INTR_TRIG, INTR_STS);
    sc_core::wait(sc_core::sc_time(1, sc_core::SC_US));
    tb.check(tb.notif_sig.read(), "notification trigger drives the output");
    tb.wr(abr::OFF_NOTIF_INTERNAL_INTR, INTR_STS);
    tb.wr(abr::OFF_NOTIF_INTR_TRIG, 0u);
    sc_core::wait(sc_core::sc_time(1, sc_core::SC_US));

    tb.section("Interrupts: global mask");

    tb.wr(abr::OFF_GLOBAL_INTR_EN, 0u);
    tb.wr(abr::OFF_ERROR_INTR_TRIG, INTR_STS);
    sc_core::wait(sc_core::sc_time(1, sc_core::SC_US));
    tb.check_eq(tb.rd(abr::OFF_ERROR_INTERNAL_INTR) & INTR_STS, INTR_STS,
                "status latches with the global mask clear");
    tb.check(!tb.err_sig.read(), "global mask suppresses the output");
    tb.wr(abr::OFF_ERROR_INTERNAL_INTR, INTR_STS);
    sc_core::wait(sc_core::sc_time(1, sc_core::SC_US));

    tb.section("Key Vault: sideload port");

    tb.reset_dut();

    const std::vector<uint8_t> material = kv_material(0x40u);
    tb.kv_push(3u, material);
    tb.check(tb.kv_read(3u, static_cast<unsigned int>(material.size())) == material,
             "sideload write is readable back through the port");

    tb.check(tb.kv_access_ok(0u, 64u), "in-range sideload access is accepted");
    tb.check(!tb.kv_access_ok(
                 static_cast<uint64_t>(abr::KV_NUM_ENTRIES) * abr::KV_ENTRY_BYTES, 4u),
             "out-of-range sideload access is rejected");

    tb.section("Key Vault: ML-DSA seed read");

    tb.check(tb.zeroize_mldsa(), "zeroize before KV seed read");
    tb.kv_push(3u, material);

    tb.wr(abr::OFF_KV_MLDSA_SEED_RD_CTRL, KV_READ_EN | (3u << KV_ENTRY_SHIFT));
    const uint32_t kv_status = tb.rd(abr::OFF_KV_MLDSA_SEED_RD_STATUS);
    tb.check_eq(kv_status & KV_STATUS_VALID, KV_STATUS_VALID, "KV seed read reports VALID");
    tb.check_eq((kv_status >> KV_ERROR_SHIFT) & KV_ERROR_MASK, abr::KV_SUCCESS,
                "KV seed read reports SUCCESS");
    tb.check_eq(tb.rd(abr::OFF_KV_MLDSA_SEED_RD_CTRL) & KV_READ_EN, 0u,
                "read_en is hardware-cleared after the copy");

    // The KV seed must override whatever software left in MLDSA_SEED.
    tb.wr_n(abr::OFF_MLDSA_SEED, std::vector<uint32_t>(8, 0xFFFFFFFFu));
    tb.check(tb.run_mldsa(CMD_KEYGEN), "KEYGEN with a KV-sourced seed completes");
    const std::vector<uint32_t> kv_pubkey =
        tb.rd_n(abr::OFF_MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY);
    tb.check(any_nonzero(kv_pubkey), "KV-seeded KEYGEN produced a public key");

    // Same KV entry, same result; a different entry must diverge.
    tb.check(tb.zeroize_mldsa(), "zeroize between KV seed comparisons");
    tb.kv_push(3u, material);
    tb.wr(abr::OFF_KV_MLDSA_SEED_RD_CTRL, KV_READ_EN | (3u << KV_ENTRY_SHIFT));
    tb.check(tb.run_mldsa(CMD_KEYGEN), "repeat KV-seeded KEYGEN completes");
    tb.check(tb.rd_n(abr::OFF_MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY) == kv_pubkey,
             "same KV entry reproduces the same public key");

    tb.check(tb.zeroize_mldsa(), "zeroize before differing KV entry");
    tb.kv_push(4u, kv_material(0x90u));
    tb.wr(abr::OFF_KV_MLDSA_SEED_RD_CTRL, KV_READ_EN | (4u << KV_ENTRY_SHIFT));
    tb.check(tb.run_mldsa(CMD_KEYGEN), "KEYGEN from a different KV entry completes");
    tb.check(tb.rd_n(abr::OFF_MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY) != kv_pubkey,
             "a different KV entry yields a different public key");

    tb.section("Key Vault: failure paths");

    tb.check(tb.zeroize_mldsa(), "zeroize before KV failure check");
    // Entry 31 was never populated, so the copy must fail.
    tb.wr(abr::OFF_KV_MLDSA_SEED_RD_CTRL, KV_READ_EN | (31u << KV_ENTRY_SHIFT));
    const uint32_t fail_status = tb.rd(abr::OFF_KV_MLDSA_SEED_RD_STATUS);
    tb.check_eq(fail_status & KV_STATUS_VALID, 0u, "empty KV entry does not report VALID");
    tb.check_eq((fail_status >> KV_ERROR_SHIFT) & KV_ERROR_MASK, abr::KV_READ_FAIL,
                "empty KV entry reports KV_READ_FAIL");

    // read_en clear is a no-op.
    tb.wr(abr::OFF_KV_MLDSA_SEED_RD_CTRL, 0u);
    tb.check_eq(tb.rd(abr::OFF_KV_MLDSA_SEED_RD_CTRL) & KV_READ_EN, 0u,
                "writing read_en=0 is a no-op");

    // Back-door loader rejects an out-of-range entry without mutating state.
    const std::vector<uint8_t> kv0_before = tb.kv_read(0u, abr::KV_ENTRY_BYTES);
    const std::vector<uint8_t> kv31_before = tb.kv_read(31u, abr::KV_ENTRY_BYTES);
    const uint32_t seed_rd_status_before = tb.rd(abr::OFF_KV_MLDSA_SEED_RD_STATUS);
    const uint32_t mldsa_status_before = tb.rd(abr::OFF_MLDSA_STATUS);
    tb.dut.load_kv_entry(abr::KV_NUM_ENTRIES + 1u, material);
    tb.check(tb.kv_read(0u, abr::KV_ENTRY_BYTES) == kv0_before,
             "OOR load_kv_entry leaves KV entry 0 unchanged");
    tb.check(tb.kv_read(31u, abr::KV_ENTRY_BYTES) == kv31_before,
             "OOR load_kv_entry leaves KV entry 31 unchanged");
    tb.check_eq(tb.rd(abr::OFF_KV_MLDSA_SEED_RD_STATUS), seed_rd_status_before,
                "OOR load_kv_entry leaves KV seed RD status unchanged");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS), mldsa_status_before,
                "OOR load_kv_entry leaves MLDSA_STATUS unchanged");

    tb.section("Key Vault: ML-KEM lanes");

    tb.check(tb.zeroize_mlkem(), "zeroize before ML-KEM KV reads");
    tb.kv_push(7u, kv_material(0x10u));
    tb.wr(abr::OFF_KV_MLKEM_SEED_RD_CTRL, KV_READ_EN | (7u << KV_ENTRY_SHIFT));
    tb.check_eq(tb.rd(abr::OFF_KV_MLKEM_SEED_RD_STATUS) & KV_STATUS_VALID, KV_STATUS_VALID,
                "ML-KEM KV seed read reports VALID");

    tb.kv_push(8u, kv_material(0x20u));
    tb.wr(abr::OFF_KV_MLKEM_MSG_RD_CTRL, KV_READ_EN | (8u << KV_ENTRY_SHIFT));
    tb.check_eq(tb.rd(abr::OFF_KV_MLKEM_MSG_RD_STATUS) & KV_STATUS_VALID, KV_STATUS_VALID,
                "ML-KEM KV message read reports VALID");

    tb.wr(abr::OFF_KV_MLKEM_SEED_RD_CTRL, 0u);
    tb.wr(abr::OFF_KV_MLKEM_MSG_RD_CTRL, 0u);

    tb.wr_n(abr::OFF_MLKEM_SEED_Z, std::vector<uint32_t>(8, 0x0A0B0C0Du));
    tb.check(tb.run_mlkem(0x1u), "ML-KEM KEYGEN with a KV-sourced seed completes");

    tb.section("Key Vault: shared-key export");

    tb.check(tb.zeroize_mlkem(), "zeroize before shared-key export");
    tb.wr_n(abr::OFF_MLKEM_SEED_D, std::vector<uint32_t>(8, 0x01010101u));
    tb.wr_n(abr::OFF_MLKEM_SEED_Z, std::vector<uint32_t>(8, 0x02020202u));
    tb.check(tb.run_mlkem(0x1u), "ML-KEM KEYGEN before export");
    const std::vector<uint32_t> ek =
        tb.rd_n(abr::OFF_MLKEM_ENCAPS_KEY, abr::N_MLKEM_ENCAPS_KEY);

    tb.check(tb.zeroize_mlkem(), "zeroize before ENCAPS for export");
    tb.wr_n(abr::OFF_MLKEM_ENCAPS_KEY, ek);
    tb.wr_n(abr::OFF_MLKEM_MSG, std::vector<uint32_t>(8, 0x03030303u));
    tb.check(tb.run_mlkem(0x2u), "ENCAPS before export");

    const std::vector<uint32_t> shared_key =
        tb.rd_n(abr::OFF_MLKEM_SHARED_KEY, abr::N_MLKEM_SHARED_KEY);

    tb.wr(abr::OFF_KV_MLKEM_SHAREDKEY_WR_CTRL, KV_READ_EN | (9u << KV_ENTRY_SHIFT));
    const uint32_t wr_status = tb.rd(abr::OFF_KV_MLKEM_SHAREDKEY_WR_STATUS);
    tb.check_eq(wr_status & KV_STATUS_VALID, KV_STATUS_VALID,
                "shared-key export reports VALID");
    tb.check_eq((wr_status >> KV_ERROR_SHIFT) & KV_ERROR_MASK, abr::KV_SUCCESS,
                "shared-key export reports SUCCESS");

    // The exported entry must hold the shared key, little-endian per word.
    const std::vector<uint8_t> exported =
        tb.kv_read(9u, abr::MLKEM_SHARED_KEY_BYTES);
    bool export_matches = true;
    for (unsigned int i = 0; i < abr::N_MLKEM_SHARED_KEY; ++i) {
        const uint32_t w = shared_key[i];
        export_matches &= (exported[i * 4u + 0u] == (w & 0xFFu));
        export_matches &= (exported[i * 4u + 1u] == ((w >> 8) & 0xFFu));
        export_matches &= (exported[i * 4u + 2u] == ((w >> 16) & 0xFFu));
        export_matches &= (exported[i * 4u + 3u] == ((w >> 24) & 0xFFu));
    }
    tb.check(export_matches, "exported KV entry holds the shared key");

    tb.wr(abr::OFF_KV_MLKEM_SHAREDKEY_WR_CTRL, 0u);
    tb.check_eq(tb.rd(abr::OFF_KV_MLKEM_SHAREDKEY_WR_CTRL) & KV_READ_EN, 0u,
                "write_en=0 is a no-op");

    // Exporting with no shared key available must fail cleanly.
    tb.check(tb.zeroize_mlkem(), "zeroize clears the latched shared key");
    tb.wr(abr::OFF_KV_MLKEM_SHAREDKEY_WR_CTRL, KV_READ_EN | (10u << KV_ENTRY_SHIFT));
    const uint32_t wr_fail = tb.rd(abr::OFF_KV_MLKEM_SHAREDKEY_WR_STATUS);
    tb.check_eq(wr_fail & KV_STATUS_VALID, 0u, "export without a shared key is not VALID");
    tb.check_eq((wr_fail >> KV_ERROR_SHIFT) & KV_ERROR_MASK, abr::KV_WRITE_FAIL,
                "export without a shared key reports KV_WRITE_FAIL");

    tb.section("Key Vault: KM dual-share sideload");

    tb.reset_dut();

    const std::vector<uint32_t> pal_seed = {
        0x0badc0deu, 0x13572468u, 0xa5a5a5a5u, 0xfeedfaceu,
        0xfeedfaceu, 0xa5a5a5a5u, 0x13572468u, 0x0badc0deu};
    auto words_to_le = [](const std::vector<uint32_t> &words) {
        std::vector<uint8_t> out(words.size() * 4u, 0u);
        for (std::size_t i = 0; i < words.size(); ++i) {
            const uint32_t w = words[i];
            out[i * 4u + 0u] = static_cast<uint8_t>(w & 0xFFu);
            out[i * 4u + 1u] = static_cast<uint8_t>((w >> 8) & 0xFFu);
            out[i * 4u + 2u] = static_cast<uint8_t>((w >> 16) & 0xFFu);
            out[i * 4u + 3u] = static_cast<uint8_t>((w >> 24) & 0xFFu);
        }
        return out;
    };

    tb.km_share_commit(0u, pal_seed);
    tb.check(tb.kv_read(0u, abr::MLDSA_SEED_BYTES) == words_to_le(pal_seed),
             "KM ML-DSA share XOR reconstructs into KV entry 0");

    tb.wr_n(abr::OFF_MLDSA_SEED, pal_seed);
    tb.check(tb.run_mldsa(CMD_KEYGEN), "direct palindromic KEYGEN");
    const std::vector<uint32_t> pk_direct =
        tb.rd_n(abr::OFF_MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY);

    tb.check(tb.zeroize_mldsa(), "zeroize before KM palindromic KEYGEN");
    tb.km_share_commit(0u, pal_seed);
    tb.wr(abr::OFF_KV_MLDSA_SEED_RD_CTRL, KV_READ_EN);
    tb.check_eq(tb.rd(abr::OFF_KV_MLDSA_SEED_RD_STATUS) & KV_STATUS_VALID, KV_STATUS_VALID,
                "KM-sourced KV read reports VALID");
    tb.check(tb.run_mldsa(CMD_KEYGEN), "KEYGEN from KM palindromic seed");
    tb.check(tb.rd_n(abr::OFF_MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY) == pk_direct,
             "palindromic KM seed matches a direct software seed (dword reversal invariant)");

    tb.check(tb.zeroize_mldsa(), "zeroize before non-palindromic reversal check");
    const std::vector<uint32_t> seq_seed = {
        0x00010203u, 0x04050607u, 0x08090a0bu, 0x0c0d0e0fu,
        0x10111213u, 0x14151617u, 0x18191a1bu, 0x1c1d1e1fu};
    tb.wr_n(abr::OFF_MLDSA_SEED, seq_seed);
    tb.check(tb.run_mldsa(CMD_KEYGEN), "direct sequential KEYGEN");
    const std::vector<uint32_t> pk_seq =
        tb.rd_n(abr::OFF_MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY);

    // Independently reverse the dword order (matches abr_ip::reverse_le_dwords).
    std::vector<uint32_t> rev_seed = seq_seed;
    std::reverse(rev_seed.begin(), rev_seed.end());

    tb.check(tb.zeroize_mldsa(), "zeroize before independently-reversed KEYGEN");
    tb.wr_n(abr::OFF_MLDSA_SEED, rev_seed);
    tb.check(tb.run_mldsa(CMD_KEYGEN), "KEYGEN from independently reversed seed");
    const std::vector<uint32_t> pk_rev =
        tb.rd_n(abr::OFF_MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY);
    tb.check(pk_rev != pk_seq, "dword-reversed seed diverges from sequential seed");

    tb.check(tb.zeroize_mldsa(), "zeroize before KM sequential KEYGEN");
    tb.km_share_commit(0u, seq_seed);
    tb.wr(abr::OFF_KV_MLDSA_SEED_RD_CTRL, KV_READ_EN);
    tb.check(tb.run_mldsa(CMD_KEYGEN), "KEYGEN from KM sequential seed");
    tb.check(tb.rd_n(abr::OFF_MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY) == pk_rev,
             "non-palindromic KM seed matches the independently dword-reversed oracle");

    const std::vector<uint32_t> lane_key(8, 0x11223344u);
    tb.km_share_commit(1u, lane_key);
    tb.km_share_commit(2u, lane_key);
    tb.km_share_commit(3u, lane_key);
    tb.check(tb.kv_read(1u, 32u) == words_to_le(lane_key), "ML-KEM D share lands in KV entry 1");
    tb.check(tb.kv_read(2u, 32u) == words_to_le(lane_key), "ML-KEM Z share lands in KV entry 2");
    tb.check(tb.kv_read(3u, 32u) == words_to_le(lane_key), "ML-KEM MSG share lands in KV entry 3");

    tb.km_share_commit(0u, pal_seed, false);
    const std::vector<uint8_t> shredded = tb.kv_read(0u, abr::MLDSA_SEED_BYTES);
    bool shredded_zero = true;
    for (uint8_t b : shredded) {
        shredded_zero &= (b == 0u);
    }
    tb.check(shredded_zero, "KEY_CTRL=0 shreds KV entry 0");

    tb.check(!tb.km_share_access_ok(0u, 0x00u, 4u, false), "KM share reads are rejected");
    tb.check(!tb.km_share_access_ok(0u, 0x80u, 4u, true), "unknown KM share offset is rejected");
    tb.check(!tb.km_share_access_ok(0u, 0x01u, 4u, true), "unaligned KM share write is rejected");
    tb.check(!tb.km_share_access_ok(0u, 0x00u, 2u, true), "short KM share write is rejected");

    tb.section("Crypto backend seam");

    tb.check(std::string(tb.dut.crypto_backend_name()) == "pqclean-ml-dsa-87+ml-kem-1024",
             "default backend is PQClean FIPS 204/203");

    tb.dut.set_crypto_backend(nullptr);
    tb.check(std::string(tb.dut.crypto_backend_name()) == "pqclean-ml-dsa-87+ml-kem-1024",
             "set_crypto_backend(nullptr) is ignored");

    tb.dut.set_crypto_backend(std::make_unique<abr::abr_shake_backend>());
    tb.check(std::string(tb.dut.crypto_backend_name()) == "shake256-functional",
             "SHAKE256 stand-in can be installed");
    tb.reset_dut();
    tb.wr_n(abr::OFF_MLDSA_SEED, std::vector<uint32_t>(8, 0x01020304u));
    tb.check(tb.run_mldsa(CMD_KEYGEN), "KEYGEN on SHAKE stand-in");
    const std::vector<uint32_t> shake_pk =
        tb.rd_n(abr::OFF_MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY);
    const std::vector<uint32_t> shake_sk =
        tb.rd_n(abr::OFF_MLDSA_PRIVKEY_OUT, abr::N_MLDSA_PRIVKEY);
    tb.check(tb.zeroize_mldsa(), "zeroize after SHAKE keygen");
    tb.wr_n(abr::OFF_MLDSA_PRIVKEY_IN, shake_sk);
    tb.wr_n(abr::OFF_MLDSA_MSG, std::vector<uint32_t>(abr::N_MLDSA_MSG, 0x41424344u));
    tb.check(tb.run_mldsa(0x2u), "SIGN on SHAKE stand-in");
    const std::vector<uint32_t> shake_sig =
        tb.rd_n(abr::OFF_MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE);
    tb.check(tb.zeroize_mldsa(), "zeroize before SHAKE verify");
    tb.wr_n(abr::OFF_MLDSA_PUBKEY, shake_pk);
    tb.wr_n(abr::OFF_MLDSA_SIGNATURE, shake_sig);
    tb.wr_n(abr::OFF_MLDSA_MSG, std::vector<uint32_t>(abr::N_MLDSA_MSG, 0x41424344u));
    tb.check(tb.run_mldsa(0x3u), "VERIFY on SHAKE stand-in");
    const std::vector<uint32_t> shake_ct =
        tb.rd_n(abr::OFF_MLDSA_VERIFY_RES, abr::N_VERIFY_RES);
    tb.check_eq(shake_ct[0], shake_sig[0], "SHAKE verify accepts its own signature");
    tb.check(!tb.dut.crypto_backend_is_conformant(),
             "SHAKE stand-in is not standards-conformant");

    tb.check(tb.zeroize_mldsa(), "zeroize before SHAKE bad-signature verify");
    std::vector<uint32_t> bad_sig = shake_sig;
    bad_sig[0] ^= 1u;
    tb.wr_n(abr::OFF_MLDSA_PUBKEY, shake_pk);
    tb.wr_n(abr::OFF_MLDSA_SIGNATURE, bad_sig);
    tb.wr_n(abr::OFF_MLDSA_MSG, std::vector<uint32_t>(abr::N_MLDSA_MSG, 0x41424344u));
    tb.check(tb.run_mldsa(0x3u), "VERIFY of a tampered SHAKE signature completes");
    tb.check(tb.rd(abr::OFF_MLDSA_VERIFY_RES) != bad_sig[0],
             "SHAKE verify rejects a tampered signature");

    tb.check(tb.zeroize_mlkem(), "zeroize ML-KEM before SHAKE round-trip");
    tb.wr_n(abr::OFF_MLKEM_SEED_D, std::vector<uint32_t>(abr::N_MLKEM_SEED, 0x11111111u));
    tb.wr_n(abr::OFF_MLKEM_SEED_Z, std::vector<uint32_t>(abr::N_MLKEM_SEED, 0x22222222u));
    tb.check(tb.run_mlkem(0x1u), "ML-KEM KEYGEN on SHAKE stand-in");
    const std::vector<uint32_t> shake_ek =
        tb.rd_n(abr::OFF_MLKEM_ENCAPS_KEY, abr::N_MLKEM_ENCAPS_KEY);
    const std::vector<uint32_t> shake_dk =
        tb.rd_n(abr::OFF_MLKEM_DECAPS_KEY, abr::N_MLKEM_DECAPS_KEY);
    tb.check(tb.zeroize_mlkem(), "zeroize before SHAKE encaps");
    tb.wr_n(abr::OFF_MLKEM_ENCAPS_KEY, shake_ek);
    tb.wr_n(abr::OFF_MLKEM_MSG, std::vector<uint32_t>(abr::N_MLKEM_MSG, 0x33333333u));
    tb.check(tb.run_mlkem(0x2u), "ENCAPS on SHAKE stand-in");
    const std::vector<uint32_t> shake_ss =
        tb.rd_n(abr::OFF_MLKEM_SHARED_KEY, abr::N_MLKEM_SHARED_KEY);
    const std::vector<uint32_t> shake_ctext =
        tb.rd_n(abr::OFF_MLKEM_CIPHERTEXT, abr::N_MLKEM_CIPHERTEXT);
    tb.check(tb.zeroize_mlkem(), "zeroize before SHAKE decaps");
    tb.wr_n(abr::OFF_MLKEM_DECAPS_KEY, shake_dk);
    tb.wr_n(abr::OFF_MLKEM_CIPHERTEXT, shake_ctext);
    tb.check(tb.run_mlkem(0x3u), "DECAPS on SHAKE stand-in");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_SHARED_KEY), shake_ss[0],
                "SHAKE decaps agrees with encaps");
    tb.check(tb.zeroize_mlkem(), "zeroize before SHAKE implicit rejection");
    std::vector<uint32_t> bad_ct = shake_ctext;
    bad_ct[0] ^= 1u;
    tb.wr_n(abr::OFF_MLKEM_DECAPS_KEY, shake_dk);
    tb.wr_n(abr::OFF_MLKEM_CIPHERTEXT, bad_ct);
    tb.check(tb.run_mlkem(0x3u), "SHAKE DECAPS of a corrupted ciphertext completes");
    tb.check(tb.rd(abr::OFF_MLKEM_SHARED_KEY) != shake_ss[0],
             "SHAKE implicit rejection differs from the real shared secret");

    tb.dut.set_crypto_backend(std::make_unique<stub_backend>());
    tb.check(std::string(tb.dut.crypto_backend_name()) == "unit-test-stub",
             "backend can be replaced at runtime");

    tb.reset_dut();
    tb.wr_n(abr::OFF_MLDSA_SEED, std::vector<uint32_t>(8, 0x12345678u));
    tb.check(tb.run_mldsa(CMD_KEYGEN), "KEYGEN runs on the replacement backend");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_PUBKEY), 0xABABABABu,
                "replacement backend supplied the public key");

    tb.check(tb.zeroize_mldsa(), "zeroize on the replacement backend");
    tb.wr_n(abr::OFF_MLDSA_PRIVKEY_IN, std::vector<uint32_t>(abr::N_MLDSA_PRIVKEY, 0u));
    tb.check(tb.run_mldsa(0x2u), "SIGN runs on the replacement backend");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_SIGNATURE), 0xEFEFEFEFu,
                "replacement backend supplied the signature");

    tb.check(tb.zeroize_mldsa(), "zeroize before replacement-backend verify");
    tb.check(tb.run_mldsa(0x3u), "VERIFY runs on the replacement backend");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_VERIFY_RES), 0x11111111u,
                "replacement backend supplied the verify result");

    tb.check(tb.zeroize_mlkem(), "zeroize ML-KEM on the replacement backend");
    tb.check(tb.run_mlkem(0x2u), "ENCAPS runs on the replacement backend");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_SHARED_KEY), 0x66666666u,
                "replacement backend supplied the shared key");

    tb.check(tb.zeroize_mlkem(), "zeroize before replacement-backend decaps");
    tb.check(tb.run_mlkem(0x3u), "DECAPS runs on the replacement backend");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_SHARED_KEY), 0x77777777u,
                "replacement backend supplied the decapsulated key");

    tb.check(tb.zeroize_mlkem(), "zeroize before replacement-backend keygen+decaps");
    tb.check(tb.run_mlkem(0x4u), "KEYGEN+DECAPS runs on the replacement backend");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_ENCAPS_KEY), 0x33333333u,
                "replacement backend supplied the encapsulation key");
}
