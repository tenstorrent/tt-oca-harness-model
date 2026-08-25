// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file abr_pqc_wrap.h
 * @brief C wrappers around the vendored PQClean ML-DSA-87 / ML-KEM-1024 APIs.
 *
 * Keeps params.h (and other colliding PQClean headers) out of the C++ model.
 * Signature length is the FIPS 204 size (4627); the caller pads to the ABR
 * 4628-byte register window.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int abr_mldsa87_keygen(const uint8_t *seed, uint8_t *pk, uint8_t *sk);

int abr_mldsa87_sign_mu(const uint8_t *sk, const uint8_t *mu, const uint8_t *rnd,
                        uint8_t *sig, size_t *siglen);

int abr_mldsa87_verify_mu(const uint8_t *pk, const uint8_t *mu, const uint8_t *sig,
                          size_t siglen, uint8_t *ctilde_out);

int abr_mldsa87_compute_mu(const uint8_t *pk, const uint8_t *ctx, size_t ctxlen,
                           const uint8_t *msg, size_t mlen, uint8_t *mu_out);

int abr_mldsa87_compute_mu_from_sk(const uint8_t *sk, const uint8_t *ctx,
                                   size_t ctxlen, const uint8_t *msg, size_t mlen,
                                   uint8_t *mu_out);

int abr_mlkem1024_keygen(const uint8_t *d, const uint8_t *z, uint8_t *ek,
                         uint8_t *dk);

int abr_mlkem1024_encaps(const uint8_t *ek, const uint8_t *msg, uint8_t *ct,
                         uint8_t *ss);

int abr_mlkem1024_decaps(const uint8_t *dk, const uint8_t *ct, uint8_t *ss);

#ifdef __cplusplus
}
#endif
