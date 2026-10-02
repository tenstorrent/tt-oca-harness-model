// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file mlkem_tests.cpp
 * @brief ML-KEM behavioural tests: keygen, encapsulation, decapsulation,
 *        shared-secret agreement and implicit rejection.
 */

#include "abr_testbench.h"

namespace {

constexpr uint32_t CMD_KEYGEN        = 0x1u;
constexpr uint32_t CMD_ENCAPS        = 0x2u;
constexpr uint32_t CMD_DECAPS        = 0x3u;
constexpr uint32_t CMD_KEYGEN_DECAPS = 0x4u;

constexpr uint32_t ST_READY = 1u << 0;
constexpr uint32_t ST_VALID = 1u << 1;
constexpr uint32_t ST_ERROR = 1u << 2; // ML-KEM has no MSG_STREAM_READY bit

const std::vector<uint32_t> k_seed_d = {
    0x10111213u, 0x14151617u, 0x18191a1bu, 0x1c1d1e1fu,
    0x20212223u, 0x24252627u, 0x28292a2bu, 0x2c2d2e2fu};

const std::vector<uint32_t> k_seed_z = {
    0x30313233u, 0x34353637u, 0x38393a3bu, 0x3c3d3e3fu,
    0x40414243u, 0x44454647u, 0x48494a4bu, 0x4c4d4e4fu};

const std::vector<uint32_t> k_msg = {
    0x50515253u, 0x54555657u, 0x58595a5bu, 0x5c5d5e5fu,
    0x60616263u, 0x64656667u, 0x68696a6bu, 0x6c6d6e6fu};

bool any_nonzero(const std::vector<uint32_t> &v)
{
    for (uint32_t w : v) {
        if (w != 0u) {
            return true;
        }
    }
    return false;
}

} // namespace

