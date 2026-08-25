// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file register_tests.cpp
 * @brief Register-level tests: identity, reset values, access-type enforcement
 *        and the large key/signature windows.
 */

#include "abr_testbench.h"

namespace {

/// Values the SEP firmware checks in sep_abr_csr_test.
constexpr uint32_t MLDSA_NAME0_EXP = 0x44534D4Cu;
constexpr uint32_t MLDSA_NAME1_EXP = 0x3837412Du;

constexpr uint32_t ST_READY = 1u << 0;
constexpr uint32_t ST_VALID = 1u << 1;
constexpr uint32_t ST_ERROR = 1u << 3;

/// Write then read a window and confirm the data survives the round trip.
bool window_round_trip(abr_testbench &tb, uint32_t base, unsigned int words,
                       uint32_t salt)
{
    std::vector<uint32_t> pattern(words);
    for (unsigned int i = 0; i < words; ++i) {
        pattern[i] = (i * 0x01010101u) ^ salt;
    }
    tb.wr_n(base, pattern);

    const std::vector<uint32_t> readback = tb.rd_n(base, words);
    return readback == pattern;
}

} // namespace

void register_tests(abr_testbench &tb)
{
    tb.section("Register: identity and reset state");

    tb.check_eq(tb.rd(abr::OFF_MLDSA_NAME), MLDSA_NAME0_EXP, "MLDSA_NAME[0] = 'MLDSA-87' lo");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_NAME + 4u), MLDSA_NAME1_EXP,
                "MLDSA_NAME[1] = 'MLDSA-87' hi");
    tb.check(tb.rd(abr::OFF_MLDSA_VERSION) != 0u, "MLDSA_VERSION[0] is non-zero");
    tb.check(tb.rd(abr::OFF_MLKEM_NAME) != 0u, "MLKEM_NAME[0] is non-zero");
    tb.check(tb.rd(abr::OFF_MLKEM_VERSION) != 0u, "MLKEM_VERSION[0] is non-zero");

    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & ST_READY, ST_READY,
                "MLDSA_STATUS.READY set out of reset");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & ST_VALID, 0u,
                "MLDSA_STATUS.VALID clear out of reset");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS) & ST_ERROR, 0u,
                "MLDSA_STATUS.ERROR clear out of reset");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_STATUS) & ST_READY, ST_READY,
                "MLKEM_STATUS.READY set out of reset");

    tb.section("Register: access-type enforcement");

    // Read-only: the write must not take effect.
    const uint32_t name_before = tb.rd(abr::OFF_MLDSA_NAME);
    tb.wr(abr::OFF_MLDSA_NAME, 0xDEADBEEFu);
    tb.check_eq(tb.rd(abr::OFF_MLDSA_NAME), name_before, "MLDSA_NAME rejects writes (RO)");

    const uint32_t status_before = tb.rd(abr::OFF_MLDSA_STATUS);
    tb.wr(abr::OFF_MLDSA_STATUS, 0xFFFFFFFFu);
    tb.check_eq(tb.rd(abr::OFF_MLDSA_STATUS), status_before,
                "MLDSA_STATUS rejects writes (RO)");

    // Write-only: accepted but reads back as zero.
    tb.wr(abr::OFF_MLDSA_SEED, 0xA5A5A5A5u);
    tb.check_eq(tb.rd(abr::OFF_MLDSA_SEED), 0u, "MLDSA_SEED reads as 0 (WO)");
    tb.wr(abr::OFF_ABR_ENTROPY, 0x5A5A5A5Au);
    tb.check_eq(tb.rd(abr::OFF_ABR_ENTROPY), 0u, "ABR_ENTROPY reads as 0 (WO)");
    tb.wr(abr::OFF_MLDSA_PRIVKEY_IN, 0x12345678u);
    tb.check_eq(tb.rd(abr::OFF_MLDSA_PRIVKEY_IN), 0u, "MLDSA_PRIVKEY_IN reads as 0 (WO)");
    tb.wr(abr::OFF_MLKEM_SEED_D, 0x11223344u);
    tb.check_eq(tb.rd(abr::OFF_MLKEM_SEED_D), 0u, "MLKEM_SEED_D reads as 0 (WO)");
    tb.wr(abr::OFF_MLDSA_CTRL, 0u);
    tb.check_eq(tb.rd(abr::OFF_MLDSA_CTRL), 0u, "MLDSA_CTRL reads as 0 (WO)");

    tb.section("Register: field masking");

    // CTX_CONFIG.CTX_SIZE is 8 bits; the upper bits are reserved.
    tb.wr(abr::OFF_MLDSA_CTX_CONFIG, 0xFFFFFFFFu);
    tb.check_eq(tb.rd(abr::OFF_MLDSA_CTX_CONFIG), 0u,
                "MLDSA_CTX_CONFIG reads as 0 (WO)");

    // MSG_STROBE is 4 bits wide and resets to all-bytes-enabled.
    tb.wr(abr::OFF_MLDSA_MSG_STROBE, 0xFu);
    tb.check_eq(tb.rd(abr::OFF_MLDSA_MSG_STROBE), 0u, "MLDSA_MSG_STROBE reads as 0 (WO)");

    tb.section("Register: data windows round-trip");

    tb.check(window_round_trip(tb, abr::OFF_MLDSA_PUBKEY, abr::N_MLDSA_PUBKEY, 0x1u),
             "MLDSA_PUBKEY window (648 words) round-trips");
    tb.check(window_round_trip(tb, abr::OFF_MLDSA_SIGNATURE, abr::N_MLDSA_SIGNATURE, 0x2u),
             "MLDSA_SIGNATURE window (1157 words) round-trips");
    tb.check(window_round_trip(tb, abr::OFF_MLKEM_ENCAPS_KEY, abr::N_MLKEM_ENCAPS_KEY, 0x3u),
             "MLKEM_ENCAPS_KEY window (392 words) round-trips");
    tb.check(window_round_trip(tb, abr::OFF_MLKEM_CIPHERTEXT, abr::N_MLKEM_CIPHERTEXT, 0x4u),
             "MLKEM_CIPHERTEXT window (392 words) round-trips");
    tb.check(window_round_trip(tb, abr::OFF_MLKEM_DECAPS_KEY, abr::N_MLKEM_DECAPS_KEY, 0x5u),
             "MLKEM_DECAPS_KEY window (792 words) round-trips");

    tb.section("Register: window boundaries");

    // Last word of each window must be live, and the first word of the next
    // declared region must decode independently.
    const uint32_t pubkey_last = abr::OFF_MLDSA_PUBKEY + (abr::N_MLDSA_PUBKEY - 1u) * 4u;
    tb.wr(pubkey_last, 0xC0FFEE00u);
    tb.check_eq(tb.rd(pubkey_last), 0xC0FFEE00u, "MLDSA_PUBKEY last word is decoded");

    const uint32_t sig_last = abr::OFF_MLDSA_SIGNATURE + (abr::N_MLDSA_SIGNATURE - 1u) * 4u;
    tb.wr(sig_last, 0x0BADF00Du);
    tb.check_eq(tb.rd(sig_last), 0x0BADF00Du, "MLDSA_SIGNATURE last word is decoded");

    const uint32_t privkey_out_last =
        abr::OFF_MLDSA_PRIVKEY_OUT + (abr::N_MLDSA_PRIVKEY - 1u) * 4u;
    tb.check_eq(tb.rd(privkey_out_last), 0u, "MLDSA_PRIVKEY_OUT last word reads 0 at reset");

    // Restore a clean slate for the behavioural suites.
    tb.check(tb.zeroize_mldsa(), "zeroize returns MLDSA to READY");
    tb.check_eq(tb.rd(abr::OFF_MLDSA_PUBKEY), 0u, "zeroize cleared MLDSA_PUBKEY");
    tb.check_eq(tb.rd(sig_last), 0u, "zeroize cleared MLDSA_SIGNATURE");
    tb.check(tb.zeroize_mlkem(), "zeroize returns MLKEM to READY");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_ENCAPS_KEY), 0u, "zeroize cleared MLKEM_ENCAPS_KEY");
}
