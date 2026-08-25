// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file abr_pqc_mlkem.c
 * @brief PQClean ML-KEM-1024 wrappers (derandomized keygen / encaps).
 */

#include "abr_pqc_wrap.h"

#include "kem.h"
#include "params.h"

#include <string.h>

int abr_mlkem1024_keygen(const uint8_t *d, const uint8_t *z, uint8_t *ek,
                         uint8_t *dk)
{
    uint8_t coins[2 * KYBER_SYMBYTES];
    memcpy(coins, d, KYBER_SYMBYTES);
    memcpy(coins + KYBER_SYMBYTES, z, KYBER_SYMBYTES);
    return PQCLEAN_MLKEM1024_CLEAN_crypto_kem_keypair_derand(ek, dk, coins);
}

int abr_mlkem1024_encaps(const uint8_t *ek, const uint8_t *msg, uint8_t *ct,
                         uint8_t *ss)
{
    return PQCLEAN_MLKEM1024_CLEAN_crypto_kem_enc_derand(ct, ss, ek, msg);
}

int abr_mlkem1024_decaps(const uint8_t *dk, const uint8_t *ct, uint8_t *ss)
{
    return PQCLEAN_MLKEM1024_CLEAN_crypto_kem_dec(ss, ct, dk);
}
