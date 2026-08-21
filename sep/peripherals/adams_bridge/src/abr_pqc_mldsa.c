/**
 * @file abr_pqc_mldsa.c
 * @brief PQClean ML-DSA-87 wrappers (seeded keygen, external-mu sign/verify).
 */

#include "abr_pqc_wrap.h"

#include "fips202.h"
#include "params.h"
#include "sign.h"

#include <string.h>

/* pack_sk layout: rho (32) || key (32) || tr (64) || ... */
#define ABR_MLDSA_TR_OFF (SEEDBYTES + SEEDBYTES)

static int compute_mu_from_tr(const uint8_t *tr, const uint8_t *ctx, size_t ctxlen,
                              const uint8_t *msg, size_t mlen, uint8_t *mu_out)
{
    uint8_t pre[2];
    shake256incctx state;

    if (ctxlen > 255u) { // GCOV_EXCL_LINE
        return -1;       // GCOV_EXCL_LINE
    }

    pre[0] = 0;
    pre[1] = (uint8_t)ctxlen;

    shake256_inc_init(&state);
    shake256_inc_absorb(&state, tr, TRBYTES);
    shake256_inc_absorb(&state, pre, 2);
    if (ctxlen > 0u && ctx != NULL) {
        shake256_inc_absorb(&state, ctx, ctxlen);
    }
    if (mlen > 0u && msg != NULL) {
        shake256_inc_absorb(&state, msg, mlen);
    }
    shake256_inc_finalize(&state);
    shake256_inc_squeeze(mu_out, CRHBYTES, &state);
    shake256_inc_ctx_release(&state);
    return 0;
}

int abr_mldsa87_keygen(const uint8_t *seed, uint8_t *pk, uint8_t *sk)
{
    return PQCLEAN_MLDSA87_CLEAN_crypto_sign_keypair_internal(pk, sk, seed);
}

int abr_mldsa87_sign_mu(const uint8_t *sk, const uint8_t *mu, const uint8_t *rnd,
                        uint8_t *sig, size_t *siglen)
{
    return PQCLEAN_MLDSA87_CLEAN_crypto_sign_signature_internal(sig, siglen, mu, rnd, sk);
}

int abr_mldsa87_verify_mu(const uint8_t *pk, const uint8_t *mu, const uint8_t *sig,
                          size_t siglen, uint8_t *ctilde_out)
{
    return PQCLEAN_MLDSA87_CLEAN_crypto_sign_verify_internal(sig, siglen, mu, pk, ctilde_out);
}

int abr_mldsa87_compute_mu(const uint8_t *pk, const uint8_t *ctx, size_t ctxlen,
                           const uint8_t *msg, size_t mlen, uint8_t *mu_out)
{
    uint8_t tr[TRBYTES];
    shake256(tr, TRBYTES, pk, PQCLEAN_MLDSA87_CLEAN_CRYPTO_PUBLICKEYBYTES);
    return compute_mu_from_tr(tr, ctx, ctxlen, msg, mlen, mu_out);
}

int abr_mldsa87_compute_mu_from_sk(const uint8_t *sk, const uint8_t *ctx,
                                   size_t ctxlen, const uint8_t *msg, size_t mlen,
                                   uint8_t *mu_out)
{
    return compute_mu_from_tr(sk + ABR_MLDSA_TR_OFF, ctx, ctxlen, msg, mlen, mu_out);
}
