/**
 * @file abr_crypto.h
 * @brief Pluggable cryptographic backend for the Adams Bridge model.
 *
 * The register/FSM/timing layers of the model are independent of how the
 * ML-DSA and ML-KEM math is actually performed, so the math sits behind this
 * interface. Two things follow from that:
 *
 *   - The default backend (abr_fips_backend) is PQClean ML-DSA-87 + ML-KEM-1024
 *     and is expected to reproduce NIST ACVP / FIPS 204 / FIPS 203 KATs.
 *   - abr_shake_backend remains as a deterministic functional stand-in for
 *     tests that do not need bit-exact NIST vectors; install it with
 *     abr_ip::set_crypto_backend().
 *
 * The stand-in is built from SHAKE256, the same XOF the real algorithms use
 * for expansion, and is available in OpenSSL 1.1.1 as well as 3.x.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace abr {

// --- ML-DSA-87 parameter sizes (bytes) ---------------------------------------
static constexpr std::size_t MLDSA_SEED_BYTES    = 32u;
static constexpr std::size_t MLDSA_RND_BYTES     = 32u;
static constexpr std::size_t MLDSA_MU_BYTES      = 64u;
static constexpr std::size_t MLDSA_CTILDE_BYTES  = 64u;
static constexpr std::size_t MLDSA_PUBKEY_BYTES  = 2592u;
static constexpr std::size_t MLDSA_PRIVKEY_BYTES = 4896u;
static constexpr std::size_t MLDSA_SIG_BYTES     = 4628u;

// --- ML-KEM-1024 parameter sizes (bytes) --------------------------------------
static constexpr std::size_t MLKEM_SEED_BYTES       = 32u;
static constexpr std::size_t MLKEM_MSG_BYTES        = 32u;
static constexpr std::size_t MLKEM_SHARED_KEY_BYTES = 32u;
static constexpr std::size_t MLKEM_ENCAPS_KEY_BYTES = 1568u;
static constexpr std::size_t MLKEM_DECAPS_KEY_BYTES = 3168u;
static constexpr std::size_t MLKEM_CIPHERTEXT_BYTES = 1568u;

/// Byte buffer used across the backend interface.
using bytes = std::vector<uint8_t>;

/**
 * @brief Interface implemented by any ABR math provider.
 *
 * All buffers are exactly the sizes declared above; the model guarantees this
 * before calling, so implementations may assume it.
 */
class abr_crypto_backend
{
  public:
    virtual ~abr_crypto_backend() = default;

    /// Human-readable backend name, reported at elaboration.
    virtual const char *name() const = 0;

    /// True when the backend reproduces NIST ACVP/KAT vectors bit-exactly.
    virtual bool is_standards_conformant() const = 0;

    // --- ML-DSA -----------------------------------------------------------------

    /// Expand @p seed into a public/private keypair.
    virtual void mldsa_keygen(const bytes &seed, bytes &pubkey, bytes &privkey) = 0;

    /// Sign message representative @p mu under @p privkey with randomizer @p rnd.
    virtual void mldsa_sign(const bytes &privkey, const bytes &mu, const bytes &rnd,
                            bytes &signature) = 0;

    /**
     * @brief Verify @p signature over @p mu under @p pubkey.
     * @param ctilde_out Recomputed challenge; firmware compares it against the
     *                   first 64 bytes of the signature, so a mismatch here is
     *                   how a bad signature is reported (there is no boolean).
     */
    virtual void mldsa_verify(const bytes &pubkey, const bytes &mu,
                              const bytes &signature, bytes &ctilde_out) = 0;

    /// Compute the message representative mu bound to @p pubkey, @p ctx, @p msg.
    virtual void mldsa_compute_mu(const bytes &pubkey, const bytes &ctx,
                                  const bytes &msg, bytes &mu_out) = 0;

    /// Compute mu from the tr bound into @p privkey (FIPS 204 Sign path).
    virtual void mldsa_compute_mu_from_sk(const bytes &privkey, const bytes &ctx,
                                          const bytes &msg, bytes &mu_out) = 0;

    // --- ML-KEM ------------------------------------------------------------------

    /// Expand seeds @p d and @p z into an encapsulation/decapsulation keypair.
    virtual void mlkem_keygen(const bytes &d, const bytes &z, bytes &encaps_key,
                              bytes &decaps_key) = 0;

    /// Encapsulate under @p encaps_key using message randomness @p msg.
    virtual void mlkem_encaps(const bytes &encaps_key, const bytes &msg,
                              bytes &ciphertext, bytes &shared_key) = 0;

    /// Decapsulate @p ciphertext under @p decaps_key (implicit rejection on failure).
    virtual void mlkem_decaps(const bytes &decaps_key, const bytes &ciphertext,
                              bytes &shared_key) = 0;
};

