// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file abr_crypto.cpp
 * @brief SHAKE256-based functional stand-in for the ABR PQC math.
 *
 * See abr_crypto.h for the construction and its (deliberate) limits: this is
 * deterministic and round-trip exact, but it is not FIPS 204 / FIPS 203 and
 * does not reproduce NIST ACVP vectors.
 */

#include "abr_crypto.h"
#include "abr_pqc_wrap.h"

#include <openssl/evp.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>

namespace abr {

namespace {

/// Append a byte range to @p dst.
void append(bytes &dst, const uint8_t *src, std::size_t len)
{
    dst.insert(dst.end(), src, src + len);
}

/// Append a domain-separation tag (without its NUL terminator).
void append_tag(bytes &dst, const char *tag)
{
    append(dst, reinterpret_cast<const uint8_t *>(tag), std::strlen(tag));
}

/// Append a whole buffer.
void append(bytes &dst, const bytes &src)
{
    dst.insert(dst.end(), src.begin(), src.end());
}

} // namespace

void shake256(bytes &out, std::size_t outlen, const bytes &in)
{
    out.assign(outlen, 0u);
    if (outlen == 0u) {
        return;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (ctx == nullptr) { // LCOV_EXCL_BR_LINE — OpenSSL alloc failure
        throw std::runtime_error("abr: EVP_MD_CTX_new failed"); // LCOV_EXCL_LINE
    }

    // Any failure here would silently produce a zero digest, which would then
    // look like a plausible (but wrong) crypto result, so fail loudly instead.
    const bool ok = (EVP_DigestInit_ex(ctx, EVP_shake256(), nullptr) == 1) &&
                    (EVP_DigestUpdate(ctx, in.empty() ? nullptr : in.data(), in.size()) == 1) &&
                    (EVP_DigestFinalXOF(ctx, out.data(), outlen) == 1);

    EVP_MD_CTX_free(ctx);

    if (!ok) { // LCOV_EXCL_BR_LINE — OpenSSL digest failure
        throw std::runtime_error("abr: SHAKE256 evaluation failed"); // LCOV_EXCL_LINE
    }
}

// =============================================================================
// ML-DSA
// =============================================================================

void abr_shake_backend::mldsa_keygen(const bytes &seed, bytes &pubkey, bytes &privkey)
{
    bytes in;
    append_tag(in, "ABR-MLDSA-PK");
    append(in, seed);
    shake256(pubkey, MLDSA_PUBKEY_BYTES, in);

    // The private key carries the public key verbatim in its leading bytes so
    // that sign() can recover pk without a separate handle, mirroring how the
    // real sk embeds rho/tr.
    bytes tail_in;
    append_tag(tail_in, "ABR-MLDSA-SK");
    append(tail_in, seed);

    bytes tail;
    shake256(tail, MLDSA_PRIVKEY_BYTES - MLDSA_PUBKEY_BYTES, tail_in);

    privkey.clear();
    privkey.reserve(MLDSA_PRIVKEY_BYTES);
    append(privkey, pubkey);
    append(privkey, tail);
}

void abr_shake_backend::mldsa_compute_mu(const bytes &pubkey, const bytes &ctx,
                                         const bytes &msg, bytes &mu_out)
{
    bytes in;
    append_tag(in, "ABR-MLDSA-MU");
    append(in, pubkey);
    // Length-prefix the context so that ctx||msg cannot be re-split ambiguously.
    in.push_back(static_cast<uint8_t>(ctx.size()));
    append(in, ctx);
    append(in, msg);
    shake256(mu_out, MLDSA_MU_BYTES, in);
}

void abr_shake_backend::mldsa_compute_mu_from_sk(const bytes &privkey, const bytes &ctx,
                                                 const bytes &msg, bytes &mu_out)
{
    const bytes pubkey(privkey.begin(), privkey.begin() + MLDSA_PUBKEY_BYTES);
    mldsa_compute_mu(pubkey, ctx, msg, mu_out);
}

void abr_shake_backend::mldsa_sign(const bytes &privkey, const bytes &mu,
                                   const bytes &rnd, bytes &signature)
{
    const bytes pubkey(privkey.begin(), privkey.begin() + MLDSA_PUBKEY_BYTES);

    // The randomizer is absorbed only into ctilde. Verification reads ctilde
    // back out of the signature, so it never needs rnd itself -- which is what
    // lets a randomized signature stay verifiable from the public key alone.
    bytes ct_in;
    append_tag(ct_in, "ABR-MLDSA-CT");
    append(ct_in, pubkey);
    append(ct_in, mu);
    append(ct_in, rnd);

    bytes ctilde;
    shake256(ctilde, MLDSA_CTILDE_BYTES, ct_in);

    bytes body_in;
    append_tag(body_in, "ABR-MLDSA-BD");
    append(body_in, ctilde);
    append(body_in, pubkey);
    append(body_in, mu);

    bytes body;
    shake256(body, MLDSA_SIG_BYTES - MLDSA_CTILDE_BYTES, body_in);

    signature.clear();
    signature.reserve(MLDSA_SIG_BYTES);
    append(signature, ctilde);
    append(signature, body);
}

void abr_shake_backend::mldsa_verify(const bytes &pubkey, const bytes &mu,
                                     const bytes &signature, bytes &ctilde_out)
{
    const bytes ctilde(signature.begin(), signature.begin() + MLDSA_CTILDE_BYTES);

    bytes body_in;
    append_tag(body_in, "ABR-MLDSA-BD");
    append(body_in, ctilde);
    append(body_in, pubkey);
    append(body_in, mu);

    bytes expected_body;
    shake256(expected_body, MLDSA_SIG_BYTES - MLDSA_CTILDE_BYTES, body_in);

    const bool body_ok = std::equal(expected_body.begin(), expected_body.end(),
                                    signature.begin() + MLDSA_CTILDE_BYTES);

    if (body_ok) {
        ctilde_out = ctilde;
        return;
    }

    // Hardware reports a bad signature by producing a c-tilde that will not
    // match the one in the signature; derive it so the result stays
    // deterministic rather than leaving stale data behind.
    bytes fail_in;
    append_tag(fail_in, "ABR-MLDSA-FAIL");
    append(fail_in, pubkey);
    append(fail_in, mu);
    shake256(ctilde_out, MLDSA_CTILDE_BYTES, fail_in);
}

// =============================================================================
// ML-KEM
// =============================================================================

void abr_shake_backend::mlkem_keygen(const bytes &d, const bytes &z, bytes &encaps_key,
                                     bytes &decaps_key)
{
    bytes ek_in;
    append_tag(ek_in, "ABR-MLKEM-EK");
    append(ek_in, d);
    shake256(encaps_key, MLKEM_ENCAPS_KEY_BYTES, ek_in);

    // As with ML-DSA, dk embeds ek so decaps can recover it; FIPS 203 dk also
    // contains a full copy of ek.
    bytes tail_in;
    append_tag(tail_in, "ABR-MLKEM-DK");
    append(tail_in, d);
    append(tail_in, z);

    bytes tail;
    shake256(tail, MLKEM_DECAPS_KEY_BYTES - MLKEM_ENCAPS_KEY_BYTES, tail_in);

    decaps_key.clear();
    decaps_key.reserve(MLKEM_DECAPS_KEY_BYTES);
    append(decaps_key, encaps_key);
    append(decaps_key, tail);
}

namespace {

/// One-time pad that lets decaps recover the encapsulated message from ct.
void mlkem_mask(const bytes &encaps_key, bytes &mask_out)
{
    bytes in;
    append_tag(in, "ABR-MLKEM-MASK");
    append(in, encaps_key);
    shake256(mask_out, MLKEM_MSG_BYTES, in);
}

/// Shared secret derived from the encapsulation key and message.
void mlkem_shared(const bytes &encaps_key, const bytes &msg, bytes &ss_out)
{
    bytes in;
    append_tag(in, "ABR-MLKEM-SS");
    append(in, encaps_key);
    append(in, msg);
    shake256(ss_out, MLKEM_SHARED_KEY_BYTES, in);
}

/// Ciphertext body (everything after the masked message prefix).
void mlkem_body(const bytes &encaps_key, const bytes &msg, bytes &body_out)
{
    bytes in;
    append_tag(in, "ABR-MLKEM-CT");
    append(in, encaps_key);
    append(in, msg);
    shake256(body_out, MLKEM_CIPHERTEXT_BYTES - MLKEM_MSG_BYTES, in);
}

} // namespace

void abr_shake_backend::mlkem_encaps(const bytes &encaps_key, const bytes &msg,
                                     bytes &ciphertext, bytes &shared_key)
{
    bytes mask;
    mlkem_mask(encaps_key, mask);

    ciphertext.assign(MLKEM_CIPHERTEXT_BYTES, 0u);
    for (std::size_t i = 0; i < MLKEM_MSG_BYTES; ++i) {
        ciphertext[i] = static_cast<uint8_t>(msg[i] ^ mask[i]);
    }

    bytes body;
    mlkem_body(encaps_key, msg, body);
    std::copy(body.begin(), body.end(), ciphertext.begin() + MLKEM_MSG_BYTES);

    mlkem_shared(encaps_key, msg, shared_key);
}

void abr_shake_backend::mlkem_decaps(const bytes &decaps_key, const bytes &ciphertext,
                                     bytes &shared_key)
{
    const bytes encaps_key(decaps_key.begin(),
                           decaps_key.begin() + MLKEM_ENCAPS_KEY_BYTES);

    bytes mask;
    mlkem_mask(encaps_key, mask);

    bytes msg(MLKEM_MSG_BYTES, 0u);
    for (std::size_t i = 0; i < MLKEM_MSG_BYTES; ++i) {
        msg[i] = static_cast<uint8_t>(ciphertext[i] ^ mask[i]);
    }

    bytes expected_body;
    mlkem_body(encaps_key, msg, expected_body);

    const bool body_ok = std::equal(expected_body.begin(), expected_body.end(),
                                    ciphertext.begin() + MLKEM_MSG_BYTES);

    if (body_ok) {
        mlkem_shared(encaps_key, msg, shared_key);
        return;
    }

    // FIPS 203 implicit rejection: a malformed ciphertext still yields a
    // shared key, derived from the rejection secret so it is unpredictable to
    // the sender but deterministic for a given (dk, ct).
    bytes in;
    append_tag(in, "ABR-MLKEM-REJECT");
    append(in, bytes(decaps_key.begin() + MLKEM_ENCAPS_KEY_BYTES, decaps_key.end()));
    append(in, ciphertext);
    shake256(shared_key, MLKEM_SHARED_KEY_BYTES, in);
}

// =============================================================================
// FIPS 204 / FIPS 203 (PQClean)
// =============================================================================

namespace {

constexpr std::size_t MLDSA_FIPS_SIG_BYTES = 4627u;

void require_ok(int rc, const char *what)
{
    if (rc != 0) { // LCOV_EXCL_BR_LINE — PQClean returns 0 for every seeded API we call
        throw std::runtime_error(std::string("abr fips backend: ") + what + " failed"); // LCOV_EXCL_LINE
    }
}

} // namespace

void abr_fips_backend::mldsa_keygen(const bytes &seed, bytes &pubkey, bytes &privkey)
{
    pubkey.assign(MLDSA_PUBKEY_BYTES, 0u);
    privkey.assign(MLDSA_PRIVKEY_BYTES, 0u);
    require_ok(abr_mldsa87_keygen(seed.data(), pubkey.data(), privkey.data()),
               "ML-DSA-87 keygen");
}

void abr_fips_backend::mldsa_compute_mu(const bytes &pubkey, const bytes &ctx,
                                        const bytes &msg, bytes &mu_out)
{
    mu_out.assign(MLDSA_MU_BYTES, 0u);
    require_ok(abr_mldsa87_compute_mu(pubkey.data(),
                                      ctx.empty() ? nullptr : ctx.data(), ctx.size(),
                                      msg.empty() ? nullptr : msg.data(), msg.size(),
                                      mu_out.data()),
               "ML-DSA-87 compute_mu");
}

void abr_fips_backend::mldsa_compute_mu_from_sk(const bytes &privkey, const bytes &ctx,
                                                const bytes &msg, bytes &mu_out)
{
    mu_out.assign(MLDSA_MU_BYTES, 0u);
    require_ok(abr_mldsa87_compute_mu_from_sk(privkey.data(),
                                              ctx.empty() ? nullptr : ctx.data(),
                                              ctx.size(),
                                              msg.empty() ? nullptr : msg.data(),
                                              msg.size(), mu_out.data()),
               "ML-DSA-87 compute_mu_from_sk");
}

void abr_fips_backend::mldsa_sign(const bytes &privkey, const bytes &mu,
                                  const bytes &rnd, bytes &signature)
{
    signature.assign(MLDSA_SIG_BYTES, 0u);
    size_t siglen = 0;
    require_ok(abr_mldsa87_sign_mu(privkey.data(), mu.data(), rnd.data(),
                                   signature.data(), &siglen),
               "ML-DSA-87 sign");
    if (siglen > MLDSA_SIG_BYTES) { // LCOV_EXCL_BR_LINE — FIPS signature length is fixed
        throw std::runtime_error("abr fips backend: signature longer than ABR window"); // LCOV_EXCL_LINE
    }
}

void abr_fips_backend::mldsa_verify(const bytes &pubkey, const bytes &mu,
                                    const bytes &signature, bytes &ctilde_out)
{
    ctilde_out.assign(MLDSA_CTILDE_BYTES, 0u);
    const size_t siglen = (signature.size() >= MLDSA_FIPS_SIG_BYTES)
                              ? MLDSA_FIPS_SIG_BYTES
                              : signature.size();
    // Always returns the recomputed c-tilde; firmware compares it to sig[0:64].
    (void)abr_mldsa87_verify_mu(pubkey.data(), mu.data(), signature.data(), siglen,
                                ctilde_out.data());
}

void abr_fips_backend::mlkem_keygen(const bytes &d, const bytes &z, bytes &encaps_key,
                                    bytes &decaps_key)
{
    encaps_key.assign(MLKEM_ENCAPS_KEY_BYTES, 0u);
    decaps_key.assign(MLKEM_DECAPS_KEY_BYTES, 0u);
    require_ok(abr_mlkem1024_keygen(d.data(), z.data(), encaps_key.data(),
                                    decaps_key.data()),
               "ML-KEM-1024 keygen");
}

void abr_fips_backend::mlkem_encaps(const bytes &encaps_key, const bytes &msg,
                                    bytes &ciphertext, bytes &shared_key)
{
    ciphertext.assign(MLKEM_CIPHERTEXT_BYTES, 0u);
    shared_key.assign(MLKEM_SHARED_KEY_BYTES, 0u);
    require_ok(abr_mlkem1024_encaps(encaps_key.data(), msg.data(), ciphertext.data(),
                                    shared_key.data()),
               "ML-KEM-1024 encaps");
}

void abr_fips_backend::mlkem_decaps(const bytes &decaps_key, const bytes &ciphertext,
                                    bytes &shared_key)
{
    shared_key.assign(MLKEM_SHARED_KEY_BYTES, 0u);
    require_ok(abr_mlkem1024_decaps(decaps_key.data(), ciphertext.data(),
                                    shared_key.data()),
               "ML-KEM-1024 decaps");
}

} // namespace abr