void mlkem_tests(abr_testbench &tb)
{
    tb.section("ML-KEM: keygen");

    tb.check(tb.zeroize_mlkem(), "zeroize before ML-KEM keygen");
    tb.wr_n(abr::OFF_MLKEM_SEED_D, k_seed_d);
    tb.wr_n(abr::OFF_MLKEM_SEED_Z, k_seed_z);

    tb.check(tb.run_mlkem(CMD_KEYGEN), "ML-KEM KEYGEN completes");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_STATUS) & ST_ERROR, 0u, "KEYGEN leaves ERROR clear");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_STATUS) & ST_READY, 0u,
                "ML-KEM KEYGEN parks with READY low");

    const std::vector<uint32_t> encaps_key =
        tb.rd_n(abr::OFF_MLKEM_ENCAPS_KEY, abr::N_MLKEM_ENCAPS_KEY);
    const std::vector<uint32_t> decaps_key =
        tb.rd_n(abr::OFF_MLKEM_DECAPS_KEY, abr::N_MLKEM_DECAPS_KEY);

    tb.check(any_nonzero(encaps_key), "KEYGEN produced a non-zero encapsulation key");
    tb.check(any_nonzero(decaps_key), "KEYGEN produced a non-zero decapsulation key");

    tb.section("ML-KEM: keygen determinism");

    tb.check(tb.zeroize_mlkem(), "zeroize after ML-KEM keygen");
    tb.wr_n(abr::OFF_MLKEM_SEED_D, k_seed_d);
    tb.wr_n(abr::OFF_MLKEM_SEED_Z, k_seed_z);
    tb.check(tb.run_mlkem(CMD_KEYGEN), "second ML-KEM KEYGEN completes");
    tb.check(tb.rd_n(abr::OFF_MLKEM_ENCAPS_KEY, abr::N_MLKEM_ENCAPS_KEY) == encaps_key,
             "same seeds reproduce the same encapsulation key");

    tb.section("ML-KEM: encapsulation");

    tb.check(tb.zeroize_mlkem(), "zeroize before ENCAPS");
    tb.wr_n(abr::OFF_MLKEM_ENCAPS_KEY, encaps_key);
    tb.wr_n(abr::OFF_MLKEM_MSG, k_msg);

    tb.check(tb.run_mlkem(CMD_ENCAPS), "ENCAPS completes");
    const std::vector<uint32_t> ciphertext =
        tb.rd_n(abr::OFF_MLKEM_CIPHERTEXT, abr::N_MLKEM_CIPHERTEXT);
    const std::vector<uint32_t> shared_key_enc =
        tb.rd_n(abr::OFF_MLKEM_SHARED_KEY, abr::N_MLKEM_SHARED_KEY);

    tb.check(any_nonzero(ciphertext), "ENCAPS produced a non-zero ciphertext");
    tb.check(any_nonzero(shared_key_enc), "ENCAPS produced a non-zero shared key");

    tb.section("ML-KEM: decapsulation agrees with encapsulation");

    tb.check(tb.zeroize_mlkem(), "zeroize before DECAPS");
    tb.wr_n(abr::OFF_MLKEM_DECAPS_KEY, decaps_key);
    tb.wr_n(abr::OFF_MLKEM_CIPHERTEXT, ciphertext);

    tb.check(tb.run_mlkem(CMD_DECAPS), "DECAPS completes");
    const std::vector<uint32_t> shared_key_dec =
        tb.rd_n(abr::OFF_MLKEM_SHARED_KEY, abr::N_MLKEM_SHARED_KEY);
    tb.check(shared_key_dec == shared_key_enc,
             "DECAPS recovers the same shared secret as ENCAPS");

    tb.section("ML-KEM: implicit rejection");

    tb.check(tb.zeroize_mlkem(), "zeroize before corrupted DECAPS");
    std::vector<uint32_t> bad_ciphertext = ciphertext;
    bad_ciphertext[100] ^= 0xFFFFFFFFu;
    tb.wr_n(abr::OFF_MLKEM_DECAPS_KEY, decaps_key);
    tb.wr_n(abr::OFF_MLKEM_CIPHERTEXT, bad_ciphertext);

    tb.check(tb.run_mlkem(CMD_DECAPS), "DECAPS of a corrupted ciphertext still completes");
    const std::vector<uint32_t> rejected_key =
        tb.rd_n(abr::OFF_MLKEM_SHARED_KEY, abr::N_MLKEM_SHARED_KEY);
    tb.check(any_nonzero(rejected_key),
             "implicit rejection still yields a shared key (no failure signalled)");
    tb.check(rejected_key != shared_key_enc,
             "implicit rejection key differs from the real shared secret");

    tb.section("ML-KEM: KEYGEN+DECAPS fused command");

    tb.check(tb.zeroize_mlkem(), "zeroize before KEYGEN+DECAPS");
    tb.wr_n(abr::OFF_MLKEM_SEED_D, k_seed_d);
    tb.wr_n(abr::OFF_MLKEM_SEED_Z, k_seed_z);
    tb.wr_n(abr::OFF_MLKEM_CIPHERTEXT, ciphertext);

    tb.check(tb.run_mlkem(CMD_KEYGEN_DECAPS), "KEYGEN+DECAPS completes");
    tb.check(tb.rd_n(abr::OFF_MLKEM_ENCAPS_KEY, abr::N_MLKEM_ENCAPS_KEY) == encaps_key,
             "KEYGEN+DECAPS regenerated the same encapsulation key");
    tb.check(tb.rd_n(abr::OFF_MLKEM_SHARED_KEY, abr::N_MLKEM_SHARED_KEY) == shared_key_enc,
             "KEYGEN+DECAPS recovered the shared secret");

    tb.section("ML-KEM: error and no-op paths");

    tb.check(tb.zeroize_mlkem(), "zeroize before invalid ML-KEM command");
    const uint32_t err_count_before = tb.rd(abr::OFF_ERROR_INTR_COUNT);
    tb.wr(abr::OFF_MLKEM_CTRL, 0x6u);
    const uint32_t bad_st = tb.rd(abr::OFF_MLKEM_STATUS);
    tb.check_eq(bad_st & ST_ERROR, ST_ERROR,
                "an invalid ML-KEM command raises STATUS.ERROR");
    tb.check_eq(bad_st & ST_VALID, 0u, "an invalid ML-KEM command leaves VALID clear");
    tb.check_eq(bad_st & ST_READY, 0u,
                "an invalid ML-KEM command parks READY low until zeroize");
    tb.check_eq(tb.rd(abr::OFF_ERROR_INTERNAL_INTR) & 1u, 1u,
                "an invalid ML-KEM command sets error interrupt status");
    tb.check_eq(tb.rd(abr::OFF_ERROR_INTR_COUNT), err_count_before + 1u,
                "an invalid ML-KEM command increments the error counter");

    tb.check(tb.zeroize_mlkem(), "zeroize recovers from the invalid ML-KEM command");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_STATUS) & ST_ERROR, 0u, "zeroize cleared ML-KEM ERROR");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_STATUS) & ST_READY, ST_READY,
                "zeroize restores ML-KEM READY after an invalid command");

    tb.wr(abr::OFF_MLKEM_CTRL, 0u);
    tb.check_eq(tb.rd(abr::OFF_MLKEM_STATUS) & ST_READY, ST_READY,
                "ML-KEM CTRL=NONE leaves the engine ready");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_STATUS) & ST_VALID, 0u,
                "ML-KEM CTRL=NONE does not assert VALID");

    tb.section("ML-KEM: zeroize clears key material");

    tb.wr_n(abr::OFF_MLKEM_SEED_D, k_seed_d);
    tb.wr_n(abr::OFF_MLKEM_SEED_Z, k_seed_z);
    tb.check(tb.run_mlkem(CMD_KEYGEN), "ML-KEM KEYGEN before zeroize check");
    tb.check(tb.zeroize_mlkem(), "ML-KEM zeroize completes");
    tb.check(!any_nonzero(tb.rd_n(abr::OFF_MLKEM_ENCAPS_KEY, abr::N_MLKEM_ENCAPS_KEY)),
             "zeroize cleared MLKEM_ENCAPS_KEY");
    tb.check(!any_nonzero(tb.rd_n(abr::OFF_MLKEM_DECAPS_KEY, abr::N_MLKEM_DECAPS_KEY)),
             "zeroize cleared MLKEM_DECAPS_KEY");
    tb.check(!any_nonzero(tb.rd_n(abr::OFF_MLKEM_SHARED_KEY, abr::N_MLKEM_SHARED_KEY)),
             "zeroize cleared MLKEM_SHARED_KEY");

    tb.section("ML-KEM: engines are independent");

    // A parked ML-DSA engine must not block ML-KEM, and vice versa.
    tb.wr_n(abr::OFF_MLDSA_SEED, std::vector<uint32_t>(8, 0x11111111u));
    tb.check(tb.run_mldsa(0x1u), "ML-DSA KEYGEN completes while ML-KEM is idle");
    tb.check_eq(tb.rd(abr::OFF_MLKEM_STATUS) & ST_READY, ST_READY,
                "ML-KEM stays ready while ML-DSA is parked");

    tb.wr_n(abr::OFF_MLKEM_SEED_D, k_seed_d);
    tb.wr_n(abr::OFF_MLKEM_SEED_Z, k_seed_z);
    tb.check(tb.run_mlkem(CMD_KEYGEN), "ML-KEM KEYGEN completes while ML-DSA is parked");

    tb.check(tb.zeroize_mldsa(), "final ML-DSA zeroize");
    tb.check(tb.zeroize_mlkem(), "final ML-KEM zeroize");
}