/**
 * @brief Default deterministic backend built on SHAKE256.
 *
 * Construction (domain-separated so no two uses collide):
 *   ML-DSA  pk  = SHAKE256("ABR-MLDSA-PK"  || seed)
 *           sk  = pk || SHAKE256("ABR-MLDSA-SK" || seed)      [pk recoverable]
 *           mu  = SHAKE256("ABR-MLDSA-MU"  || pk || ctx || msg)
 *           sig = ctilde || body
 *             ctilde = SHAKE256("ABR-MLDSA-CT" || pk || mu || rnd)
 *             body   = SHAKE256("ABR-MLDSA-BD" || ctilde || pk || mu)
 *   verify recomputes body from the presented ctilde, pk and mu; on match it
 *   returns that ctilde, otherwise it returns a value derived from the
 *   mismatch so the firmware comparison fails.
 *
 *   ML-KEM  ek  = SHAKE256("ABR-MLKEM-EK" || d)
 *           dk  = ek || SHAKE256("ABR-MLKEM-DK" || d || z)     [ek recoverable]
 *           ct  = (m XOR mask(ek)) || SHAKE256("ABR-MLKEM-CT" || ek || m)
 *           ss  = SHAKE256("ABR-MLKEM-SS" || ek || m)
 *   decaps recovers m from the masked prefix, recomputes ct and compares; a
 *   mismatch yields an implicit-rejection key derived from dk and ct, matching
 *   the FIPS 203 behaviour of never signalling failure explicitly.
 */
class abr_shake_backend : public abr_crypto_backend
{
  public:
    const char *name() const override { return "shake256-functional"; }
    bool is_standards_conformant() const override { return false; }

    void mldsa_keygen(const bytes &seed, bytes &pubkey, bytes &privkey) override;
    void mldsa_sign(const bytes &privkey, const bytes &mu, const bytes &rnd,
                    bytes &signature) override;
    void mldsa_verify(const bytes &pubkey, const bytes &mu, const bytes &signature,
                      bytes &ctilde_out) override;
    void mldsa_compute_mu(const bytes &pubkey, const bytes &ctx, const bytes &msg,
                          bytes &mu_out) override;
    void mldsa_compute_mu_from_sk(const bytes &privkey, const bytes &ctx,
                                  const bytes &msg, bytes &mu_out) override;

    void mlkem_keygen(const bytes &d, const bytes &z, bytes &encaps_key,
                      bytes &decaps_key) override;
    void mlkem_encaps(const bytes &encaps_key, const bytes &msg, bytes &ciphertext,
                      bytes &shared_key) override;
    void mlkem_decaps(const bytes &decaps_key, const bytes &ciphertext,
                      bytes &shared_key) override;
};

/**
 * @brief SHAKE256 extendable-output function.
 * @param out    Destination buffer, resized to @p outlen.
 * @param outlen Requested output length in bytes.
 * @param in     Concatenated input; callers pass a domain tag as the first piece.
 *
 * Exposed because the model itself needs an XOF for zeroize fill patterns and
 * the testbench uses it to predict expected values.
 */
void shake256(bytes &out, std::size_t outlen, const bytes &in);

/**
 * @brief FIPS 204 / FIPS 203 backend (PQClean ML-DSA-87 + ML-KEM-1024).
 *
 * Default for abr_ip. Signatures are 4627 bytes; the model pads the ABR
 * 4628-byte window. ABR_ENTROPY is unused (SCA-only in RTL).
 */
class abr_fips_backend : public abr_crypto_backend
{
  public:
    const char *name() const override { return "pqclean-ml-dsa-87+ml-kem-1024"; }
    bool is_standards_conformant() const override { return true; }

    void mldsa_keygen(const bytes &seed, bytes &pubkey, bytes &privkey) override;
    void mldsa_sign(const bytes &privkey, const bytes &mu, const bytes &rnd,
                    bytes &signature) override;
    void mldsa_verify(const bytes &pubkey, const bytes &mu, const bytes &signature,
                      bytes &ctilde_out) override;
    void mldsa_compute_mu(const bytes &pubkey, const bytes &ctx, const bytes &msg,
                          bytes &mu_out) override;
    void mldsa_compute_mu_from_sk(const bytes &privkey, const bytes &ctx,
                                  const bytes &msg, bytes &mu_out) override;

    void mlkem_keygen(const bytes &d, const bytes &z, bytes &encaps_key,
                      bytes &decaps_key) override;
    void mlkem_encaps(const bytes &encaps_key, const bytes &msg, bytes &ciphertext,
                      bytes &shared_key) override;
    void mlkem_decaps(const bytes &decaps_key, const bytes &ciphertext,
                      bytes &shared_key) override;
};

} // namespace abr
